#include "libvisionpipe/libvisionpipe.h"
#include "libvisionpipe/frame_transport.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>

namespace visionpipe {

// ============================================================================
// Session Implementation
// ============================================================================

Session::Session(const std::string& sessionId, pid_t pid, const VisionPipeConfig& config)
    : _sessionId(sessionId), _pid(pid), _config(config) {
    _state.store(SessionState::RUNNING, std::memory_order_release);
}

Session::~Session() {
    stop(2000);
}

Session::Session(Session&& other) noexcept
    : _sessionId(std::move(other._sessionId))
    , _pid(other._pid)
    , _config(std::move(other._config))
    , _state(other._state.load())
    , _exitCode(other._exitCode)
    , _sinks(std::move(other._sinks))
    , _stopRequested(other._stopRequested.load()) {
    other._pid = -1;
}

Session& Session::operator=(Session&& other) noexcept {
    if (this != &other) {
        stop(1000);
        _sessionId = std::move(other._sessionId);
        _pid = other._pid;
        _config = std::move(other._config);
        _state.store(other._state.load());
        _exitCode = other._exitCode;
        _sinks = std::move(other._sinks);
        _stopRequested.store(other._stopRequested.load());
        other._pid = -1;
    }
    return *this;
}

void Session::onFrame(const std::string& sinkName, FrameCallback callback) {
    auto& sink = _sinks[sinkName];
    sink.callbacks.push_back(std::move(callback));

    // Try to connect if not yet connected
    if (!sink.connected && _state.load() == SessionState::RUNNING) {
        connectSink(sinkName);
    }
}

bool Session::grabFrame(const std::string& sinkName, cv::Mat& frame) {
    auto it = _sinks.find(sinkName);
    if (it == _sinks.end()) {
        // Create sink entry and try to connect
        auto& sink = _sinks[sinkName];
        connectSink(sinkName);
        it = _sinks.find(sinkName);
    }

    auto& sink = it->second;
    if (!sink.connected) {
        connectSink(sinkName);
        if (!sink.connected) return false;
    }

    int rows, cols, type, step;
    const uint8_t* data;
    uint64_t seq;

    if (!sink.consumer->readFrame(rows, cols, type, step, data, seq)) {
        return false;
    }

    if (seq == sink.lastDispatchedSeq) {
        return false; // No new frame
    }

    // Create cv::Mat that references the shared memory (zero-copy read)
    // User should clone() if they need to hold onto the data
    frame = cv::Mat(rows, cols, type, const_cast<uint8_t*>(data), step);
    sink.lastDispatchedSeq = seq;
    return true;
}

void Session::spin(int pollIntervalMs) {
    while (!_stopRequested.load(std::memory_order_acquire)) {
        checkProcess();

        if (_state.load() != SessionState::RUNNING &&
            _state.load() != SessionState::STARTING) {
            break;
        }

        int dispatched = spinOnce();
        if (dispatched == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        }
    }
}

int Session::spinOnce() {
    int total = 0;
    for (auto& [name, sink] : _sinks) {
        if (!sink.connected) {
            connectSink(name);
        }
        if (sink.connected) {
            total += dispatchSink(name, sink);
        }
    }
    return total;
}

void Session::stop(int timeoutMs) {
    _stopRequested.store(true, std::memory_order_release);

    if (_pid <= 0) return;

    // Send SIGINT first (graceful)
    if (kill(_pid, SIGINT) == 0) {
        if (waitForExit(timeoutMs)) {
            return;
        }
        // Force kill
        kill(_pid, SIGKILL);
        waitForExit(1000);
    }

    // Clean up sinks
    for (auto& [name, sink] : _sinks) {
        if (sink.consumer) {
            sink.consumer->close();
        }
    }
    _sinks.clear();
}

bool Session::isRunning() const {
    SessionState s = _state.load(std::memory_order_acquire);
    return s == SessionState::RUNNING || s == SessionState::STARTING;
}

SessionState Session::state() const {
    return _state.load(std::memory_order_acquire);
}

int Session::exitCode() const {
    return _exitCode;
}

bool Session::waitForExit(int timeoutMs) {
    if (_pid <= 0) return true;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (true) {
        int status;
        pid_t result = waitpid(_pid, &status, WNOHANG);

        if (result == _pid) {
            if (WIFEXITED(status)) {
                _exitCode = WEXITSTATUS(status);
                _state.store(SessionState::STOPPED, std::memory_order_release);
            } else if (WIFSIGNALED(status)) {
                _exitCode = -WTERMSIG(status);
                _state.store(SessionState::ERROR, std::memory_order_release);
            }
            _pid = -1;
            return true;
        }

        if (result < 0) {
            // Process already gone
            _pid = -1;
            _state.store(SessionState::STOPPED, std::memory_order_release);
            return true;
        }

        if (timeoutMs == 0) {
            // Wait forever
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            return false; // Timed out
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void Session::connectSink(const std::string& sinkName) {
    auto& sink = _sinks[sinkName];
    if (sink.connected) return;

    if (!sink.consumer) {
        sink.consumer = std::make_unique<FrameShmConsumer>();
    }

    std::string shmName = buildShmName(_sessionId, sinkName);
    if (sink.consumer->open(shmName, _config.sinkTimeoutMs)) {
        sink.connected = true;
    }
}

int Session::dispatchSink(const std::string& /*sinkName*/, SinkConnection& sink) {
    if (!sink.consumer || !sink.connected) return 0;
    if (!sink.consumer->hasNewFrame()) return 0;

    int rows, cols, type, step;
    const uint8_t* data;
    uint64_t seq;

    if (!sink.consumer->readFrame(rows, cols, type, step, data, seq)) {
        return 0;
    }

    if (seq == sink.lastDispatchedSeq) return 0;

    // Create a cv::Mat referencing the shared memory data (zero-copy)
    cv::Mat frame(rows, cols, type, const_cast<uint8_t*>(data), step);

    for (auto& cb : sink.callbacks) {
        cb(frame, seq);
    }

    sink.lastDispatchedSeq = seq;
    return 1;
}

void Session::checkProcess() {
    if (_pid <= 0) return;

    int status;
    pid_t result = waitpid(_pid, &status, WNOHANG);
    if (result == _pid) {
        if (WIFEXITED(status)) {
            _exitCode = WEXITSTATUS(status);
            _state.store((_exitCode == 0) ? SessionState::STOPPED : SessionState::ERROR,
                         std::memory_order_release);
        } else if (WIFSIGNALED(status)) {
            _exitCode = -WTERMSIG(status);
            _state.store(SessionState::ERROR, std::memory_order_release);
        }
        _pid = -1;
    }
}

// ============================================================================
// VisionPipe Implementation
// ============================================================================

VisionPipe::VisionPipe() = default;

VisionPipe::VisionPipe(VisionPipeConfig config) : _config(std::move(config)) {}

VisionPipe::~VisionPipe() = default;

void VisionPipe::setExecutablePath(const std::string& path) {
    _config.executablePath = path;
}

void VisionPipe::setParam(const std::string& key, const std::string& value) {
    _config.params[key] = value;
}

void VisionPipe::setVerbose(bool v) {
    _config.verbose = v;
}

void VisionPipe::setNoDisplay(bool nd) {
    _config.noDisplay = nd;
}

std::unique_ptr<Session> VisionPipe::run(const std::string& scriptPath) {
    std::string sessionId = generateSessionId();

    // Build argv for exec
    std::vector<std::string> args;
    args.push_back(_config.executablePath);
    args.push_back("run");
    args.push_back(scriptPath);

    if (_config.verbose) {
        args.push_back("--verbose");
    }
    if (_config.noDisplay) {
        args.push_back("--no-display");
    }
    if (!_config.pipeline.empty()) {
        args.push_back("--pipeline");
        args.push_back(_config.pipeline);
    }
    for (auto& [key, val] : _config.params) {
        args.push_back("--param");
        args.push_back(key + "=" + val);
    }

    // Convert to C-style argv
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    // Fork and exec
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[VisionPipe] fork() failed: " << strerror(errno) << std::endl;
        return nullptr;
    }

    if (pid == 0) {
        // Child process
        // Set session ID in environment for frame_sink to use
        setenv("VISIONPIPE_SESSION_ID", sessionId.c_str(), 1);

        // Exec visionpipe
        execvp(argv[0], argv.data());

        // If we get here, exec failed
        std::cerr << "[VisionPipe] exec failed: " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent process
    std::cout << "[VisionPipe] Started session '" << sessionId
              << "' (pid=" << pid << "): " << scriptPath << std::endl;

    return std::unique_ptr<Session>(new Session(sessionId, pid, _config));
}

int VisionPipe::execute(const std::vector<std::string>& args) {
    std::vector<std::string> fullArgs;
    fullArgs.push_back(_config.executablePath);
    fullArgs.insert(fullArgs.end(), args.begin(), args.end());

    std::vector<char*> argv;
    argv.reserve(fullArgs.size() + 1);
    for (auto& a : fullArgs) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool VisionPipe::isAvailable() const {
    std::string cmd = _config.executablePath + " version > /dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

std::string VisionPipe::version() const {
    // Run visionpipe version and capture output
    std::string cmd = _config.executablePath + " version 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    std::string result;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    pclose(pipe);

    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::string VisionPipe::generateSessionId() const {
    // Generate a short random hex string + PID for uniqueness
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFF);
    uint32_t r = dist(gen);

    std::ostringstream oss;
    oss << std::hex << getpid() << "_" << r;
    return oss.str();
}

} // namespace visionpipe

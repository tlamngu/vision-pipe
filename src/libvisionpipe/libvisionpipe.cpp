#include "libvisionpipe/libvisionpipe.h"
#include "libvisionpipe/frame_transport.h"

#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>

// ============================================================================
// Platform includes
// ============================================================================
#if defined(_WIN32)
  #include <process.h>   // _getpid
#else
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <signal.h>
  #include <unistd.h>
  #include <cerrno>
#endif

namespace visionpipe {

// ============================================================================
// Session Implementation
// ============================================================================

#if defined(_WIN32)

Session::Session(const std::string& sessionId, HANDLE hProcess, HANDLE hThread,
                 const VisionPipeConfig& config)
    : _sessionId(sessionId), _hProcess(hProcess), _hThread(hThread), _config(config) {
    _state.store(SessionState::RUNNING, std::memory_order_release);
}

#else

Session::Session(const std::string& sessionId, pid_t pid, const VisionPipeConfig& config)
    : _sessionId(sessionId), _pid(pid), _config(config) {
    _state.store(SessionState::RUNNING, std::memory_order_release);
}

#endif

Session::~Session() {
    stop(2000);
}

Session::Session(Session&& other) noexcept
    : _sessionId(std::move(other._sessionId))
#if defined(_WIN32)
    , _hProcess(other._hProcess)
    , _hThread(other._hThread)
#else
    , _pid(other._pid)
#endif
    , _config(std::move(other._config))
    , _state(other._state.load())
    , _exitCode(other._exitCode)
    , _sinks(std::move(other._sinks))
    , _stopRequested(other._stopRequested.load()) {
#if defined(_WIN32)
    other._hProcess = NULL;
    other._hThread = NULL;
#else
    other._pid = -1;
#endif
}

Session& Session::operator=(Session&& other) noexcept {
    if (this != &other) {
        stop(1000);
        _sessionId = std::move(other._sessionId);
#if defined(_WIN32)
        _hProcess = other._hProcess;
        _hThread = other._hThread;
        other._hProcess = NULL;
        other._hThread = NULL;
#else
        _pid = other._pid;
        other._pid = -1;
#endif
        _config = std::move(other._config);
        _state.store(other._state.load());
        _exitCode = other._exitCode;
        _sinks = std::move(other._sinks);
        _stopRequested.store(other._stopRequested.load());
    }
    return *this;
}

void Session::onFrame(const std::string& sinkName, FrameCallback callback) {
    auto& sink = _sinks[sinkName];
    sink.callbacks.push_back(std::move(callback));

    if (!sink.connected && _state.load() == SessionState::RUNNING) {
        connectSink(sinkName);
    }
}

bool Session::grabFrame(const std::string& sinkName, cv::Mat& frame) {
    auto it = _sinks.find(sinkName);
    if (it == _sinks.end()) {
        _sinks[sinkName]; // create entry
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

    // Create cv::Mat referencing shared memory (zero-copy)
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

void Session::cleanupProcess() {
#if defined(_WIN32)
    if (_hThread != NULL) {
        CloseHandle(_hThread);
        _hThread = NULL;
    }
    if (_hProcess != NULL) {
        CloseHandle(_hProcess);
        _hProcess = NULL;
    }
#else
    _pid = -1;
#endif
}

// --- stop() ---

#if defined(_WIN32)

void Session::stop(int timeoutMs) {
    _stopRequested.store(true, std::memory_order_release);

    if (_hProcess == NULL) return;

    // On Windows, send Ctrl+Break event to the process group
    // If that fails, try TerminateProcess
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, GetProcessId(_hProcess));

    if (waitForExit(timeoutMs)) {
        return;
    }

    // Force kill
    TerminateProcess(_hProcess, 1);
    waitForExit(1000);

    // Clean up sinks
    for (auto& [name, sink] : _sinks) {
        if (sink.consumer) sink.consumer->close();
    }
    _sinks.clear();
}

#else

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
        if (sink.consumer) sink.consumer->close();
    }
    _sinks.clear();
}

#endif

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

// --- waitForExit() ---

#if defined(_WIN32)

bool Session::waitForExit(int timeoutMs) {
    if (_hProcess == NULL) return true;

    DWORD waitMs = (timeoutMs == 0) ? INFINITE : static_cast<DWORD>(timeoutMs);
    DWORD result = WaitForSingleObject(_hProcess, waitMs);

    if (result == WAIT_OBJECT_0) {
        DWORD code;
        GetExitCodeProcess(_hProcess, &code);
        _exitCode = static_cast<int>(code);
        _state.store((_exitCode == 0) ? SessionState::STOPPED : SessionState::ERROR,
                     std::memory_order_release);
        cleanupProcess();
        return true;
    }

    return false; // Timed out or error
}

#else

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
            _pid = -1;
            _state.store(SessionState::STOPPED, std::memory_order_release);
            return true;
        }

        if (timeoutMs == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

#endif

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

    cv::Mat frame(rows, cols, type, const_cast<uint8_t*>(data), step);

    for (auto& cb : sink.callbacks) {
        cb(frame, seq);
    }

    sink.lastDispatchedSeq = seq;
    return 1;
}

// --- checkProcess() ---

#if defined(_WIN32)

void Session::checkProcess() {
    if (_hProcess == NULL) return;

    DWORD result = WaitForSingleObject(_hProcess, 0);
    if (result == WAIT_OBJECT_0) {
        DWORD code;
        GetExitCodeProcess(_hProcess, &code);
        _exitCode = static_cast<int>(code);
        _state.store((_exitCode == 0) ? SessionState::STOPPED : SessionState::ERROR,
                     std::memory_order_release);
        cleanupProcess();
    }
}

#else

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

#endif

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

// --- run() ---

#if defined(_WIN32)

std::unique_ptr<Session> VisionPipe::run(const std::string& scriptPath) {
    std::string sessionId = generateSessionId();

    // Build command line string for CreateProcess
    std::ostringstream cmdLine;
    cmdLine << "\"" << _config.executablePath << "\" run \"" << scriptPath << "\"";

    if (_config.verbose) cmdLine << " --verbose";
    if (_config.noDisplay) cmdLine << " --no-display";
    if (!_config.pipeline.empty()) {
        cmdLine << " --pipeline \"" << _config.pipeline << "\"";
    }
    for (auto& [key, val] : _config.params) {
        cmdLine << " --param " << key << "=" << val;
    }

    std::string cmdStr = cmdLine.str();

    // Set session ID in environment for child process
    SetEnvironmentVariableA("VISIONPIPE_SESSION_ID", sessionId.c_str());

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessA(
        NULL,                            // lpApplicationName
        const_cast<char*>(cmdStr.c_str()), // lpCommandLine
        NULL, NULL,                      // security attributes
        FALSE,                           // inherit handles
        CREATE_NEW_PROCESS_GROUP,        // creation flags (for Ctrl+Break)
        NULL,                            // environment (inherits parent)
        NULL,                            // working directory
        &si, &pi
    );

    if (!ok) {
        std::cerr << "[VisionPipe] CreateProcess failed: " << GetLastError() << std::endl;
        return nullptr;
    }

    std::cout << "[VisionPipe] Started session '" << sessionId
              << "' (pid=" << pi.dwProcessId << "): " << scriptPath << std::endl;

    return std::unique_ptr<Session>(new Session(sessionId, pi.hProcess, pi.hThread, _config));
}

#else

std::unique_ptr<Session> VisionPipe::run(const std::string& scriptPath) {
    std::string sessionId = generateSessionId();

    // Build argv for exec
    std::vector<std::string> args;
    args.push_back(_config.executablePath);
    args.push_back("run");
    args.push_back(scriptPath);

    if (_config.verbose) args.push_back("--verbose");
    if (_config.noDisplay) args.push_back("--no-display");
    if (!_config.pipeline.empty()) {
        args.push_back("--pipeline");
        args.push_back(_config.pipeline);
    }
    for (auto& [key, val] : _config.params) {
        args.push_back("--param");
        args.push_back(key + "=" + val);
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[VisionPipe] fork() failed: " << strerror(errno) << std::endl;
        return nullptr;
    }

    if (pid == 0) {
        // Child process
        setenv("VISIONPIPE_SESSION_ID", sessionId.c_str(), 1);
        execvp(argv[0], argv.data());
        std::cerr << "[VisionPipe] exec failed: " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent
    std::cout << "[VisionPipe] Started session '" << sessionId
              << "' (pid=" << pid << "): " << scriptPath << std::endl;

    return std::unique_ptr<Session>(new Session(sessionId, pid, _config));
}

#endif

// --- execute() ---

#if defined(_WIN32)

int VisionPipe::execute(const std::vector<std::string>& args) {
    std::ostringstream cmdLine;
    cmdLine << "\"" << _config.executablePath << "\"";
    for (auto& a : args) {
        cmdLine << " \"" << a << "\"";
    }
    std::string cmdStr = cmdLine.str();

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessA(
        NULL, const_cast<char*>(cmdStr.c_str()),
        NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi
    );
    if (!ok) return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(code);
}

#else

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

#endif

// --- isAvailable() ---

bool VisionPipe::isAvailable() const {
#if defined(_WIN32)
    std::string cmd = "\"" + _config.executablePath + "\" version > NUL 2>&1";
#else
    std::string cmd = _config.executablePath + " version > /dev/null 2>&1";
#endif
    return system(cmd.c_str()) == 0;
}

// --- version() ---

std::string VisionPipe::version() const {
#if defined(_WIN32)
    std::string cmd = "\"" + _config.executablePath + "\" version 2>&1";
#else
    std::string cmd = _config.executablePath + " version 2>&1";
#endif

#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";

    std::string result;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

// --- generateSessionId() ---

std::string VisionPipe::generateSessionId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFF);
    uint32_t r = dist(gen);

    int pid;
#if defined(_WIN32)
    pid = _getpid();
#else
    pid = getpid();
#endif

    std::ostringstream oss;
    oss << std::hex << pid << "_" << r;
    return oss.str();
}

} // namespace visionpipe

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
#include <fstream>
#include <cstdio>

// ============================================================================
// TCP socket helpers for param client
// ============================================================================
#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using SocketFd = SOCKET;
  #define CLOSE_SOCK closesocket
  #define INVALID_SOCKFD INVALID_SOCKET
  using ssize_t = SSIZE_T;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  using SocketFd = int;
  #define CLOSE_SOCK ::close
  #define INVALID_SOCKFD (-1)
#endif

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
    startParamPortProbe();
}

#endif

Session::~Session() {
    stop(2000);
    // Stop all param watchers
    {
        std::lock_guard<std::mutex> lk(_watcherMutex);
        for (auto& w : _watchers) {
            w->stop.store(true);
            if (w->sockFd >= 0) {
                CLOSE_SOCK(static_cast<SocketFd>(w->sockFd));
                w->sockFd = -1;
            }
        }
        for (auto& w : _watchers) {
            if (w->thread.joinable()) w->thread.join();
        }
        _watchers.clear();
    }
    // Stop port probe thread
    if (_paramPortThread.joinable()) _paramPortThread.join();
    // Clean up temp script file if created via runString()
    if (!_tempScriptPath.empty()) {
        std::remove(_tempScriptPath.c_str());
    }
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
    , _stopRequested(other._stopRequested.load())
    , _tempScriptPath(std::move(other._tempScriptPath)) {
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
        _tempScriptPath = std::move(other._tempScriptPath);
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

// --- runString() ---

std::unique_ptr<Session> VisionPipe::runString(const std::string& script,
                                                const std::string& name) {
    // Write script content to a temporary file
    std::string tempPath;

#if defined(_WIN32)
    // Windows: use GetTempPath + GetTempFileName
    char tempDir[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, tempDir);
    if (len == 0 || len > MAX_PATH) {
        std::cerr << "[VisionPipe] Failed to get temp directory\n";
        return nullptr;
    }
    char tempFile[MAX_PATH];
    if (GetTempFileNameA(tempDir, "vsp", 0, tempFile) == 0) {
        std::cerr << "[VisionPipe] Failed to create temp file\n";
        return nullptr;
    }
    // Rename to .vsp extension
    tempPath = std::string(tempFile) + ".vsp";
    MoveFileA(tempFile, tempPath.c_str());
#else
    // POSIX: use /tmp with mkstemp
    std::string tmpl = "/tmp/visionpipe_" + name + "_XXXXXX";
    std::vector<char> tmplBuf(tmpl.begin(), tmpl.end());
    tmplBuf.push_back('\0');

    int fd = mkstemp(tmplBuf.data());
    if (fd < 0) {
        std::cerr << "[VisionPipe] Failed to create temp file: " << strerror(errno) << std::endl;
        return nullptr;
    }
    close(fd);

    // Rename with .vsp extension
    std::string basePath(tmplBuf.data());
    tempPath = basePath + ".vsp";
    if (rename(basePath.c_str(), tempPath.c_str()) != 0) {
        // rename failed, use original path
        tempPath = basePath;
    }
#endif

    // Write script content
    {
        std::ofstream ofs(tempPath, std::ios::out | std::ios::trunc);
        if (!ofs) {
            std::cerr << "[VisionPipe] Failed to write temp script: " << tempPath << std::endl;
            std::remove(tempPath.c_str());
            return nullptr;
        }
        ofs << script;
    }

    // Run the temp file
    auto session = run(tempPath);
    if (session) {
        // Store temp path so Session destructor cleans it up
        session->_tempScriptPath = tempPath;
    } else {
        // Launch failed, clean up now
        std::remove(tempPath.c_str());
    }

    return session;
}

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

// ============================================================================
// Session — runtime parameter client
// ============================================================================

// Poll /tmp/visionpipe-params-<pid>.port until it exists or timeout
static int readParamPortFile(int pid, int timeoutMs) {
    std::string path = "/tmp/visionpipe-params-" + std::to_string(pid) + ".port";
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream f(path);
        if (f.is_open()) {
            int port = 0;
            f >> port;
            if (port > 0) return port;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return -1;
}

void Session::startParamPortProbe() {
    int pid = static_cast<int>(_pid);
    _paramPortThread = std::thread([this, pid]() {
        int port = readParamPortFile(pid, 10000); // up to 10 s
        if (port > 0) _paramPort.store(port, std::memory_order_release);
    });
    // detach so destructor doesn't need to join (but we also join in ~Session)
    // actually don't detach — we join in ~Session
}

// Open a TCP connection to 127.0.0.1:_paramPort, send one command line,
// read one JSON response line, then close.
bool Session::sendParamCommand(const std::string& cmd, std::string& response, int timeoutMs) {
    int port = _paramPort.load(std::memory_order_acquire);
    if (port <= 0) return false;

    SocketFd fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKFD) return false;

    // Set receive / send timeout
#if defined(_WIN32)
    DWORD tv = static_cast<DWORD>(timeoutMs);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        CLOSE_SOCK(fd);
        return false;
    }

    // Send command
    std::string line = cmd + "\n";
    if (::send(fd, line.c_str(), static_cast<int>(line.size()), 0)
            != static_cast<ssize_t>(line.size())) {
        CLOSE_SOCK(fd);
        return false;
    }

    // Read one response line
    response.clear();
    char ch;
    while (true) {
        ssize_t n = ::recv(fd, &ch, 1, 0);
        if (n <= 0) break;
        if (ch == '\n') break;
        response += ch;
    }

    CLOSE_SOCK(fd);
    return !response.empty();
}

bool Session::setParam(const std::string& name, const std::string& value) {
    std::string resp;
    return sendParamCommand("SET " + name + " " + value, resp);
}

bool Session::getParam(const std::string& name, std::string& outValue) {
    std::string resp;
    if (!sendParamCommand("GET " + name, resp)) return false;
    // Response: {"ok":true,"name":"brightness","value":75}
    // Simple extraction: find "value": and grab the rest up to }
    auto pos = resp.find("\"value\":");
    if (pos == std::string::npos) return false;
    pos += 8; // skip "value":
    while (pos < resp.size() && resp[pos] == ' ') ++pos;
    // value is either a quoted string or a bare token
    std::string raw;
    if (pos < resp.size() && resp[pos] == '"') {
        ++pos;
        while (pos < resp.size() && resp[pos] != '"') raw += resp[pos++];
    } else {
        while (pos < resp.size() && resp[pos] != '}' && resp[pos] != ',') raw += resp[pos++];
        // trim trailing whitespace
        while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t')) raw.pop_back();
    }
    outValue = raw;
    return true;
}

std::map<std::string, std::string> Session::listParams() {
    std::string resp;
    std::map<std::string, std::string> result;
    if (!sendParamCommand("LIST", resp)) return result;
    // Parse simple JSON: {"ok":true,"params":{"name":value,...}}
    auto start = resp.find("\"params\":");
    if (start == std::string::npos) return result;
    start = resp.find('{', start + 9);
    if (start == std::string::npos) return result;
    // Iterate key-value pairs in this object crudely
    std::size_t pos = start + 1;
    while (pos < resp.size()) {
        // skip whitespace and commas
        while (pos < resp.size() && (resp[pos] == ' ' || resp[pos] == ',')) ++pos;
        if (pos >= resp.size() || resp[pos] == '}') break;
        // parse key
        if (resp[pos] != '"') { ++pos; continue; }
        ++pos;
        std::string key;
        while (pos < resp.size() && resp[pos] != '"') key += resp[pos++];
        if (pos < resp.size()) ++pos; // skip closing "
        // skip :
        while (pos < resp.size() && (resp[pos] == ' ' || resp[pos] == ':')) ++pos;
        // parse value (just the "value" field from inner object or bare token)
        // inner object {"value":..., ...}
        if (pos < resp.size() && resp[pos] == '{') {
            // find "value": inside
            auto vPos = resp.find("\"value\":", pos);
            std::string val;
            if (vPos != std::string::npos) {
                vPos += 8;
                while (vPos < resp.size() && resp[vPos] == ' ') ++vPos;
                if (vPos < resp.size() && resp[vPos] == '"') {
                    ++vPos;
                    while (vPos < resp.size() && resp[vPos] != '"') val += resp[vPos++];
                } else {
                    while (vPos < resp.size() && resp[vPos] != ',' && resp[vPos] != '}')
                        val += resp[vPos++];
                    while (!val.empty() && val.back() == ' ') val.pop_back();
                }
            }
            result[key] = val;
            // skip to closing } of inner object
            int depth = 1;
            ++pos;
            while (pos < resp.size() && depth > 0) {
                if (resp[pos] == '{') ++depth;
                else if (resp[pos] == '}') --depth;
                ++pos;
            }
        } else {
            std::string val;
            if (resp[pos] == '"') {
                ++pos;
                while (pos < resp.size() && resp[pos] != '"') val += resp[pos++];
                if (pos < resp.size()) ++pos;
            } else {
                while (pos < resp.size() && resp[pos] != ',' && resp[pos] != '}') val += resp[pos++];
                while (!val.empty() && val.back() == ' ') val.pop_back();
            }
            result[key] = val;
        }
    }
    return result;
}

uint64_t Session::watchParam(const std::string& name,
                              std::function<void(const std::string&, const std::string&)> callback) {
    int port = _paramPort.load(std::memory_order_acquire);
    if (port <= 0) return 0;

    auto sub = std::make_shared<WatchSubscription>();
    sub->id       = _nextWatchId.fetch_add(1, std::memory_order_relaxed);
    sub->sockFd   = -1;
    sub->stop.store(false);
    sub->callback = std::move(callback);

    // Connect
    SocketFd fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKFD) return 0;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        CLOSE_SOCK(fd);
        return 0;
    }
    sub->sockFd = static_cast<int>(fd);

    // Send WATCH command
    std::string cmd = "WATCH " + name + "\n";
    ::send(fd, cmd.c_str(), static_cast<int>(cmd.size()), 0);
    // Read ACK line
    { std::string ack; char ch;
      while (::recv(fd, &ch, 1, 0) == 1 && ch != '\n') ack += ch; }

    // Start push-event reader thread
    uint64_t subId = sub->id;
    sub->thread = std::thread([sub, fd]() {
        std::string line;
        char ch;
        while (!sub->stop.load()) {
            ssize_t n = ::recv(static_cast<SocketFd>(fd), &ch, 1, 0);
            if (n <= 0) break;
            if (ch == '\n') {
                // Parse: {"event":"changed","name":"brightness","old":50,"new":75}
                // Extract "name" and "new" fields
                auto extractStr = [&](const std::string& key) -> std::string {
                    auto pos = line.find("\"" + key + "\":");
                    if (pos == std::string::npos) return "";
                    pos += key.size() + 3;
                    while (pos < line.size() && line[pos] == ' ') ++pos;
                    std::string val;
                    if (pos < line.size() && line[pos] == '"') {
                        ++pos;
                        while (pos < line.size() && line[pos] != '"') val += line[pos++];
                    } else {
                        while (pos < line.size() && line[pos] != ',' && line[pos] != '}')
                            val += line[pos++];
                        while (!val.empty() && val.back() == ' ') val.pop_back();
                    }
                    return val;
                };
                if (line.find("\"event\":\"changed\"") != std::string::npos) {
                    std::string pName = extractStr("name");
                    std::string pVal  = extractStr("new");
                    if (!sub->stop.load()) sub->callback(pName, pVal);
                }
                line.clear();
            } else {
                line += ch;
            }
        }
    });

    {
        std::lock_guard<std::mutex> lk(_watcherMutex);
        _watchers.push_back(sub);
    }
    return subId;
}

void Session::unwatchParam(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lk(_watcherMutex);
    for (auto it = _watchers.begin(); it != _watchers.end(); ++it) {
        if ((*it)->id == subscriptionId) {
            auto& w = *it;
            w->stop.store(true);
            if (w->sockFd >= 0) {
                CLOSE_SOCK(static_cast<SocketFd>(w->sockFd));
                w->sockFd = -1;
            }
            if (w->thread.joinable()) w->thread.join();
            _watchers.erase(it);
            return;
        }
    }
}

} // namespace visionpipe

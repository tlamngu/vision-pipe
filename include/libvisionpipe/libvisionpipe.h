#ifndef LIBVISIONPIPE_H
#define LIBVISIONPIPE_H

/**
 * @file libvisionpipe.h
 * @brief libvisionpipe - Cross-platform C++ shared library client for VisionPipe
 *
 * Supported platforms: Linux, macOS, Windows
 *
 * This library spawns a VisionPipe process as a sidecar and provides
 * zero-copy frame access to frames produced by frame_sink() items
 * in .vsp scripts.
 *
 * Quick start:
 * @code
 *   #include <libvisionpipe/libvisionpipe.h>
 *
 *   visionpipe::VisionPipe camera;
 *   auto session = camera.run("my_pipeline.vsp");
 *
 *   session->onFrame("output", [](const cv::Mat& frame, uint64_t seq) {
 *       // Process frame...
 *   });
 *
 *   session->spin();  // Block and dispatch frames
 * @endcode
 *
 * The .vsp script should use frame_sink("output") to publish frames:
 * @code
 *   video_cap(0)
 *   resize(640, 480)
 *   frame_sink("output")
 * @endcode
 */

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <atomic>
#include <opencv2/core/mat.hpp>

// Platform-specific process handle type
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

// Shared library export macros
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef LIBVISIONPIPE_BUILDING
    #define LIBVISIONPIPE_API __declspec(dllexport)
  #else
    #define LIBVISIONPIPE_API __declspec(dllimport)
  #endif
#else
  #define LIBVISIONPIPE_API __attribute__((visibility("default")))
#endif

namespace visionpipe {

// Forward declarations
class FrameShmConsumer;

// ============================================================================
// Types
// ============================================================================

/**
 * @brief Callback type for frame processing
 * @param frame The received cv::Mat frame (read-only, backed by shared memory)
 * @param frameNumber Monotonically increasing frame counter
 */
using FrameCallback = std::function<void(const cv::Mat& frame, uint64_t frameNumber)>;

/**
 * @brief Session state
 */
enum class SessionState {
    CREATED,     ///< Session created, not yet started
    STARTING,    ///< VisionPipe process is launching
    RUNNING,     ///< VisionPipe is running and sinks may be active
    STOPPING,    ///< Stop requested, waiting for process to exit
    STOPPED,     ///< VisionPipe process has exited normally
    ERROR        ///< VisionPipe process exited with error
};

/**
 * @brief Configuration for VisionPipe execution
 */
struct LIBVISIONPIPE_API VisionPipeConfig {
#if defined(_WIN32)
    std::string executablePath = "visionpipe.exe"; ///< Path to visionpipe binary
#else
    std::string executablePath = "visionpipe";     ///< Path to visionpipe binary
#endif
    bool verbose = false;                           ///< Pass --verbose to visionpipe
    bool noDisplay = true;                          ///< Pass --no-display (typical for library use)
    std::map<std::string, std::string> params;      ///< Script parameters (--param key=value)
    std::string pipeline;                           ///< Specific pipeline to run (--pipeline name)
    int sinkTimeoutMs = 10000;                      ///< Timeout waiting for frame_sink shm to appear
};

// ============================================================================
// Session
// ============================================================================

/**
 * @brief Represents a running VisionPipe session (one .vsp file execution)
 *
 * A session manages:
 * - The child visionpipe process
 * - Shared memory connections to frame_sink() outputs
 * - Frame dispatch callbacks
 *
 * Cross-platform: uses fork/exec on POSIX, CreateProcess on Windows.
 */
class LIBVISIONPIPE_API Session {
public:
    ~Session();

    // No copy
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Move OK
    Session(Session&& other) noexcept;
    Session& operator=(Session&& other) noexcept;

    /**
     * @brief Register a callback for frames from a named sink
     * @param sinkName Must match a frame_sink("name") call in the .vsp script
     * @param callback Function to call with each new frame
     *
     * Multiple callbacks can be registered per sink.
     */
    void onFrame(const std::string& sinkName, FrameCallback callback);

    /**
     * @brief Grab the latest frame from a sink (polling mode)
     * @param sinkName Sink name
     * @param[out] frame Output Mat
     * @return true if a new frame was obtained
     */
    bool grabFrame(const std::string& sinkName, cv::Mat& frame);

    /**
     * @brief Block and dispatch frame callbacks until the session ends
     * @param pollIntervalMs How often to check for new frames (default: 1ms)
     *
     * This is the main loop for callback-driven usage.
     */
    void spin(int pollIntervalMs = 1);

    /**
     * @brief Dispatch pending frames once (non-blocking)
     * @return Number of frames dispatched
     *
     * Use this in your own event loop instead of spin().
     */
    int spinOnce();

    /**
     * @brief Stop the VisionPipe process
     * @param timeoutMs Max time to wait for graceful shutdown before force kill
     */
    void stop(int timeoutMs = 3000);

    /**
     * @brief Check if visionpipe process is still running
     */
    bool isRunning() const;

    /**
     * @brief Get current session state
     */
    SessionState state() const;

    /**
     * @brief Get the session ID
     */
    const std::string& sessionId() const { return _sessionId; }

    /**
     * @brief Get the exit code of the visionpipe process (-1 if still running)
     */
    int exitCode() const;

    /**
     * @brief Wait for the visionpipe process to exit
     * @param timeoutMs Timeout in ms (0 = wait forever)
     * @return true if process exited within timeout
     */
    bool waitForExit(int timeoutMs = 0);

private:
    friend class VisionPipe;

#if defined(_WIN32)
    Session(const std::string& sessionId, HANDLE hProcess, HANDLE hThread,
            const VisionPipeConfig& config);
#else
    Session(const std::string& sessionId, pid_t pid, const VisionPipeConfig& config);
#endif

    struct SinkConnection {
        std::unique_ptr<FrameShmConsumer> consumer;
        std::vector<FrameCallback> callbacks;
        uint64_t lastDispatchedSeq = 0;
        bool connected = false;
    };

    std::string _sessionId;

#if defined(_WIN32)
    HANDLE _hProcess = NULL;
    HANDLE _hThread = NULL;
#else
    pid_t _pid = -1;
#endif

    VisionPipeConfig _config;
    std::atomic<SessionState> _state{SessionState::CREATED};
    int _exitCode = -1;

    std::map<std::string, SinkConnection> _sinks;
    std::atomic<bool> _stopRequested{false};

    void connectSink(const std::string& sinkName);
    int dispatchSink(const std::string& sinkName, SinkConnection& sink);
    void checkProcess();
    void cleanupProcess();
};

// ============================================================================
// VisionPipe (main entry point)
// ============================================================================

/**
 * @brief Main VisionPipe client class
 *
 * Usage:
 * @code
 *   visionpipe::VisionPipe vp;
 *   vp.setExecutablePath("/usr/local/bin/visionpipe");
 *
 *   auto session = vp.run("camera_pipeline.vsp");
 *   session->onFrame("output", myCallback);
 *   session->spin();
 * @endcode
 */
class LIBVISIONPIPE_API VisionPipe {
public:
    VisionPipe();
    explicit VisionPipe(VisionPipeConfig config);
    ~VisionPipe();

    /**
     * @brief Set the path to the visionpipe executable
     */
    void setExecutablePath(const std::string& path);

    /**
     * @brief Set a script parameter (passed as --param key=value)
     */
    void setParam(const std::string& key, const std::string& value);

    /**
     * @brief Set verbose mode
     */
    void setVerbose(bool v);

    /**
     * @brief Set display mode (default: disabled for library usage)
     */
    void setNoDisplay(bool nd);

    /**
     * @brief Get current configuration
     */
    const VisionPipeConfig& config() const { return _config; }

    /**
     * @brief Execute a .vsp file and return a Session
     * @param scriptPath Path to the .vsp file
     * @return Unique pointer to the session. nullptr on launch failure.
     */
    std::unique_ptr<Session> run(const std::string& scriptPath);

    /**
     * @brief Execute a visionpipe command (like "execute") synchronously
     * @param args Command line arguments
     * @return Exit code
     */
    int execute(const std::vector<std::string>& args);

    /**
     * @brief Check if the visionpipe executable is available
     * @return true if executable is found and runnable
     */
    bool isAvailable() const;

    /**
     * @brief Get the version string of the visionpipe executable
     */
    std::string version() const;

private:
    VisionPipeConfig _config;

    std::string generateSessionId() const;
};

} // namespace visionpipe

#endif // LIBVISIONPIPE_H

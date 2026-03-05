#ifndef VISIONPIPE_RUNTIME_H
#define VISIONPIPE_RUNTIME_H

#include "interpreter/interpreter.h"
#include "interpreter/param_store.h"
#include "interpreter/param_server.h"
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

namespace visionpipe {

/**
 * @brief Runtime configuration
 */
struct RuntimeConfig {
    // Interpreter settings
    InterpreterConfig interpreterConfig;
    
    // Runtime settings
    bool autoRegisterBuiltins = true;
    bool enableProfiling = false;
    bool enableLogging = true;
    std::string logFile = "";
    
    // Loop settings
    double targetFps = 0;           // 0 = run as fast as possible
    bool waitForKeyPress = false;   // Wait for key after each frame
    int waitKeyMs = 1;              // Milliseconds to wait for key
    
    // Display settings
    bool enableDisplay = true;      // Allow display() function
    std::string windowPrefix = "VisionPipe: ";
};

/**
 * @brief Runtime statistics
 */
struct RuntimeStats {
    uint64_t framesProcessed = 0;
    uint64_t totalProcessingTimeMs = 0;
    double averageFps = 0;
    double currentFps = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    
    std::string toString() const;
};

/**
 * @brief Callback types for runtime events
 */
using FrameCallback = std::function<void(const cv::Mat& frame, uint64_t frameNumber)>;
using ErrorCallback = std::function<void(const std::string& error)>;
using StopCallback = std::function<void()>;

/**
 * @brief VisionPipe Runtime
 * 
 * High-level runtime for executing .vsp files.
 * Provides:
 * - File loading and execution
 * - Event callbacks
 * - Performance monitoring
 * - Graceful shutdown
 * 
 * Usage:
 * ```cpp
 * Runtime runtime;
 * runtime.loadBuiltins();
 * runtime.run("stereo.vsp");
 * ```
 */
class Runtime {
public:
    Runtime();
    explicit Runtime(RuntimeConfig config);
    ~Runtime();
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /**
     * @brief Set runtime configuration
     */
    void setConfig(const RuntimeConfig& config);
    
    /**
     * @brief Get current configuration
     */
    const RuntimeConfig& config() const { return _config; }
    
    // =========================================================================
    // Item registration
    // =========================================================================
    
    /**
     * @brief Load all built-in interpreter items
     */
    void loadBuiltins();
    
    /**
     * @brief Add a custom interpreter item
     */
    void addItem(std::shared_ptr<InterpreterItem> item);
    
    /**
     * @brief Add a custom item using its type
     */
    template<typename T>
    void addItem() {
        _interpreter.add<T>();
    }
    
    /**
     * @brief Get the interpreter
     */
    Interpreter& interpreter() { return _interpreter; }
    const Interpreter& interpreter() const { return _interpreter; }
    
    // =========================================================================
    // Execution
    // =========================================================================
    
    /**
     * @brief Load and run a .vsp file
     * @param filename Path to the .vsp file
     * @return Exit code (0 = success)
     */
    int run(const std::string& filename);
    
    /**
     * @brief Load a .vsp file without executing
     */
    void load(const std::string& filename);
    
    /**
     * @brief Execute a specific pipeline
     */
    cv::Mat executePipeline(const std::string& name,
                            const std::vector<RuntimeValue>& args = {},
                            const cv::Mat& input = cv::Mat());
    
    /**
     * @brief Execute source code directly
     */
    int runSource(const std::string& source, const std::string& name = "<source>");
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Set callback for each processed frame
     */
    void onFrame(FrameCallback callback);
    
    /**
     * @brief Set callback for errors
     */
    void onError(ErrorCallback callback);
    
    /**
     * @brief Set callback when runtime stops
     */
    void onStop(StopCallback callback);
    
    // =========================================================================
    // Control
    // =========================================================================
    
    /**
     * @brief Request the runtime to stop
     */
    void stop();
    
    /**
     * @brief Check if runtime is currently running
     */
    bool isRunning() const { return _running.load(); }
    
    /**
     * @brief Pause execution
     */
    void pause();
    
    /**
     * @brief Resume execution
     */
    void resume();
    
    /**
     * @brief Check if paused
     */
    bool isPaused() const { return _paused.load(); }
    
    /**
     * @brief Wait for runtime to finish
     */
    void wait();
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    /**
     * @brief Get runtime statistics
     */
    RuntimeStats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();
    
    // =========================================================================
    // Documentation
    // =========================================================================
    
    /**
     * @brief Generate documentation for all registered items
     * @return Markdown documentation string
     */
    std::string generateDocs() const;
    
    /**
     * @brief Generate JSON schema for all registered items
     */
    std::string generateJsonSchema() const;
    
    /**
     * @brief Start documentation server
     * @param port Port to listen on
     */
    void startDocsServer(int port = 8080);
    
    /**
     * @brief Stop documentation server
     */
    void stopDocsServer();

    // =========================================================================
    // Runtime parameter API
    // =========================================================================

    /**
     * @brief Set a runtime parameter (accessible via @name in VSP).
     *        Creates the parameter if not declared via params [].
     */
    void setParam(const std::string& name, const ParamValue& value);
    void setParam(const std::string& name, int64_t value);
    void setParam(const std::string& name, int value);
    void setParam(const std::string& name, double value);
    void setParam(const std::string& name, const std::string& value);
    void setParam(const std::string& name, bool value);

    /**
     * @brief Get a runtime parameter value.
     */
    ParamValue getParam(const std::string& name) const;

    /**
     * @brief Check whether a parameter exists.
     */
    bool hasParam(const std::string& name) const;

    /**
     * @brief List all current parameter values.
     */
    std::unordered_map<std::string, ParamValue> listParams() const;

    /**
     * @brief Subscribe to parameter changes.
     * @param name  Parameter name, or "*" for all parameters.
     * @return Subscription ID (for unsubscription).
     */
    uint64_t onParamChange(const std::string& name, ParamChangeCallback cb);
    uint64_t onAnyParamChange(ParamChangeCallback cb);

    /**
     * @brief Remove a param change subscription.
     */
    void removeParamSubscription(uint64_t id);

    /**
     * @brief Get the shared parameter store.
     */
    std::shared_ptr<ParameterStore> paramStore() const { return _paramStore; }

    // =========================================================================
    // Param server
    // =========================================================================

    /**
     * @brief Explicitly enable the TCP parameter server.
     *        The server is also auto-started when `params [...]` is present
     *        in a script.  Call this to force-enable even without a params block.
     * @param startPort First port to try (default: 8765).
     * @return Bound port, or -1 on failure.
     */
    int enableParamServer(int startPort = 8765);

    /**
     * @brief Stop the TCP parameter server.
     */
    void disableParamServer();

    /**
     * @brief Get the port the param server is listening on (0 if not started).
     */
    int paramServerPort() const;
    
private:
    RuntimeConfig _config;
    Interpreter _interpreter;

    // Runtime parameter store & server (lazy — created on first use)
    std::shared_ptr<ParameterStore> _paramStore;
    std::unique_ptr<ParamServer>    _paramServer;
    void ensureParamStore();  ///< create store + attach to interpreter if needed
    
    // State
    std::atomic<bool> _running{false};
    std::atomic<bool> _paused{false};
    std::atomic<bool> _stopRequested{false};
    
    // Callbacks
    FrameCallback _frameCallback;
    ErrorCallback _errorCallback;
    StopCallback _stopCallback;
    
    // Statistics
    mutable std::mutex _statsMutex;
    RuntimeStats _stats;
    std::chrono::steady_clock::time_point _lastFrameTime;
    std::chrono::steady_clock::time_point _startTime;
    
    // FPS timing
    std::chrono::steady_clock::duration _frameDuration;
    
    // Internal methods
    void updateStats(const cv::Mat& frame);
    void enforceFrameRate();
    void handleError(const std::string& error);
};

/**
 * @brief Create a default runtime with all builtins loaded
 */
std::unique_ptr<Runtime> createRuntime();

/**
 * @brief Quick run a .vsp file with default settings
 * @param filename Path to the .vsp file
 * @return Exit code
 */
int quickRun(const std::string& filename);

} // namespace visionpipe

#endif // VISIONPIPE_RUNTIME_H

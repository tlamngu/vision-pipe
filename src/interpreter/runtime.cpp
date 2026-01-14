#include "interpreter/runtime.h"
#include "interpreter/items/all_items.h"
#include "interpreter/parser.h"
#ifdef VISIONPIPE_WITH_DNN
#include "interpreter/ml/model_registry.h"
#endif
#include <iostream>
#include <sstream>
#include <opencv2/highgui.hpp>

namespace visionpipe {

// ============================================================================
// RuntimeStats
// ============================================================================

std::string RuntimeStats::toString() const {
    std::ostringstream oss;
    oss << "RuntimeStats {\n";
    oss << "  Frames processed: " << framesProcessed << "\n";
    oss << "  Total time: " << totalProcessingTimeMs << " ms\n";
    oss << "  Average FPS: " << averageFps << "\n";
    oss << "  Current FPS: " << currentFps << "\n";
    oss << "  Cache hits: " << cacheHits << "\n";
    oss << "  Cache misses: " << cacheMisses << "\n";
    oss << "}\n";
    return oss.str();
}

// ============================================================================
// Runtime
// ============================================================================

Runtime::Runtime() : Runtime(RuntimeConfig{}) {}

Runtime::Runtime(RuntimeConfig config) 
    : _config(std::move(config))
    , _interpreter(_config.interpreterConfig) {
    
    if (_config.autoRegisterBuiltins) {
        loadBuiltins();
    }
    
#ifdef VISIONPIPE_WITH_DNN
    // Set DNN verbose mode
    ml::ModelRegistry::setVerbose(_config.interpreterConfig.verbose);
#endif
    
    // Calculate frame duration for target FPS
    if (_config.targetFps > 0) {
        _frameDuration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / _config.targetFps)
        );
    }
}

Runtime::~Runtime() {
    stop();
    stopDocsServer();
}

void Runtime::setConfig(const RuntimeConfig& config) {
    _config = config;
    _interpreter.setConfig(_config.interpreterConfig);
    
#ifdef VISIONPIPE_WITH_DNN
    ml::ModelRegistry::setVerbose(_config.interpreterConfig.verbose);
#endif
    
    if (_config.targetFps > 0) {
        _frameDuration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / _config.targetFps)
        );
    }
}

// ============================================================================
// Item registration
// ============================================================================

void Runtime::loadBuiltins() {
    registerAllItems(_interpreter.registry());
}

void Runtime::addItem(std::shared_ptr<InterpreterItem> item) {
    _interpreter.add(item);
}

// ============================================================================
// Execution
// ============================================================================

int Runtime::run(const std::string& filename) {
    try {
        // Set working directory from filename
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            auto cfg = _config.interpreterConfig;
            cfg.workingDirectory = filename.substr(0, lastSlash);
            _interpreter.setConfig(cfg);
        }
        
        load(filename);
        
        _running = true;
        _stopRequested = false;
        _startTime = std::chrono::steady_clock::now();
        _lastFrameTime = _startTime;
        
        // Execute the program (which may include exec_loop)
        // The interpreter handles the main loop internally
        _interpreter.execute(_interpreter.getPipeline("main") ? 
            parseFile(filename) : parseFile(filename));
        
        _running = false;
        
        if (_stopCallback) {
            _stopCallback();
        }
        
        return _interpreter.hasError() ? 1 : 0;
        
    } catch (const std::exception& e) {
        handleError(e.what());
        _running = false;
        return 1;
    }
}

void Runtime::load(const std::string& filename) {
    auto program = parseFile(filename);
    _interpreter.execute(program);
}

cv::Mat Runtime::executePipeline(const std::string& name,
                                  const std::vector<RuntimeValue>& args,
                                  const cv::Mat& input) {
    return _interpreter.executePipeline(name, args, input);
}

int Runtime::runSource(const std::string& source, const std::string& name) {
    try {
        auto program = parseSource(source, name);
        
        _running = true;
        _stopRequested = false;
        
        _interpreter.execute(program);
        
        _running = false;
        
        if (_stopCallback) {
            _stopCallback();
        }
        
        return _interpreter.hasError() ? 1 : 0;
        
    } catch (const std::exception& e) {
        handleError(e.what());
        _running = false;
        return 1;
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void Runtime::onFrame(FrameCallback callback) {
    _frameCallback = callback;
}

void Runtime::onError(ErrorCallback callback) {
    _errorCallback = callback;
}

void Runtime::onStop(StopCallback callback) {
    _stopCallback = callback;
}

// ============================================================================
// Control
// ============================================================================

void Runtime::stop() {
    _stopRequested = true;
    _interpreter.requestStop();
}

void Runtime::pause() {
    _paused = true;
}

void Runtime::resume() {
    _paused = false;
}

void Runtime::wait() {
    while (_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ============================================================================
// Statistics
// ============================================================================

RuntimeStats Runtime::getStats() const {
    std::lock_guard<std::mutex> lock(_statsMutex);
    RuntimeStats stats = _stats;
    // Get frames processed from interpreter
    stats.framesProcessed = _interpreter.framesProcessed();
    return stats;
}

void Runtime::resetStats() {
    std::lock_guard<std::mutex> lock(_statsMutex);
    _stats = RuntimeStats{};
    _startTime = std::chrono::steady_clock::now();
    _lastFrameTime = _startTime;
}

// ============================================================================
// Documentation
// ============================================================================

std::string Runtime::generateDocs() const {
    return _interpreter.registry().generateDocumentation();
}

std::string Runtime::generateJsonSchema() const {
    return _interpreter.registry().generateJsonSchema();
}

void Runtime::startDocsServer(int port) {
    // Simple HTTP server for documentation
    // This would require additional HTTP library (like httplib)
    // For now, just log a message
    std::cout << "Documentation server would start on port " << port << std::endl;
    std::cout << "Use 'visionpipe docs --export <output_dir>' to export documentation" << std::endl;
}

void Runtime::stopDocsServer() {
    // Stop the docs server if running
}

// ============================================================================
// Internal methods
// ============================================================================

void Runtime::updateStats(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(_statsMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastFrameTime);
    
    ++_stats.framesProcessed;
    _stats.totalProcessingTimeMs += elapsed.count();
    
    if (elapsed.count() > 0) {
        _stats.currentFps = 1000.0 / elapsed.count();
    }
    
    if (_stats.framesProcessed > 0) {
        auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime);
        if (totalElapsed.count() > 0) {
            _stats.averageFps = (_stats.framesProcessed * 1000.0) / totalElapsed.count();
        }
    }
    
    _lastFrameTime = now;
    
    // Invoke frame callback
    if (_frameCallback && !frame.empty()) {
        _frameCallback(frame, _stats.framesProcessed);
    }
}

void Runtime::enforceFrameRate() {
    if (_config.targetFps <= 0) return;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - _lastFrameTime;
    
    if (elapsed < _frameDuration) {
        std::this_thread::sleep_for(_frameDuration - elapsed);
    }
}

void Runtime::handleError(const std::string& error) {
    if (_errorCallback) {
        _errorCallback(error);
    } else {
        std::cerr << "Runtime error: " << error << std::endl;
    }
}

// ============================================================================
// Utility functions
// ============================================================================

std::unique_ptr<Runtime> createRuntime() {
    auto runtime = std::make_unique<Runtime>();
    return runtime;
}

int quickRun(const std::string& filename) {
    auto runtime = createRuntime();
    return runtime->run(filename);
}

} // namespace visionpipe

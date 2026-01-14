#include "interpreter/ml/model_registry.h"
#include "interpreter/ml/opencv_dnn_backend.h"
#ifdef VISIONPIPE_WITH_ONNXRUNTIME
#include "interpreter/ml/onnx_runtime_backend.h"
#endif
#include <iostream>
#include <chrono>

namespace visionpipe {
namespace ml {

// Static verbose flag definition
bool ModelRegistry::s_verbose = false;

ModelRegistry& ModelRegistry::instance() {
    static ModelRegistry instance;
    return instance;
}

ModelRegistry::ModelRegistry() {
    // Register default backends
    registerBackend("opencv", createOpenCVDNNModel);
    
#ifdef VISIONPIPE_WITH_ONNXRUNTIME
    // Register ONNX Runtime backend
    registerBackend("onnx", createOnnxRuntimeModel);
    registerBackend("onnxruntime", createOnnxRuntimeModel);  // Alias
    
    if (s_verbose) {
        std::cout << "[ModelRegistry] ONNX Runtime backend registered" << std::endl;
    }
#endif
}

bool ModelRegistry::loadModel(const std::string& id, const std::string& path, 
                               const ModelConfig& config) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Check if model already exists
    if (_models.find(id) != _models.end()) {
        std::cerr << "[ModelRegistry] Model '" << id << "' already loaded. Unload first." << std::endl;
        return false;
    }
    
    // Create model using appropriate backend
    std::string backend = config.backend;
    if (backend.empty()) {
        backend = "opencv";  // Default backend
    }
    
    auto model = createModel(backend);
    if (!model) {
        std::cerr << "[ModelRegistry] Unknown backend: " << backend << std::endl;
        return false;
    }
    
    // Load the model
    if (!model->load(path, config)) {
        std::cerr << "[ModelRegistry] Failed to load model from: " << path << std::endl;
        return false;
    }
    
    // Store model and info
    _models[id] = model;
    
    ModelInfo info;
    info.id = id;
    info.path = path;
    info.backend = backend;
    info.device = config.device;
    info.inputSize = model->getInputSize();
    info.isLoaded = true;
    _modelInfo[id] = info;
    
    if (s_verbose) {
        std::cout << "[ModelRegistry] Loaded model '" << id << "' from " << path 
                  << " (backend: " << backend << ")" << std::endl;
    }
    
    return true;
}

bool ModelRegistry::unloadModel(const std::string& id) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _models.find(id);
    if (it == _models.end()) {
        return false;
    }
    
    _models.erase(it);
    _modelInfo.erase(id);
    
    if (s_verbose) {
        std::cout << "[ModelRegistry] Unloaded model '" << id << "'" << std::endl;
    }
    return true;
}

std::shared_ptr<MLModel> ModelRegistry::getModel(const std::string& id) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _models.find(id);
    if (it != _models.end()) {
        return it->second;
    }
    return nullptr;
}

bool ModelRegistry::hasModel(const std::string& id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _models.find(id) != _models.end();
}

std::optional<ModelInfo> ModelRegistry::getModelInfo(const std::string& id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _modelInfo.find(id);
    if (it != _modelInfo.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> ModelRegistry::listModels() const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::vector<std::string> ids;
    ids.reserve(_models.size());
    for (const auto& pair : _models) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<ModelInfo> ModelRegistry::listModelInfo() const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::vector<ModelInfo> infos;
    infos.reserve(_modelInfo.size());
    for (const auto& pair : _modelInfo) {
        infos.push_back(pair.second);
    }
    return infos;
}

void ModelRegistry::clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    _models.clear();
    _modelInfo.clear();
}

InferenceResult ModelRegistry::runInference(const std::string& id, const cv::Mat& input) {
    std::shared_ptr<MLModel> model;
    
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _models.find(id);
        if (it == _models.end()) {
            return InferenceResult::fail("Model not found: " + id);
        }
        model = it->second;
    }
    
    // Run inference outside lock
    auto start = std::chrono::high_resolution_clock::now();
    auto result = model->forward(input);
    auto end = std::chrono::high_resolution_clock::now();
    
    result.inferenceTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _modelInfo.find(id);
        if (it != _modelInfo.end()) {
            it->second.inferenceCount++;
            it->second.totalInferenceTimeMs += result.inferenceTimeMs;
        }
    }
    
    return result;
}

void ModelRegistry::registerBackend(const std::string& backend, ModelFactory factory) {
    std::lock_guard<std::mutex> lock(_mutex);
    _backends[backend] = factory;
}

bool ModelRegistry::hasBackend(const std::string& backend) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _backends.find(backend) != _backends.end();
}

std::vector<std::string> ModelRegistry::getBackends() const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::vector<std::string> backends;
    backends.reserve(_backends.size());
    for (const auto& pair : _backends) {
        backends.push_back(pair.first);
    }
    return backends;
}

std::shared_ptr<MLModel> ModelRegistry::createModel(const std::string& backend) {
    auto it = _backends.find(backend);
    if (it != _backends.end()) {
        return it->second();
    }
    return nullptr;
}

} // namespace ml
} // namespace visionpipe

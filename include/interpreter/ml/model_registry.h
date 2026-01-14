#ifndef VISIONPIPE_MODEL_REGISTRY_H
#define VISIONPIPE_MODEL_REGISTRY_H

#include "interpreter/ml/ml_model.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace visionpipe {
namespace ml {

/**
 * @brief Information about a loaded model
 */
struct ModelInfo {
    std::string id;
    std::string path;
    std::string backend;
    std::string device;
    cv::Size inputSize;
    bool isLoaded = false;
    size_t inferenceCount = 0;
    double totalInferenceTimeMs = 0.0;
    
    double averageInferenceTimeMs() const {
        return inferenceCount > 0 ? totalInferenceTimeMs / inferenceCount : 0.0;
    }
};

/**
 * @brief Central registry for managing ML models
 * 
 * This class provides a singleton registry for loading, storing, and retrieving
 * ML models. It supports lazy loading and thread-safe access.
 */
class ModelRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static ModelRegistry& instance();
    
    /**
     * @brief Load a model and register it with an ID
     * @param id Unique identifier for the model
     * @param path Path to the model file
     * @param config Model configuration
     * @return true if successful
     */
    bool loadModel(const std::string& id, const std::string& path, 
                   const ModelConfig& config = ModelConfig());
    
    /**
     * @brief Unload a model by ID
     * @param id Model identifier
     * @return true if model was unloaded
     */
    bool unloadModel(const std::string& id);
    
    /**
     * @brief Get a loaded model by ID
     * @param id Model identifier
     * @return Shared pointer to model, or nullptr if not found
     */
    std::shared_ptr<MLModel> getModel(const std::string& id);
    
    /**
     * @brief Check if a model is loaded
     * @param id Model identifier
     */
    bool hasModel(const std::string& id) const;
    
    /**
     * @brief Get information about a loaded model
     * @param id Model identifier
     * @return Model info, or nullopt if not found
     */
    std::optional<ModelInfo> getModelInfo(const std::string& id) const;
    
    /**
     * @brief List all loaded model IDs
     */
    std::vector<std::string> listModels() const;
    
    /**
     * @brief Get info for all loaded models
     */
    std::vector<ModelInfo> listModelInfo() const;
    
    /**
     * @brief Unload all models
     */
    void clear();
    
    /**
     * @brief Run inference on a model
     * @param id Model identifier
     * @param input Input image
     * @return Inference result
     */
    InferenceResult runInference(const std::string& id, const cv::Mat& input);
    
    /**
     * @brief Register a custom model factory
     * @param backend Backend name
     * @param factory Factory function
     */
    void registerBackend(const std::string& backend, ModelFactory factory);
    
    /**
     * @brief Check if a backend is registered
     */
    bool hasBackend(const std::string& backend) const;
    
    /**
     * @brief Get registered backend names
     */
    std::vector<std::string> getBackends() const;
    
    /**
     * @brief Set verbose mode for DNN logging
     */
    static void setVerbose(bool v) { s_verbose = v; }
    
    /**
     * @brief Check if verbose mode is enabled
     */
    static bool isVerbose() { return s_verbose; }

private:
    ModelRegistry();
    ~ModelRegistry() = default;
    
    // Prevent copying
    ModelRegistry(const ModelRegistry&) = delete;
    ModelRegistry& operator=(const ModelRegistry&) = delete;
    
    std::shared_ptr<MLModel> createModel(const std::string& backend);
    
    mutable std::mutex _mutex;
    std::unordered_map<std::string, std::shared_ptr<MLModel>> _models;
    std::unordered_map<std::string, ModelInfo> _modelInfo;
    std::unordered_map<std::string, ModelFactory> _backends;
    
    static bool s_verbose;
};

} // namespace ml
} // namespace visionpipe

#endif // VISIONPIPE_MODEL_REGISTRY_H

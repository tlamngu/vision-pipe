#ifndef VISIONPIPE_ML_MODEL_H
#define VISIONPIPE_ML_MODEL_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <functional>
#include <opencv2/core/mat.hpp>

namespace visionpipe {
namespace ml {

/**
 * @brief Configuration for ML model loading and inference
 */
struct ModelConfig {
    std::string backend = "opencv";          // Backend: "opencv", "onnx", "tensorrt"
    std::string device = "cpu";              // Device: "cpu", "cuda", "cuda:0", etc.
    std::string precision = "fp32";          // Precision: "fp32", "fp16", "int8"
    cv::Size inputSize = cv::Size(0, 0);     // Input size (0,0 = use model default)
    int inputChannels = 3;                   // Number of input channels
    bool swapRB = true;                      // Swap Red and Blue channels
    double scaleFactor = 1.0 / 255.0;        // Scale factor for normalization
    cv::Scalar mean = cv::Scalar(0, 0, 0);   // Mean subtraction values
    bool crop = false;                       // Whether to crop input
    
    // Additional config as key-value pairs
    std::unordered_map<std::string, std::string> extra;
};

/**
 * @brief Information about a model's input/output layer
 */
struct LayerInfo {
    std::string name;
    std::vector<int> shape;
    int type;  // CV type (CV_32F, etc.)
};

/**
 * @brief Result of model inference
 */
struct InferenceResult {
    bool success = true;
    std::vector<cv::Mat> outputs;
    std::vector<std::string> outputNames;
    double inferenceTimeMs = 0.0;
    std::optional<std::string> error;
    
    static InferenceResult ok(const std::vector<cv::Mat>& outputs, 
                               const std::vector<std::string>& names = {}) {
        InferenceResult r;
        r.outputs = outputs;
        r.outputNames = names;
        return r;
    }
    
    static InferenceResult fail(const std::string& error) {
        InferenceResult r;
        r.success = false;
        r.error = error;
        return r;
    }
    
    cv::Mat getPrimaryOutput() const {
        return outputs.empty() ? cv::Mat() : outputs[0];
    }
    
    cv::Mat getOutput(const std::string& name) const {
        for (size_t i = 0; i < outputNames.size(); ++i) {
            if (outputNames[i] == name && i < outputs.size()) {
                return outputs[i];
            }
        }
        return cv::Mat();
    }
};

/**
 * @brief Abstract base class for ML models
 * 
 * This interface allows different backends (OpenCV DNN, ONNX Runtime, TensorRT)
 * to be used interchangeably.
 */
class MLModel {
public:
    virtual ~MLModel() = default;
    
    /**
     * @brief Load model from file
     * @param path Path to model file
     * @param config Model configuration
     * @return true if successful
     */
    virtual bool load(const std::string& path, const ModelConfig& config = ModelConfig()) = 0;
    
    /**
     * @brief Check if model is loaded
     */
    virtual bool isLoaded() const = 0;
    
    /**
     * @brief Run inference on a single input
     * @param input Input image/tensor
     * @return Inference result
     */
    virtual InferenceResult forward(const cv::Mat& input) = 0;
    
    /**
     * @brief Run inference on multiple inputs
     * @param inputs Vector of input images/tensors
     * @return Inference result
     */
    virtual InferenceResult forwardMulti(const std::vector<cv::Mat>& inputs) = 0;
    
    /**
     * @brief Get input layer information
     */
    virtual std::vector<LayerInfo> getInputLayers() const = 0;
    
    /**
     * @brief Get output layer information
     */
    virtual std::vector<LayerInfo> getOutputLayers() const = 0;
    
    /**
     * @brief Get expected input size
     */
    virtual cv::Size getInputSize() const = 0;
    
    /**
     * @brief Get number of input channels
     */
    virtual int getInputChannels() const = 0;
    
    /**
     * @brief Get model path
     */
    virtual std::string getModelPath() const = 0;
    
    /**
     * @brief Get backend name
     */
    virtual std::string getBackendName() const = 0;
    
    /**
     * @brief Get model configuration
     */
    virtual const ModelConfig& getConfig() const = 0;
    
    /**
     * @brief Preprocess input image according to model config
     * @param input Raw input image
     * @return Preprocessed blob ready for inference
     */
    virtual cv::Mat preprocess(const cv::Mat& input) = 0;
    
    /**
     * @brief Set preferred backend and target
     * @param backend Backend ID (cv::dnn::DNN_BACKEND_*)
     * @param target Target ID (cv::dnn::DNN_TARGET_*)
     */
    virtual void setPreferableBackend(int backend, int target) = 0;
};

/**
 * @brief Factory function type for creating model instances
 */
using ModelFactory = std::function<std::shared_ptr<MLModel>()>;

/**
 * @brief Get available backend names
 */
std::vector<std::string> getAvailableBackends();

/**
 * @brief Check if a backend is available
 */
bool isBackendAvailable(const std::string& backend);

} // namespace ml
} // namespace visionpipe

#endif // VISIONPIPE_ML_MODEL_H

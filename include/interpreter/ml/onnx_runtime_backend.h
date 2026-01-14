#ifndef VISIONPIPE_ONNX_RUNTIME_BACKEND_H
#define VISIONPIPE_ONNX_RUNTIME_BACKEND_H

#include "interpreter/ml/ml_model.h"

// ONNX Runtime API version compatibility
// If ORT_API_VERSION is not defined or mismatched, you need to use matching
// header files and runtime library. Download from:
// https://github.com/microsoft/onnxruntime/releases
//
// The headers and DLL/SO must be from the SAME version!
// Example: If using onnxruntime 1.17.1 DLL, use 1.17.1 headers.

#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>

namespace visionpipe {
namespace ml {

/**
 * @brief Execution provider types supported by ONNX Runtime
 */
enum class OrtExecutionProvider {
    CPU,        ///< Default CPU execution
    CUDA,       ///< NVIDIA CUDA
    TensorRT,   ///< NVIDIA TensorRT
    DirectML,   ///< DirectX Machine Learning (Windows AMD/Intel/NVIDIA)
    OpenVINO,   ///< Intel OpenVINO
    CoreML,     ///< Apple CoreML (macOS/iOS)
    NNAPI       ///< Android Neural Networks API
};

/**
 * @brief Configuration for ONNX Runtime execution providers
 */
struct OrtExecutionProviderConfig {
    OrtExecutionProvider provider = OrtExecutionProvider::CPU;
    int deviceId = 0;                       ///< GPU device ID
    size_t gpuMemoryLimit = 0;              ///< GPU memory limit (0 = unlimited)
    int cudnnConvAlgoSearch = 1;            ///< 0=DEFAULT, 1=EXHAUSTIVE, 2=HEURISTIC
    bool enableCudaGraph = false;           ///< Enable CUDA Graph for static shapes
    bool enableTensorRtFp16 = false;        ///< Enable TensorRT FP16 mode
    bool enableTensorRtInt8 = false;        ///< Enable TensorRT INT8 mode
    bool enableTensorRtCache = true;        ///< Cache TensorRT engines
    std::string tensorRtCachePath;          ///< Path for TensorRT engine cache
    std::string openVinoDeviceType;         ///< OpenVINO device: CPU, GPU, MYRIAD
};

/**
 * @brief ONNX Runtime backend implementation
 * 
 * Provides inference using ONNX Runtime with support for multiple
 * execution providers including CUDA, TensorRT, DirectML, OpenVINO, and CoreML.
 * 
 * Key advantages over OpenCV DNN:
 * - Better optimization with graph-level transformations
 * - More execution providers (DirectML for AMD GPUs, TensorRT, etc.)
 * - Better support for dynamic shapes
 * - INT8/FP16 quantization support
 */
class OnnxRuntimeModel : public MLModel {
public:
    OnnxRuntimeModel();
    ~OnnxRuntimeModel() override;
    
    // ========================================================================
    // MLModel interface implementation
    // ========================================================================
    
    bool load(const std::string& path, const ModelConfig& config = ModelConfig()) override;
    bool isLoaded() const override;
    
    InferenceResult forward(const cv::Mat& input) override;
    InferenceResult forwardMulti(const std::vector<cv::Mat>& inputs) override;
    
    std::vector<LayerInfo> getInputLayers() const override;
    std::vector<LayerInfo> getOutputLayers() const override;
    
    cv::Size getInputSize() const override;
    int getInputChannels() const override;
    
    std::string getModelPath() const override { return _modelPath; }
    std::string getBackendName() const override { return "onnx"; }
    const ModelConfig& getConfig() const override { return _config; }
    
    cv::Mat preprocess(const cv::Mat& input) override;
    void setPreferableBackend(int backend, int target) override;
    
    // ========================================================================
    // ONNX Runtime specific methods
    // ========================================================================
    
    /**
     * @brief Get available execution providers on this system
     */
    static std::vector<std::string> getAvailableProviders();
    
    /**
     * @brief Check if a specific execution provider is available
     */
    static bool isProviderAvailable(OrtExecutionProvider provider);
    
    /**
     * @brief Get input tensor names
     */
    std::vector<std::string> getInputNames() const { return _inputNames; }
    
    /**
     * @brief Get output tensor names
     */
    std::vector<std::string> getOutputNames() const { return _outputNames; }
    
    /**
     * @brief Get input tensor shapes
     */
    std::vector<std::vector<int64_t>> getInputShapes() const { return _inputShapes; }
    
    /**
     * @brief Get output tensor shapes
     */
    std::vector<std::vector<int64_t>> getOutputShapes() const { return _outputShapes; }
    
    /**
     * @brief Get current execution provider configuration
     */
    const OrtExecutionProviderConfig& getProviderConfig() const { return _epConfig; }

private:
    // ONNX Runtime objects
    std::unique_ptr<Ort::Env> _env;
    std::unique_ptr<Ort::Session> _session;
    std::unique_ptr<Ort::SessionOptions> _sessionOptions;
    Ort::AllocatorWithDefaultOptions _allocator;
    
    // Model state
    std::string _modelPath;
    ModelConfig _config;
    OrtExecutionProviderConfig _epConfig;
    bool _loaded = false;
    
    // Input/Output metadata
    std::vector<std::string> _inputNames;
    std::vector<std::string> _outputNames;
    std::vector<std::vector<int64_t>> _inputShapes;
    std::vector<std::vector<int64_t>> _outputShapes;
    std::vector<ONNXTensorElementDataType> _inputTypes;
    std::vector<ONNXTensorElementDataType> _outputTypes;
    
    // Cached char* pointers for ORT C API (must persist during session lifetime)
    std::vector<const char*> _inputNamesCStr;
    std::vector<const char*> _outputNamesCStr;
    
    // ========================================================================
    // Private helper methods
    // ========================================================================
    
    void initializeEnvironment();
    void configureSessionOptions();
    void parseDeviceConfig(const std::string& device);
    void addExecutionProvider();
    void extractModelMetadata();
    
    // Execution provider setup
    void addCPUProvider();
    void addCUDAProvider();
    void addTensorRTProvider();
    void addDirectMLProvider();
    void addOpenVINOProvider();
    void addCoreMLProvider();
    
    // Tensor conversion helpers
    Ort::Value createInputTensor(const cv::Mat& input);
    std::vector<Ort::Value> createInputTensors(const std::vector<cv::Mat>& inputs);
    cv::Mat tensorToMat(const Ort::Value& tensor);
    std::vector<float> matToFloatVector(const cv::Mat& mat);
    
    // Data type helpers
    static int ortTypeToOpenCVType(ONNXTensorElementDataType ortType);
    static std::string ortTypeToString(ONNXTensorElementDataType ortType);
};

/**
 * @brief Factory function for ONNX Runtime backend
 */
std::shared_ptr<MLModel> createOnnxRuntimeModel();

} // namespace ml
} // namespace visionpipe

#endif // VISIONPIPE_ONNX_RUNTIME_BACKEND_H

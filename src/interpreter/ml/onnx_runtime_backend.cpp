#include "interpreter/ml/onnx_runtime_backend.h"
#include "interpreter/ml/model_registry.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <thread>


namespace visionpipe {
namespace ml {

// ============================================================================
// Constructor / Destructor
// ============================================================================

OnnxRuntimeModel::OnnxRuntimeModel() {
    initializeEnvironment();
}

OnnxRuntimeModel::~OnnxRuntimeModel() {
    // Session must be destroyed before Env
    _session.reset();
    _sessionOptions.reset();
    _env.reset();
}

void OnnxRuntimeModel::initializeEnvironment() {
    try {
        // Create ONNX Runtime environment
        _env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "VisionPipe");
        _sessionOptions = std::make_unique<Ort::SessionOptions>();
        
        // Default optimizations
        _sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // Enable memory pattern for better memory allocation
        _sessionOptions->EnableMemPattern();
        _sessionOptions->EnableCpuMemArena();
        
    } catch (const Ort::Exception& e) {
        std::cerr << "[OnnxRuntime] Failed to initialize environment: " << e.what() << std::endl;
    }
}

// ============================================================================
// Static utility methods
// ============================================================================

std::vector<std::string> OnnxRuntimeModel::getAvailableProviders() {
    return Ort::GetAvailableProviders();
}

bool OnnxRuntimeModel::isProviderAvailable(OrtExecutionProvider provider) {
    auto available = getAvailableProviders();
    
    std::string providerName;
    switch (provider) {
        case OrtExecutionProvider::CPU:
            providerName = "CPUExecutionProvider";
            break;
        case OrtExecutionProvider::CUDA:
            providerName = "CUDAExecutionProvider";
            break;
        case OrtExecutionProvider::TensorRT:
            providerName = "TensorrtExecutionProvider";
            break;
        case OrtExecutionProvider::DirectML:
            providerName = "DmlExecutionProvider";
            break;
        case OrtExecutionProvider::OpenVINO:
            providerName = "OpenVINOExecutionProvider";
            break;
        case OrtExecutionProvider::CoreML:
            providerName = "CoreMLExecutionProvider";
            break;
        case OrtExecutionProvider::NNAPI:
            providerName = "NnapiExecutionProvider";
            break;
        default:
            return false;
    }
    
    return std::find(available.begin(), available.end(), providerName) != available.end();
}

// ============================================================================
// Model Loading
// ============================================================================

bool OnnxRuntimeModel::load(const std::string& path, const ModelConfig& config) {
    _modelPath = path;
    _config = config;
    
    try {
        // Parse device configuration to set execution provider
        parseDeviceConfig(config.device);
        
        // Configure session options
        configureSessionOptions();
        
        // Add appropriate execution provider
        addExecutionProvider();
        
        // Create session from model file
#ifdef _WIN32
        // Windows requires wide string path
        std::wstring wpath(path.begin(), path.end());
        _session = std::make_unique<Ort::Session>(*_env, wpath.c_str(), *_sessionOptions);
#else
        _session = std::make_unique<Ort::Session>(*_env, path.c_str(), *_sessionOptions);
#endif
        
        // Extract model metadata (input/output names, shapes, types)
        extractModelMetadata();
        
        _loaded = true;
        
        if (ModelRegistry::isVerbose()) {
            std::cout << "[OnnxRuntime] Loaded model: " << path << std::endl;
            std::cout << "[OnnxRuntime] Inputs: " << _inputNames.size() 
                      << ", Outputs: " << _outputNames.size() << std::endl;
            
            // Print available providers
            auto providers = getAvailableProviders();
            std::cout << "[OnnxRuntime] Available providers: ";
            for (size_t i = 0; i < providers.size(); ++i) {
                std::cout << providers[i];
                if (i < providers.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
        
        return true;
        
    } catch (const Ort::Exception& e) {
        std::cerr << "[OnnxRuntime] Error loading model: " << e.what() << std::endl;
        _loaded = false;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[OnnxRuntime] Error: " << e.what() << std::endl;
        _loaded = false;
        return false;
    }
}

bool OnnxRuntimeModel::isLoaded() const {
    return _loaded && _session != nullptr;
}

// ============================================================================
// Configuration
// ============================================================================

void OnnxRuntimeModel::parseDeviceConfig(const std::string& device) {
    // Reset to defaults
    _epConfig = OrtExecutionProviderConfig{};
    
    std::string deviceLower = device;
    std::transform(deviceLower.begin(), deviceLower.end(), deviceLower.begin(), ::tolower);
    
    // Determine execution provider from device string
    if (deviceLower.find("tensorrt") != std::string::npos || 
        deviceLower.find("trt") != std::string::npos) {
        _epConfig.provider = OrtExecutionProvider::TensorRT;
    } else if (deviceLower.find("cuda") != std::string::npos) {
        _epConfig.provider = OrtExecutionProvider::CUDA;
    } else if (deviceLower.find("dml") != std::string::npos || 
               deviceLower.find("directml") != std::string::npos) {
        _epConfig.provider = OrtExecutionProvider::DirectML;
    } else if (deviceLower.find("openvino") != std::string::npos) {
        _epConfig.provider = OrtExecutionProvider::OpenVINO;
    } else if (deviceLower.find("coreml") != std::string::npos) {
        _epConfig.provider = OrtExecutionProvider::CoreML;
    } else if (deviceLower.find("nnapi") != std::string::npos) {
        _epConfig.provider = OrtExecutionProvider::NNAPI;
    } else {
        _epConfig.provider = OrtExecutionProvider::CPU;
    }
    
    // Parse device ID (e.g., "cuda:1", "dml:0")
    auto colonPos = device.find(':');
    if (colonPos != std::string::npos) {
        try {
            std::string idStr = device.substr(colonPos + 1);
            // Remove any trailing qualifiers like "fp16"
            auto spacePos = idStr.find_first_of(" _");
            if (spacePos != std::string::npos) {
                idStr = idStr.substr(0, spacePos);
            }
            _epConfig.deviceId = std::stoi(idStr);
        } catch (...) {
            _epConfig.deviceId = 0;
        }
    }
    
    // Parse precision hints
    if (deviceLower.find("fp16") != std::string::npos) {
        _epConfig.enableTensorRtFp16 = true;
        _config.precision = "fp16";
    }
    if (deviceLower.find("int8") != std::string::npos) {
        _epConfig.enableTensorRtInt8 = true;
        _config.precision = "int8";
    }
}

void OnnxRuntimeModel::configureSessionOptions() {
    if (!_sessionOptions) return;
    
    // Graph optimization level
    _sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    
    // Thread configuration - use hardware concurrency
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    _sessionOptions->SetIntraOpNumThreads(static_cast<int>(numThreads));
    _sessionOptions->SetInterOpNumThreads(std::max(1u, numThreads / 2));
    
    // Execution mode - parallel is generally faster
    _sessionOptions->SetExecutionMode(ExecutionMode::ORT_PARALLEL);
    
    // Memory optimizations
    _sessionOptions->EnableMemPattern();
    _sessionOptions->EnableCpuMemArena();
}

void OnnxRuntimeModel::addExecutionProvider() {
    switch (_epConfig.provider) {
        case OrtExecutionProvider::CUDA:
            addCUDAProvider();
            break;
        case OrtExecutionProvider::TensorRT:
            addTensorRTProvider();
            break;
        case OrtExecutionProvider::DirectML:
            addDirectMLProvider();
            break;
        case OrtExecutionProvider::OpenVINO:
            addOpenVINOProvider();
            break;
        case OrtExecutionProvider::CoreML:
            addCoreMLProvider();
            break;
        case OrtExecutionProvider::CPU:
        default:
            addCPUProvider();
            break;
    }
}

void OnnxRuntimeModel::addCPUProvider() {
    // CPU is always available as default fallback
    if (ModelRegistry::isVerbose()) {
        std::cout << "[OnnxRuntime] Using CPU execution provider" << std::endl;
    }
}

void OnnxRuntimeModel::addCUDAProvider() {
#ifdef VISIONPIPE_ONNX_CUDA
    if (isProviderAvailable(OrtExecutionProvider::CUDA)) {
        try {
            OrtCUDAProviderOptions cudaOptions;
            cudaOptions.device_id = _epConfig.deviceId;
            cudaOptions.arena_extend_strategy = 0;  // kNextPowerOfTwo
            cudaOptions.cudnn_conv_algo_search = static_cast<OrtCudnnConvAlgoSearch>(
                _epConfig.cudnnConvAlgoSearch);
            cudaOptions.do_copy_in_default_stream = 1;
            
            if (_epConfig.gpuMemoryLimit > 0) {
                cudaOptions.gpu_mem_limit = _epConfig.gpuMemoryLimit;
            }
            
            _sessionOptions->AppendExecutionProvider_CUDA(cudaOptions);
            
            if (ModelRegistry::isVerbose()) {
                std::cout << "[OnnxRuntime] Added CUDA execution provider (device " 
                          << _epConfig.deviceId << ")" << std::endl;
            }
            return;
        } catch (const Ort::Exception& e) {
            std::cerr << "[OnnxRuntime] Failed to add CUDA provider: " << e.what() 
                      << ", falling back to CPU" << std::endl;
        }
    } else {
        std::cerr << "[OnnxRuntime] CUDA provider not available, using CPU" << std::endl;
    }
#else
    std::cerr << "[OnnxRuntime] CUDA support not compiled, using CPU" << std::endl;
#endif
    addCPUProvider();
}

void OnnxRuntimeModel::addTensorRTProvider() {
#ifdef VISIONPIPE_ONNX_TENSORRT
    if (isProviderAvailable(OrtExecutionProvider::TensorRT)) {
        try {
            OrtTensorRTProviderOptions trtOptions;
            trtOptions.device_id = _epConfig.deviceId;
            trtOptions.trt_max_workspace_size = 1ULL << 30;  // 1GB
            trtOptions.trt_fp16_enable = _epConfig.enableTensorRtFp16 ? 1 : 0;
            trtOptions.trt_int8_enable = _epConfig.enableTensorRtInt8 ? 1 : 0;
            
            if (_epConfig.enableTensorRtCache && !_epConfig.tensorRtCachePath.empty()) {
                trtOptions.trt_engine_cache_enable = 1;
                trtOptions.trt_engine_cache_path = _epConfig.tensorRtCachePath.c_str();
            }
            
            _sessionOptions->AppendExecutionProvider_TensorRT(trtOptions);
            
            if (ModelRegistry::isVerbose()) {
                std::cout << "[OnnxRuntime] Added TensorRT execution provider" << std::endl;
            }
            return;
        } catch (const Ort::Exception& e) {
            std::cerr << "[OnnxRuntime] Failed to add TensorRT provider: " << e.what() 
                      << ", trying CUDA" << std::endl;
        }
    }
    // Fall back to CUDA
    addCUDAProvider();
#else
    std::cerr << "[OnnxRuntime] TensorRT support not compiled, trying CUDA" << std::endl;
    addCUDAProvider();
#endif
}

void OnnxRuntimeModel::addDirectMLProvider() {
#ifdef VISIONPIPE_ONNX_DML
    if (isProviderAvailable(OrtExecutionProvider::DirectML)) {
        try {
            _sessionOptions->AppendExecutionProvider_DML(_epConfig.deviceId);
            
            if (ModelRegistry::isVerbose()) {
                std::cout << "[OnnxRuntime] Added DirectML execution provider (device " 
                          << _epConfig.deviceId << ")" << std::endl;
            }
            return;
        } catch (const Ort::Exception& e) {
            std::cerr << "[OnnxRuntime] Failed to add DirectML provider: " << e.what() 
                      << ", falling back to CPU" << std::endl;
        }
    } else {
        std::cerr << "[OnnxRuntime] DirectML provider not available, using CPU" << std::endl;
    }
#else
    std::cerr << "[OnnxRuntime] DirectML support not compiled (Windows only), using CPU" << std::endl;
#endif
    addCPUProvider();
}

void OnnxRuntimeModel::addOpenVINOProvider() {
#ifdef VISIONPIPE_ONNX_OPENVINO
    if (isProviderAvailable(OrtExecutionProvider::OpenVINO)) {
        try {
            OrtOpenVINOProviderOptions ovOptions;
            
            std::string deviceType = _epConfig.openVinoDeviceType.empty() 
                ? "CPU" : _epConfig.openVinoDeviceType;
            ovOptions.device_type = deviceType.c_str();
            
            _sessionOptions->AppendExecutionProvider_OpenVINO(ovOptions);
            
            if (ModelRegistry::isVerbose()) {
                std::cout << "[OnnxRuntime] Added OpenVINO execution provider (" 
                          << deviceType << ")" << std::endl;
            }
            return;
        } catch (const Ort::Exception& e) {
            std::cerr << "[OnnxRuntime] Failed to add OpenVINO provider: " << e.what() 
                      << ", falling back to CPU" << std::endl;
        }
    } else {
        std::cerr << "[OnnxRuntime] OpenVINO provider not available, using CPU" << std::endl;
    }
#else
    std::cerr << "[OnnxRuntime] OpenVINO support not compiled, using CPU" << std::endl;
#endif
    addCPUProvider();
}

void OnnxRuntimeModel::addCoreMLProvider() {
#ifdef VISIONPIPE_ONNX_COREML
    if (isProviderAvailable(OrtExecutionProvider::CoreML)) {
        try {
            uint32_t coremlFlags = 0;
            // COREMLFlags::COREML_FLAG_ENABLE_ON_SUBGRAPH = 1
            _sessionOptions->AppendExecutionProvider_CoreML(coremlFlags);
            
            if (ModelRegistry::isVerbose()) {
                std::cout << "[OnnxRuntime] Added CoreML execution provider" << std::endl;
            }
            return;
        } catch (const Ort::Exception& e) {
            std::cerr << "[OnnxRuntime] Failed to add CoreML provider: " << e.what() 
                      << ", falling back to CPU" << std::endl;
        }
    } else {
        std::cerr << "[OnnxRuntime] CoreML provider not available, using CPU" << std::endl;
    }
#else
    std::cerr << "[OnnxRuntime] CoreML support not compiled (macOS/iOS only), using CPU" << std::endl;
#endif
    addCPUProvider();
}

// ============================================================================
// Metadata Extraction
// ============================================================================

void OnnxRuntimeModel::extractModelMetadata() {
    if (!_session) return;
    
    // Clear existing metadata
    _inputNames.clear();
    _outputNames.clear();
    _inputShapes.clear();
    _outputShapes.clear();
    _inputTypes.clear();
    _outputTypes.clear();
    _inputNamesCStr.clear();
    _outputNamesCStr.clear();
    
    // Get input metadata
    size_t numInputs = _session->GetInputCount();
    for (size_t i = 0; i < numInputs; ++i) {
        // Get input name
        Ort::AllocatedStringPtr inputName = _session->GetInputNameAllocated(i, _allocator);
        _inputNames.push_back(std::string(inputName.get()));
        
        // Get input type and shape
        Ort::TypeInfo typeInfo = _session->GetInputTypeInfo(i);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        
        _inputTypes.push_back(tensorInfo.GetElementType());
        _inputShapes.push_back(tensorInfo.GetShape());
    }
    
    // Get output metadata
    size_t numOutputs = _session->GetOutputCount();
    for (size_t i = 0; i < numOutputs; ++i) {
        // Get output name
        Ort::AllocatedStringPtr outputName = _session->GetOutputNameAllocated(i, _allocator);
        _outputNames.push_back(std::string(outputName.get()));
        
        // Get output type and shape
        Ort::TypeInfo typeInfo = _session->GetOutputTypeInfo(i);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        
        _outputTypes.push_back(tensorInfo.GetElementType());
        _outputShapes.push_back(tensorInfo.GetShape());
    }
    
    // Prepare C-string pointers (required by ORT Run API)
    for (const auto& name : _inputNames) {
        _inputNamesCStr.push_back(name.c_str());
    }
    for (const auto& name : _outputNames) {
        _outputNamesCStr.push_back(name.c_str());
    }
    
    if (ModelRegistry::isVerbose()) {
        std::cout << "[OnnxRuntime] Model metadata:" << std::endl;
        for (size_t i = 0; i < _inputNames.size(); ++i) {
            std::cout << "  Input[" << i << "]: " << _inputNames[i] 
                      << " type=" << ortTypeToString(_inputTypes[i]) 
                      << " shape=[";
            for (size_t j = 0; j < _inputShapes[i].size(); ++j) {
                std::cout << _inputShapes[i][j];
                if (j < _inputShapes[i].size() - 1) std::cout << ",";
            }
            std::cout << "]" << std::endl;
        }
        for (size_t i = 0; i < _outputNames.size(); ++i) {
            std::cout << "  Output[" << i << "]: " << _outputNames[i] 
                      << " type=" << ortTypeToString(_outputTypes[i])
                      << " shape=[";
            for (size_t j = 0; j < _outputShapes[i].size(); ++j) {
                std::cout << _outputShapes[i][j];
                if (j < _outputShapes[i].size() - 1) std::cout << ",";
            }
            std::cout << "]" << std::endl;
        }
    }
}

// ============================================================================
// Inference
// ============================================================================

InferenceResult OnnxRuntimeModel::forward(const cv::Mat& input) {
    if (!isLoaded()) {
        return InferenceResult::fail("Model not loaded");
    }
    
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Preprocess input image
        cv::Mat processed = preprocess(input);
        
        // Get expected input shape
        std::vector<int64_t> inputDims;
        if (!_inputShapes.empty() && !_inputShapes[0].empty()) {
            inputDims = _inputShapes[0];
            
            // Handle dynamic dimensions
            for (size_t i = 0; i < inputDims.size(); ++i) {
                if (inputDims[i] < 0) {
                    // Dynamic dimension - infer from input
                    if (i == 0) inputDims[i] = 1;  // Batch
                    else if (i == 1) inputDims[i] = processed.channels();  // Channels
                    else if (i == 2) inputDims[i] = processed.rows;  // Height
                    else if (i == 3) inputDims[i] = processed.cols;  // Width
                }
            }
        } else {
            // Default NCHW layout
            inputDims = {1, processed.channels(), processed.rows, processed.cols};
        }
        
        // Convert cv::Mat to float vector in NCHW format
        // IMPORTANT: This vector must stay alive until after inference completes!
        std::vector<float> inputData = matToFloatVector(processed);
        
        // Create memory info for CPU
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);
        
        // Create input tensor (uses inputData buffer directly, no copy)
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            inputData.data(),
            inputData.size(),
            inputDims.data(),
            inputDims.size()
        );
        
        // Run inference
        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(std::move(inputTensor));
        
        auto outputTensors = _session->Run(
            Ort::RunOptions{nullptr},
            _inputNamesCStr.data(),
            inputTensors.data(),
            inputTensors.size(),
            _outputNamesCStr.data(),
            _outputNames.size()
        );
        
        // Convert outputs to cv::Mat
        std::vector<cv::Mat> outputs;
        for (auto& tensor : outputTensors) {
            outputs.push_back(tensorToMat(tensor));
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double inferenceMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        auto result = InferenceResult::ok(outputs, _outputNames);
        result.inferenceTimeMs = inferenceMs;
        return result;
        
    } catch (const Ort::Exception& e) {
        return InferenceResult::fail(std::string("Inference failed: ") + e.what());
    } catch (const std::exception& e) {
        return InferenceResult::fail(std::string("Inference failed: ") + e.what());
    }
}

InferenceResult OnnxRuntimeModel::forwardMulti(const std::vector<cv::Mat>& inputs) {
    if (!isLoaded()) {
        return InferenceResult::fail("Model not loaded");
    }
    
    if (inputs.empty()) {
        return InferenceResult::fail("No inputs provided");
    }
    
    // For now, process first input only
    // TODO: Implement proper batching
    return forward(inputs[0]);
}

// ============================================================================
// Tensor Conversion
// ============================================================================

std::vector<float> OnnxRuntimeModel::matToFloatVector(const cv::Mat& mat) {
    // Ensure float type
    cv::Mat floatMat;
    if (mat.type() != CV_32F && mat.type() != CV_32FC3) {
        mat.convertTo(floatMat, CV_32F);
    } else {
        floatMat = mat;
    }
    
    // Convert HWC to CHW format
    int height = floatMat.rows;
    int width = floatMat.cols;
    int channels = floatMat.channels();
    
    std::vector<float> data(height * width * channels);
    
    if (channels == 1) {
        // Single channel - direct copy
        std::memcpy(data.data(), floatMat.ptr<float>(), data.size() * sizeof(float));
    } else {
        // Multi-channel - convert HWC to CHW
        std::vector<cv::Mat> chw(channels);
        for (int c = 0; c < channels; ++c) {
            chw[c] = cv::Mat(height, width, CV_32F, data.data() + c * height * width);
        }
        cv::split(floatMat, chw);
    }
    
    return data;
}

cv::Mat OnnxRuntimeModel::tensorToMat(const Ort::Value& tensor) {
    // Get tensor info
    auto tensorInfo = tensor.GetTensorTypeAndShapeInfo();
    auto shape = tensorInfo.GetShape();
    auto elementType = tensorInfo.GetElementType();
    
    // Get data pointer
    const float* data = tensor.GetTensorData<float>();
    
    // Determine output dimensions
    int numDims = static_cast<int>(shape.size());
    
    if (numDims == 1) {
        // 1D tensor -> 1xN Mat
        int size = static_cast<int>(shape[0]);
        cv::Mat result(1, size, CV_32F);
        std::memcpy(result.ptr<float>(), data, size * sizeof(float));
        return result;
        
    } else if (numDims == 2) {
        // 2D tensor -> HxW Mat
        int rows = static_cast<int>(shape[0]);
        int cols = static_cast<int>(shape[1]);
        cv::Mat result(rows, cols, CV_32F);
        std::memcpy(result.ptr<float>(), data, rows * cols * sizeof(float));
        return result;
        
    } else if (numDims == 3) {
        // 3D tensor (CHW) -> HxWxC Mat
        int channels = static_cast<int>(shape[0]);
        int height = static_cast<int>(shape[1]);
        int width = static_cast<int>(shape[2]);
        
        if (channels <= 4) {
            // Convert CHW to HWC
            std::vector<cv::Mat> channelMats;
            for (int c = 0; c < channels; ++c) {
                cv::Mat channel(height, width, CV_32F, 
                    const_cast<float*>(data + c * height * width));
                channelMats.push_back(channel.clone());
            }
            cv::Mat result;
            cv::merge(channelMats, result);
            return result;
        } else {
            // Too many channels - return as 2D (flatten first dim)
            int totalRows = channels * height;
            cv::Mat result(totalRows, width, CV_32F);
            std::memcpy(result.ptr<float>(), data, totalRows * width * sizeof(float));
            return result;
        }
        
    } else if (numDims == 4) {
        // 4D tensor (NCHW) -> For batch=1, treat as CHW
        int batch = static_cast<int>(shape[0]);
        int channels = static_cast<int>(shape[1]);
        int height = static_cast<int>(shape[2]);
        int width = static_cast<int>(shape[3]);
        
        if (batch == 1 && channels <= 4) {
            // Single batch, convert CHW to HWC
            std::vector<cv::Mat> channelMats;
            for (int c = 0; c < channels; ++c) {
                cv::Mat channel(height, width, CV_32F,
                    const_cast<float*>(data + c * height * width));
                channelMats.push_back(channel.clone());
            }
            cv::Mat result;
            cv::merge(channelMats, result);
            return result;
        } else {
            // Multiple batches or many channels - flatten to 2D
            int totalRows = batch * channels * height;
            cv::Mat result(totalRows, width, CV_32F);
            std::memcpy(result.ptr<float>(), data, totalRows * width * sizeof(float));
            return result;
        }
    }
    
    // Fallback: return as 1D
    size_t totalSize = 1;
    for (auto dim : shape) totalSize *= dim;
    cv::Mat result(1, static_cast<int>(totalSize), CV_32F);
    std::memcpy(result.ptr<float>(), data, totalSize * sizeof(float));
    return result;
}

// ============================================================================
// MLModel Interface Implementation
// ============================================================================

cv::Mat OnnxRuntimeModel::preprocess(const cv::Mat& input) {
    cv::Mat processed = input.clone();
    
    // Convert to float
    if (processed.depth() != CV_32F) {
        processed.convertTo(processed, CV_32F);
    }
    
    // Apply scale factor
    if (_config.scaleFactor != 1.0) {
        processed *= _config.scaleFactor;
    }
    
    // Subtract mean
    if (_config.mean != cv::Scalar(0, 0, 0)) {
        processed -= _config.mean;
    }
    
    // Resize if input size is specified
    cv::Size targetSize = getInputSize();
    if (targetSize.width > 0 && targetSize.height > 0 && 
        (processed.cols != targetSize.width || processed.rows != targetSize.height)) {
        cv::resize(processed, processed, targetSize);
    }
    
    // Swap R and B channels if needed
    if (_config.swapRB && processed.channels() == 3) {
        cv::cvtColor(processed, processed, cv::COLOR_BGR2RGB);
    }
    
    return processed;
}

void OnnxRuntimeModel::setPreferableBackend(int backend, int target) {
    // ONNX Runtime uses execution providers instead of backends
    // This is provided for interface compatibility
    (void)backend;
    (void)target;
}

std::vector<LayerInfo> OnnxRuntimeModel::getInputLayers() const {
    std::vector<LayerInfo> layers;
    for (size_t i = 0; i < _inputNames.size(); ++i) {
        LayerInfo info;
        info.name = _inputNames[i];
        for (auto dim : _inputShapes[i]) {
            info.shape.push_back(static_cast<int>(dim));
        }
        info.type = ortTypeToOpenCVType(_inputTypes[i]);
        layers.push_back(info);
    }
    return layers;
}

std::vector<LayerInfo> OnnxRuntimeModel::getOutputLayers() const {
    std::vector<LayerInfo> layers;
    for (size_t i = 0; i < _outputNames.size(); ++i) {
        LayerInfo info;
        info.name = _outputNames[i];
        for (auto dim : _outputShapes[i]) {
            info.shape.push_back(static_cast<int>(dim));
        }
        info.type = ortTypeToOpenCVType(_outputTypes[i]);
        layers.push_back(info);
    }
    return layers;
}

cv::Size OnnxRuntimeModel::getInputSize() const {
    // Use configured size if available
    if (_config.inputSize.width > 0 && _config.inputSize.height > 0) {
        return _config.inputSize;
    }
    
    // Try to infer from model metadata
    if (!_inputShapes.empty() && _inputShapes[0].size() >= 4) {
        // Assume NCHW format
        int64_t height = _inputShapes[0][2];
        int64_t width = _inputShapes[0][3];
        
        // Check for dynamic dimensions
        if (height > 0 && width > 0) {
            return cv::Size(static_cast<int>(width), static_cast<int>(height));
        }
    }
    
    // Return empty size for dynamic input
    return cv::Size(0, 0);
}

int OnnxRuntimeModel::getInputChannels() const {
    if (_config.inputChannels > 0) {
        return _config.inputChannels;
    }
    
    if (!_inputShapes.empty() && _inputShapes[0].size() >= 2) {
        // Assume NCHW format, channels at index 1
        int64_t channels = _inputShapes[0][1];
        if (channels > 0) {
            return static_cast<int>(channels);
        }
    }
    
    return 3;  // Default to RGB
}

// ============================================================================
// Utility Functions
// ============================================================================

int OnnxRuntimeModel::ortTypeToOpenCVType(ONNXTensorElementDataType ortType) {
    switch (ortType) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            return CV_32F;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            return CV_64F;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
            return CV_32S;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
            return CV_16S;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
            return CV_16U;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
            return CV_8S;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
            return CV_8U;
        default:
            return CV_32F;
    }
}

std::string OnnxRuntimeModel::ortTypeToString(ONNXTensorElementDataType ortType) {
    switch (ortType) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            return "float32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            return "float64";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
            return "int32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
            return "int64";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
            return "int16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
            return "int8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
            return "uint8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
            return "uint16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
            return "float16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
            return "bfloat16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
            return "bool";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
            return "string";
        default:
            return "unknown";
    }
}

// ============================================================================
// Factory Function
// ============================================================================

std::shared_ptr<MLModel> createOnnxRuntimeModel() {
    return std::make_shared<OnnxRuntimeModel>();
}

} // namespace ml
} // namespace visionpipe

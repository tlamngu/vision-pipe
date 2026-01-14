#include "interpreter/ml/opencv_dnn_backend.h"
#include "interpreter/ml/model_registry.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <filesystem>
#include <chrono>

namespace visionpipe {
namespace ml {

OpenCVDNNModel::OpenCVDNNModel() {}

bool OpenCVDNNModel::load(const std::string& path, const ModelConfig& config) {
    _modelPath = path;
    _config = config;
    
    try {
        // Determine model format from extension
        std::filesystem::path modelPath(path);
        std::string ext = modelPath.extension().string();
        
        if (ext == ".onnx") {
            _net = cv::dnn::readNetFromONNX(path);
        } else if (ext == ".caffemodel") {
            // Need prototxt file
            std::string prototxt = path.substr(0, path.length() - 11) + ".prototxt";
            _net = cv::dnn::readNetFromCaffe(prototxt, path);
        } else if (ext == ".pb") {
            // TensorFlow format
            std::string pbtxt = path.substr(0, path.length() - 3) + ".pbtxt";
            if (std::filesystem::exists(pbtxt)) {
                _net = cv::dnn::readNetFromTensorflow(path, pbtxt);
            } else {
                _net = cv::dnn::readNetFromTensorflow(path);
            }
        } else if (ext == ".weights") {
            // Darknet format - need cfg file
            std::string cfg = path.substr(0, path.length() - 8) + ".cfg";
            _net = cv::dnn::readNetFromDarknet(cfg, path);
        } else if (ext == ".cfg") {
            // Darknet cfg - find weights
            std::string weights = path.substr(0, path.length() - 4) + ".weights";
            _net = cv::dnn::readNetFromDarknet(path, weights);
        } else if (ext == ".t7" || ext == ".net") {
            // Torch format
            _net = cv::dnn::readNetFromTorch(path);
        } else {
            // Try ONNX by default
            _net = cv::dnn::readNetFromONNX(path);
        }
        
        if (_net.empty()) {
            std::cerr << "[OpenCVDNN] Failed to load model: " << path << std::endl;
            return false;
        }
        
        // Set backend and target
        int backend = parseBackendFromConfig(config.device);
        int target = parseTargetFromConfig(config.device);
        _net.setPreferableBackend(backend);
        _net.setPreferableTarget(target);
        
        // Detect output layers
        detectOutputLayers();
        
        _loaded = true;
        
        if (ModelRegistry::isVerbose()) {
            std::cout << "[OpenCVDNN] Loaded model: " << path << std::endl;
            std::cout << "[OpenCVDNN] Backend: " << backend << ", Target: " << target << std::endl;
        }
        
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "[OpenCVDNN] OpenCV error loading model: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[OpenCVDNN] Error loading model: " << e.what() << std::endl;
        return false;
    }
}

bool OpenCVDNNModel::isLoaded() const {
    return _loaded && !_net.empty();
}

InferenceResult OpenCVDNNModel::forward(const cv::Mat& input) {
    if (!isLoaded()) {
        return InferenceResult::fail("Model not loaded");
    }
    
    try {
        // Preprocess input
        cv::Mat blob = preprocess(input);
        
        // Set input
        _net.setInput(blob);
        
        // Forward pass
        std::vector<cv::Mat> outputs;
        _net.forward(outputs, _outputNames);
        
        return InferenceResult::ok(outputs, _outputNames);
    } catch (const cv::Exception& e) {
        return InferenceResult::fail(std::string("Inference failed: ") + e.what());
    }
}

InferenceResult OpenCVDNNModel::forwardMulti(const std::vector<cv::Mat>& inputs) {
    if (!isLoaded()) {
        return InferenceResult::fail("Model not loaded");
    }
    
    if (inputs.empty()) {
        return InferenceResult::fail("No inputs provided");
    }
    
    try {
        // Create batch blob
        std::vector<cv::Mat> blobs;
        for (const auto& input : inputs) {
            blobs.push_back(preprocess(input));
        }
        
        // Concatenate into batch
        cv::Mat batchBlob;
        cv::vconcat(blobs, batchBlob);
        
        _net.setInput(batchBlob);
        
        std::vector<cv::Mat> outputs;
        _net.forward(outputs, _outputNames);
        
        return InferenceResult::ok(outputs, _outputNames);
    } catch (const cv::Exception& e) {
        return InferenceResult::fail(std::string("Batch inference failed: ") + e.what());
    }
}

std::vector<LayerInfo> OpenCVDNNModel::getInputLayers() const {
    std::vector<LayerInfo> layers;
    // OpenCV DNN doesn't expose detailed input layer info easily
    // Return basic info based on config
    LayerInfo info;
    info.name = "input";
    info.shape = {1, _config.inputChannels, _config.inputSize.height, _config.inputSize.width};
    info.type = CV_32F;
    layers.push_back(info);
    return layers;
}

std::vector<LayerInfo> OpenCVDNNModel::getOutputLayers() const {
    std::vector<LayerInfo> layers;
    for (const auto& name : _outputNames) {
        LayerInfo info;
        info.name = name;
        // Shape is dynamic, would need to run inference to get actual shape
        info.type = CV_32F;
        layers.push_back(info);
    }
    return layers;
}

cv::Size OpenCVDNNModel::getInputSize() const {
    return _config.inputSize;
}

int OpenCVDNNModel::getInputChannels() const {
    return _config.inputChannels;
}

cv::Mat OpenCVDNNModel::preprocess(const cv::Mat& input) {
    // If input is empty, return empty mat
    if (input.empty()) {
        std::cerr << "[OpenCVDNN] Warning: Empty input for preprocessing" << std::endl;
        return cv::Mat();
    }
    
    // If input is already a 4D blob (NCHW format), use it directly
    // Blobs have dims == 4 and are float type
    if (input.dims == 4) {
        return input.clone();
    }
    
    cv::Size size = _config.inputSize;
    if (size.width == 0 || size.height == 0) {
        size = input.size();
    }
    
    return cv::dnn::blobFromImage(
        input,
        _config.scaleFactor,
        size,
        _config.mean,
        _config.swapRB,
        _config.crop
    );
}

void OpenCVDNNModel::setPreferableBackend(int backend, int target) {
    _net.setPreferableBackend(backend);
    _net.setPreferableTarget(target);
}

std::vector<std::string> OpenCVDNNModel::getOutputNames() const {
    return _outputNames;
}

void OpenCVDNNModel::detectOutputLayers() {
    _outputNames = _net.getUnconnectedOutLayersNames();
    
    if (_outputNames.empty()) {
        // Fallback: get all layer names and use the last one
        std::vector<std::string> layerNames = _net.getLayerNames();
        if (!layerNames.empty()) {
            _outputNames.push_back(layerNames.back());
        }
    }
}

int OpenCVDNNModel::parseBackendFromConfig(const std::string& device) {
    if (device.find("cuda") != std::string::npos) {
#ifdef VISIONPIPE_CUDA_ENABLED
        return cv::dnn::DNN_BACKEND_CUDA;
#else
        std::cerr << "[OpenCVDNN] CUDA requested but not available, using default" << std::endl;
        return cv::dnn::DNN_BACKEND_DEFAULT;
#endif
    } else if (device.find("opencl") != std::string::npos || device.find("gpu") != std::string::npos) {
        return cv::dnn::DNN_BACKEND_OPENCV;  // OpenCV uses OpenCL by default for GPU
    } else if (device.find("vulkan") != std::string::npos) {
        return cv::dnn::DNN_BACKEND_VKCOM;
    }
    return cv::dnn::DNN_BACKEND_DEFAULT;
}

int OpenCVDNNModel::parseTargetFromConfig(const std::string& device) {
    if (device.find("cuda") != std::string::npos) {
#ifdef VISIONPIPE_CUDA_ENABLED
        if (device.find("fp16") != std::string::npos) {
            return cv::dnn::DNN_TARGET_CUDA_FP16;
        }
        return cv::dnn::DNN_TARGET_CUDA;
#else
        return cv::dnn::DNN_TARGET_CPU;
#endif
    } else if (device.find("opencl") != std::string::npos) {
#ifdef VISIONPIPE_OPENCL_ENABLED
        if (device.find("fp16") != std::string::npos) {
            return cv::dnn::DNN_TARGET_OPENCL_FP16;
        }
        return cv::dnn::DNN_TARGET_OPENCL;
#else
        return cv::dnn::DNN_TARGET_CPU;
#endif
    } else if (device.find("vulkan") != std::string::npos) {
        return cv::dnn::DNN_TARGET_VULKAN;
    }
    return cv::dnn::DNN_TARGET_CPU;
}

std::shared_ptr<MLModel> createOpenCVDNNModel() {
    return std::make_shared<OpenCVDNNModel>();
}

} // namespace ml
} // namespace visionpipe

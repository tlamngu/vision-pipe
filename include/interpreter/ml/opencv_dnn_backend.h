#ifndef VISIONPIPE_OPENCV_DNN_BACKEND_H
#define VISIONPIPE_OPENCV_DNN_BACKEND_H

#include "interpreter/ml/ml_model.h"
#include <opencv2/dnn.hpp>

namespace visionpipe {
namespace ml {

/**
 * @brief OpenCV DNN backend implementation
 * 
 * This backend uses OpenCV's DNN module to load and run models.
 * Supports ONNX, Caffe, TensorFlow, Darknet, and TorchScript formats.
 */
class OpenCVDNNModel : public MLModel {
public:
    OpenCVDNNModel();
    ~OpenCVDNNModel() override = default;
    
    bool load(const std::string& path, const ModelConfig& config = ModelConfig()) override;
    bool isLoaded() const override;
    
    InferenceResult forward(const cv::Mat& input) override;
    InferenceResult forwardMulti(const std::vector<cv::Mat>& inputs) override;
    
    std::vector<LayerInfo> getInputLayers() const override;
    std::vector<LayerInfo> getOutputLayers() const override;
    
    cv::Size getInputSize() const override;
    int getInputChannels() const override;
    
    std::string getModelPath() const override { return _modelPath; }
    std::string getBackendName() const override { return "opencv"; }
    const ModelConfig& getConfig() const override { return _config; }
    
    cv::Mat preprocess(const cv::Mat& input) override;
    void setPreferableBackend(int backend, int target) override;
    
    /**
     * @brief Get the underlying OpenCV DNN network
     */
    cv::dnn::Net& getNet() { return _net; }
    const cv::dnn::Net& getNet() const { return _net; }
    
    /**
     * @brief Get output layer names
     */
    std::vector<std::string> getOutputNames() const;

private:
    cv::dnn::Net _net;
    std::string _modelPath;
    ModelConfig _config;
    bool _loaded = false;
    std::vector<std::string> _outputNames;
    
    void detectOutputLayers();
    int parseBackendFromConfig(const std::string& device);
    int parseTargetFromConfig(const std::string& device);
};

/**
 * @brief Factory function for OpenCV DNN backend
 */
std::shared_ptr<MLModel> createOpenCVDNNModel();

} // namespace ml
} // namespace visionpipe

#endif // VISIONPIPE_OPENCV_DNN_BACKEND_H

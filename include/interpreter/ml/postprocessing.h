#ifndef VISIONPIPE_ML_POSTPROCESSING_H
#define VISIONPIPE_ML_POSTPROCESSING_H

#include <opencv2/core/mat.hpp>
#include <opencv2/dnn.hpp>
#include <vector>
#include <string>

namespace visionpipe {
namespace ml {

/**
 * @brief Detection result structure
 */
struct Detection {
    int classId;
    float confidence;
    cv::Rect box;
    std::string label;
    
    // Optional: for pose/keypoint models
    std::vector<cv::Point2f> keypoints;
    std::vector<float> keypointConfidences;
    
    // Optional: for instance segmentation
    cv::Mat mask;
};

/**
 * @brief Classification result
 */
struct Classification {
    int classId;
    float confidence;
    std::string label;
};

/**
 * @brief Postprocessing utilities for ML model outputs
 */
class Postprocessing {
public:
    /**
     * @brief Apply Non-Maximum Suppression
     * @param boxes Bounding boxes
     * @param scores Confidence scores
     * @param scoreThreshold Minimum score threshold
     * @param nmsThreshold NMS IoU threshold
     * @return Indices of kept boxes
     */
    static std::vector<int> nms(const std::vector<cv::Rect>& boxes,
                                 const std::vector<float>& scores,
                                 float scoreThreshold,
                                 float nmsThreshold);
    
    /**
     * @brief Decode YOLO output (supports v5, v8, v10, v11)
     * @param output Raw model output
     * @param imageSize Original image size
     * @param inputSize Model input size
     * @param confThreshold Confidence threshold
     * @param version YOLO version ("v5", "v8", "v10", "v11")
     * @return Vector of detections
     */
    static std::vector<Detection> decodeYolo(const cv::Mat& output,
                                              cv::Size imageSize,
                                              cv::Size inputSize,
                                              float confThreshold,
                                              const std::string& version = "v8");
    
    /**
     * @brief Decode YOLOv8 format specifically
     */
    static std::vector<Detection> decodeYolov8(const cv::Mat& output,
                                                cv::Size imageSize,
                                                cv::Size inputSize,
                                                float confThreshold);
    
    /**
     * @brief Decode YOLOv5 format specifically  
     */
    static std::vector<Detection> decodeYolov5(const cv::Mat& output,
                                                cv::Size imageSize,
                                                cv::Size inputSize,
                                                float confThreshold);
    
    /**
     * @brief Apply softmax to tensor
     */
    static cv::Mat softmax(const cv::Mat& input, int axis = 1);
    
    /**
     * @brief Apply sigmoid to tensor
     */
    static cv::Mat sigmoid(const cv::Mat& input);
    
    /**
     * @brief Get argmax along axis
     */
    static cv::Mat argmax(const cv::Mat& input, int axis = 1);
    
    /**
     * @brief Get top-k predictions
     * @param input Input scores (1D or 2D)
     * @param k Number of top predictions
     * @return Vector of (classId, score) pairs
     */
    static std::vector<Classification> topK(const cv::Mat& input, int k);
    
    /**
     * @brief Scale boxes from model input size to original image size
     */
    static void scaleBoxes(std::vector<Detection>& detections,
                           cv::Size fromSize, cv::Size toSize);
    
    /**
     * @brief Rescale boxes accounting for letterbox padding
     */
    static void rescaleBoxesLetterbox(std::vector<Detection>& detections,
                                       cv::Size inputSize, cv::Size imageSize,
                                       float scale, int padX, int padY);
    
    /**
     * @brief Load class labels from file
     */
    static std::vector<std::string> loadLabels(const std::string& path);
    
    /**
     * @brief Apply labels to detections
     */
    static void applyLabels(std::vector<Detection>& detections,
                            const std::vector<std::string>& labels);
};

} // namespace ml
} // namespace visionpipe

#endif // VISIONPIPE_ML_POSTPROCESSING_H

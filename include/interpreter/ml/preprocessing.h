#ifndef VISIONPIPE_ML_PREPROCESSING_H
#define VISIONPIPE_ML_PREPROCESSING_H

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace visionpipe {
namespace ml {

/**
 * @brief Preprocessing utilities for ML models
 */
class Preprocessing {
public:
    /**
     * @brief Letterbox resize (maintain aspect ratio with padding)
     * @param input Input image
     * @param targetSize Target size
     * @param color Padding color (default: gray)
     * @return Letterboxed image and scale info
     */
    struct LetterboxResult {
        cv::Mat image;
        float scale;
        int padX;
        int padY;
    };
    static LetterboxResult letterbox(const cv::Mat& input, cv::Size targetSize, 
                                      cv::Scalar color = cv::Scalar(114, 114, 114));
    
    /**
     * @brief Normalize image values
     * @param input Input image
     * @param minIn Minimum input value
     * @param maxIn Maximum input value
     * @param minOut Minimum output value
     * @param maxOut Maximum output value
     * @return Normalized image
     */
    static cv::Mat normalize(const cv::Mat& input, double minIn, double maxIn, 
                             double minOut, double maxOut);
    
    /**
     * @brief Normalize with mean and std (ImageNet-style)
     * @param input Input image (assumed [0-255] or [0-1])
     * @param mean Per-channel mean values
     * @param std Per-channel std values
     * @return Normalized image
     */
    static cv::Mat normalizeMeanStd(const cv::Mat& input, 
                                     const std::vector<double>& mean,
                                     const std::vector<double>& std);
    
    /**
     * @brief Create blob from image (OpenCV DNN style)
     * @param input Input image
     * @param scaleFactor Scale factor
     * @param size Target size
     * @param mean Mean subtraction
     * @param swapRB Swap R and B channels
     * @param crop Crop after resize
     * @return 4D blob
     */
    static cv::Mat blobFromImage(const cv::Mat& input, double scaleFactor,
                                  cv::Size size, cv::Scalar mean, 
                                  bool swapRB = true, bool crop = false);
    
    /**
     * @brief Convert NHWC to NCHW format
     */
    static cv::Mat nhwcToNchw(const cv::Mat& input);
    
    /**
     * @brief Convert NCHW to NHWC format
     */
    static cv::Mat nchwToNhwc(const cv::Mat& input);
    
    /**
     * @brief Pad image to target size
     */
    static cv::Mat padToSize(const cv::Mat& input, cv::Size targetSize, 
                             cv::Scalar color = cv::Scalar(0, 0, 0));
    
    /**
     * @brief Center crop image
     */
    static cv::Mat centerCrop(const cv::Mat& input, cv::Size cropSize);
};

} // namespace ml
} // namespace visionpipe

#endif // VISIONPIPE_ML_PREPROCESSING_H

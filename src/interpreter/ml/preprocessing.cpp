#include "interpreter/ml/preprocessing.h"
#include <opencv2/dnn.hpp>

namespace visionpipe {
namespace ml {

Preprocessing::LetterboxResult Preprocessing::letterbox(const cv::Mat& input, cv::Size targetSize, 
                                                         cv::Scalar color) {
    LetterboxResult result;
    
    int inputW = input.cols;
    int inputH = input.rows;
    int targetW = targetSize.width;
    int targetH = targetSize.height;
    
    // Calculate scale
    float scaleW = static_cast<float>(targetW) / inputW;
    float scaleH = static_cast<float>(targetH) / inputH;
    result.scale = std::min(scaleW, scaleH);
    
    // Calculate new size
    int newW = static_cast<int>(inputW * result.scale);
    int newH = static_cast<int>(inputH * result.scale);
    
    // Calculate padding
    result.padX = (targetW - newW) / 2;
    result.padY = (targetH - newH) / 2;
    
    // Resize image
    cv::Mat resized;
    cv::resize(input, resized, cv::Size(newW, newH));
    
    // Create output with padding
    result.image = cv::Mat(targetH, targetW, input.type(), color);
    
    // Copy resized image to center
    resized.copyTo(result.image(cv::Rect(result.padX, result.padY, newW, newH)));
    
    return result;
}

cv::Mat Preprocessing::normalize(const cv::Mat& input, double minIn, double maxIn,
                                  double minOut, double maxOut) {
    cv::Mat output;
    input.convertTo(output, CV_32F);
    
    // Normalize: out = (in - minIn) / (maxIn - minIn) * (maxOut - minOut) + minOut
    double scale = (maxOut - minOut) / (maxIn - minIn);
    double shift = minOut - minIn * scale;
    
    output = output * scale + shift;
    
    return output;
}

cv::Mat Preprocessing::normalizeMeanStd(const cv::Mat& input,
                                         const std::vector<double>& mean,
                                         const std::vector<double>& std) {
    cv::Mat output;
    input.convertTo(output, CV_32F);
    
    // Normalize to [0, 1] if needed (assuming input is [0, 255])
    if (output.depth() == CV_32F) {
        double minVal, maxVal;
        cv::minMaxLoc(output, &minVal, &maxVal);
        if (maxVal > 1.0) {
            output = output / 255.0;
        }
    }
    
    // Split channels
    std::vector<cv::Mat> channels;
    cv::split(output, channels);
    
    // Apply mean/std normalization per channel
    for (size_t i = 0; i < channels.size() && i < mean.size(); ++i) {
        channels[i] = (channels[i] - mean[i]) / std[i];
    }
    
    cv::merge(channels, output);
    
    return output;
}

cv::Mat Preprocessing::blobFromImage(const cv::Mat& input, double scaleFactor,
                                      cv::Size size, cv::Scalar mean,
                                      bool swapRB, bool crop) {
    return cv::dnn::blobFromImage(input, scaleFactor, size, mean, swapRB, crop);
}

cv::Mat Preprocessing::nhwcToNchw(const cv::Mat& input) {
    // Assuming input is HWC (height, width, channels)
    // Output should be CHW (channels, height, width)
    
    if (input.dims != 3 && input.channels() == 1) {
        // Already in correct format or grayscale
        return input.clone();
    }
    
    std::vector<cv::Mat> channels;
    cv::split(input, channels);
    
    cv::Mat output;
    cv::vconcat(channels, output);
    
    return output.reshape(1, {static_cast<int>(channels.size()), input.rows, input.cols});
}

cv::Mat Preprocessing::nchwToNhwc(const cv::Mat& input) {
    // Assuming input is CHW, convert to HWC
    if (input.dims != 3) {
        return input.clone();
    }
    
    int channels = input.size[0];
    int height = input.size[1];
    int width = input.size[2];
    
    std::vector<cv::Mat> channelMats;
    for (int c = 0; c < channels; ++c) {
        cv::Mat channelMat(height, width, CV_32F, 
                          const_cast<float*>(input.ptr<float>(c)));
        channelMats.push_back(channelMat.clone());
    }
    
    cv::Mat output;
    cv::merge(channelMats, output);
    
    return output;
}

cv::Mat Preprocessing::padToSize(const cv::Mat& input, cv::Size targetSize, cv::Scalar color) {
    if (input.cols >= targetSize.width && input.rows >= targetSize.height) {
        return input.clone();
    }
    
    cv::Mat output(targetSize, input.type(), color);
    input.copyTo(output(cv::Rect(0, 0, input.cols, input.rows)));
    
    return output;
}

cv::Mat Preprocessing::centerCrop(const cv::Mat& input, cv::Size cropSize) {
    int x = (input.cols - cropSize.width) / 2;
    int y = (input.rows - cropSize.height) / 2;
    
    x = std::max(0, x);
    y = std::max(0, y);
    
    int w = std::min(cropSize.width, input.cols - x);
    int h = std::min(cropSize.height, input.rows - y);
    
    return input(cv::Rect(x, y, w, h)).clone();
}

} // namespace ml
} // namespace visionpipe

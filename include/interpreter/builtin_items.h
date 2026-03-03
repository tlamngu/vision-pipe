#ifndef VISIONPIPE_BUILTIN_ITEMS_H
#define VISIONPIPE_BUILTIN_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <memory>
#include <map>

namespace visionpipe {

/**
 * @brief Register all built-in interpreter items
 */
void registerBuiltinItems(ItemRegistry& registry);

// ============================================================================
// Video I/O Items
// ============================================================================

/**
 * @brief Captures video from a source
 * 
 * Supports:
 * - Device index (0, 1, 2, ...)
 * - Video file path
 * - RTSP/HTTP URLs
 */
class VideoCaptureItem : public InterpreterItem {
public:
    VideoCaptureItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    std::map<std::string, std::shared_ptr<cv::VideoCapture>> _captures;
};

/**
 * @brief Writes frames to a video file
 */
class VideoWriteItem : public InterpreterItem {
public:
    VideoWriteItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    std::map<std::string, std::shared_ptr<cv::VideoWriter>> _writers;
};

/**
 * @brief Saves the current frame to an image file
 */
class SaveImageItem : public InterpreterItem {
public:
    SaveImageItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Loads an image from a file
 */
class LoadImageItem : public InterpreterItem {
public:
    LoadImageItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Image Processing Items
// ============================================================================

/**
 * @brief Resizes the current frame
 */
class ResizeItem : public InterpreterItem {
public:
    ResizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }
};

/**
 * @brief Converts color space
 */
class ColorConvertItem : public InterpreterItem {
public:
    ColorConvertItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }
};

/**
 * @brief Applies Gaussian blur
 */
class GaussianBlurItem : public InterpreterItem {
public:
    GaussianBlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies median blur
 */
class MedianBlurItem : public InterpreterItem {
public:
    MedianBlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies bilateral filter
 */
class BilateralFilterItem : public InterpreterItem {
public:
    BilateralFilterItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Detects edges using Canny algorithm
 */
class CannyEdgeItem : public InterpreterItem {
public:
    CannyEdgeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies threshold
 */
class ThresholdItem : public InterpreterItem {
public:
    ThresholdItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies adaptive threshold
 */
class AdaptiveThresholdItem : public InterpreterItem {
public:
    AdaptiveThresholdItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Morphological operations (erode, dilate, open, close)
 */
class MorphologyItem : public InterpreterItem {
public:
    MorphologyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies histogram equalization
 */
class EqualizeHistItem : public InterpreterItem {
public:
    EqualizeHistItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Stereo Vision Items
// ============================================================================

/**
 * @brief Undistorts an image using calibration parameters
 */
class UndistortItem : public InterpreterItem {
public:
    UndistortItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    std::map<std::string, std::pair<cv::Mat, cv::Mat>> _rectifyMaps;
};

/**
 * @brief Computes stereo disparity using SGBM
 */
class StereoSGBMItem : public InterpreterItem {
public:
    StereoSGBMItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    cv::Ptr<cv::StereoSGBM> _sgbm;
};

/**
 * @brief Computes stereo disparity using BM
 */
class StereoBMItem : public InterpreterItem {
public:
    StereoBMItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    cv::Ptr<cv::StereoBM> _bm;
};

/**
 * @brief Applies colormap to disparity/depth visualization
 */
class ApplyColormapItem : public InterpreterItem {
public:
    ApplyColormapItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Display & Debug Items
// ============================================================================

/**
 * @brief Displays the current frame in a window
 */
class DisplayItem : public InterpreterItem {
public:
    DisplayItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Waits for a key press
 */
class WaitKeyItem : public InterpreterItem {
public:
    WaitKeyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Prints debug information
 */
class PrintItem : public InterpreterItem {
public:
    PrintItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Logs Mat properties
 */
class DebugMatItem : public InterpreterItem {
public:
    DebugMatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Cache & Control Items
// ============================================================================

/**
 * @brief Loads a cached Mat
 */
class UseItem : public InterpreterItem {
public:
    UseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Caches the current Mat
 */
class CacheItem : public InterpreterItem {
public:
    CacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

/**
 * @brief Creates a pass-through (no-op)
 */
class PassItem : public InterpreterItem {
public:
    PassItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
    bool isPure() const override { return true; }
};

/**
 * @brief Sleeps for specified milliseconds
 */
class SleepItem : public InterpreterItem {
public:
    SleepItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

// ============================================================================
// Drawing Items
// ============================================================================

/**
 * @brief Draws text on the image
 */
class DrawTextItem : public InterpreterItem {
public:
    DrawTextItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws a rectangle on the image
 */
class DrawRectItem : public InterpreterItem {
public:
    DrawRectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws a circle on the image
 */
class DrawCircleItem : public InterpreterItem {
public:
    DrawCircleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws a line on the image
 */
class DrawLineItem : public InterpreterItem {
public:
    DrawLineItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Frame Manipulation Items
// ============================================================================

/**
 * @brief Horizontally stacks two frames
 */
class HStackItem : public InterpreterItem {
public:
    HStackItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Vertically stacks two frames
 */
class VStackItem : public InterpreterItem {
public:
    VStackItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Crops a region of interest
 */
class CropItem : public InterpreterItem {
public:
    CropItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Rotates the image
 */
class RotateItem : public InterpreterItem {
public:
    RotateItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Flips the image
 */
class FlipItem : public InterpreterItem {
public:
    FlipItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// IPC Items (Iceoryx2 support)
// ============================================================================

#ifdef VISIONPIPE_ICEORYX2_ENABLED

/**
 * @brief Publishes the current frame via Iceoryx2
 */
class PublishFrameItem : public InterpreterItem {
public:
    PublishFrameItem();
    ~PublishFrameItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
    
private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief Subscribes to frames from another process
 */
class SubscribeFrameItem : public InterpreterItem {
public:
    SubscribeFrameItem();
    ~SubscribeFrameItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // VISIONPIPE_ICEORYX2_ENABLED

// ============================================================================
// FastCV Items (optional acceleration)
// ============================================================================

#ifdef VISIONPIPE_FASTCV_ENABLED

/**
 * @brief Fast Gaussian blur using FastCV (supports 3x3 and 5x5 kernels)
 */
class FastGaussianBlurItem : public InterpreterItem {
public:
    FastGaussianBlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fast FAST10 corner detection using FastCV
 */
class FastCornerDetectItem : public InterpreterItem {
public:
    FastCornerDetectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fast image pyramid generation using FastCV
 */
class FastPyramidItem : public InterpreterItem {
public:
    FastPyramidItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV-accelerated FFT
 */
class FastFFTItem : public InterpreterItem {
public:
    FastFFTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV-accelerated IFFT
 */
class FastIFFTItem : public InterpreterItem {
public:
    FastIFFTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV Hough line detection
 */
class FastHoughLinesItem : public InterpreterItem {
public:
    FastHoughLinesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV image moments
 */
class FastMomentsItem : public InterpreterItem {
public:
    FastMomentsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV range threshold - sets pixels in [lowThresh, highThresh] to trueValue, others to falseValue
 */
class FastThresholdRangeItem : public InterpreterItem {
public:
    FastThresholdRangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV bilateral filter (5x5, 7x7, or 9x9 kernels)
 */
class FastBilateralFilterItem : public InterpreterItem {
public:
    FastBilateralFilterItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV recursive bilateral filter (edge-preserving, fast)
 */
class FastBilateralRecursiveItem : public InterpreterItem {
public:
    FastBilateralRecursiveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV Sobel edge detection (produces dx and dy gradients stored to cache)
 */
class FastSobelItem : public InterpreterItem {
public:
    FastSobelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV accelerated downscale resize
 */
class FastResizeDownItem : public InterpreterItem {
public:
    FastResizeDownItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV 2D convolution filter
 */
class FastFilter2DItem : public InterpreterItem {
public:
    FastFilter2DItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV MSER region detector
 */
class FastMSERItem : public InterpreterItem {
public:
    FastMSERItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV warp perspective transform
 */
class FastWarpPerspectiveItem : public InterpreterItem {
public:
    FastWarpPerspectiveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV warp affine transform
 */
class FastWarpAffineItem : public InterpreterItem {
public:
    FastWarpAffineItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV mean shift tracking
 */
class FastMeanShiftItem : public InterpreterItem {
public:
    FastMeanShiftItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV LK sparse optical flow tracking
 */
class FastTrackOpticalFlowLKItem : public InterpreterItem {
public:
    FastTrackOpticalFlowLKItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;

private:
    cv::Mat _prevFrame;
    std::vector<cv::Point2f> _prevPts;
};

/**
 * @brief FastCV histogram calculation (grayscale histogram visualization)
 */
class FastCalcHistItem : public InterpreterItem {
public:
    FastCalcHistItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FastCV local box normalization
 */
class FastNormalizeLocalBoxItem : public InterpreterItem {
public:
    FastNormalizeLocalBoxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

#endif // VISIONPIPE_FASTCV_ENABLED


// ============================================================================
// Mat Geometry Utilities
// ============================================================================

/**
 * @brief Returns or checks the size of the current Mat
 *
 * Signatures:
 *   mat_size()        → [width, height]  (array of two integers)
 *   mat_size(w, h)    → bool  (true if current mat matches w × h)
 *   mat_size(w, h, c) → bool  (true if current mat matches w × h and has c channels)
 */
class MatSizeItem : public InterpreterItem {
public:
    MatSizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Returns the number of channels in the current Mat
 *
 *   mat_channels() → int
 */
class MatChannelsItem : public InterpreterItem {
public:
    MatChannelsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_BUILTIN_ITEMS_H

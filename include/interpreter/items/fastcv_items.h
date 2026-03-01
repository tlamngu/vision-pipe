#ifndef VISIONPIPE_FASTCV_ITEMS_H
#define VISIONPIPE_FASTCV_ITEMS_H

#ifdef VISIONPIPE_FASTCV_ENABLED

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <vector>

namespace visionpipe {

void registerFastCVItems(ItemRegistry& registry);

// ============================================================================
// FastCV Items — Qualcomm FastCV accelerated operations
// (available only when VISIONPIPE_FASTCV_ENABLED is defined)
// ============================================================================

/** @brief Fast Gaussian blur (FastCV-accelerated, 3x3 or 5x5 kernels) */
class FastGaussianBlurItem : public InterpreterItem {
public:
    FastGaussianBlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief FAST10 corner detection (FastCV-accelerated) */
class FastCornerDetectItem : public InterpreterItem {
public:
    FastCornerDetectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Image pyramid builder (FastCV-accelerated, stores levels in cache) */
class FastPyramidItem : public InterpreterItem {
public:
    FastPyramidItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief FFT (FastCV-accelerated, returns magnitude spectrum visualization) */
class FastFFTItem : public InterpreterItem {
public:
    FastFFTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief IFFT (FastCV-accelerated, input must be 2-channel CV_32F) */
class FastIFFTItem : public InterpreterItem {
public:
    FastIFFTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Hough line detection (FastCV-accelerated) */
class FastHoughLinesItem : public InterpreterItem {
public:
    FastHoughLinesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Image moments (FastCV-accelerated, prints centroid to console) */
class FastMomentsItem : public InterpreterItem {
public:
    FastMomentsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Range threshold: in-range → trueValue, out-of-range → falseValue */
class FastThresholdRangeItem : public InterpreterItem {
public:
    FastThresholdRangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Bilateral filter (FastCV-accelerated, d: 5, 7, or 9) */
class FastBilateralFilterItem : public InterpreterItem {
public:
    FastBilateralFilterItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Recursive bilateral filter (FastCV-accelerated, fast edge-preserving) */
class FastBilateralRecursiveItem : public InterpreterItem {
public:
    FastBilateralRecursiveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Sobel edge detection (FastCV-accelerated, stores dx/dy to cache) */
class FastSobelItem : public InterpreterItem {
public:
    FastSobelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Optimized downscale resize (FastCV-accelerated) */
class FastResizeDownItem : public InterpreterItem {
public:
    FastResizeDownItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief 2D convolution filter (FastCV-accelerated) */
class FastFilter2DItem : public InterpreterItem {
public:
    FastFilter2DItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief MSER region detector (FastCV-accelerated, draws contours) */
class FastMSERItem : public InterpreterItem {
public:
    FastMSERItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Perspective warp (FastCV-accelerated, matrix loaded from cache) */
class FastWarpPerspectiveItem : public InterpreterItem {
public:
    FastWarpPerspectiveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Affine warp (FastCV-accelerated, matrix loaded from cache) */
class FastWarpAffineItem : public InterpreterItem {
public:
    FastWarpAffineItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Mean shift tracking (FastCV-accelerated, ROI rect persisted in cache) */
class FastMeanShiftItem : public InterpreterItem {
public:
    FastMeanShiftItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief LK sparse optical flow (FastCV-accelerated, re-detects corners on loss) */
class FastTrackOpticalFlowLKItem : public InterpreterItem {
public:
    FastTrackOpticalFlowLKItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;

private:
    cv::Mat _prevFrame;
    std::vector<cv::Point2f> _prevPts;
};

/** @brief Grayscale histogram (FastCV-accelerated, returns bar chart visualization) */
class FastCalcHistItem : public InterpreterItem {
public:
    FastCalcHistItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/** @brief Local box normalization / contrast normalization (FastCV-accelerated) */
class FastNormalizeLocalBoxItem : public InterpreterItem {
public:
    FastNormalizeLocalBoxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_FASTCV_ENABLED
#endif // VISIONPIPE_FASTCV_ITEMS_H

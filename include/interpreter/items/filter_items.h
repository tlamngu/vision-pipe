#ifndef VISIONPIPE_FILTER_ITEMS_H
#define VISIONPIPE_FILTER_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerFilterItems(ItemRegistry& registry);

// ============================================================================
// Blur / Smoothing
// ============================================================================

/**
 * @brief Applies Gaussian blur
 * 
 * Parameters:
 * - ksize: Kernel size (odd number, default: 5)
 * - sigma_x: Gaussian sigma X (default: 0 = auto from ksize)
 * - sigma_y: Gaussian sigma Y (default: 0 = same as sigma_x)
 * - border: Border type ("default", "reflect", "replicate", "wrap")
 */
class GaussianBlurItem : public InterpreterItem {
public:
    GaussianBlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies median blur (good for salt-and-pepper noise)
 * 
 * Parameters:
 * - ksize: Kernel size (odd number, default: 5)
 */
class MedianBlurItem : public InterpreterItem {
public:
    MedianBlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies bilateral filter (edge-preserving smoothing)
 * 
 * Parameters:
 * - d: Diameter of pixel neighborhood (default: 9)
 * - sigma_color: Filter sigma in color space (default: 75)
 * - sigma_space: Filter sigma in coordinate space (default: 75)
 */
class BilateralFilterItem : public InterpreterItem {
public:
    BilateralFilterItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies box blur (simple averaging)
 * 
 * Parameters:
 * - ksize: Kernel size (default: 5)
 * - normalize: Normalize kernel (default: true)
 */
class BoxFilterItem : public InterpreterItem {
public:
    BoxFilterItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies simple blur (normalized box filter)
 */
class BlurItem : public InterpreterItem {
public:
    BlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies stack blur (fast approximation of Gaussian)
 */
class StackBlurItem : public InterpreterItem {
public:
    StackBlurItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Sharpening
// ============================================================================

/**
 * @brief Sharpens image using unsharp masking
 * 
 * Parameters:
 * - sigma: Gaussian sigma for blur (default: 1.0)
 * - strength: Sharpening strength (default: 1.5)
 * - threshold: Threshold for edge detection (default: 0)
 */
class SharpenItem : public InterpreterItem {
public:
    SharpenItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies Laplacian filter
 * 
 * Parameters:
 * - ksize: Kernel size (1, 3, 5, or 7; default: 3)
 * - scale: Scale factor (default: 1)
 * - delta: Optional delta added to results (default: 0)
 */
class LaplacianItem : public InterpreterItem {
public:
    LaplacianItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Custom Filters
// ============================================================================

/**
 * @brief Applies custom 2D convolution filter
 * 
 * Parameters:
 * - kernel: Custom kernel as 2D array
 * - anchor_x, anchor_y: Anchor point (default: center)
 * - delta: Optional value added to results
 * - border: Border type
 */
class Filter2DItem : public InterpreterItem {
public:
    Filter2DItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies separable linear filter
 * 
 * Parameters:
 * - kernel_x: Row filter kernel
 * - kernel_y: Column filter kernel
 */
class SepFilter2DItem : public InterpreterItem {
public:
    SepFilter2DItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Gradient Filters
// ============================================================================

/**
 * @brief Applies Sobel derivative filter
 * 
 * Parameters:
 * - dx: Order of derivative in x (default: 1)
 * - dy: Order of derivative in y (default: 0)
 * - ksize: Kernel size (1, 3, 5, or 7; default: 3)
 * - scale: Scale factor (default: 1)
 */
class SobelItem : public InterpreterItem {
public:
    SobelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies Scharr derivative filter (more accurate than Sobel 3x3)
 * 
 * Parameters:
 * - dx: Order of derivative in x (default: 1)
 * - dy: Order of derivative in y (default: 0)
 * - scale: Scale factor (default: 1)
 */
class ScharrItem : public InterpreterItem {
public:
    ScharrItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Noise Reduction
// ============================================================================

/**
 * @brief Fast Non-Local Means Denoising
 * 
 * Parameters:
 * - h: Filter strength (default: 10)
 * - template_window: Size of template patch (default: 7)
 * - search_window: Size of search area (default: 21)
 */
class FastNlMeansItem : public InterpreterItem {
public:
    FastNlMeansItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fast Non-Local Means Denoising for colored images
 */
class FastNlMeansColoredItem : public InterpreterItem {
public:
    FastNlMeansColoredItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_FILTER_ITEMS_H

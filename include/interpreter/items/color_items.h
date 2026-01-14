#ifndef VISIONPIPE_COLOR_ITEMS_H
#define VISIONPIPE_COLOR_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerColorItems(ItemRegistry& registry);

// ============================================================================
// Color Space Conversion
// ============================================================================

/**
 * @brief Converts between color spaces
 * 
 * Parameters:
 * - conversion: Conversion code string (e.g., "BGR2GRAY", "BGR2HSV", "BGR2LAB")
 * - dst_cn: Number of channels in destination (default: 0 = auto)
 * 
 * Supported conversions:
 * - BGR2GRAY, GRAY2BGR
 * - BGR2RGB, RGB2BGR
 * - BGR2HSV, HSV2BGR, BGR2HSV_FULL
 * - BGR2HLS, HLS2BGR
 * - BGR2LAB, LAB2BGR
 * - BGR2LUV, LUV2BGR
 * - BGR2YCrCb, YCrCb2BGR
 * - BGR2XYZ, XYZ2BGR
 * - BGR2YUV, YUV2BGR
 * - BGRA2GRAY, GRAY2BGRA
 * - And many more...
 */
class ColorConvertItem : public InterpreterItem {
public:
    ColorConvertItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Extracts a single channel from multi-channel image
 * 
 * Parameters:
 * - channel: Channel index (0, 1, 2, ...)
 */
class ExtractChannelItem : public InterpreterItem {
public:
    ExtractChannelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Splits image into separate channels
 * 
 * Returns array of single-channel images
 */
class SplitChannelsItem : public InterpreterItem {
public:
    SplitChannelsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Merges multiple single-channel images into multi-channel
 * 
 * Parameters:
 * - channels: Array of single-channel images or cache IDs
 */
class MergeChannelsItem : public InterpreterItem {
public:
    MergeChannelsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Mixes channels from input images
 * 
 * Parameters:
 * - from_to: Array of index pairs [src_idx, dst_idx, ...]
 */
class MixChannelsItem : public InterpreterItem {
public:
    MixChannelsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Histogram Operations
// ============================================================================

/**
 * @brief Equalizes the histogram of a grayscale image
 */
class EqualizeHistItem : public InterpreterItem {
public:
    EqualizeHistItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies CLAHE (Contrast Limited Adaptive Histogram Equalization)
 * 
 * Parameters:
 * - clip_limit: Contrast limiting threshold (default: 40.0)
 * - tile_grid_size: Size of grid for histogram equalization (default: 8)
 */
class ClaheItem : public InterpreterItem {
public:
    ClaheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates histogram
 * 
 * Parameters:
 * - channels: Channel indices to calculate (default: [0])
 * - hist_size: Number of bins (default: 256)
 * - ranges: Value ranges (default: [0, 256])
 */
class CalcHistItem : public InterpreterItem {
public:
    CalcHistItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Compares two histograms
 * 
 * Parameters:
 * - hist1, hist2: Histograms to compare
 * - method: Comparison method ("correlation", "chi_square", "intersection", "bhattacharyya")
 */
class CompareHistItem : public InterpreterItem {
public:
    CompareHistItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Back-projects histogram
 */
class CalcBackProjectItem : public InterpreterItem {
public:
    CalcBackProjectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Color Adjustments
// ============================================================================

/**
 * @brief Adjusts brightness and contrast
 * 
 * Parameters:
 * - alpha: Contrast (1.0-3.0, default: 1.0)
 * - beta: Brightness (-100 to 100, default: 0)
 */
class BrightnessContrastItem : public InterpreterItem {
public:
    BrightnessContrastItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Adjusts gamma correction
 * 
 * Parameters:
 * - gamma: Gamma value (default: 1.0, <1 = brighter, >1 = darker)
 */
class GammaItem : public InterpreterItem {
public:
    GammaItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies color map / LUT
 * 
 * Parameters:
 * - colormap: Colormap name ("jet", "hot", "cool", "spring", "summer", 
 *             "autumn", "winter", "bone", "ocean", "rainbow", "parula", etc.)
 */
class ApplyColormapItem : public InterpreterItem {
public:
    ApplyColormapItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies custom LUT (Look-Up Table)
 * 
 * Parameters:
 * - lut: 256-element lookup table
 */
class LutItem : public InterpreterItem {
public:
    LutItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Color Detection / Masking
// ============================================================================

/**
 * @brief Creates mask for color range in HSV/other color space
 * 
 * Parameters:
 * - lower: Lower bound [H, S, V] or [B, G, R]
 * - upper: Upper bound [H, S, V] or [B, G, R]
 */
class InRangeItem : public InterpreterItem {
public:
    InRangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Converts image to grayscale using custom weights
 * 
 * Parameters:
 * - weights: [B, G, R] weights (default: [0.114, 0.587, 0.299])
 */
class WeightedGrayItem : public InterpreterItem {
public:
    WeightedGrayItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_COLOR_ITEMS_H

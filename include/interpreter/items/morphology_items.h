#ifndef VISIONPIPE_MORPHOLOGY_ITEMS_H
#define VISIONPIPE_MORPHOLOGY_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerMorphologyItems(ItemRegistry& registry);

// ============================================================================
// Thresholding
// ============================================================================

/**
 * @brief Applies fixed-level threshold
 * 
 * Parameters:
 * - thresh: Threshold value
 * - maxval: Maximum value for THRESH_BINARY (default: 255)
 * - type: "binary", "binary_inv", "trunc", "tozero", "tozero_inv", "otsu", "triangle"
 */
class ThresholdItem : public InterpreterItem {
public:
    ThresholdItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies adaptive threshold
 * 
 * Parameters:
 * - maxval: Maximum value (default: 255)
 * - method: "mean" or "gaussian" (default: "gaussian")
 * - type: "binary" or "binary_inv" (default: "binary")
 * - block_size: Size of pixel neighborhood (odd number, default: 11)
 * - C: Constant subtracted from mean (default: 2)
 */
class AdaptiveThresholdItem : public InterpreterItem {
public:
    AdaptiveThresholdItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates Otsu's threshold value
 * 
 * Returns optimal threshold value for bimodal images
 */
class OtsuThresholdItem : public InterpreterItem {
public:
    OtsuThresholdItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Basic Morphological Operations
// ============================================================================

/**
 * @brief Applies morphological erosion
 * 
 * Parameters:
 * - kernel_size: Structuring element size (default: 3)
 * - kernel_shape: "rect", "cross", "ellipse" (default: "rect")
 * - iterations: Number of times to apply (default: 1)
 */
class ErodeItem : public InterpreterItem {
public:
    ErodeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies morphological dilation
 * 
 * Parameters:
 * - kernel_size: Structuring element size (default: 3)
 * - kernel_shape: "rect", "cross", "ellipse" (default: "rect")
 * - iterations: Number of times to apply (default: 1)
 */
class DilateItem : public InterpreterItem {
public:
    DilateItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies advanced morphological operation
 * 
 * Parameters:
 * - op: Operation type:
 *       "open" - erosion followed by dilation (removes small objects)
 *       "close" - dilation followed by erosion (fills small holes)
 *       "gradient" - difference between dilation and erosion
 *       "tophat" - difference between input and opening
 *       "blackhat" - difference between closing and input
 *       "hitmiss" - hit-or-miss transform
 * - kernel_size: Structuring element size
 * - kernel_shape: Shape of structuring element
 * - iterations: Number of times to apply
 */
class MorphologyExItem : public InterpreterItem {
public:
    MorphologyExItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates structuring element
 * 
 * Parameters:
 * - shape: "rect", "cross", "ellipse"
 * - ksize: Kernel size
 * - anchor: Anchor point (default: center)
 */
class GetStructuringElementItem : public InterpreterItem {
public:
    GetStructuringElementItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Advanced Morphological Operations
// ============================================================================

/**
 * @brief Morphological skeleton
 * 
 * Extracts skeleton/medial axis of binary shape
 */
class SkeletonizeItem : public InterpreterItem {
public:
    SkeletonizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Morphological thinning
 * 
 * Parameters:
 * - type: "zhang_suen" or "guo_hall" (default: "zhang_suen")
 */
class ThinningItem : public InterpreterItem {
public:
    ThinningItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fills holes in binary image
 */
class FillHolesItem : public InterpreterItem {
public:
    FillHolesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Removes small objects from binary image
 * 
 * Parameters:
 * - min_size: Minimum object size in pixels
 * - connectivity: 4 or 8 connectivity (default: 8)
 */
class RemoveSmallObjectsItem : public InterpreterItem {
public:
    RemoveSmallObjectsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Connected Components
// ============================================================================

/**
 * @brief Labels connected components
 * 
 * Parameters:
 * - connectivity: 4 or 8 (default: 8)
 * - ltype: Label type (default: CV_32S)
 */
class ConnectedComponentsItem : public InterpreterItem {
public:
    ConnectedComponentsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Labels connected components with statistics
 * 
 * Returns labels and stats (area, bounding box, centroid)
 */
class ConnectedComponentsWithStatsItem : public InterpreterItem {
public:
    ConnectedComponentsWithStatsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Distance Transform
// ============================================================================

/**
 * @brief Calculates distance to nearest zero pixel
 * 
 * Parameters:
 * - distance_type: "l1", "l2", "c", "l12", "fair", "welsch", "huber"
 * - mask_size: 3, 5, or 0 for precise (default: 3)
 * - label_type: "ccomp" or "pixel" (optional)
 */
class DistanceTransformItem : public InterpreterItem {
public:
    DistanceTransformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_MORPHOLOGY_ITEMS_H

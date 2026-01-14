#ifndef VISIONPIPE_TRANSFORM_ITEMS_H
#define VISIONPIPE_TRANSFORM_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerTransformItems(ItemRegistry& registry);

// ============================================================================
// Geometric Transforms
// ============================================================================

/**
 * @brief Resizes image
 * 
 * Parameters:
 * - width: Target width (0 = auto from height)
 * - height: Target height (0 = auto from width)
 * - fx: Scale factor X (used if width=0)
 * - fy: Scale factor Y (used if height=0)
 * - interpolation: "nearest", "linear", "cubic", "area", "lanczos4" (default: "linear")
 */
class ResizeItem : public InterpreterItem {
public:
    ResizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Rotates image by arbitrary angle
 * 
 * Parameters:
 * - angle: Rotation angle in degrees (positive = counter-clockwise)
 * - center_x, center_y: Rotation center (default: image center)
 * - scale: Scaling factor (default: 1.0)
 * - border_mode: "constant", "replicate", "reflect", "wrap" (default: "constant")
 * - border_value: Border fill value for "constant" mode
 */
class RotateItem : public InterpreterItem {
public:
    RotateItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Rotates image by 90-degree increments (fast)
 * 
 * Parameters:
 * - code: "90_cw", "90_ccw", "180"
 */
class Rotate90Item : public InterpreterItem {
public:
    Rotate90Item();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Flips image
 * 
 * Parameters:
 * - mode: "horizontal", "vertical", "both" (or flip_code: 0, 1, -1)
 */
class FlipItem : public InterpreterItem {
public:
    FlipItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Transposes image (rows <-> columns)
 */
class TransposeItem : public InterpreterItem {
public:
    TransposeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Region of Interest
// ============================================================================

/**
 * @brief Crops a rectangular region from image
 * 
 * Parameters:
 * - x, y: Top-left corner
 * - width, height: Crop dimensions
 */
class CropItem : public InterpreterItem {
public:
    CropItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Adds padding/border around image
 * 
 * Parameters:
 * - top, bottom, left, right: Padding sizes
 * - border_type: "constant", "replicate", "reflect", "reflect101", "wrap"
 * - value: Border value for "constant" type (default: 0)
 */
class CopyMakeBorderItem : public InterpreterItem {
public:
    CopyMakeBorderItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Affine & Perspective Transforms
// ============================================================================

/**
 * @brief Applies affine transformation
 * 
 * Parameters:
 * - matrix: 2x3 transformation matrix
 * - width, height: Output size (default: same as input)
 * - interpolation: Interpolation method
 * - border_mode: Border handling mode
 */
class WarpAffineItem : public InterpreterItem {
public:
    WarpAffineItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies perspective transformation
 * 
 * Parameters:
 * - matrix: 3x3 transformation matrix
 * - width, height: Output size
 * - interpolation: Interpolation method
 */
class WarpPerspectiveItem : public InterpreterItem {
public:
    WarpPerspectiveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Computes affine transform from 3 point pairs
 * 
 * Parameters:
 * - src_points: Array of 3 source points [[x1,y1], [x2,y2], [x3,y3]]
 * - dst_points: Array of 3 destination points
 */
class GetAffineTransformItem : public InterpreterItem {
public:
    GetAffineTransformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Computes perspective transform from 4 point pairs
 * 
 * Parameters:
 * - src_points: Array of 4 source points
 * - dst_points: Array of 4 destination points
 */
class GetPerspectiveTransformItem : public InterpreterItem {
public:
    GetPerspectiveTransformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Computes 2D rotation matrix
 * 
 * Parameters:
 * - center: [x, y] rotation center
 * - angle: Rotation angle in degrees
 * - scale: Isotropic scale factor
 */
class GetRotationMatrix2DItem : public InterpreterItem {
public:
    GetRotationMatrix2DItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Inverts affine transformation
 */
class InvertAffineTransformItem : public InterpreterItem {
public:
    InvertAffineTransformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Remapping
// ============================================================================

/**
 * @brief Applies generic geometric transformation using maps
 * 
 * Parameters:
 * - map_x: X coordinate map
 * - map_y: Y coordinate map
 * - interpolation: Interpolation method
 */
class RemapItem : public InterpreterItem {
public:
    RemapItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Converts floating-point remap arrays to fixed-point
 */
class ConvertMapsItem : public InterpreterItem {
public:
    ConvertMapsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Image Pyramids
// ============================================================================

/**
 * @brief Downsamples image (Gaussian pyramid down)
 * 
 * Parameters:
 * - dst_size: Output size (default: half of input)
 * - border_type: Border handling
 */
class PyrDownItem : public InterpreterItem {
public:
    PyrDownItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Upsamples image (Gaussian pyramid up)
 * 
 * Parameters:
 * - dst_size: Output size (default: double of input)
 */
class PyrUpItem : public InterpreterItem {
public:
    PyrUpItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Builds Gaussian pyramid
 * 
 * Parameters:
 * - max_level: Maximum pyramid level
 */
class BuildPyramidItem : public InterpreterItem {
public:
    BuildPyramidItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Stacking / Concatenation
// ============================================================================

/**
 * @brief Horizontally stacks images (side by side)
 * 
 * Parameters:
 * - images: Array of images or cache IDs
 */
class HStackItem : public InterpreterItem {
public:
    HStackItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Vertically stacks images (top to bottom)
 */
class VStackItem : public InterpreterItem {
public:
    VStackItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates a grid/mosaic of images
 * 
 * Parameters:
 * - images: Array of images
 * - cols: Number of columns
 * - border_size: Border between images (default: 0)
 */
class MosaicItem : public InterpreterItem {
public:
    MosaicItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_TRANSFORM_ITEMS_H

#ifndef VISIONPIPE_EDGE_ITEMS_H
#define VISIONPIPE_EDGE_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerEdgeItems(ItemRegistry& registry);

// ============================================================================
// Edge Detection
// ============================================================================

/**
 * @brief Applies Canny edge detector
 * 
 * Parameters:
 * - threshold1: First threshold for hysteresis (default: 100)
 * - threshold2: Second threshold for hysteresis (default: 200)
 * - aperture_size: Sobel aperture size (3, 5, or 7; default: 3)
 * - l2_gradient: Use L2 norm for gradient magnitude (default: false)
 */
class CannyItem : public InterpreterItem {
public:
    CannyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Auto Canny with automatic threshold calculation
 * 
 * Parameters:
 * - sigma: Standard deviation for threshold calculation (default: 0.33)
 */
class AutoCannyItem : public InterpreterItem {
public:
    AutoCannyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Contour Detection
// ============================================================================

/**
 * @brief Finds contours in binary image
 * 
 * Parameters:
 * - mode: Retrieval mode:
 *         "external" - only extreme outer contours
 *         "list" - all contours, no hierarchy
 *         "ccomp" - two-level hierarchy
 *         "tree" - full hierarchy
 *         "floodfill" - uses floodfill algorithm
 * - method: Approximation method:
 *           "none" - all points
 *           "simple" - compress horizontal/vertical/diagonal
 *           "tc89_l1" - Teh-Chin L1
 *           "tc89_kcos" - Teh-Chin KCOS
 * - offset_x, offset_y: Optional offset for contour points
 */
class FindContoursItem : public InterpreterItem {
public:
    FindContoursItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws contours on image
 * 
 * Parameters:
 * - contours: Array of contours
 * - contour_idx: Index of contour to draw (-1 for all)
 * - color: Drawing color [B, G, R]
 * - thickness: Line thickness (-1 for filled)
 * - line_type: "line_4", "line_8", "line_aa"
 * - hierarchy: Optional hierarchy for nested drawing
 * - max_level: Maximum nesting level to draw
 */
class DrawContoursItem : public InterpreterItem {
public:
    DrawContoursItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Approximates contour with polygon
 * 
 * Parameters:
 * - epsilon: Approximation accuracy (default: 0.02 * arc_length)
 * - closed: Whether contour is closed (default: true)
 */
class ApproxPolyDPItem : public InterpreterItem {
public:
    ApproxPolyDPItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates contour perimeter (arc length)
 * 
 * Parameters:
 * - closed: Whether contour is closed (default: true)
 */
class ArcLengthItem : public InterpreterItem {
public:
    ArcLengthItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates contour area
 * 
 * Parameters:
 * - oriented: Return signed area (default: false)
 */
class ContourAreaItem : public InterpreterItem {
public:
    ContourAreaItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Bounding Shapes
// ============================================================================

/**
 * @brief Calculates bounding rectangle
 * 
 * Returns [x, y, width, height]
 */
class BoundingRectItem : public InterpreterItem {
public:
    BoundingRectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates minimum area rotated rectangle
 * 
 * Returns [center_x, center_y, width, height, angle]
 */
class MinAreaRectItem : public InterpreterItem {
public:
    MinAreaRectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates minimum enclosing circle
 * 
 * Returns [center_x, center_y, radius]
 */
class MinEnclosingCircleItem : public InterpreterItem {
public:
    MinEnclosingCircleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates minimum enclosing triangle
 */
class MinEnclosingTriangleItem : public InterpreterItem {
public:
    MinEnclosingTriangleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fits ellipse to contour
 * 
 * Returns [center_x, center_y, axis1, axis2, angle]
 */
class FitEllipseItem : public InterpreterItem {
public:
    FitEllipseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fits line to points
 * 
 * Parameters:
 * - dist_type: "l2", "l1", "l12", "fair", "welsch", "huber"
 */
class FitLineItem : public InterpreterItem {
public:
    FitLineItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates convex hull
 * 
 * Parameters:
 * - clockwise: Orientation flag (default: false)
 * - return_points: Return hull points vs indices (default: true)
 */
class ConvexHullItem : public InterpreterItem {
public:
    ConvexHullItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Finds convexity defects
 */
class ConvexityDefectsItem : public InterpreterItem {
public:
    ConvexityDefectsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Shape Analysis
// ============================================================================

/**
 * @brief Calculates image moments
 * 
 * Parameters:
 * - binary: Treat image as binary (default: false)
 */
class MomentsItem : public InterpreterItem {
public:
    MomentsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates Hu invariant moments
 */
class HuMomentsItem : public InterpreterItem {
public:
    HuMomentsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Compares two shapes using Hu moments
 * 
 * Parameters:
 * - method: "i1", "i2", "i3" (default: "i1")
 */
class MatchShapesItem : public InterpreterItem {
public:
    MatchShapesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Tests if point is inside contour
 * 
 * Parameters:
 * - point: [x, y] point to test
 * - measure_dist: Return signed distance (default: false)
 */
class PointPolygonTestItem : public InterpreterItem {
public:
    PointPolygonTestItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Tests if contour is convex
 */
class IsContourConvexItem : public InterpreterItem {
public:
    IsContourConvexItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Line Detection
// ============================================================================

/**
 * @brief Detects lines using Hough transform
 * 
 * Parameters:
 * - rho: Distance resolution (default: 1)
 * - theta: Angle resolution in radians (default: PI/180)
 * - threshold: Accumulator threshold (default: 100)
 */
class HoughLinesItem : public InterpreterItem {
public:
    HoughLinesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Detects line segments using probabilistic Hough transform
 * 
 * Parameters:
 * - rho: Distance resolution (default: 1)
 * - theta: Angle resolution (default: PI/180)
 * - threshold: Accumulator threshold (default: 50)
 * - min_line_length: Minimum line length (default: 50)
 * - max_line_gap: Maximum gap between line segments (default: 10)
 */
class HoughLinesPItem : public InterpreterItem {
public:
    HoughLinesPItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Detects circles using Hough transform
 * 
 * Parameters:
 * - method: "gradient" or "alt" (default: "gradient")
 * - dp: Inverse ratio of accumulator resolution (default: 1)
 * - min_dist: Minimum distance between circle centers
 * - param1: Higher Canny threshold (default: 100)
 * - param2: Accumulator threshold (default: 30)
 * - min_radius: Minimum circle radius (default: 0)
 * - max_radius: Maximum circle radius (default: 0 = unlimited)
 */
class HoughCirclesItem : public InterpreterItem {
public:
    HoughCirclesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_EDGE_ITEMS_H

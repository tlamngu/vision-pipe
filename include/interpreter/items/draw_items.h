#ifndef VISIONPIPE_DRAW_ITEMS_H
#define VISIONPIPE_DRAW_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerDrawItems(ItemRegistry& registry);

// ============================================================================
// Basic Shapes
// ============================================================================

/**
 * @brief Draws a line
 * 
 * Parameters:
 * - pt1: Start point [x, y]
 * - pt2: End point [x, y]
 * - color: Color [B, G, R] or [B, G, R, A]
 * - thickness: Line thickness (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Number of fractional bits (default: 0)
 */
class LineItem : public InterpreterItem {
public:
    LineItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws an arrowed line
 * 
 * Parameters:
 * - pt1: Start point [x, y]
 * - pt2: End point [x, y] (arrow tip)
 * - color: Color [B, G, R]
 * - thickness: Line thickness (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Fractional bits (default: 0)
 * - tip_length: Arrow tip length relative to line length (default: 0.1)
 */
class ArrowedLineItem : public InterpreterItem {
public:
    ArrowedLineItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws a rectangle
 * 
 * Parameters:
 * - pt1: Top-left corner [x, y] OR rect: [x, y, width, height]
 * - pt2: Bottom-right corner [x, y] (not needed if rect used)
 * - color: Color [B, G, R]
 * - thickness: Line thickness, -1 for filled (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Fractional bits (default: 0)
 */
class RectangleItem : public InterpreterItem {
public:
    RectangleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws a circle
 * 
 * Parameters:
 * - center: Center point [x, y]
 * - radius: Circle radius
 * - color: Color [B, G, R]
 * - thickness: Line thickness, -1 for filled (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Fractional bits (default: 0)
 */
class CircleItem : public InterpreterItem {
public:
    CircleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws an ellipse
 * 
 * Parameters:
 * - center: Center point [x, y]
 * - axes: Half of ellipse size [width/2, height/2]
 * - angle: Rotation angle in degrees
 * - start_angle: Start angle (default: 0)
 * - end_angle: End angle (default: 360)
 * - color: Color [B, G, R]
 * - thickness: Line thickness, -1 for filled (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Fractional bits (default: 0)
 */
class EllipseItem : public InterpreterItem {
public:
    EllipseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws polygon or polylines
 * 
 * Parameters:
 * - pts: Array of points [[x1,y1], [x2,y2], ...]
 * - is_closed: Whether to close the polygon (default: true)
 * - color: Color [B, G, R]
 * - thickness: Line thickness (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Fractional bits (default: 0)
 */
class PolylinesItem : public InterpreterItem {
public:
    PolylinesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fills a polygon
 * 
 * Parameters:
 * - pts: Array of points [[x1,y1], [x2,y2], ...]
 * - color: Color [B, G, R]
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Fractional bits (default: 0)
 */
class FillPolyItem : public InterpreterItem {
public:
    FillPolyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Fills a convex polygon (faster than fillPoly)
 * 
 * Parameters:
 * - pts: Array of points
 * - color: Color [B, G, R]
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - shift: Fractional bits (default: 0)
 */
class FillConvexPolyItem : public InterpreterItem {
public:
    FillConvexPolyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Text
// ============================================================================

/**
 * @brief Draws text on image
 * 
 * Parameters:
 * - text: Text string to draw
 * - org: Bottom-left corner [x, y]
 * - font_face: Font type: "simplex", "plain", "duplex", "complex", 
 *              "triplex", "complex_small", "script_simplex", "script_complex",
 *              "italic" (can combine with italic)
 * - font_scale: Font scale factor (default: 1.0)
 * - color: Text color [B, G, R]
 * - thickness: Line thickness (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 * - bottom_left_origin: Origin at bottom-left (default: false)
 */
class PutTextItem : public InterpreterItem {
public:
    PutTextItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets size of text bounding box
 * 
 * Parameters:
 * - text: Text string
 * - font_face: Font type (same as putText)
 * - font_scale: Font scale factor
 * - thickness: Line thickness
 * 
 * Returns: [width, height, baseline]
 */
class GetTextSizeItem : public InterpreterItem {
public:
    GetTextSizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets list of available font names
 */
class GetFontNamesItem : public InterpreterItem {
public:
    GetFontNamesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Markers
// ============================================================================

/**
 * @brief Draws a marker at a point
 * 
 * Parameters:
 * - position: Marker position [x, y]
 * - color: Marker color [B, G, R]
 * - marker_type: "cross", "tilted_cross", "star", "diamond", "square", 
 *                "triangle_up", "triangle_down" (default: "cross")
 * - marker_size: Marker size (default: 20)
 * - thickness: Line thickness (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 */
class DrawMarkerItem : public InterpreterItem {
public:
    DrawMarkerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Composite Drawing
// ============================================================================

/**
 * @brief Draws a labeled rectangle (bounding box with text)
 * 
 * Parameters:
 * - rect: [x, y, width, height]
 * - label: Label text
 * - color: Box and label color [B, G, R]
 * - thickness: Box thickness (default: 2)
 * - font_scale: Label font scale (default: 0.5)
 * - label_bg: Draw label background (default: true)
 */
class DrawLabeledRectItem : public InterpreterItem {
public:
    DrawLabeledRectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws grid overlay on image
 * 
 * Parameters:
 * - rows: Number of rows (default: 3)
 * - cols: Number of columns (default: 3)
 * - color: Grid color [B, G, R] (default: [128, 128, 128])
 * - thickness: Line thickness (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 */
class DrawGridItem : public InterpreterItem {
public:
    DrawGridItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws crosshair at center or specified point
 * 
 * Parameters:
 * - center: Center point [x, y] (default: image center)
 * - size: Crosshair size (default: 20)
 * - color: Color [B, G, R] (default: [0, 255, 0])
 * - thickness: Line thickness (default: 1)
 * - gap: Center gap (default: 5)
 */
class DrawCrosshairItem : public InterpreterItem {
public:
    DrawCrosshairItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws a rotated rectangle
 * 
 * Parameters:
 * - center: Center point [x, y]
 * - size: [width, height]
 * - angle: Rotation angle in degrees
 * - color: Color [B, G, R]
 * - thickness: Line thickness, -1 for filled (default: 1)
 * - line_type: "line_4", "line_8", "line_aa" (default: "line_8")
 */
class DrawRotatedRectItem : public InterpreterItem {
public:
    DrawRotatedRectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws axis arrows for coordinate visualization
 * 
 * Parameters:
 * - origin: Origin point [x, y]
 * - length: Axis length (default: 100)
 * - thickness: Line thickness (default: 2)
 */
class DrawAxesItem : public InterpreterItem {
public:
    DrawAxesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws 3D axis (requires camera matrix and pose)
 * 
 * Parameters:
 * - camera_matrix: Intrinsic matrix
 * - dist_coeffs: Distortion coefficients
 * - rvec: Rotation vector
 * - tvec: Translation vector
 * - length: Axis length (default: 0.1)
 * - thickness: Line thickness (default: 3)
 */
class DrawFrameAxesItem : public InterpreterItem {
public:
    DrawFrameAxesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Overlay & Visualization
// ============================================================================

/**
 * @brief Overlays one image onto another with transparency
 * 
 * Parameters:
 * - overlay: Image to overlay
 * - position: Top-left corner [x, y] (default: [0, 0])
 * - alpha: Transparency (0-1; default: 1.0)
 */
class OverlayImageItem : public InterpreterItem {
public:
    OverlayImageItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Adds semi-transparent color overlay to region
 * 
 * Parameters:
 * - rect: Region [x, y, width, height] (default: full image)
 * - color: Overlay color [B, G, R]
 * - alpha: Transparency (0-1; default: 0.5)
 */
class ColorOverlayItem : public InterpreterItem {
public:
    ColorOverlayItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws histogram on image
 * 
 * Parameters:
 * - hist: Histogram data
 * - rect: Drawing region [x, y, width, height]
 * - color: Bar color [B, G, R] (default: [255, 255, 255])
 * - filled: Fill bars (default: true)
 * - normalized: Auto-normalize to fit (default: true)
 */
class DrawHistogramItem : public InterpreterItem {
public:
    DrawHistogramItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Adds border around image
 * 
 * Parameters:
 * - border_size: [top, bottom, left, right] or single value
 * - color: Border color [B, G, R]
 * - border_type: "constant", "replicate", "reflect", "wrap" (default: "constant")
 */
class AddBorderItem : public InterpreterItem {
public:
    AddBorderItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_DRAW_ITEMS_H

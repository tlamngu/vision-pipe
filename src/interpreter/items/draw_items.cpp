#include "interpreter/items/draw_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>

namespace visionpipe {

void registerDrawItems(ItemRegistry& registry) {
    registry.add<LineItem>();
    registry.add<ArrowedLineItem>();
    registry.add<RectangleItem>();
    registry.add<CircleItem>();
    registry.add<EllipseItem>();
    registry.add<PolylinesItem>();
    registry.add<FillPolyItem>();
    registry.add<FillConvexPolyItem>();
    registry.add<PutTextItem>();
    registry.add<GetTextSizeItem>();
    registry.add<DrawMarkerItem>();
    registry.add<DrawLabeledRectItem>();
    registry.add<DrawGridItem>();
    registry.add<DrawCrosshairItem>();
    registry.add<DrawRotatedRectItem>();
    registry.add<OverlayImageItem>();
    registry.add<ColorOverlayItem>();
    registry.add<DrawHistogramItem>();
    registry.add<AddBorderItem>();
}

static cv::Scalar parseColor(const std::string& colorStr) {
    if (colorStr == "red") return cv::Scalar(0, 0, 255);
    if (colorStr == "green") return cv::Scalar(0, 255, 0);
    if (colorStr == "blue") return cv::Scalar(255, 0, 0);
    if (colorStr == "white") return cv::Scalar(255, 255, 255);
    if (colorStr == "black") return cv::Scalar(0, 0, 0);
    if (colorStr == "yellow") return cv::Scalar(0, 255, 255);
    if (colorStr == "cyan") return cv::Scalar(255, 255, 0);
    if (colorStr == "magenta") return cv::Scalar(255, 0, 255);
    if (colorStr == "orange") return cv::Scalar(0, 165, 255);
    if (colorStr == "purple") return cv::Scalar(128, 0, 128);
    if (colorStr == "pink") return cv::Scalar(203, 192, 255);
    if (colorStr == "gray" || colorStr == "grey") return cv::Scalar(128, 128, 128);
    return cv::Scalar(0, 255, 0);
}

// ============================================================================
// LineItem
// ============================================================================

LineItem::LineItem() {
    _functionName = "line";
    _description = "Draws a line segment";
    _category = "draw";
    _params = {
        ParamDef::required("x1", BaseType::INT, "Start X"),
        ParamDef::required("y1", BaseType::INT, "Start Y"),
        ParamDef::required("x2", BaseType::INT, "End X"),
        ParamDef::required("y2", BaseType::INT, "End Y"),
        ParamDef::optional("color", BaseType::STRING, "Line color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 1),
        ParamDef::optional("line_type", BaseType::STRING, "Type: 4, 8, aa", "8")
    };
    _example = "line(0, 0, 100, 100, \"red\", 2)";
    _returnType = "mat";
    _tags = {"draw", "line", "primitive"};
}

ExecutionResult LineItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x1 = static_cast<int>(args[0].asNumber());
    int y1 = static_cast<int>(args[1].asNumber());
    int x2 = static_cast<int>(args[2].asNumber());
    int y2 = static_cast<int>(args[3].asNumber());
    std::string colorStr = args.size() > 4 ? args[4].asString() : "green";
    int thickness = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 1;
    std::string lineTypeStr = args.size() > 6 ? args[6].asString() : "8";
    
    int lineType = cv::LINE_8;
    if (lineTypeStr == "4") lineType = cv::LINE_4;
    else if (lineTypeStr == "aa") lineType = cv::LINE_AA;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::line(result, cv::Point(x1, y1), cv::Point(x2, y2), parseColor(colorStr), thickness, lineType);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ArrowedLineItem
// ============================================================================

ArrowedLineItem::ArrowedLineItem() {
    _functionName = "arrowed_line";
    _description = "Draws an arrow";
    _category = "draw";
    _params = {
        ParamDef::required("x1", BaseType::INT, "Start X"),
        ParamDef::required("y1", BaseType::INT, "Start Y"),
        ParamDef::required("x2", BaseType::INT, "End X (arrow head)"),
        ParamDef::required("y2", BaseType::INT, "End Y (arrow head)"),
        ParamDef::optional("color", BaseType::STRING, "Arrow color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 1),
        ParamDef::optional("tip_length", BaseType::FLOAT, "Arrow tip length ratio", 0.1)
    };
    _example = "arrowed_line(0, 0, 100, 100, \"red\", 2, 0.1)";
    _returnType = "mat";
    _tags = {"draw", "arrow", "line"};
}

ExecutionResult ArrowedLineItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x1 = static_cast<int>(args[0].asNumber());
    int y1 = static_cast<int>(args[1].asNumber());
    int x2 = static_cast<int>(args[2].asNumber());
    int y2 = static_cast<int>(args[3].asNumber());
    std::string colorStr = args.size() > 4 ? args[4].asString() : "green";
    int thickness = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 1;
    double tipLength = args.size() > 6 ? args[6].asNumber() : 0.1;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::arrowedLine(result, cv::Point(x1, y1), cv::Point(x2, y2), parseColor(colorStr), thickness, cv::LINE_8, 0, tipLength);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// RectangleItem
// ============================================================================

RectangleItem::RectangleItem() {
    _functionName = "rectangle";
    _description = "Draws a rectangle";
    _category = "draw";
    _params = {
        ParamDef::required("x", BaseType::INT, "Top-left X"),
        ParamDef::required("y", BaseType::INT, "Top-left Y"),
        ParamDef::required("width", BaseType::INT, "Width"),
        ParamDef::required("height", BaseType::INT, "Height"),
        ParamDef::optional("color", BaseType::STRING, "Rectangle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness (-1 for filled)", 1)
    };
    _example = "rectangle(10, 10, 100, 50, \"red\", 2)";
    _returnType = "mat";
    _tags = {"draw", "rectangle", "primitive"};
}

ExecutionResult RectangleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int w = static_cast<int>(args[2].asNumber());
    int h = static_cast<int>(args[3].asNumber());
    std::string colorStr = args.size() > 4 ? args[4].asString() : "green";
    int thickness = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 1;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::rectangle(result, cv::Rect(x, y, w, h), parseColor(colorStr), thickness);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// CircleItem
// ============================================================================

CircleItem::CircleItem() {
    _functionName = "circle";
    _description = "Draws a circle";
    _category = "draw";
    _params = {
        ParamDef::required("x", BaseType::INT, "Center X"),
        ParamDef::required("y", BaseType::INT, "Center Y"),
        ParamDef::required("radius", BaseType::INT, "Radius"),
        ParamDef::optional("color", BaseType::STRING, "Circle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness (-1 for filled)", 1)
    };
    _example = "circle(100, 100, 50, \"blue\", -1)";
    _returnType = "mat";
    _tags = {"draw", "circle", "primitive"};
}

ExecutionResult CircleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int radius = static_cast<int>(args[2].asNumber());
    std::string colorStr = args.size() > 3 ? args[3].asString() : "green";
    int thickness = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 1;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::circle(result, cv::Point(x, y), radius, parseColor(colorStr), thickness, cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// EllipseItem
// ============================================================================

EllipseItem::EllipseItem() {
    _functionName = "ellipse";
    _description = "Draws an ellipse";
    _category = "draw";
    _params = {
        ParamDef::required("x", BaseType::INT, "Center X"),
        ParamDef::required("y", BaseType::INT, "Center Y"),
        ParamDef::required("axes_x", BaseType::INT, "Half-size X axis"),
        ParamDef::required("axes_y", BaseType::INT, "Half-size Y axis"),
        ParamDef::optional("angle", BaseType::FLOAT, "Rotation angle", 0.0),
        ParamDef::optional("start_angle", BaseType::FLOAT, "Start angle", 0.0),
        ParamDef::optional("end_angle", BaseType::FLOAT, "End angle", 360.0),
        ParamDef::optional("color", BaseType::STRING, "Ellipse color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness (-1 for filled)", 1)
    };
    _example = "ellipse(100, 100, 50, 30, 45, 0, 360, \"blue\", 2)";
    _returnType = "mat";
    _tags = {"draw", "ellipse", "primitive"};
}

ExecutionResult EllipseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int ax = static_cast<int>(args[2].asNumber());
    int ay = static_cast<int>(args[3].asNumber());
    double angle = args.size() > 4 ? args[4].asNumber() : 0.0;
    double startAngle = args.size() > 5 ? args[5].asNumber() : 0.0;
    double endAngle = args.size() > 6 ? args[6].asNumber() : 360.0;
    std::string colorStr = args.size() > 7 ? args[7].asString() : "green";
    int thickness = args.size() > 8 ? static_cast<int>(args[8].asNumber()) : 1;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::ellipse(result, cv::Point(x, y), cv::Size(ax, ay), angle, startAngle, endAngle, parseColor(colorStr), thickness, cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// PolylinesItem
// ============================================================================

PolylinesItem::PolylinesItem() {
    _functionName = "polylines";
    _description = "Draws polylines from cached points";
    _category = "draw";
    _params = {
        ParamDef::required("points_cache", BaseType::STRING, "Points cache ID"),
        ParamDef::optional("closed", BaseType::BOOL, "Close polyline", false),
        ParamDef::optional("color", BaseType::STRING, "Line color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 1)
    };
    _example = "polylines(\"points\", true, \"green\", 2)";
    _returnType = "mat";
    _tags = {"draw", "polyline", "primitive"};
}

ExecutionResult PolylinesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string pointsCache = args[0].asString();
    bool closed = args.size() > 1 ? args[1].asBool() : false;
    std::string colorStr = args.size() > 2 ? args[2].asString() : "green";
    int thickness = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 1;
    
    cv::Mat pointsOpt = ctx.cacheManager->get(pointsCache);
    if (pointsOpt.empty()) {
        return ExecutionResult::fail("Points not found in cache: " + pointsCache);
    }
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    std::vector<cv::Point> points;
    for (int i = 0; i < pointsOpt.rows; i++) {
        points.push_back(pointsOpt.at<cv::Point>(i));
    }
    
    std::vector<std::vector<cv::Point>> pts = {points};
    cv::polylines(result, pts, closed, parseColor(colorStr), thickness, cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FillPolyItem
// ============================================================================

FillPolyItem::FillPolyItem() {
    _functionName = "fill_poly";
    _description = "Fills polygon from cached points";
    _category = "draw";
    _params = {
        ParamDef::required("points_cache", BaseType::STRING, "Points cache ID"),
        ParamDef::optional("color", BaseType::STRING, "Fill color", "green")
    };
    _example = "fill_poly(\"points\", \"blue\")";
    _returnType = "mat";
    _tags = {"draw", "polygon", "fill"};
}

ExecutionResult FillPolyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string pointsCache = args[0].asString();
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    
    cv::Mat pointsOpt = ctx.cacheManager->get(pointsCache);
    if (pointsOpt.empty()) {
        return ExecutionResult::fail("Points not found in cache: " + pointsCache);
    }
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    std::vector<cv::Point> points;
    for (int i = 0; i < pointsOpt.rows; i++) {
        points.push_back(pointsOpt.at<cv::Point>(i));
    }
    
    std::vector<std::vector<cv::Point>> pts = {points};
    cv::fillPoly(result, pts, parseColor(colorStr), cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FillConvexPolyItem
// ============================================================================

FillConvexPolyItem::FillConvexPolyItem() {
    _functionName = "fill_convex_poly";
    _description = "Fills convex polygon from cached points";
    _category = "draw";
    _params = {
        ParamDef::required("points_cache", BaseType::STRING, "Points cache ID"),
        ParamDef::optional("color", BaseType::STRING, "Fill color", "green")
    };
    _example = "fill_convex_poly(\"points\", \"red\")";
    _returnType = "mat";
    _tags = {"draw", "polygon", "convex", "fill"};
}

ExecutionResult FillConvexPolyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string pointsCache = args[0].asString();
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    
    cv::Mat pointsOpt = ctx.cacheManager->get(pointsCache);
    if (pointsOpt.empty()) {
        return ExecutionResult::fail("Points not found in cache: " + pointsCache);
    }
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    std::vector<cv::Point> points;
    for (int i = 0; i < pointsOpt.rows; i++) {
        points.push_back(pointsOpt.at<cv::Point>(i));
    }
    
    cv::fillConvexPoly(result, points, parseColor(colorStr), cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// PutTextItem
// ============================================================================

PutTextItem::PutTextItem() {
    _functionName = "put_text";
    _description = "Draws text on image";
    _category = "draw";
    _params = {
        ParamDef::required("text", BaseType::STRING, "Text to draw"),
        ParamDef::required("x", BaseType::INT, "Bottom-left X"),
        ParamDef::required("y", BaseType::INT, "Bottom-left Y"),
        ParamDef::optional("font", BaseType::STRING, 
            "Font: simplex, plain, duplex, complex, triplex, complex_small, script_simplex, script_complex, italic", "simplex"),
        ParamDef::optional("scale", BaseType::FLOAT, "Font scale", 1.0),
        ParamDef::optional("color", BaseType::STRING, "Text color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Text thickness", 1)
    };
    _example = "put_text(\"Hello\", 10, 30, \"simplex\", 1.0, \"white\", 2)";
    _returnType = "mat";
    _tags = {"draw", "text", "label"};
}

ExecutionResult PutTextItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string text = args[0].asString();
    int x = static_cast<int>(args[1].asNumber());
    int y = static_cast<int>(args[2].asNumber());
    std::string fontStr = args.size() > 3 ? args[3].asString() : "simplex";
    double scale = args.size() > 4 ? args[4].asNumber() : 1.0;
    std::string colorStr = args.size() > 5 ? args[5].asString() : "green";
    int thickness = args.size() > 6 ? static_cast<int>(args[6].asNumber()) : 1;
    
    int font = cv::FONT_HERSHEY_SIMPLEX;
    if (fontStr == "plain") font = cv::FONT_HERSHEY_PLAIN;
    else if (fontStr == "duplex") font = cv::FONT_HERSHEY_DUPLEX;
    else if (fontStr == "complex") font = cv::FONT_HERSHEY_COMPLEX;
    else if (fontStr == "triplex") font = cv::FONT_HERSHEY_TRIPLEX;
    else if (fontStr == "complex_small") font = cv::FONT_HERSHEY_COMPLEX_SMALL;
    else if (fontStr == "script_simplex") font = cv::FONT_HERSHEY_SCRIPT_SIMPLEX;
    else if (fontStr == "script_complex") font = cv::FONT_HERSHEY_SCRIPT_COMPLEX;
    else if (fontStr == "italic") font = cv::FONT_ITALIC;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::putText(result, text, cv::Point(x, y), font, scale, parseColor(colorStr), thickness, cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// GetTextSizeItem
// ============================================================================

GetTextSizeItem::GetTextSizeItem() {
    _functionName = "get_text_size";
    _description = "Calculates text bounding box size";
    _category = "draw";
    _params = {
        ParamDef::required("text", BaseType::STRING, "Text string"),
        ParamDef::optional("font", BaseType::STRING, "Font face", "simplex"),
        ParamDef::optional("scale", BaseType::FLOAT, "Font scale", 1.0),
        ParamDef::optional("thickness", BaseType::INT, "Text thickness", 1),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache prefix for size", "text_size")
    };
    _example = "get_text_size(\"Hello\", \"simplex\", 1.0, 1, \"ts\")";
    _returnType = "mat";
    _tags = {"text", "size", "measure"};
}

ExecutionResult GetTextSizeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string text = args[0].asString();
    std::string fontStr = args.size() > 1 ? args[1].asString() : "simplex";
    double scale = args.size() > 2 ? args[2].asNumber() : 1.0;
    int thickness = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 1;
    std::string prefix = args.size() > 4 ? args[4].asString() : "text_size";
    
    int font = cv::FONT_HERSHEY_SIMPLEX;
    if (fontStr == "plain") font = cv::FONT_HERSHEY_PLAIN;
    else if (fontStr == "complex") font = cv::FONT_HERSHEY_COMPLEX;
    
    int baseline;
    cv::Size size = cv::getTextSize(text, font, scale, thickness, &baseline);
    
    ctx.cacheManager->set(prefix + "_width", cv::Mat(1, 1, CV_32S, cv::Scalar(size.width)));
    ctx.cacheManager->set(prefix + "_height", cv::Mat(1, 1, CV_32S, cv::Scalar(size.height)));
    ctx.cacheManager->set(prefix + "_baseline", cv::Mat(1, 1, CV_32S, cv::Scalar(baseline)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DrawMarkerItem
// ============================================================================

DrawMarkerItem::DrawMarkerItem() {
    _functionName = "draw_marker";
    _description = "Draws a marker at specified position";
    _category = "draw";
    _params = {
        ParamDef::required("x", BaseType::INT, "Marker X"),
        ParamDef::required("y", BaseType::INT, "Marker Y"),
        ParamDef::optional("type", BaseType::STRING, 
            "Type: cross, tilted_cross, star, diamond, square, triangle_up, triangle_down", "cross"),
        ParamDef::optional("color", BaseType::STRING, "Marker color", "green"),
        ParamDef::optional("size", BaseType::INT, "Marker size", 20),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 1)
    };
    _example = "draw_marker(100, 100, \"star\", \"red\", 30, 2)";
    _returnType = "mat";
    _tags = {"draw", "marker", "point"};
}

ExecutionResult DrawMarkerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    std::string typeStr = args.size() > 2 ? args[2].asString() : "cross";
    std::string colorStr = args.size() > 3 ? args[3].asString() : "green";
    int size = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 20;
    int thickness = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 1;
    
    int markerType = cv::MARKER_CROSS;
    if (typeStr == "tilted_cross") markerType = cv::MARKER_TILTED_CROSS;
    else if (typeStr == "star") markerType = cv::MARKER_STAR;
    else if (typeStr == "diamond") markerType = cv::MARKER_DIAMOND;
    else if (typeStr == "square") markerType = cv::MARKER_SQUARE;
    else if (typeStr == "triangle_up") markerType = cv::MARKER_TRIANGLE_UP;
    else if (typeStr == "triangle_down") markerType = cv::MARKER_TRIANGLE_DOWN;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::drawMarker(result, cv::Point(x, y), parseColor(colorStr), markerType, size, thickness, cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DrawLabeledRectItem
// ============================================================================

DrawLabeledRectItem::DrawLabeledRectItem() {
    _functionName = "draw_labeled_rect";
    _description = "Draws rectangle with text label";
    _category = "draw";
    _params = {
        ParamDef::required("x", BaseType::INT, "Top-left X"),
        ParamDef::required("y", BaseType::INT, "Top-left Y"),
        ParamDef::required("width", BaseType::INT, "Width"),
        ParamDef::required("height", BaseType::INT, "Height"),
        ParamDef::required("label", BaseType::STRING, "Label text"),
        ParamDef::optional("color", BaseType::STRING, "Rectangle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2),
        ParamDef::optional("font_scale", BaseType::FLOAT, "Font scale", 0.5)
    };
    _example = "draw_labeled_rect(10, 10, 100, 50, \"Object\", \"green\", 2, 0.5)";
    _returnType = "mat";
    _tags = {"draw", "rectangle", "label", "detection"};
}

ExecutionResult DrawLabeledRectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int w = static_cast<int>(args[2].asNumber());
    int h = static_cast<int>(args[3].asNumber());
    std::string label = args[4].asString();
    std::string colorStr = args.size() > 5 ? args[5].asString() : "green";
    int thickness = args.size() > 6 ? static_cast<int>(args[6].asNumber()) : 2;
    double fontScale = args.size() > 7 ? args[7].asNumber() : 0.5;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::Scalar color = parseColor(colorStr);
    cv::rectangle(result, cv::Rect(x, y, w, h), color, thickness);
    
    // Draw label background
    int baseline;
    cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fontScale, 1, &baseline);
    cv::rectangle(result, cv::Point(x, y - textSize.height - 5), cv::Point(x + textSize.width + 4, y), color, -1);
    
    // Draw label text
    cv::putText(result, label, cv::Point(x + 2, y - 3), cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DrawGridItem
// ============================================================================

DrawGridItem::DrawGridItem() {
    _functionName = "draw_grid";
    _description = "Draws grid lines on image";
    _category = "draw";
    _params = {
        ParamDef::optional("cell_width", BaseType::INT, "Grid cell width", 50),
        ParamDef::optional("cell_height", BaseType::INT, "Grid cell height", 50),
        ParamDef::optional("color", BaseType::STRING, "Grid color", "gray"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 1)
    };
    _example = "draw_grid(50, 50, \"gray\", 1)";
    _returnType = "mat";
    _tags = {"draw", "grid", "overlay"};
}

ExecutionResult DrawGridItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int cellW = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 50;
    int cellH = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 50;
    std::string colorStr = args.size() > 2 ? args[2].asString() : "gray";
    int thickness = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 1;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::Scalar color = parseColor(colorStr);
    
    for (int x = cellW; x < result.cols; x += cellW) {
        cv::line(result, cv::Point(x, 0), cv::Point(x, result.rows), color, thickness);
    }
    for (int y = cellH; y < result.rows; y += cellH) {
        cv::line(result, cv::Point(0, y), cv::Point(result.cols, y), color, thickness);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DrawCrosshairItem
// ============================================================================

DrawCrosshairItem::DrawCrosshairItem() {
    _functionName = "draw_crosshair";
    _description = "Draws crosshair at center or specified position";
    _category = "draw";
    _params = {
        ParamDef::optional("x", BaseType::INT, "Center X (-1 for image center)", -1),
        ParamDef::optional("y", BaseType::INT, "Center Y (-1 for image center)", -1),
        ParamDef::optional("size", BaseType::INT, "Crosshair size", 20),
        ParamDef::optional("color", BaseType::STRING, "Crosshair color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 1),
        ParamDef::optional("gap", BaseType::INT, "Center gap", 0)
    };
    _example = "draw_crosshair(-1, -1, 30, \"red\", 2, 5)";
    _returnType = "mat";
    _tags = {"draw", "crosshair", "center"};
}

ExecutionResult DrawCrosshairItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : -1;
    int y = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : -1;
    int size = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 20;
    std::string colorStr = args.size() > 3 ? args[3].asString() : "green";
    int thickness = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 1;
    int gap = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 0;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    if (x < 0) x = result.cols / 2;
    if (y < 0) y = result.rows / 2;
    
    cv::Scalar color = parseColor(colorStr);
    
    cv::line(result, cv::Point(x - size, y), cv::Point(x - gap, y), color, thickness, cv::LINE_AA);
    cv::line(result, cv::Point(x + gap, y), cv::Point(x + size, y), color, thickness, cv::LINE_AA);
    cv::line(result, cv::Point(x, y - size), cv::Point(x, y - gap), color, thickness, cv::LINE_AA);
    cv::line(result, cv::Point(x, y + gap), cv::Point(x, y + size), color, thickness, cv::LINE_AA);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DrawRotatedRectItem
// ============================================================================

DrawRotatedRectItem::DrawRotatedRectItem() {
    _functionName = "draw_rotated_rect";
    _description = "Draws a rotated rectangle";
    _category = "draw";
    _params = {
        ParamDef::required("x", BaseType::INT, "Center X"),
        ParamDef::required("y", BaseType::INT, "Center Y"),
        ParamDef::required("width", BaseType::INT, "Width"),
        ParamDef::required("height", BaseType::INT, "Height"),
        ParamDef::required("angle", BaseType::FLOAT, "Rotation angle in degrees"),
        ParamDef::optional("color", BaseType::STRING, "Rectangle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 1)
    };
    _example = "draw_rotated_rect(100, 100, 80, 40, 45, \"red\", 2)";
    _returnType = "mat";
    _tags = {"draw", "rectangle", "rotated"};
}

ExecutionResult DrawRotatedRectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int cx = static_cast<int>(args[0].asNumber());
    int cy = static_cast<int>(args[1].asNumber());
    int w = static_cast<int>(args[2].asNumber());
    int h = static_cast<int>(args[3].asNumber());
    float angle = static_cast<float>(args[4].asNumber());
    std::string colorStr = args.size() > 5 ? args[5].asString() : "green";
    int thickness = args.size() > 6 ? static_cast<int>(args[6].asNumber()) : 1;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::RotatedRect rRect(cv::Point2f(static_cast<float>(cx), static_cast<float>(cy)), 
                          cv::Size2f(static_cast<float>(w), static_cast<float>(h)), angle);
    cv::Point2f vertices[4];
    rRect.points(vertices);
    
    cv::Scalar color = parseColor(colorStr);
    for (int i = 0; i < 4; i++) {
        cv::line(result, vertices[i], vertices[(i+1)%4], color, thickness, cv::LINE_AA);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// OverlayImageItem
// ============================================================================

OverlayImageItem::OverlayImageItem() {
    _functionName = "overlay_image";
    _description = "Overlays cached image on current image";
    _category = "draw";
    _params = {
        ParamDef::required("overlay_cache", BaseType::STRING, "Overlay image cache ID"),
        ParamDef::required("x", BaseType::INT, "Top-left X position"),
        ParamDef::required("y", BaseType::INT, "Top-left Y position"),
        ParamDef::optional("alpha", BaseType::FLOAT, "Opacity (0-1)", 1.0)
    };
    _example = "overlay_image(\"logo\", 10, 10, 0.5)";
    _returnType = "mat";
    _tags = {"overlay", "image", "composite"};
}

ExecutionResult OverlayImageItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string overlayCache = args[0].asString();
    int x = static_cast<int>(args[1].asNumber());
    int y = static_cast<int>(args[2].asNumber());
    double alpha = args.size() > 3 ? args[3].asNumber() : 1.0;
    
    cv::Mat overlayOpt = ctx.cacheManager->get(overlayCache);
    if (overlayOpt.empty()) {
        return ExecutionResult::fail("Overlay image not found: " + overlayCache);
    }
    
    cv::Mat result = ctx.currentMat.clone();
    cv::Mat overlay = overlayOpt;
    
    // Ensure both images have same number of channels
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    if (overlay.channels() == 1) {
        cv::cvtColor(overlay, overlay, cv::COLOR_GRAY2BGR);
    }
    
    // Calculate ROI
    int w = std::min(overlay.cols, result.cols - x);
    int h = std::min(overlay.rows, result.rows - y);
    
    if (w <= 0 || h <= 0) return ExecutionResult::ok(result);
    
    cv::Mat roi = result(cv::Rect(x, y, w, h));
    cv::Mat overlayRoi = overlay(cv::Rect(0, 0, w, h));
    
    cv::addWeighted(overlayRoi, alpha, roi, 1.0 - alpha, 0, roi);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ColorOverlayItem
// ============================================================================

ColorOverlayItem::ColorOverlayItem() {
    _functionName = "color_overlay";
    _description = "Applies color overlay to image";
    _category = "draw";
    _params = {
        ParamDef::optional("color", BaseType::STRING, "Overlay color", "blue"),
        ParamDef::optional("alpha", BaseType::FLOAT, "Opacity (0-1)", 0.3)
    };
    _example = "color_overlay(\"blue\", 0.3)";
    _returnType = "mat";
    _tags = {"overlay", "color", "tint"};
}

ExecutionResult ColorOverlayItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string colorStr = args.size() > 0 ? args[0].asString() : "blue";
    double alpha = args.size() > 1 ? args[1].asNumber() : 0.3;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::Mat overlay(result.size(), result.type(), parseColor(colorStr));
    cv::addWeighted(overlay, alpha, result, 1.0 - alpha, 0, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DrawHistogramItem
// ============================================================================

DrawHistogramItem::DrawHistogramItem() {
    _functionName = "draw_histogram";
    _description = "Draws histogram visualization";
    _category = "draw";
    _params = {
        ParamDef::optional("width", BaseType::INT, "Histogram width", 256),
        ParamDef::optional("height", BaseType::INT, "Histogram height", 200),
        ParamDef::optional("color", BaseType::STRING, "Histogram color", "white"),
        ParamDef::optional("background", BaseType::STRING, "Background color", "black")
    };
    _example = "draw_histogram(256, 200, \"white\", \"black\")";
    _returnType = "mat";
    _tags = {"histogram", "draw", "visualization"};
}

ExecutionResult DrawHistogramItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int width = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 256;
    int height = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 200;
    std::string colorStr = args.size() > 2 ? args[2].asString() : "white";
    std::string bgStr = args.size() > 3 ? args[3].asString() : "black";
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    // Calculate histogram
    cv::Mat hist;
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::calcHist(&input, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
    
    // Normalize
    cv::normalize(hist, hist, 0, height, cv::NORM_MINMAX);
    
    // Draw
    cv::Mat histImage(height, width, CV_8UC3, parseColor(bgStr));
    int binW = cvRound(static_cast<double>(width) / histSize);
    
    cv::Scalar color = parseColor(colorStr);
    for (int i = 1; i < histSize; i++) {
        cv::line(histImage,
                 cv::Point(binW * (i - 1), height - cvRound(hist.at<float>(i - 1))),
                 cv::Point(binW * i, height - cvRound(hist.at<float>(i))),
                 color, 2, cv::LINE_AA);
    }
    
    return ExecutionResult::ok(histImage);
}

// ============================================================================
// AddBorderItem
// ============================================================================

AddBorderItem::AddBorderItem() {
    _functionName = "add_border";
    _description = "Adds border around image";
    _category = "draw";
    _params = {
        ParamDef::optional("top", BaseType::INT, "Top border", 10),
        ParamDef::optional("bottom", BaseType::INT, "Bottom border", 10),
        ParamDef::optional("left", BaseType::INT, "Left border", 10),
        ParamDef::optional("right", BaseType::INT, "Right border", 10),
        ParamDef::optional("type", BaseType::STRING, "Type: constant, replicate, reflect, wrap, reflect_101", "constant"),
        ParamDef::optional("color", BaseType::STRING, "Border color (for constant)", "black")
    };
    _example = "add_border(10, 10, 10, 10, \"constant\", \"white\")";
    _returnType = "mat";
    _tags = {"border", "padding", "frame"};
}

ExecutionResult AddBorderItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int top = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 10;
    int bottom = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 10;
    int left = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 10;
    int right = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 10;
    std::string typeStr = args.size() > 4 ? args[4].asString() : "constant";
    std::string colorStr = args.size() > 5 ? args[5].asString() : "black";
    
    int borderType = cv::BORDER_CONSTANT;
    if (typeStr == "replicate") borderType = cv::BORDER_REPLICATE;
    else if (typeStr == "reflect") borderType = cv::BORDER_REFLECT;
    else if (typeStr == "wrap") borderType = cv::BORDER_WRAP;
    else if (typeStr == "reflect_101") borderType = cv::BORDER_REFLECT_101;
    
    cv::Mat result;
    cv::copyMakeBorder(ctx.currentMat, result, top, bottom, left, right, borderType, parseColor(colorStr));
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

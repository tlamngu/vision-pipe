#include "interpreter/items/edge_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>

namespace visionpipe {

void registerEdgeItems(ItemRegistry& registry) {
    registry.add<CannyItem>();
    registry.add<AutoCannyItem>();
    registry.add<FindContoursItem>();
    registry.add<DrawContoursItem>();
    registry.add<ApproxPolyDPItem>();
    registry.add<ArcLengthItem>();
    registry.add<ContourAreaItem>();
    registry.add<BoundingRectItem>();
    registry.add<MinAreaRectItem>();
    registry.add<MinEnclosingCircleItem>();
    registry.add<MinEnclosingTriangleItem>();
    registry.add<FitEllipseItem>();
    registry.add<FitLineItem>();
    registry.add<ConvexHullItem>();
    registry.add<ConvexityDefectsItem>();
    registry.add<MomentsItem>();
    registry.add<HuMomentsItem>();
    registry.add<MatchShapesItem>();
    registry.add<PointPolygonTestItem>();
    registry.add<IsContourConvexItem>();
    registry.add<HoughLinesItem>();
    registry.add<HoughLinesPItem>();
    registry.add<HoughCirclesItem>();
}

// ============================================================================
// CannyItem
// ============================================================================

CannyItem::CannyItem() {
    _functionName = "canny";
    _description = "Finds edges using Canny algorithm";
    _category = "edge";
    _params = {
        ParamDef::required("threshold1", BaseType::FLOAT, "First threshold for hysteresis"),
        ParamDef::required("threshold2", BaseType::FLOAT, "Second threshold for hysteresis"),
        ParamDef::optional("aperture_size", BaseType::INT, "Sobel aperture size (3, 5, 7)", 3),
        ParamDef::optional("l2_gradient", BaseType::BOOL, "Use L2 norm for gradient", false)
    };
    _example = "canny(50, 150) | canny(100, 200, 5, true)";
    _returnType = "mat";
    _tags = {"edge", "canny", "detection"};
}

ExecutionResult CannyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double thresh1 = args[0].asNumber();
    double thresh2 = args[1].asNumber();
    int apertureSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 3;
    bool L2gradient = args.size() > 3 ? args[3].asBool() : false;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat result;
    cv::Canny(input, result, thresh1, thresh2, apertureSize, L2gradient);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// AutoCannyItem
// ============================================================================

AutoCannyItem::AutoCannyItem() {
    _functionName = "auto_canny";
    _description = "Automatic Canny edge detection using median-based thresholds";
    _category = "edge";
    _params = {
        ParamDef::optional("sigma", BaseType::FLOAT, "Threshold adjustment factor", 0.33)
    };
    _example = "auto_canny() | auto_canny(0.5)";
    _returnType = "mat";
    _tags = {"edge", "canny", "automatic"};
}

ExecutionResult AutoCannyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double sigma = args.size() > 0 ? args[0].asNumber() : 0.33;
    
    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }
    
    // Calculate median
    std::vector<uchar> pixels;
    if (gray.isContinuous()) {
        pixels.assign(gray.data, gray.data + gray.total());
    } else {
        for (int i = 0; i < gray.rows; i++) {
            pixels.insert(pixels.end(), gray.ptr<uchar>(i), gray.ptr<uchar>(i) + gray.cols);
        }
    }
    std::nth_element(pixels.begin(), pixels.begin() + pixels.size()/2, pixels.end());
    double median = pixels[pixels.size()/2];
    
    double lower = std::max(0.0, (1.0 - sigma) * median);
    double upper = std::min(255.0, (1.0 + sigma) * median);
    
    cv::Mat result;
    cv::Canny(gray, result, lower, upper);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FindContoursItem
// ============================================================================

FindContoursItem::FindContoursItem() {
    _functionName = "find_contours";
    _description = "Finds contours in binary image";
    _category = "edge";
    _params = {
        ParamDef::optional("mode", BaseType::STRING, "Mode: external, list, ccomp, tree", "external"),
        ParamDef::optional("method", BaseType::STRING, "Method: none, simple, tc89_l1, tc89_kcos", "simple"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for contours", "contours")
    };
    _example = "find_contours(\"external\", \"simple\", \"contours\")";
    _returnType = "mat";
    _tags = {"contours", "find", "edge"};
}

ExecutionResult FindContoursItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string modeStr = args.size() > 0 ? args[0].asString() : "external";
    std::string methodStr = args.size() > 1 ? args[1].asString() : "simple";
    std::string cacheId = args.size() > 2 ? args[2].asString() : "contours";
    
    int mode = cv::RETR_EXTERNAL;
    if (modeStr == "list") mode = cv::RETR_LIST;
    else if (modeStr == "ccomp") mode = cv::RETR_CCOMP;
    else if (modeStr == "tree") mode = cv::RETR_TREE;
    
    int method = cv::CHAIN_APPROX_SIMPLE;
    if (methodStr == "none") method = cv::CHAIN_APPROX_NONE;
    else if (methodStr == "tc89_l1") method = cv::CHAIN_APPROX_TC89_L1;
    else if (methodStr == "tc89_kcos") method = cv::CHAIN_APPROX_TC89_KCOS;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(input, contours, hierarchy, mode, method);
    
    // Store contours as a serialized Mat for caching
    // Each contour is stored with separator
    std::vector<cv::Point> allPoints;
    std::vector<int> contourSizes;
    for (const auto& c : contours) {
        contourSizes.push_back(static_cast<int>(c.size()));
        allPoints.insert(allPoints.end(), c.begin(), c.end());
    }
    
    // Cache contour data
    cv::Mat pointsMat(allPoints);
    cv::Mat sizesMat(contourSizes);
    ctx.cacheManager->set(cacheId + "_points", pointsMat);
    ctx.cacheManager->set(cacheId + "_sizes", sizesMat);
    ctx.cacheManager->set(cacheId + "_count", cv::Mat(1, 1, CV_32S, cv::Scalar(static_cast<int>(contours.size()))));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DrawContoursItem
// ============================================================================

DrawContoursItem::DrawContoursItem() {
    _functionName = "draw_contours";
    _description = "Draws contours on image";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID of contours", "contours"),
        ParamDef::optional("idx", BaseType::INT, "Contour index (-1 for all)", -1),
        ParamDef::optional("color", BaseType::STRING, "Color: green, red, blue, white, etc", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness (-1 for filled)", 2)
    };
    _example = "draw_contours(\"contours\", -1, \"green\", 2)";
    _returnType = "mat";
    _tags = {"contours", "draw", "edge"};
}

ExecutionResult DrawContoursItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    int idx = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : -1;
    std::string colorStr = args.size() > 2 ? args[2].asString() : "green";
    int thickness = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    else if (colorStr == "white") color = cv::Scalar(255, 255, 255);
    else if (colorStr == "yellow") color = cv::Scalar(0, 255, 255);
    else if (colorStr == "cyan") color = cv::Scalar(255, 255, 0);
    else if (colorStr == "magenta") color = cv::Scalar(255, 0, 255);
    
    // Retrieve contours from cache
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found in cache: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    std::vector<std::vector<cv::Point>> contours;
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point> contour;
        for (int j = 0; j < size; j++) {
            contour.push_back(pointsMat.at<cv::Point>(offset + j));
        }
        contours.push_back(contour);
        offset += size;
    }
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    cv::drawContours(result, contours, idx, color, thickness);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ApproxPolyDPItem
// ============================================================================

ApproxPolyDPItem::ApproxPolyDPItem() {
    _functionName = "approx_poly_dp";
    _description = "Approximates contour with polygon";
    _category = "edge";
    _params = {
        ParamDef::optional("epsilon", BaseType::FLOAT, "Approximation accuracy", 0.02),
        ParamDef::optional("closed", BaseType::BOOL, "Closed curve", true),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID of contours", "contours")
    };
    _example = "approx_poly_dp(0.02, true, \"contours\")";
    _returnType = "mat";
    _tags = {"contour", "polygon", "approximate"};
}

ExecutionResult ApproxPolyDPItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double epsilon = args.size() > 0 ? args[0].asNumber() : 0.02;
    bool closed = args.size() > 1 ? args[1].asBool() : true;
    std::string cacheId = args.size() > 2 ? args[2].asString() : "contours";
    
    // This is a utility - typically used in processing contours
    // For now, just pass through
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ArcLengthItem
// ============================================================================

ArcLengthItem::ArcLengthItem() {
    _functionName = "arc_length";
    _description = "Calculates contour perimeter";
    _category = "edge";
    _params = {
        ParamDef::optional("closed", BaseType::BOOL, "Closed contour", true),
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours")
    };
    _example = "arc_length(true, \"contours\")";
    _returnType = "number";
    _tags = {"contour", "perimeter", "length"};
}

ExecutionResult ArcLengthItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ContourAreaItem
// ============================================================================

ContourAreaItem::ContourAreaItem() {
    _functionName = "contour_area";
    _description = "Calculates contour area";
    _category = "edge";
    _params = {
        ParamDef::optional("oriented", BaseType::BOOL, "Return signed area", false),
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours")
    };
    _example = "contour_area(false, \"contours\")";
    _returnType = "number";
    _tags = {"contour", "area"};
}

ExecutionResult ContourAreaItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// BoundingRectItem
// ============================================================================

BoundingRectItem::BoundingRectItem() {
    _functionName = "bounding_rect";
    _description = "Calculates bounding rectangle of contours and draws them";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("color", BaseType::STRING, "Rectangle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "bounding_rect(\"contours\", \"green\", 2)";
    _returnType = "mat";
    _tags = {"contour", "bounding", "rect"};
}

ExecutionResult BoundingRectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    int thickness = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point> contour;
        for (int j = 0; j < size; j++) {
            contour.push_back(pointsMat.at<cv::Point>(offset + j));
        }
        cv::Rect rect = cv::boundingRect(contour);
        cv::rectangle(result, rect, color, thickness);
        offset += size;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MinAreaRectItem
// ============================================================================

MinAreaRectItem::MinAreaRectItem() {
    _functionName = "min_area_rect";
    _description = "Finds minimum area rotated rectangle";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("color", BaseType::STRING, "Rectangle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "min_area_rect(\"contours\", \"green\", 2)";
    _returnType = "mat";
    _tags = {"contour", "rotated", "rect", "min"};
}

ExecutionResult MinAreaRectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    int thickness = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point> contour;
        for (int j = 0; j < size; j++) {
            contour.push_back(pointsMat.at<cv::Point>(offset + j));
        }
        if (contour.size() >= 5) {
            cv::RotatedRect rRect = cv::minAreaRect(contour);
            cv::Point2f vertices[4];
            rRect.points(vertices);
            for (int k = 0; k < 4; k++) {
                cv::line(result, vertices[k], vertices[(k+1)%4], color, thickness);
            }
        }
        offset += size;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MinEnclosingCircleItem
// ============================================================================

MinEnclosingCircleItem::MinEnclosingCircleItem() {
    _functionName = "min_enclosing_circle";
    _description = "Finds minimum enclosing circle";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("color", BaseType::STRING, "Circle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "min_enclosing_circle(\"contours\", \"green\", 2)";
    _returnType = "mat";
    _tags = {"contour", "circle", "min", "enclosing"};
}

ExecutionResult MinEnclosingCircleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    int thickness = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point> contour;
        for (int j = 0; j < size; j++) {
            contour.push_back(pointsMat.at<cv::Point>(offset + j));
        }
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(contour, center, radius);
        cv::circle(result, center, static_cast<int>(radius), color, thickness);
        offset += size;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MinEnclosingTriangleItem
// ============================================================================

MinEnclosingTriangleItem::MinEnclosingTriangleItem() {
    _functionName = "min_enclosing_triangle";
    _description = "Finds minimum enclosing triangle";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("color", BaseType::STRING, "Triangle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "min_enclosing_triangle(\"contours\", \"green\", 2)";
    _returnType = "mat";
    _tags = {"contour", "triangle", "min", "enclosing"};
}

ExecutionResult MinEnclosingTriangleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    int thickness = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point2f> contour;
        for (int j = 0; j < size; j++) {
            cv::Point p = pointsMat.at<cv::Point>(offset + j);
            contour.push_back(cv::Point2f(static_cast<float>(p.x), static_cast<float>(p.y)));
        }
        std::vector<cv::Point2f> triangle;
        cv::minEnclosingTriangle(contour, triangle);
        if (triangle.size() == 3) {
            for (int k = 0; k < 3; k++) {
                cv::line(result, triangle[k], triangle[(k+1)%3], color, thickness);
            }
        }
        offset += size;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FitEllipseItem
// ============================================================================

FitEllipseItem::FitEllipseItem() {
    _functionName = "fit_ellipse";
    _description = "Fits ellipse to contour points";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("color", BaseType::STRING, "Ellipse color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "fit_ellipse(\"contours\", \"green\", 2)";
    _returnType = "mat";
    _tags = {"contour", "ellipse", "fit"};
}

ExecutionResult FitEllipseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    int thickness = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point> contour;
        for (int j = 0; j < size; j++) {
            contour.push_back(pointsMat.at<cv::Point>(offset + j));
        }
        if (contour.size() >= 5) {
            cv::RotatedRect ellipse = cv::fitEllipse(contour);
            cv::ellipse(result, ellipse, color, thickness);
        }
        offset += size;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FitLineItem
// ============================================================================

FitLineItem::FitLineItem() {
    _functionName = "fit_line";
    _description = "Fits line to 2D point set";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("dist_type", BaseType::STRING, "Distance type: l2, l1, l12, fair, welsch, huber", "l2"),
        ParamDef::optional("color", BaseType::STRING, "Line color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "fit_line(\"contours\", \"l2\", \"green\", 2)";
    _returnType = "mat";
    _tags = {"contour", "line", "fit"};
}

ExecutionResult FitLineItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    std::string distType = args.size() > 1 ? args[1].asString() : "l2";
    std::string colorStr = args.size() > 2 ? args[2].asString() : "green";
    int thickness = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    int distTypeVal = cv::DIST_L2;
    if (distType == "l1") distTypeVal = cv::DIST_L1;
    else if (distType == "l12") distTypeVal = cv::DIST_L12;
    else if (distType == "fair") distTypeVal = cv::DIST_FAIR;
    else if (distType == "welsch") distTypeVal = cv::DIST_WELSCH;
    else if (distType == "huber") distTypeVal = cv::DIST_HUBER;
    
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point> contour;
        for (int j = 0; j < size; j++) {
            contour.push_back(pointsMat.at<cv::Point>(offset + j));
        }
        if (contour.size() >= 2) {
            cv::Vec4f line;
            cv::fitLine(contour, line, distTypeVal, 0, 0.01, 0.01);
            
            float vx = line[0], vy = line[1], x = line[2], y = line[3];
            int lefty = static_cast<int>((-x * vy / vx) + y);
            int righty = static_cast<int>(((result.cols - x) * vy / vx) + y);
            cv::line(result, cv::Point(result.cols-1, righty), cv::Point(0, lefty), color, thickness);
        }
        offset += size;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ConvexHullItem
// ============================================================================

ConvexHullItem::ConvexHullItem() {
    _functionName = "convex_hull";
    _description = "Finds convex hull of contours";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("color", BaseType::STRING, "Hull color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "convex_hull(\"contours\", \"green\", 2)";
    _returnType = "mat";
    _tags = {"contour", "convex", "hull"};
}

ExecutionResult ConvexHullItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "contours";
    std::string colorStr = args.size() > 1 ? args[1].asString() : "green";
    int thickness = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat pointsMatOpt = ctx.cacheManager->get(cacheId + "_points");
    cv::Mat sizesMatOpt = ctx.cacheManager->get(cacheId + "_sizes");
    
    if (pointsMatOpt.empty() || sizesMatOpt.empty()) {
        return ExecutionResult::fail("Contours not found: " + cacheId);
    }
    
    cv::Mat pointsMat = pointsMatOpt;
    cv::Mat sizesMat = sizesMatOpt;
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    int offset = 0;
    for (int i = 0; i < sizesMat.rows; i++) {
        int size = sizesMat.at<int>(i);
        std::vector<cv::Point> contour;
        for (int j = 0; j < size; j++) {
            contour.push_back(pointsMat.at<cv::Point>(offset + j));
        }
        std::vector<cv::Point> hull;
        cv::convexHull(contour, hull);
        std::vector<std::vector<cv::Point>> hulls = {hull};
        cv::drawContours(result, hulls, 0, color, thickness);
        offset += size;
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ConvexityDefectsItem
// ============================================================================

ConvexityDefectsItem::ConvexityDefectsItem() {
    _functionName = "convexity_defects";
    _description = "Finds convexity defects of contour";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours")
    };
    _example = "convexity_defects(\"contours\")";
    _returnType = "mat";
    _tags = {"contour", "convexity", "defects"};
}

ExecutionResult ConvexityDefectsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// MomentsItem
// ============================================================================

MomentsItem::MomentsItem() {
    _functionName = "moments";
    _description = "Calculates image moments";
    _category = "edge";
    _params = {
        ParamDef::optional("binary", BaseType::BOOL, "Treat as binary image", false)
    };
    _example = "moments() | moments(true)";
    _returnType = "mat";
    _tags = {"moments", "analysis"};
}

ExecutionResult MomentsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool binary = args.size() > 0 ? args[0].asBool() : false;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Moments m = cv::moments(input, binary);
    
    // Store moments values as a Mat row
    cv::Mat momentsMat(1, 10, CV_64F);
    momentsMat.at<double>(0) = m.m00;
    momentsMat.at<double>(1) = m.m10;
    momentsMat.at<double>(2) = m.m01;
    momentsMat.at<double>(3) = m.m20;
    momentsMat.at<double>(4) = m.m11;
    momentsMat.at<double>(5) = m.m02;
    momentsMat.at<double>(6) = m.m30;
    momentsMat.at<double>(7) = m.m21;
    momentsMat.at<double>(8) = m.m12;
    momentsMat.at<double>(9) = m.m03;
    
    ctx.cacheManager->set("moments", momentsMat);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// HuMomentsItem
// ============================================================================

HuMomentsItem::HuMomentsItem() {
    _functionName = "hu_moments";
    _description = "Calculates 7 Hu invariant moments";
    _category = "edge";
    _params = {};
    _example = "hu_moments()";
    _returnType = "mat";
    _tags = {"moments", "hu", "invariant"};
}

ExecutionResult HuMomentsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Moments m = cv::moments(input);
    double hu[7];
    cv::HuMoments(m, hu);
    
    cv::Mat huMat(1, 7, CV_64F);
    for (int i = 0; i < 7; i++) {
        huMat.at<double>(i) = hu[i];
    }
    
    ctx.cacheManager->set("hu_moments", huMat);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// MatchShapesItem
// ============================================================================

MatchShapesItem::MatchShapesItem() {
    _functionName = "match_shapes";
    _description = "Compares two shapes using Hu moments";
    _category = "edge";
    _params = {
        ParamDef::required("template_cache", BaseType::STRING, "Template contour cache ID"),
        ParamDef::optional("method", BaseType::STRING, "Method: i1, i2, i3", "i1")
    };
    _example = "match_shapes(\"template\", \"i1\")";
    _returnType = "number";
    _tags = {"match", "shapes", "compare"};
}

ExecutionResult MatchShapesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// PointPolygonTestItem
// ============================================================================

PointPolygonTestItem::PointPolygonTestItem() {
    _functionName = "point_polygon_test";
    _description = "Tests point-contour relationship";
    _category = "edge";
    _params = {
        ParamDef::required("x", BaseType::FLOAT, "Point X coordinate"),
        ParamDef::required("y", BaseType::FLOAT, "Point Y coordinate"),
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours"),
        ParamDef::optional("measure_dist", BaseType::BOOL, "Measure signed distance", false)
    };
    _example = "point_polygon_test(100, 100, \"contours\", true)";
    _returnType = "number";
    _tags = {"point", "polygon", "test"};
}

ExecutionResult PointPolygonTestItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// IsContourConvexItem
// ============================================================================

IsContourConvexItem::IsContourConvexItem() {
    _functionName = "is_contour_convex";
    _description = "Tests whether contour is convex";
    _category = "edge";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Contour cache ID", "contours")
    };
    _example = "is_contour_convex(\"contours\")";
    _returnType = "bool";
    _tags = {"contour", "convex", "test"};
}

ExecutionResult IsContourConvexItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// HoughLinesItem
// ============================================================================

HoughLinesItem::HoughLinesItem() {
    _functionName = "hough_lines";
    _description = "Finds lines using Standard Hough Transform";
    _category = "edge";
    _params = {
        ParamDef::optional("rho", BaseType::FLOAT, "Distance resolution", 1.0),
        ParamDef::optional("theta", BaseType::FLOAT, "Angle resolution (degrees)", 1.0),
        ParamDef::optional("threshold", BaseType::INT, "Accumulator threshold", 100),
        ParamDef::optional("color", BaseType::STRING, "Line color", "red"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "hough_lines(1, 1, 100, \"red\", 2)";
    _returnType = "mat";
    _tags = {"hough", "lines", "detection"};
}

ExecutionResult HoughLinesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double rho = args.size() > 0 ? args[0].asNumber() : 1.0;
    double theta = args.size() > 1 ? args[1].asNumber() * CV_PI / 180.0 : CV_PI / 180.0;
    int threshold = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 100;
    std::string colorStr = args.size() > 3 ? args[3].asString() : "red";
    int thickness = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 2;
    
    cv::Scalar color(0, 0, 255);
    if (colorStr == "green") color = cv::Scalar(0, 255, 0);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    std::vector<cv::Vec2f> lines;
    cv::HoughLines(input, lines, rho, theta, threshold);
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    for (const auto& line : lines) {
        float r = line[0], t = line[1];
        double a = std::cos(t), b = std::sin(t);
        double x0 = a * r, y0 = b * r;
        cv::Point pt1(cvRound(x0 + 1000 * (-b)), cvRound(y0 + 1000 * (a)));
        cv::Point pt2(cvRound(x0 - 1000 * (-b)), cvRound(y0 - 1000 * (a)));
        cv::line(result, pt1, pt2, color, thickness);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// HoughLinesPItem
// ============================================================================

HoughLinesPItem::HoughLinesPItem() {
    _functionName = "hough_lines_p";
    _description = "Finds line segments using Probabilistic Hough Transform";
    _category = "edge";
    _params = {
        ParamDef::optional("rho", BaseType::FLOAT, "Distance resolution", 1.0),
        ParamDef::optional("theta", BaseType::FLOAT, "Angle resolution (degrees)", 1.0),
        ParamDef::optional("threshold", BaseType::INT, "Accumulator threshold", 50),
        ParamDef::optional("min_line_length", BaseType::FLOAT, "Minimum line length", 50.0),
        ParamDef::optional("max_line_gap", BaseType::FLOAT, "Maximum gap between segments", 10.0),
        ParamDef::optional("color", BaseType::STRING, "Line color", "red"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "hough_lines_p(1, 1, 50, 50, 10, \"red\", 2)";
    _returnType = "mat";
    _tags = {"hough", "lines", "probabilistic"};
}

ExecutionResult HoughLinesPItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double rho = args.size() > 0 ? args[0].asNumber() : 1.0;
    double theta = args.size() > 1 ? args[1].asNumber() * CV_PI / 180.0 : CV_PI / 180.0;
    int threshold = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 50;
    double minLineLength = args.size() > 3 ? args[3].asNumber() : 50.0;
    double maxLineGap = args.size() > 4 ? args[4].asNumber() : 10.0;
    std::string colorStr = args.size() > 5 ? args[5].asString() : "red";
    int thickness = args.size() > 6 ? static_cast<int>(args[6].asNumber()) : 2;
    
    cv::Scalar color(0, 0, 255);
    if (colorStr == "green") color = cv::Scalar(0, 255, 0);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(input, lines, rho, theta, threshold, minLineLength, maxLineGap);
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    for (const auto& l : lines) {
        cv::line(result, cv::Point(l[0], l[1]), cv::Point(l[2], l[3]), color, thickness);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// HoughCirclesItem
// ============================================================================

HoughCirclesItem::HoughCirclesItem() {
    _functionName = "hough_circles";
    _description = "Finds circles using Hough Transform";
    _category = "edge";
    _params = {
        ParamDef::optional("dp", BaseType::FLOAT, "Inverse accumulator resolution ratio", 1.0),
        ParamDef::optional("min_dist", BaseType::FLOAT, "Minimum distance between centers", 20.0),
        ParamDef::optional("param1", BaseType::FLOAT, "Canny edge threshold", 100.0),
        ParamDef::optional("param2", BaseType::FLOAT, "Accumulator threshold", 30.0),
        ParamDef::optional("min_radius", BaseType::INT, "Minimum radius", 0),
        ParamDef::optional("max_radius", BaseType::INT, "Maximum radius", 0),
        ParamDef::optional("color", BaseType::STRING, "Circle color", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "hough_circles(1, 20, 100, 30, 10, 100, \"green\", 2)";
    _returnType = "mat";
    _tags = {"hough", "circles", "detection"};
}

ExecutionResult HoughCirclesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double dp = args.size() > 0 ? args[0].asNumber() : 1.0;
    double minDist = args.size() > 1 ? args[1].asNumber() : 20.0;
    double param1 = args.size() > 2 ? args[2].asNumber() : 100.0;
    double param2 = args.size() > 3 ? args[3].asNumber() : 30.0;
    int minRadius = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 0;
    int maxRadius = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 0;
    std::string colorStr = args.size() > 6 ? args[6].asString() : "green";
    int thickness = args.size() > 7 ? static_cast<int>(args[7].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorStr == "red") color = cv::Scalar(0, 0, 255);
    else if (colorStr == "blue") color = cv::Scalar(255, 0, 0);
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(input, circles, cv::HOUGH_GRADIENT, dp, minDist, param1, param2, minRadius, maxRadius);
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    for (const auto& c : circles) {
        cv::Point center(cvRound(c[0]), cvRound(c[1]));
        int radius = cvRound(c[2]);
        cv::circle(result, center, radius, color, thickness);
        cv::circle(result, center, 2, color, -1);
    }
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

#include "interpreter/items/transform_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>

namespace visionpipe {

void registerTransformItems(ItemRegistry& registry) {
    registry.add<ResizeItem>();
    registry.add<RotateItem>();
    registry.add<Rotate90Item>();
    registry.add<FlipItem>();
    registry.add<TransposeItem>();
    registry.add<CropItem>();
    registry.add<CopyMakeBorderItem>();
    registry.add<WarpAffineItem>();
    registry.add<WarpPerspectiveItem>();
    registry.add<GetAffineTransformItem>();
    registry.add<GetPerspectiveTransformItem>();
    registry.add<GetRotationMatrix2DItem>();
    registry.add<InvertAffineTransformItem>();
    registry.add<RemapItem>();
    registry.add<ConvertMapsItem>();
    registry.add<PyrDownItem>();
    registry.add<PyrUpItem>();
    registry.add<BuildPyramidItem>();
    registry.add<HStackItem>();
    registry.add<VStackItem>();
    registry.add<MosaicItem>();
}

// ============================================================================
// ResizeItem
// ============================================================================

ResizeItem::ResizeItem() {
    _functionName = "resize";
    _description = "Resizes the current frame";
    _category = "transform";
    _params = {
        ParamDef::required("width", BaseType::INT, "Target width in pixels (0 = compute from scale)"),
        ParamDef::required("height", BaseType::INT, "Target height in pixels (0 = compute from scale)"),
        ParamDef::optional("interpolation", BaseType::STRING, 
            "Interpolation: nearest, linear, cubic, area, lanczos, linear_exact", "linear"),
        ParamDef::optional("fx", BaseType::FLOAT, "Scale factor along X (when width=0)", 0.0),
        ParamDef::optional("fy", BaseType::FLOAT, "Scale factor along Y (when height=0)", 0.0)
    };
    _example = "resize(640, 480) | resize(0, 0, \"linear\", 0.5, 0.5)";
    _returnType = "mat";
    _tags = {"resize", "transform", "scale"};
}

ExecutionResult ResizeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int width = static_cast<int>(args[0].asNumber());
    int height = static_cast<int>(args[1].asNumber());
    
    int interp = cv::INTER_LINEAR;
    if (args.size() > 2) {
        std::string method = args[2].asString();
        if (method == "nearest") interp = cv::INTER_NEAREST;
        else if (method == "cubic") interp = cv::INTER_CUBIC;
        else if (method == "area") interp = cv::INTER_AREA;
        else if (method == "lanczos") interp = cv::INTER_LANCZOS4;
        else if (method == "linear_exact") interp = cv::INTER_LINEAR_EXACT;
    }
    
    double fx = args.size() > 3 ? args[3].asNumber() : 0.0;
    double fy = args.size() > 4 ? args[4].asNumber() : 0.0;
    
    cv::Mat result;
    cv::resize(ctx.currentMat, result, cv::Size(width, height), fx, fy, interp);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// RotateItem
// ============================================================================

RotateItem::RotateItem() {
    _functionName = "rotate";
    _description = "Rotates image by an arbitrary angle";
    _category = "transform";
    _params = {
        ParamDef::required("angle", BaseType::FLOAT, "Rotation angle in degrees"),
        ParamDef::optional("center_x", BaseType::FLOAT, "Center X (default: image center)", -1.0),
        ParamDef::optional("center_y", BaseType::FLOAT, "Center Y (default: image center)", -1.0),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0),
        ParamDef::optional("border_mode", BaseType::STRING, "Border: constant, replicate, reflect, wrap", "constant"),
        ParamDef::optional("border_value", BaseType::INT, "Border color for constant mode", 0)
    };
    _example = "rotate(45) | rotate(90, -1, -1, 1.0)";
    _returnType = "mat";
    _tags = {"rotate", "transform"};
}

ExecutionResult RotateItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double angle = args[0].asNumber();
    double centerX = args.size() > 1 ? args[1].asNumber() : -1.0;
    double centerY = args.size() > 2 ? args[2].asNumber() : -1.0;
    double scale = args.size() > 3 ? args[3].asNumber() : 1.0;
    
    if (centerX < 0) centerX = ctx.currentMat.cols / 2.0;
    if (centerY < 0) centerY = ctx.currentMat.rows / 2.0;
    
    int borderMode = cv::BORDER_CONSTANT;
    if (args.size() > 4) {
        std::string border = args[4].asString();
        if (border == "replicate") borderMode = cv::BORDER_REPLICATE;
        else if (border == "reflect") borderMode = cv::BORDER_REFLECT;
        else if (border == "wrap") borderMode = cv::BORDER_WRAP;
    }
    
    cv::Scalar borderValue = cv::Scalar::all(args.size() > 5 ? args[5].asNumber() : 0);
    
    cv::Mat rotMat = cv::getRotationMatrix2D(cv::Point2f(centerX, centerY), angle, scale);
    
    cv::Mat result;
    cv::warpAffine(ctx.currentMat, result, rotMat, ctx.currentMat.size(), 
                   cv::INTER_LINEAR, borderMode, borderValue);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// Rotate90Item
// ============================================================================

Rotate90Item::Rotate90Item() {
    _functionName = "rotate90";
    _description = "Rotates image by 90, 180, or 270 degrees";
    _category = "transform";
    _params = {
        ParamDef::required("code", BaseType::STRING, "Rotation: 90, 180, 270, or cw, ccw, 180")
    };
    _example = "rotate90(\"90\") | rotate90(\"cw\")";
    _returnType = "mat";
    _tags = {"rotate", "transform", "90"};
}

ExecutionResult Rotate90Item::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string code = args[0].asString();
    
    int rotCode = cv::ROTATE_90_CLOCKWISE;
    if (code == "180") rotCode = cv::ROTATE_180;
    else if (code == "270" || code == "ccw") rotCode = cv::ROTATE_90_COUNTERCLOCKWISE;
    else if (code == "90" || code == "cw") rotCode = cv::ROTATE_90_CLOCKWISE;
    
    cv::Mat result;
    cv::rotate(ctx.currentMat, result, rotCode);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FlipItem
// ============================================================================

FlipItem::FlipItem() {
    _functionName = "flip";
    _description = "Flips image around vertical, horizontal, or both axes";
    _category = "transform";
    _params = {
        ParamDef::required("flip_code", BaseType::STRING, "Flip: horizontal, vertical, both (or h, v, hv)")
    };
    _example = "flip(\"horizontal\") | flip(\"v\")";
    _returnType = "mat";
    _tags = {"flip", "transform", "mirror"};
}

ExecutionResult FlipItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string code = args[0].asString();
    
    int flipCode = 1;  // horizontal
    if (code == "vertical" || code == "v") flipCode = 0;
    else if (code == "both" || code == "hv") flipCode = -1;
    else if (code == "horizontal" || code == "h") flipCode = 1;
    
    cv::Mat result;
    cv::flip(ctx.currentMat, result, flipCode);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// TransposeItem
// ============================================================================

TransposeItem::TransposeItem() {
    _functionName = "transpose";
    _description = "Transposes the image (swaps rows and columns)";
    _category = "transform";
    _params = {};
    _example = "transpose()";
    _returnType = "mat";
    _tags = {"transpose", "transform"};
}

ExecutionResult TransposeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat result;
    cv::transpose(ctx.currentMat, result);
    return ExecutionResult::ok(result);
}

// ============================================================================
// CropItem
// ============================================================================

CropItem::CropItem() {
    _functionName = "crop";
    _description = "Crops a rectangular region from the image";
    _category = "transform";
    _params = {
        ParamDef::required("x", BaseType::INT, "Left X coordinate"),
        ParamDef::required("y", BaseType::INT, "Top Y coordinate"),
        ParamDef::required("width", BaseType::INT, "Width of crop region"),
        ParamDef::required("height", BaseType::INT, "Height of crop region")
    };
    _example = "crop(100, 100, 200, 200)";
    _returnType = "mat";
    _tags = {"crop", "transform", "roi"};
}

ExecutionResult CropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int width = static_cast<int>(args[2].asNumber());
    int height = static_cast<int>(args[3].asNumber());
    
    // Clamp to image bounds
    x = std::max(0, std::min(x, ctx.currentMat.cols - 1));
    y = std::max(0, std::min(y, ctx.currentMat.rows - 1));
    width = std::min(width, ctx.currentMat.cols - x);
    height = std::min(height, ctx.currentMat.rows - y);
    
    cv::Rect roi(x, y, width, height);
    cv::Mat result = ctx.currentMat(roi).clone();
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// CopyMakeBorderItem
// ============================================================================

CopyMakeBorderItem::CopyMakeBorderItem() {
    _functionName = "copy_make_border";
    _description = "Adds border around the image";
    _category = "transform";
    _params = {
        ParamDef::required("top", BaseType::INT, "Top border width"),
        ParamDef::required("bottom", BaseType::INT, "Bottom border width"),
        ParamDef::required("left", BaseType::INT, "Left border width"),
        ParamDef::required("right", BaseType::INT, "Right border width"),
        ParamDef::optional("border_type", BaseType::STRING, "Type: constant, replicate, reflect, wrap, reflect_101", "constant"),
        ParamDef::optional("value", BaseType::INT, "Border value for constant type", 0)
    };
    _example = "copy_make_border(10, 10, 10, 10, \"constant\", 255)";
    _returnType = "mat";
    _tags = {"border", "transform", "padding"};
}

ExecutionResult CopyMakeBorderItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int top = static_cast<int>(args[0].asNumber());
    int bottom = static_cast<int>(args[1].asNumber());
    int left = static_cast<int>(args[2].asNumber());
    int right = static_cast<int>(args[3].asNumber());
    
    int borderType = cv::BORDER_CONSTANT;
    if (args.size() > 4) {
        std::string type = args[4].asString();
        if (type == "replicate") borderType = cv::BORDER_REPLICATE;
        else if (type == "reflect") borderType = cv::BORDER_REFLECT;
        else if (type == "wrap") borderType = cv::BORDER_WRAP;
        else if (type == "reflect_101") borderType = cv::BORDER_REFLECT_101;
    }
    
    cv::Scalar value = cv::Scalar::all(args.size() > 5 ? args[5].asNumber() : 0);
    
    cv::Mat result;
    cv::copyMakeBorder(ctx.currentMat, result, top, bottom, left, right, borderType, value);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// WarpAffineItem
// ============================================================================

WarpAffineItem::WarpAffineItem() {
    _functionName = "warp_affine";
    _description = "Applies affine transformation";
    _category = "transform";
    _params = {
        ParamDef::required("matrix", BaseType::STRING, "Cache ID of 2x3 transformation matrix"),
        ParamDef::optional("width", BaseType::INT, "Output width (default: same as input)", -1),
        ParamDef::optional("height", BaseType::INT, "Output height (default: same as input)", -1),
        ParamDef::optional("interpolation", BaseType::STRING, "Interpolation method", "linear"),
        ParamDef::optional("border_mode", BaseType::STRING, "Border mode", "constant")
    };
    _example = "warp_affine(\"transform_matrix\")";
    _returnType = "mat";
    _tags = {"warp", "affine", "transform"};
}

ExecutionResult WarpAffineItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string matrixId = args[0].asString();
    cv::Mat matrix = ctx.cacheManager->get(matrixId);
    
    if (matrix.empty()) {
        return ExecutionResult::fail("Transform matrix not found: " + matrixId);
    }
    
    int width = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : -1;
    int height = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : -1;
    
    if (width < 0) width = ctx.currentMat.cols;
    if (height < 0) height = ctx.currentMat.rows;
    
    int interp = cv::INTER_LINEAR;
    if (args.size() > 3) {
        std::string method = args[3].asString();
        if (method == "nearest") interp = cv::INTER_NEAREST;
        else if (method == "cubic") interp = cv::INTER_CUBIC;
    }
    
    int borderMode = cv::BORDER_CONSTANT;
    if (args.size() > 4) {
        std::string border = args[4].asString();
        if (border == "replicate") borderMode = cv::BORDER_REPLICATE;
        else if (border == "reflect") borderMode = cv::BORDER_REFLECT;
    }
    
    cv::Mat result;
    cv::warpAffine(ctx.currentMat, result, matrix, cv::Size(width, height), interp, borderMode);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// WarpPerspectiveItem
// ============================================================================

WarpPerspectiveItem::WarpPerspectiveItem() {
    _functionName = "warp_perspective";
    _description = "Applies perspective transformation";
    _category = "transform";
    _params = {
        ParamDef::required("matrix", BaseType::STRING, "Cache ID of 3x3 transformation matrix"),
        ParamDef::optional("width", BaseType::INT, "Output width", -1),
        ParamDef::optional("height", BaseType::INT, "Output height", -1),
        ParamDef::optional("interpolation", BaseType::STRING, "Interpolation method", "linear"),
        ParamDef::optional("border_mode", BaseType::STRING, "Border mode", "constant")
    };
    _example = "warp_perspective(\"homography\")";
    _returnType = "mat";
    _tags = {"warp", "perspective", "transform", "homography"};
}

ExecutionResult WarpPerspectiveItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string matrixId = args[0].asString();
    cv::Mat matrix = ctx.cacheManager->get(matrixId);
    
    if (matrix.empty()) {
        return ExecutionResult::fail("Transform matrix not found: " + matrixId);
    }
    
    int width = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : -1;
    int height = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : -1;
    
    if (width < 0) width = ctx.currentMat.cols;
    if (height < 0) height = ctx.currentMat.rows;
    
    int interp = cv::INTER_LINEAR;
    int borderMode = cv::BORDER_CONSTANT;
    
    cv::Mat result;
    cv::warpPerspective(ctx.currentMat, result, matrix, cv::Size(width, height), interp, borderMode);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// GetAffineTransformItem
// ============================================================================

GetAffineTransformItem::GetAffineTransformItem() {
    _functionName = "get_affine_transform";
    _description = "Calculates affine transform from 3 point correspondences";
    _category = "transform";
    _params = {
        ParamDef::required("src_points", BaseType::ARRAY, "Source points [[x1,y1], [x2,y2], [x3,y3]]"),
        ParamDef::required("dst_points", BaseType::ARRAY, "Destination points [[x1,y1], [x2,y2], [x3,y3]]"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID to store matrix", "affine_matrix")
    };
    _example = "get_affine_transform(src, dst, \"my_affine\")";
    _returnType = "mat";
    _tags = {"affine", "transform", "matrix"};
}

ExecutionResult GetAffineTransformItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Simplified - in full implementation would parse point arrays
    std::string cacheId = args.size() > 2 ? args[2].asString() : "affine_matrix";
    
    // Default identity-like transform
    cv::Point2f srcTri[3] = {
        cv::Point2f(0, 0),
        cv::Point2f(ctx.currentMat.cols - 1, 0),
        cv::Point2f(0, ctx.currentMat.rows - 1)
    };
    cv::Point2f dstTri[3] = {
        cv::Point2f(0, 0),
        cv::Point2f(ctx.currentMat.cols - 1, 0),
        cv::Point2f(0, ctx.currentMat.rows - 1)
    };
    
    cv::Mat warpMat = cv::getAffineTransform(srcTri, dstTri);
    ctx.cacheManager->set(cacheId, warpMat);
    
    return ExecutionResult::ok(warpMat);
}

// ============================================================================
// GetPerspectiveTransformItem
// ============================================================================

GetPerspectiveTransformItem::GetPerspectiveTransformItem() {
    _functionName = "get_perspective_transform";
    _description = "Calculates perspective transform from 4 point correspondences";
    _category = "transform";
    _params = {
        ParamDef::required("src_points", BaseType::ARRAY, "Source points [[x1,y1], [x2,y2], [x3,y3], [x4,y4]]"),
        ParamDef::required("dst_points", BaseType::ARRAY, "Destination points"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID to store matrix", "perspective_matrix")
    };
    _example = "get_perspective_transform(src, dst, \"homography\")";
    _returnType = "mat";
    _tags = {"perspective", "transform", "homography", "matrix"};
}

ExecutionResult GetPerspectiveTransformItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 2 ? args[2].asString() : "perspective_matrix";
    
    // Default identity
    cv::Point2f srcQuad[4] = {
        cv::Point2f(0, 0),
        cv::Point2f(ctx.currentMat.cols - 1, 0),
        cv::Point2f(ctx.currentMat.cols - 1, ctx.currentMat.rows - 1),
        cv::Point2f(0, ctx.currentMat.rows - 1)
    };
    cv::Point2f dstQuad[4] = {
        cv::Point2f(0, 0),
        cv::Point2f(ctx.currentMat.cols - 1, 0),
        cv::Point2f(ctx.currentMat.cols - 1, ctx.currentMat.rows - 1),
        cv::Point2f(0, ctx.currentMat.rows - 1)
    };
    
    cv::Mat perspMat = cv::getPerspectiveTransform(srcQuad, dstQuad);
    ctx.cacheManager->set(cacheId, perspMat);
    
    return ExecutionResult::ok(perspMat);
}

// ============================================================================
// GetRotationMatrix2DItem
// ============================================================================

GetRotationMatrix2DItem::GetRotationMatrix2DItem() {
    _functionName = "get_rotation_matrix_2d";
    _description = "Calculates 2D rotation matrix";
    _category = "transform";
    _params = {
        ParamDef::required("angle", BaseType::FLOAT, "Rotation angle in degrees"),
        ParamDef::optional("center_x", BaseType::FLOAT, "Center X (default: image center)", -1.0),
        ParamDef::optional("center_y", BaseType::FLOAT, "Center Y (default: image center)", -1.0),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID to store matrix", "rotation_matrix")
    };
    _example = "get_rotation_matrix_2d(45, -1, -1, 1.0, \"rot\")";
    _returnType = "mat";
    _tags = {"rotation", "matrix", "transform"};
}

ExecutionResult GetRotationMatrix2DItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double angle = args[0].asNumber();
    double centerX = args.size() > 1 ? args[1].asNumber() : -1.0;
    double centerY = args.size() > 2 ? args[2].asNumber() : -1.0;
    double scale = args.size() > 3 ? args[3].asNumber() : 1.0;
    std::string cacheId = args.size() > 4 ? args[4].asString() : "rotation_matrix";
    
    if (centerX < 0) centerX = ctx.currentMat.cols / 2.0;
    if (centerY < 0) centerY = ctx.currentMat.rows / 2.0;
    
    cv::Mat rotMat = cv::getRotationMatrix2D(cv::Point2f(centerX, centerY), angle, scale);
    ctx.cacheManager->set(cacheId, rotMat);
    
    return ExecutionResult::ok(rotMat);
}

// ============================================================================
// InvertAffineTransformItem
// ============================================================================

InvertAffineTransformItem::InvertAffineTransformItem() {
    _functionName = "invert_affine_transform";
    _description = "Inverts an affine transformation";
    _category = "transform";
    _params = {
        ParamDef::required("matrix", BaseType::STRING, "Cache ID of affine matrix to invert"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for inverted matrix", "inverted_affine")
    };
    _example = "invert_affine_transform(\"affine\", \"affine_inv\")";
    _returnType = "mat";
    _tags = {"affine", "invert", "transform"};
}

ExecutionResult InvertAffineTransformItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string matrixId = args[0].asString();
    std::string cacheId = args.size() > 1 ? args[1].asString() : "inverted_affine";
    
    cv::Mat matrix = ctx.cacheManager->get(matrixId);
    if (matrix.empty()) {
        return ExecutionResult::fail("Matrix not found: " + matrixId);
    }
    
    cv::Mat inverted;
    cv::invertAffineTransform(matrix, inverted);
    ctx.cacheManager->set(cacheId, inverted);
    
    return ExecutionResult::ok(inverted);
}

// ============================================================================
// RemapItem
// ============================================================================

RemapItem::RemapItem() {
    _functionName = "remap";
    _description = "Applies generic geometric transformation using lookup maps";
    _category = "transform";
    _params = {
        ParamDef::required("map1", BaseType::STRING, "Cache ID of first map (x coordinates)"),
        ParamDef::required("map2", BaseType::STRING, "Cache ID of second map (y coordinates)"),
        ParamDef::optional("interpolation", BaseType::STRING, "Interpolation method", "linear"),
        ParamDef::optional("border_mode", BaseType::STRING, "Border mode", "constant")
    };
    _example = "remap(\"map_x\", \"map_y\")";
    _returnType = "mat";
    _tags = {"remap", "transform", "distortion"};
}

ExecutionResult RemapItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string map1Id = args[0].asString();
    std::string map2Id = args[1].asString();
    
    cv::Mat map1 = ctx.cacheManager->get(map1Id);
    cv::Mat map2 = ctx.cacheManager->get(map2Id);
    
    if (map1.empty() || map2.empty()) {
        return ExecutionResult::fail("Remap maps not found");
    }
    
    int interp = cv::INTER_LINEAR;
    int borderMode = cv::BORDER_CONSTANT;
    
    cv::Mat result;
    cv::remap(ctx.currentMat, result, map1, map2, interp, borderMode);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ConvertMapsItem
// ============================================================================

ConvertMapsItem::ConvertMapsItem() {
    _functionName = "convert_maps";
    _description = "Converts floating-point maps to fixed-point for faster remapping";
    _category = "transform";
    _params = {
        ParamDef::required("map1", BaseType::STRING, "Cache ID of first input map"),
        ParamDef::required("map2", BaseType::STRING, "Cache ID of second input map"),
        ParamDef::optional("dstmap1type", BaseType::INT, "Type of first output map", 0)
    };
    _example = "convert_maps(\"map1\", \"map2\")";
    _returnType = "mat";
    _tags = {"remap", "convert", "optimize"};
}

ExecutionResult ConvertMapsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Placeholder
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// PyrDownItem
// ============================================================================

PyrDownItem::PyrDownItem() {
    _functionName = "pyr_down";
    _description = "Blurs and downsamples image (Gaussian pyramid down)";
    _category = "transform";
    _params = {
        ParamDef::optional("dst_width", BaseType::INT, "Output width (default: half)", -1),
        ParamDef::optional("dst_height", BaseType::INT, "Output height (default: half)", -1)
    };
    _example = "pyr_down() | pyr_down(320, 240)";
    _returnType = "mat";
    _tags = {"pyramid", "downsample", "blur"};
}

ExecutionResult PyrDownItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int dstW = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : -1;
    int dstH = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : -1;
    
    cv::Size dstSize;
    if (dstW > 0 && dstH > 0) {
        dstSize = cv::Size(dstW, dstH);
    }
    
    cv::Mat result;
    cv::pyrDown(ctx.currentMat, result, dstSize);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// PyrUpItem
// ============================================================================

PyrUpItem::PyrUpItem() {
    _functionName = "pyr_up";
    _description = "Upsamples and blurs image (Gaussian pyramid up)";
    _category = "transform";
    _params = {
        ParamDef::optional("dst_width", BaseType::INT, "Output width (default: double)", -1),
        ParamDef::optional("dst_height", BaseType::INT, "Output height (default: double)", -1)
    };
    _example = "pyr_up()";
    _returnType = "mat";
    _tags = {"pyramid", "upsample", "blur"};
}

ExecutionResult PyrUpItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int dstW = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : -1;
    int dstH = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : -1;
    
    cv::Size dstSize;
    if (dstW > 0 && dstH > 0) {
        dstSize = cv::Size(dstW, dstH);
    }
    
    cv::Mat result;
    cv::pyrUp(ctx.currentMat, result, dstSize);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BuildPyramidItem
// ============================================================================

BuildPyramidItem::BuildPyramidItem() {
    _functionName = "build_pyramid";
    _description = "Builds a Gaussian pyramid and caches all levels";
    _category = "transform";
    _params = {
        ParamDef::required("max_level", BaseType::INT, "Number of pyramid levels"),
        ParamDef::optional("prefix", BaseType::STRING, "Cache prefix for levels", "pyr")
    };
    _example = "build_pyramid(4, \"pyr\")  // Creates pyr_0, pyr_1, pyr_2, pyr_3";
    _returnType = "mat";
    _tags = {"pyramid", "build", "levels"};
}

ExecutionResult BuildPyramidItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int maxLevel = static_cast<int>(args[0].asNumber());
    std::string prefix = args.size() > 1 ? args[1].asString() : "pyr";
    
    std::vector<cv::Mat> pyramid;
    cv::buildPyramid(ctx.currentMat, pyramid, maxLevel);
    
    for (size_t i = 0; i < pyramid.size(); i++) {
        std::string cacheId = prefix + "_" + std::to_string(i);
        ctx.cacheManager->set(cacheId, pyramid[i]);
    }
    
    return ExecutionResult::ok(pyramid.empty() ? ctx.currentMat : pyramid[0]);
}

// ============================================================================
// HStackItem
// ============================================================================

HStackItem::HStackItem() {
    _functionName = "hstack";
    _description = "Stacks images horizontally";
    _category = "transform";
    _params = {
        ParamDef::required("other", BaseType::STRING, "Cache ID of image to stack on the right")
    };
    _example = "hstack(\"right_image\")";
    _returnType = "mat";
    _tags = {"stack", "horizontal", "concatenate"};
}

ExecutionResult HStackItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string otherId = args[0].asString();
    cv::Mat other = ctx.cacheManager->get(otherId);
    
    if (other.empty()) {
        return ExecutionResult::fail("Image not found: " + otherId);
    }
    
    // Ensure same height
    if (ctx.currentMat.rows != other.rows) {
        cv::resize(other, other, cv::Size(other.cols * ctx.currentMat.rows / other.rows, ctx.currentMat.rows));
    }
    
    // Ensure same type
    if (ctx.currentMat.type() != other.type()) {
        other.convertTo(other, ctx.currentMat.type());
    }
    
    cv::Mat result;
    cv::hconcat(ctx.currentMat, other, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// VStackItem
// ============================================================================

VStackItem::VStackItem() {
    _functionName = "vstack";
    _description = "Stacks images vertically";
    _category = "transform";
    _params = {
        ParamDef::required("other", BaseType::STRING, "Cache ID of image to stack below")
    };
    _example = "vstack(\"bottom_image\")";
    _returnType = "mat";
    _tags = {"stack", "vertical", "concatenate"};
}

ExecutionResult VStackItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string otherId = args[0].asString();
    cv::Mat other = ctx.cacheManager->get(otherId);
    
    if (other.empty()) {
        return ExecutionResult::fail("Image not found: " + otherId);
    }
    
    // Ensure same width
    if (ctx.currentMat.cols != other.cols) {
        cv::resize(other, other, cv::Size(ctx.currentMat.cols, other.rows * ctx.currentMat.cols / other.cols));
    }
    
    // Ensure same type
    if (ctx.currentMat.type() != other.type()) {
        other.convertTo(other, ctx.currentMat.type());
    }
    
    cv::Mat result;
    cv::vconcat(ctx.currentMat, other, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MosaicItem
// ============================================================================

MosaicItem::MosaicItem() {
    _functionName = "mosaic";
    _description = "Creates a mosaic/grid from multiple cached images";
    _category = "transform";
    _params = {
        ParamDef::required("images", BaseType::ARRAY, "Array of cache IDs"),
        ParamDef::optional("cols", BaseType::INT, "Number of columns (default: auto)", -1),
        ParamDef::optional("cell_width", BaseType::INT, "Cell width (default: auto)", -1),
        ParamDef::optional("cell_height", BaseType::INT, "Cell height (default: auto)", -1)
    };
    _example = "mosaic([\"img1\", \"img2\", \"img3\", \"img4\"], 2)";
    _returnType = "mat";
    _tags = {"mosaic", "grid", "layout"};
}

ExecutionResult MosaicItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Placeholder - needs array support for full implementation
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

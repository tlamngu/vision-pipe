#include "interpreter/items/morphology_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>

namespace visionpipe {

void registerMorphologyItems(ItemRegistry& registry) {
    registry.add<ThresholdItem>();
    registry.add<AdaptiveThresholdItem>();
    registry.add<OtsuThresholdItem>();
    registry.add<ErodeItem>();
    registry.add<DilateItem>();
    registry.add<MorphologyExItem>();
    registry.add<GetStructuringElementItem>();
    registry.add<SkeletonizeItem>();
    registry.add<ThinningItem>();
    registry.add<FillHolesItem>();
    registry.add<RemoveSmallObjectsItem>();
    registry.add<ConnectedComponentsItem>();
    registry.add<ConnectedComponentsWithStatsItem>();
    registry.add<DistanceTransformItem>();
}

// ============================================================================
// ThresholdItem
// ============================================================================

ThresholdItem::ThresholdItem() {
    _functionName = "threshold";
    _description = "Applies fixed-level threshold to image";
    _category = "morphology";
    _params = {
        ParamDef::required("thresh", BaseType::FLOAT, "Threshold value"),
        ParamDef::optional("maxval", BaseType::FLOAT, "Maximum value for binary thresholding", 255.0),
        ParamDef::optional("type", BaseType::STRING, 
            "Type: binary, binary_inv, trunc, tozero, tozero_inv", "binary")
    };
    _example = "threshold(127) | threshold(100, 255, \"binary_inv\")";
    _returnType = "mat";
    _tags = {"threshold", "binary", "segmentation"};
}

ExecutionResult ThresholdItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double thresh = args[0].asNumber();
    double maxval = args.size() > 1 ? args[1].asNumber() : 255.0;
    
    int type = cv::THRESH_BINARY;
    if (args.size() > 2) {
        std::string typeStr = args[2].asString();
        if (typeStr == "binary_inv") type = cv::THRESH_BINARY_INV;
        else if (typeStr == "trunc") type = cv::THRESH_TRUNC;
        else if (typeStr == "tozero") type = cv::THRESH_TOZERO;
        else if (typeStr == "tozero_inv") type = cv::THRESH_TOZERO_INV;
    }
    
    cv::Mat result;
    cv::threshold(ctx.currentMat, result, thresh, maxval, type);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// AdaptiveThresholdItem
// ============================================================================

AdaptiveThresholdItem::AdaptiveThresholdItem() {
    _functionName = "adaptive_threshold";
    _description = "Applies adaptive threshold using local neighborhood";
    _category = "morphology";
    _params = {
        ParamDef::optional("maxval", BaseType::FLOAT, "Maximum value", 255.0),
        ParamDef::optional("method", BaseType::STRING, "Method: mean, gaussian", "gaussian"),
        ParamDef::optional("type", BaseType::STRING, "Type: binary, binary_inv", "binary"),
        ParamDef::optional("block_size", BaseType::INT, "Neighborhood size (odd)", 11),
        ParamDef::optional("c", BaseType::FLOAT, "Constant subtracted from mean", 2.0)
    };
    _example = "adaptive_threshold(255, \"gaussian\", \"binary\", 11, 2)";
    _returnType = "mat";
    _tags = {"threshold", "adaptive", "segmentation"};
}

ExecutionResult AdaptiveThresholdItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double maxval = args.size() > 0 ? args[0].asNumber() : 255.0;
    std::string methodStr = args.size() > 1 ? args[1].asString() : "gaussian";
    std::string typeStr = args.size() > 2 ? args[2].asString() : "binary";
    int blockSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 11;
    double C = args.size() > 4 ? args[4].asNumber() : 2.0;
    
    if (blockSize % 2 == 0) blockSize++;
    
    int method = cv::ADAPTIVE_THRESH_GAUSSIAN_C;
    if (methodStr == "mean") method = cv::ADAPTIVE_THRESH_MEAN_C;
    
    int type = cv::THRESH_BINARY;
    if (typeStr == "binary_inv") type = cv::THRESH_BINARY_INV;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat result;
    cv::adaptiveThreshold(input, result, maxval, method, type, blockSize, C);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// OtsuThresholdItem
// ============================================================================

OtsuThresholdItem::OtsuThresholdItem() {
    _functionName = "otsu_threshold";
    _description = "Applies Otsu's automatic threshold";
    _category = "morphology";
    _params = {
        ParamDef::optional("maxval", BaseType::FLOAT, "Maximum value", 255.0),
        ParamDef::optional("type", BaseType::STRING, "Type: binary, binary_inv", "binary")
    };
    _example = "otsu_threshold() | otsu_threshold(255, \"binary_inv\")";
    _returnType = "mat";
    _tags = {"threshold", "otsu", "automatic"};
}

ExecutionResult OtsuThresholdItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double maxval = args.size() > 0 ? args[0].asNumber() : 255.0;
    std::string typeStr = args.size() > 1 ? args[1].asString() : "binary";
    
    int type = cv::THRESH_BINARY | cv::THRESH_OTSU;
    if (typeStr == "binary_inv") type = cv::THRESH_BINARY_INV | cv::THRESH_OTSU;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat result;
    cv::threshold(input, result, 0, maxval, type);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ErodeItem
// ============================================================================

ErodeItem::ErodeItem() {
    _functionName = "erode";
    _description = "Erodes image using a structuring element";
    _category = "morphology";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT, "Kernel size", 3),
        ParamDef::optional("shape", BaseType::STRING, "Shape: rect, cross, ellipse", "rect"),
        ParamDef::optional("iterations", BaseType::INT, "Number of iterations", 1)
    };
    _example = "erode(3) | erode(5, \"ellipse\", 2)";
    _returnType = "mat";
    _tags = {"erode", "morphology"};
}

ExecutionResult ErodeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 3;
    std::string shape = args.size() > 1 ? args[1].asString() : "rect";
    int iterations = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 1;
    
    int morphShape = cv::MORPH_RECT;
    if (shape == "cross") morphShape = cv::MORPH_CROSS;
    else if (shape == "ellipse") morphShape = cv::MORPH_ELLIPSE;
    
    cv::Mat kernel = cv::getStructuringElement(morphShape, cv::Size(ksize, ksize));
    
    cv::Mat result;
    cv::erode(ctx.currentMat, result, kernel, cv::Point(-1,-1), iterations);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DilateItem
// ============================================================================

DilateItem::DilateItem() {
    _functionName = "dilate";
    _description = "Dilates image using a structuring element";
    _category = "morphology";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT, "Kernel size", 3),
        ParamDef::optional("shape", BaseType::STRING, "Shape: rect, cross, ellipse", "rect"),
        ParamDef::optional("iterations", BaseType::INT, "Number of iterations", 1)
    };
    _example = "dilate(3) | dilate(5, \"ellipse\", 2)";
    _returnType = "mat";
    _tags = {"dilate", "morphology"};
}

ExecutionResult DilateItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 3;
    std::string shape = args.size() > 1 ? args[1].asString() : "rect";
    int iterations = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 1;
    
    int morphShape = cv::MORPH_RECT;
    if (shape == "cross") morphShape = cv::MORPH_CROSS;
    else if (shape == "ellipse") morphShape = cv::MORPH_ELLIPSE;
    
    cv::Mat kernel = cv::getStructuringElement(morphShape, cv::Size(ksize, ksize));
    
    cv::Mat result;
    cv::dilate(ctx.currentMat, result, kernel, cv::Point(-1,-1), iterations);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MorphologyExItem
// ============================================================================

MorphologyExItem::MorphologyExItem() {
    _functionName = "morphology_ex";
    _description = "Performs advanced morphological operations";
    _category = "morphology";
    _params = {
        ParamDef::required("op", BaseType::STRING, 
            "Operation: open, close, gradient, tophat, blackhat, hitmiss"),
        ParamDef::optional("kernel_size", BaseType::INT, "Kernel size", 3),
        ParamDef::optional("shape", BaseType::STRING, "Shape: rect, cross, ellipse", "rect"),
        ParamDef::optional("iterations", BaseType::INT, "Number of iterations", 1)
    };
    _example = "morphology_ex(\"open\", 5) | morphology_ex(\"gradient\", 3, \"ellipse\")";
    _returnType = "mat";
    _tags = {"morphology", "open", "close", "gradient"};
}

ExecutionResult MorphologyExItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string op = args[0].asString();
    int ksize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    std::string shape = args.size() > 2 ? args[2].asString() : "rect";
    int iterations = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 1;
    
    int morphOp = cv::MORPH_OPEN;
    if (op == "close") morphOp = cv::MORPH_CLOSE;
    else if (op == "gradient") morphOp = cv::MORPH_GRADIENT;
    else if (op == "tophat") morphOp = cv::MORPH_TOPHAT;
    else if (op == "blackhat") morphOp = cv::MORPH_BLACKHAT;
    else if (op == "hitmiss") morphOp = cv::MORPH_HITMISS;
    
    int morphShape = cv::MORPH_RECT;
    if (shape == "cross") morphShape = cv::MORPH_CROSS;
    else if (shape == "ellipse") morphShape = cv::MORPH_ELLIPSE;
    
    cv::Mat kernel = cv::getStructuringElement(morphShape, cv::Size(ksize, ksize));
    
    cv::Mat result;
    cv::morphologyEx(ctx.currentMat, result, morphOp, kernel, cv::Point(-1,-1), iterations);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// GetStructuringElementItem
// ============================================================================

GetStructuringElementItem::GetStructuringElementItem() {
    _functionName = "get_structuring_element";
    _description = "Creates a structuring element and caches it";
    _category = "morphology";
    _params = {
        ParamDef::required("shape", BaseType::STRING, "Shape: rect, cross, ellipse"),
        ParamDef::required("size", BaseType::INT, "Element size"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID to store element", "kernel")
    };
    _example = "get_structuring_element(\"ellipse\", 5, \"my_kernel\")";
    _returnType = "mat";
    _tags = {"kernel", "structuring", "morphology"};
}

ExecutionResult GetStructuringElementItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string shape = args[0].asString();
    int size = static_cast<int>(args[1].asNumber());
    std::string cacheId = args.size() > 2 ? args[2].asString() : "kernel";
    
    int morphShape = cv::MORPH_RECT;
    if (shape == "cross") morphShape = cv::MORPH_CROSS;
    else if (shape == "ellipse") morphShape = cv::MORPH_ELLIPSE;
    
    cv::Mat kernel = cv::getStructuringElement(morphShape, cv::Size(size, size));
    ctx.cacheManager->set(cacheId, kernel);
    
    return ExecutionResult::ok(kernel);
}

// ============================================================================
// SkeletonizeItem
// ============================================================================

SkeletonizeItem::SkeletonizeItem() {
    _functionName = "skeletonize";
    _description = "Extracts skeleton of binary image";
    _category = "morphology";
    _params = {};
    _example = "skeletonize()";
    _returnType = "mat";
    _tags = {"skeleton", "thin", "morphology"};
}

ExecutionResult SkeletonizeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat img = ctx.currentMat.clone();
    if (img.channels() > 1) {
        cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat skel = cv::Mat::zeros(img.size(), CV_8UC1);
    cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));
    cv::Mat temp;
    
    bool done = false;
    while (!done) {
        cv::morphologyEx(img, temp, cv::MORPH_OPEN, element);
        cv::bitwise_not(temp, temp);
        cv::bitwise_and(img, temp, temp);
        cv::bitwise_or(skel, temp, skel);
        cv::erode(img, img, element);
        
        double maxVal;
        cv::minMaxLoc(img, nullptr, &maxVal);
        done = (maxVal == 0);
    }
    
    return ExecutionResult::ok(skel);
}

// ============================================================================
// ThinningItem
// ============================================================================

ThinningItem::ThinningItem() {
    _functionName = "thinning";
    _description = "Applies morphological thinning using ximgproc";
    _category = "morphology";
    _params = {
        ParamDef::optional("type", BaseType::STRING, "Type: zhangsuen, guohall", "zhangsuen")
    };
    _example = "thinning() | thinning(\"guohall\")";
    _returnType = "mat";
    _tags = {"thin", "skeleton", "morphology"};
}

ExecutionResult ThinningItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Use skeletonize as fallback - ximgproc::thinning requires contrib
    return SkeletonizeItem().execute(args, ctx);
}

// ============================================================================
// FillHolesItem
// ============================================================================

FillHolesItem::FillHolesItem() {
    _functionName = "fill_holes";
    _description = "Fills holes in binary image";
    _category = "morphology";
    _params = {};
    _example = "fill_holes()";
    _returnType = "mat";
    _tags = {"fill", "holes", "morphology"};
}

ExecutionResult FillHolesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    // Flood fill from edges
    cv::Mat floodFilled = input.clone();
    cv::floodFill(floodFilled, cv::Point(0,0), cv::Scalar(255));
    
    // Invert flood filled image
    cv::Mat floodFilledInv;
    cv::bitwise_not(floodFilled, floodFilledInv);
    
    // Combine with original
    cv::Mat result = (input | floodFilledInv);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// RemoveSmallObjectsItem
// ============================================================================

RemoveSmallObjectsItem::RemoveSmallObjectsItem() {
    _functionName = "remove_small_objects";
    _description = "Removes small connected components from binary image";
    _category = "morphology";
    _params = {
        ParamDef::required("min_size", BaseType::INT, "Minimum object area in pixels")
    };
    _example = "remove_small_objects(100)";
    _returnType = "mat";
    _tags = {"remove", "small", "connected", "morphology"};
}

ExecutionResult RemoveSmallObjectsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int minSize = static_cast<int>(args[0].asNumber());
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat labels, stats, centroids;
    int numLabels = cv::connectedComponentsWithStats(input, labels, stats, centroids);
    
    cv::Mat result = cv::Mat::zeros(input.size(), CV_8UC1);
    for (int i = 1; i < numLabels; i++) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= minSize) {
            result.setTo(255, labels == i);
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ConnectedComponentsItem
// ============================================================================

ConnectedComponentsItem::ConnectedComponentsItem() {
    _functionName = "connected_components";
    _description = "Labels connected components in binary image";
    _category = "morphology";
    _params = {
        ParamDef::optional("connectivity", BaseType::INT, "Connectivity: 4 or 8", 8)
    };
    _example = "connected_components(8)";
    _returnType = "mat";
    _tags = {"connected", "components", "label"};
}

ExecutionResult ConnectedComponentsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int connectivity = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 8;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat labels;
    int numLabels = cv::connectedComponents(input, labels, connectivity);
    
    // Normalize labels to 0-255 for visualization
    cv::Mat result;
    labels.convertTo(result, CV_8U, 255.0 / (numLabels - 1));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ConnectedComponentsWithStatsItem
// ============================================================================

ConnectedComponentsWithStatsItem::ConnectedComponentsWithStatsItem() {
    _functionName = "connected_components_with_stats";
    _description = "Labels components and computes statistics";
    _category = "morphology";
    _params = {
        ParamDef::optional("connectivity", BaseType::INT, "Connectivity: 4 or 8", 8),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache prefix for stats", "cc")
    };
    _example = "connected_components_with_stats(8, \"cc\")";
    _returnType = "mat";
    _tags = {"connected", "components", "stats"};
}

ExecutionResult ConnectedComponentsWithStatsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int connectivity = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 8;
    std::string prefix = args.size() > 1 ? args[1].asString() : "cc";
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat labels, stats, centroids;
    int numLabels = cv::connectedComponentsWithStats(input, labels, stats, centroids, connectivity);
    
    ctx.cacheManager->set(prefix + "_labels", labels);
    ctx.cacheManager->set(prefix + "_stats", stats);
    ctx.cacheManager->set(prefix + "_centroids", centroids);
    
    // Normalize labels for visualization
    cv::Mat result;
    labels.convertTo(result, CV_8U, 255.0 / std::max(1, numLabels - 1));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DistanceTransformItem
// ============================================================================

DistanceTransformItem::DistanceTransformItem() {
    _functionName = "distance_transform";
    _description = "Calculates distance to nearest zero pixel";
    _category = "morphology";
    _params = {
        ParamDef::optional("distance_type", BaseType::STRING, "Type: l1, l2, c", "l2"),
        ParamDef::optional("mask_size", BaseType::INT, "Mask size: 3, 5, 0 (precise)", 3)
    };
    _example = "distance_transform(\"l2\", 5)";
    _returnType = "mat";
    _tags = {"distance", "transform", "morphology"};
}

ExecutionResult DistanceTransformItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string distType = args.size() > 0 ? args[0].asString() : "l2";
    int maskSize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    
    int distanceType = cv::DIST_L2;
    if (distType == "l1") distanceType = cv::DIST_L1;
    else if (distType == "c") distanceType = cv::DIST_C;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat result;
    cv::distanceTransform(input, result, distanceType, maskSize);
    
    // Normalize for visualization
    cv::normalize(result, result, 0, 255, cv::NORM_MINMAX);
    result.convertTo(result, CV_8U);
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

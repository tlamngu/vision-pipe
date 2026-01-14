#include "interpreter/items/filter_items.h"
#include <iostream>

namespace visionpipe {

void registerFilterItems(ItemRegistry& registry) {
    registry.add<GaussianBlurItem>();
    registry.add<MedianBlurItem>();
    registry.add<BilateralFilterItem>();
    registry.add<BoxFilterItem>();
    registry.add<BlurItem>();
    registry.add<StackBlurItem>();
    registry.add<SharpenItem>();
    registry.add<LaplacianItem>();
    registry.add<Filter2DItem>();
    registry.add<SepFilter2DItem>();
    registry.add<SobelItem>();
    registry.add<ScharrItem>();
    registry.add<FastNlMeansItem>();
    registry.add<FastNlMeansColoredItem>();
}

// ============================================================================
// GaussianBlurItem
// ============================================================================

GaussianBlurItem::GaussianBlurItem() {
    _functionName = "gaussian_blur";
    _description = "Applies Gaussian blur to the current frame";
    _category = "filter";
    _params = {
        ParamDef::optional("ksize", BaseType::INT, "Kernel size (odd number)", 5),
        ParamDef::optional("sigma_x", BaseType::FLOAT, "Sigma X (0 = auto from ksize)", 0.0),
        ParamDef::optional("sigma_y", BaseType::FLOAT, "Sigma Y (0 = same as sigma_x)", 0.0),
        ParamDef::optional("border", BaseType::STRING, "Border type: default, replicate, reflect, wrap, reflect_101", "default")
    };
    _example = "gaussian_blur(5) | gaussian_blur(7, 1.5)";
    _returnType = "mat";
    _tags = {"blur", "filter", "smooth", "gaussian"};
}

ExecutionResult GaussianBlurItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    double sigmaX = args.size() > 1 ? args[1].asNumber() : 0.0;
    double sigmaY = args.size() > 2 ? args[2].asNumber() : 0.0;
    
    if (ksize % 2 == 0) ksize++;  // Ensure odd
    
    int borderType = cv::BORDER_DEFAULT;
    if (args.size() > 3) {
        std::string border = args[3].asString();
        if (border == "replicate") borderType = cv::BORDER_REPLICATE;
        else if (border == "reflect") borderType = cv::BORDER_REFLECT;
        else if (border == "wrap") borderType = cv::BORDER_WRAP;
        else if (border == "reflect_101") borderType = cv::BORDER_REFLECT_101;
    }
    
    cv::Mat result;
    cv::GaussianBlur(ctx.currentMat, result, cv::Size(ksize, ksize), sigmaX, sigmaY, borderType);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MedianBlurItem
// ============================================================================

MedianBlurItem::MedianBlurItem() {
    _functionName = "median_blur";
    _description = "Applies median blur (effective for salt-and-pepper noise)";
    _category = "filter";
    _params = {
        ParamDef::optional("ksize", BaseType::INT, "Kernel size (odd number)", 5)
    };
    _example = "median_blur(5)";
    _returnType = "mat";
    _tags = {"blur", "filter", "denoise", "median"};
}

ExecutionResult MedianBlurItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    if (ksize % 2 == 0) ksize++;
    
    cv::Mat result;
    cv::medianBlur(ctx.currentMat, result, ksize);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BilateralFilterItem
// ============================================================================

BilateralFilterItem::BilateralFilterItem() {
    _functionName = "bilateral_filter";
    _description = "Applies bilateral filter (edge-preserving smoothing)";
    _category = "filter";
    _params = {
        ParamDef::optional("d", BaseType::INT, "Diameter of pixel neighborhood (neg = computed from sigma)", 9),
        ParamDef::optional("sigma_color", BaseType::FLOAT, "Filter sigma in color space", 75.0),
        ParamDef::optional("sigma_space", BaseType::FLOAT, "Filter sigma in coordinate space", 75.0),
        ParamDef::optional("border", BaseType::STRING, "Border type", "default")
    };
    _example = "bilateral_filter(9, 75, 75)";
    _returnType = "mat";
    _tags = {"blur", "filter", "smooth", "edge-preserving", "bilateral"};
}

ExecutionResult BilateralFilterItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int d = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 9;
    double sigmaColor = args.size() > 1 ? args[1].asNumber() : 75.0;
    double sigmaSpace = args.size() > 2 ? args[2].asNumber() : 75.0;
    
    int borderType = cv::BORDER_DEFAULT;
    if (args.size() > 3) {
        std::string border = args[3].asString();
        if (border == "replicate") borderType = cv::BORDER_REPLICATE;
        else if (border == "reflect") borderType = cv::BORDER_REFLECT;
        else if (border == "wrap") borderType = cv::BORDER_WRAP;
    }
    
    cv::Mat result;
    cv::bilateralFilter(ctx.currentMat, result, d, sigmaColor, sigmaSpace, borderType);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BoxFilterItem
// ============================================================================

BoxFilterItem::BoxFilterItem() {
    _functionName = "box_filter";
    _description = "Applies box filter (averaging filter with option for unnormalized)";
    _category = "filter";
    _params = {
        ParamDef::optional("ksize", BaseType::INT, "Kernel size", 5),
        ParamDef::optional("normalize", BaseType::BOOL, "Normalize by kernel area", true),
        ParamDef::optional("ddepth", BaseType::INT, "Output depth (-1 = same as input)", -1),
        ParamDef::optional("anchor_x", BaseType::INT, "Anchor X (-1 = center)", -1),
        ParamDef::optional("anchor_y", BaseType::INT, "Anchor Y (-1 = center)", -1)
    };
    _example = "box_filter(5) | box_filter(3, false)";
    _returnType = "mat";
    _tags = {"blur", "filter", "box", "average"};
}

ExecutionResult BoxFilterItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    bool normalize = args.size() > 1 ? args[1].asBool() : true;
    int ddepth = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : -1;
    int anchorX = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : -1;
    int anchorY = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : -1;
    
    cv::Mat result;
    cv::boxFilter(ctx.currentMat, result, ddepth, cv::Size(ksize, ksize), 
                  cv::Point(anchorX, anchorY), normalize);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BlurItem
// ============================================================================

BlurItem::BlurItem() {
    _functionName = "blur";
    _description = "Applies simple averaging blur";
    _category = "filter";
    _params = {
        ParamDef::optional("ksize", BaseType::INT, "Kernel size", 5)
    };
    _example = "blur(5)";
    _returnType = "mat";
    _tags = {"blur", "filter", "average"};
}

ExecutionResult BlurItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    
    cv::Mat result;
    cv::blur(ctx.currentMat, result, cv::Size(ksize, ksize));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// StackBlurItem
// ============================================================================

StackBlurItem::StackBlurItem() {
    _functionName = "stack_blur";
    _description = "Applies stack blur (fast approximation of Gaussian blur)";
    _category = "filter";
    _params = {
        ParamDef::optional("ksize", BaseType::INT, "Kernel size (odd, 1+)", 5)
    };
    _example = "stack_blur(5)";
    _returnType = "mat";
    _tags = {"blur", "filter", "fast"};
}

ExecutionResult StackBlurItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    if (ksize % 2 == 0) ksize++;
    if (ksize < 1) ksize = 1;
    
    cv::Mat result = ctx.currentMat.clone();
    cv::stackBlur(result, result, cv::Size(ksize, ksize));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SharpenItem
// ============================================================================

SharpenItem::SharpenItem() {
    _functionName = "sharpen";
    _description = "Applies unsharp masking to sharpen the image";
    _category = "filter";
    _params = {
        ParamDef::optional("amount", BaseType::FLOAT, "Sharpening strength (0-3)", 1.0),
        ParamDef::optional("radius", BaseType::INT, "Blur radius for mask", 5),
        ParamDef::optional("threshold", BaseType::INT, "Threshold for sharpening (0-255)", 0)
    };
    _example = "sharpen(1.5) | sharpen(1.0, 3)";
    _returnType = "mat";
    _tags = {"sharpen", "filter", "enhance"};
}

ExecutionResult SharpenItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double amount = args.size() > 0 ? args[0].asNumber() : 1.0;
    int radius = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 5;
    int threshold = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;
    
    if (radius % 2 == 0) radius++;
    
    cv::Mat blurred;
    cv::GaussianBlur(ctx.currentMat, blurred, cv::Size(radius, radius), 0);
    
    cv::Mat result;
    if (threshold == 0) {
        // Simple unsharp mask
        cv::addWeighted(ctx.currentMat, 1.0 + amount, blurred, -amount, 0, result);
    } else {
        // Threshold-based sharpening
        cv::Mat diff;
        cv::absdiff(ctx.currentMat, blurred, diff);
        cv::Mat mask;
        cv::cvtColor(diff, mask, cv::COLOR_BGR2GRAY);
        cv::threshold(mask, mask, threshold, 255, cv::THRESH_BINARY);
        mask.convertTo(mask, CV_32F, 1.0/255.0);
        cv::cvtColor(mask, mask, cv::COLOR_GRAY2BGR);
        
        cv::Mat sharpened;
        cv::addWeighted(ctx.currentMat, 1.0 + amount, blurred, -amount, 0, sharpened);
        
        // Blend based on mask
        ctx.currentMat.convertTo(result, CV_32F);
        sharpened.convertTo(sharpened, CV_32F);
        result = result.mul(1.0 - mask) + sharpened.mul(mask);
        result.convertTo(result, ctx.currentMat.type());
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// LaplacianItem
// ============================================================================

LaplacianItem::LaplacianItem() {
    _functionName = "laplacian";
    _description = "Applies Laplacian operator for edge detection";
    _category = "filter";
    _params = {
        ParamDef::optional("ksize", BaseType::INT, "Aperture size (1, 3, 5, 7)", 3),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0),
        ParamDef::optional("delta", BaseType::FLOAT, "Value added to result", 0.0),
        ParamDef::optional("ddepth", BaseType::INT, "Output depth", -1)
    };
    _example = "laplacian(3)";
    _returnType = "mat";
    _tags = {"edge", "filter", "laplacian", "derivative"};
}

ExecutionResult LaplacianItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 3;
    double scale = args.size() > 1 ? args[1].asNumber() : 1.0;
    double delta = args.size() > 2 ? args[2].asNumber() : 0.0;
    int ddepth = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : CV_16S;
    
    cv::Mat result;
    cv::Laplacian(ctx.currentMat, result, ddepth, ksize, scale, delta);
    
    // Convert to displayable format if needed
    if (ddepth == CV_16S) {
        cv::convertScaleAbs(result, result);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// Filter2DItem
// ============================================================================

Filter2DItem::Filter2DItem() {
    _functionName = "filter2d";
    _description = "Applies a custom convolution kernel";
    _category = "filter";
    _params = {
        ParamDef::required("kernel", BaseType::ARRAY, "Convolution kernel (flattened array)"),
        ParamDef::optional("ksize", BaseType::INT, "Kernel size (sqrt of array length)", 3),
        ParamDef::optional("delta", BaseType::FLOAT, "Value added to result", 0.0),
        ParamDef::optional("ddepth", BaseType::INT, "Output depth (-1 = same)", -1)
    };
    _example = "filter2d([0,-1,0,-1,5,-1,0,-1,0], 3)";
    _returnType = "mat";
    _tags = {"filter", "convolution", "custom"};
}

ExecutionResult Filter2DItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // For now, use preset kernels - full array support would need more work
    int ksize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    double delta = args.size() > 2 ? args[2].asNumber() : 0.0;
    int ddepth = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : -1;
    
    // Default sharpening kernel
    cv::Mat kernel = (cv::Mat_<float>(3,3) << 
        0, -1, 0,
        -1, 5, -1,
        0, -1, 0);
    
    cv::Mat result;
    cv::filter2D(ctx.currentMat, result, ddepth, kernel, cv::Point(-1,-1), delta);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SepFilter2DItem
// ============================================================================

SepFilter2DItem::SepFilter2DItem() {
    _functionName = "sep_filter2d";
    _description = "Applies separable filter (faster for large kernels)";
    _category = "filter";
    _params = {
        ParamDef::required("kernel_x", BaseType::ARRAY, "Row kernel"),
        ParamDef::required("kernel_y", BaseType::ARRAY, "Column kernel"),
        ParamDef::optional("ddepth", BaseType::INT, "Output depth", -1)
    };
    _example = "sep_filter2d([1,2,1], [1,2,1])";
    _returnType = "mat";
    _tags = {"filter", "separable", "convolution"};
}

ExecutionResult SepFilter2DItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Default Gaussian-like separable kernel
    cv::Mat kernelX = (cv::Mat_<float>(1,3) << 1, 2, 1);
    cv::Mat kernelY = (cv::Mat_<float>(1,3) << 1, 2, 1);
    int ddepth = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : -1;
    
    cv::Mat result;
    cv::sepFilter2D(ctx.currentMat, result, ddepth, kernelX, kernelY);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SobelItem
// ============================================================================

SobelItem::SobelItem() {
    _functionName = "sobel";
    _description = "Applies Sobel operator for gradient calculation";
    _category = "filter";
    _params = {
        ParamDef::optional("dx", BaseType::INT, "Order of derivative in X", 1),
        ParamDef::optional("dy", BaseType::INT, "Order of derivative in Y", 0),
        ParamDef::optional("ksize", BaseType::INT, "Kernel size (1, 3, 5, 7)", 3),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0),
        ParamDef::optional("delta", BaseType::FLOAT, "Value added to result", 0.0)
    };
    _example = "sobel(1, 0) | sobel(0, 1)";
    _returnType = "mat";
    _tags = {"edge", "gradient", "sobel", "derivative"};
}

ExecutionResult SobelItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int dx = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 1;
    int dy = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    int ksize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 3;
    double scale = args.size() > 3 ? args[3].asNumber() : 1.0;
    double delta = args.size() > 4 ? args[4].asNumber() : 0.0;
    
    cv::Mat result;
    cv::Sobel(ctx.currentMat, result, CV_16S, dx, dy, ksize, scale, delta);
    cv::convertScaleAbs(result, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ScharrItem
// ============================================================================

ScharrItem::ScharrItem() {
    _functionName = "scharr";
    _description = "Applies Scharr operator (more accurate than Sobel for 3x3)";
    _category = "filter";
    _params = {
        ParamDef::optional("dx", BaseType::INT, "Order of X derivative (0 or 1)", 1),
        ParamDef::optional("dy", BaseType::INT, "Order of Y derivative (0 or 1)", 0),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0),
        ParamDef::optional("delta", BaseType::FLOAT, "Value added to result", 0.0)
    };
    _example = "scharr(1, 0) | scharr(0, 1)";
    _returnType = "mat";
    _tags = {"edge", "gradient", "scharr", "derivative"};
}

ExecutionResult ScharrItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int dx = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 1;
    int dy = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    double scale = args.size() > 2 ? args[2].asNumber() : 1.0;
    double delta = args.size() > 3 ? args[3].asNumber() : 0.0;
    
    cv::Mat result;
    cv::Scharr(ctx.currentMat, result, CV_16S, dx, dy, scale, delta);
    cv::convertScaleAbs(result, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FastNlMeansItem
// ============================================================================

FastNlMeansItem::FastNlMeansItem() {
    _functionName = "fast_nl_means";
    _description = "Non-local means denoising for grayscale images";
    _category = "filter";
    _params = {
        ParamDef::optional("h", BaseType::FLOAT, "Filter strength (higher = more denoising)", 10.0),
        ParamDef::optional("template_size", BaseType::INT, "Template window size (odd)", 7),
        ParamDef::optional("search_size", BaseType::INT, "Search window size (odd)", 21)
    };
    _example = "fast_nl_means(10) | fast_nl_means(3, 7, 21)";
    _returnType = "mat";
    _tags = {"denoise", "filter", "nlmeans"};
}

ExecutionResult FastNlMeansItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float h = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 10.0f;
    int templateSize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 7;
    int searchSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 21;
    
    // Ensure odd sizes
    if (templateSize % 2 == 0) templateSize++;
    if (searchSize % 2 == 0) searchSize++;
    
    cv::Mat result;
    if (ctx.currentMat.channels() == 1) {
        cv::fastNlMeansDenoising(ctx.currentMat, result, h, templateSize, searchSize);
    } else {
        // Convert to grayscale first
        cv::Mat gray;
        cv::cvtColor(ctx.currentMat, gray, cv::COLOR_BGR2GRAY);
        cv::fastNlMeansDenoising(gray, result, h, templateSize, searchSize);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FastNlMeansColoredItem
// ============================================================================

FastNlMeansColoredItem::FastNlMeansColoredItem() {
    _functionName = "fast_nl_means_colored";
    _description = "Non-local means denoising for color images";
    _category = "filter";
    _params = {
        ParamDef::optional("h", BaseType::FLOAT, "Filter strength for luminance", 10.0),
        ParamDef::optional("h_color", BaseType::FLOAT, "Filter strength for color components", 10.0),
        ParamDef::optional("template_size", BaseType::INT, "Template window size (odd)", 7),
        ParamDef::optional("search_size", BaseType::INT, "Search window size (odd)", 21)
    };
    _example = "fast_nl_means_colored(10, 10)";
    _returnType = "mat";
    _tags = {"denoise", "filter", "nlmeans", "color"};
}

ExecutionResult FastNlMeansColoredItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float h = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 10.0f;
    float hColor = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 10.0f;
    int templateSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 7;
    int searchSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 21;
    
    if (templateSize % 2 == 0) templateSize++;
    if (searchSize % 2 == 0) searchSize++;
    
    cv::Mat result;
    if (ctx.currentMat.channels() == 3) {
        cv::fastNlMeansDenoisingColored(ctx.currentMat, result, h, hColor, templateSize, searchSize);
    } else {
        // Grayscale - just use regular denoising
        cv::fastNlMeansDenoising(ctx.currentMat, result, h, templateSize, searchSize);
    }
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

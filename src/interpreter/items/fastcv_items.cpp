#ifdef VISIONPIPE_FASTCV_ENABLED

#include "interpreter/items/fastcv_items.h"
#include "interpreter/cache_manager.h"
#include <opencv2/fastcv.hpp>
#include <iostream>

namespace visionpipe {

// ============================================================================
// Registration
// ============================================================================

void registerFastCVItems(ItemRegistry& registry) {
    registry.add<FastGaussianBlurItem>();
    registry.add<FastCornerDetectItem>();
    registry.add<FastPyramidItem>();
    registry.add<FastFFTItem>();
    registry.add<FastIFFTItem>();
    registry.add<FastHoughLinesItem>();
    registry.add<FastMomentsItem>();
    registry.add<FastThresholdRangeItem>();
    registry.add<FastBilateralFilterItem>();
    registry.add<FastBilateralRecursiveItem>();
    registry.add<FastSobelItem>();
    registry.add<FastResizeDownItem>();
    registry.add<FastFilter2DItem>();
    registry.add<FastMSERItem>();
    registry.add<FastWarpPerspectiveItem>();
    registry.add<FastWarpAffineItem>();
    registry.add<FastMeanShiftItem>();
    registry.add<FastTrackOpticalFlowLKItem>();
    registry.add<FastCalcHistItem>();
    registry.add<FastNormalizeLocalBoxItem>();
}

// ============================================================================
// FastGaussianBlurItem
// ============================================================================

FastGaussianBlurItem::FastGaussianBlurItem() {
    _functionName = "fcv_gaussian_blur";
    _description = "Gaussian blur accelerated by FastCV (supports 3x3 and 5x5 kernels, types: 8U/16S/32S)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT, "Kernel size: 3 or 5", 5),
        ParamDef::optional("blur_border", BaseType::BOOL, "Process image border pixels", true)
    };
    _example = "fcv_gaussian_blur(5, true)";
    _returnType = "mat";
    _tags = {"fastcv", "blur", "gaussian", "filter"};
}

ExecutionResult FastGaussianBlurItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize       = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    bool blurBorder = args.size() > 1 ? args[1].asBool() : true;

    if (ksize != 3 && ksize != 5) ksize = 5;

    cv::Mat dst;
    cv::fastcv::gaussianBlur(ctx.currentMat, dst, ksize, blurBorder);
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastCornerDetectItem
// ============================================================================

FastCornerDetectItem::FastCornerDetectItem() {
    _functionName = "fcv_fast10";
    _description = "FAST10 corner detection accelerated by FastCV; draws detected corners on output";
    _category = "fastcv";
    _params = {
        ParamDef::optional("barrier", BaseType::INT, "Intensity barrier threshold", 10),
        ParamDef::optional("border", BaseType::INT, "Border width to skip around edges", 4),
        ParamDef::optional("nms", BaseType::BOOL, "Enable non-maximum suppression", true)
    };
    _example = "fcv_fast10(10, 4, true)";
    _returnType = "mat";
    _tags = {"fastcv", "corner", "feature", "detect", "fast10"};
}

ExecutionResult FastCornerDetectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int barrier     = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 10;
    int border      = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 4;
    bool nmsEnabled = args.size() > 2 ? args[2].asBool() : true;

    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);

    std::vector<int> coords, scores;
    cv::fastcv::FAST10(gray, cv::noArray(), coords, scores, barrier, border, nmsEnabled);

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);

    for (size_t i = 0; i < coords.size() / 2; i++) {
        cv::circle(output, cv::Point(coords[2*i], coords[2*i+1]), 3, cv::Scalar(0, 255, 0), -1);
    }
    return ExecutionResult::ok(output);
}

// ============================================================================
// FastPyramidItem
// ============================================================================

FastPyramidItem::FastPyramidItem() {
    _functionName = "fcv_pyramid";
    _description = "Image pyramid accelerated by FastCV; each level is stored in cache as <prefix>_0, <prefix>_1, ...";
    _category = "fastcv";
    _params = {
        ParamDef::optional("levels", BaseType::INT, "Number of pyramid levels", 4),
        ParamDef::optional("scale_by2", BaseType::BOOL, "Scale by 2 (true) or ORB factor 0.84 (false)", true),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache key prefix for pyramid levels", "pyr")
    };
    _example = "fcv_pyramid(4, true, \"pyr\")";
    _returnType = "mat";
    _tags = {"fastcv", "pyramid", "scale", "multi-scale"};
}

ExecutionResult FastPyramidItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int nLevels    = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 4;
    bool scaleBy2  = args.size() > 1 ? args[1].asBool() : true;
    std::string prefix = args.size() > 2 ? args[2].asString() : "pyr";

    std::vector<cv::Mat> pyr;
    cv::fastcv::buildPyramid(ctx.currentMat, pyr, nLevels, scaleBy2);

    if (!pyr.empty() && ctx.cacheManager) {
        for (int i = 0; i < (int)pyr.size(); i++) {
            ctx.cacheManager->set(prefix + "_" + std::to_string(i), pyr[i], false);
        }
    }

    cv::Mat out = pyr.empty() ? ctx.currentMat : pyr[0];
    return ExecutionResult::ok(out);
}

// ============================================================================
// FastFFTItem
// ============================================================================

FastFFTItem::FastFFTItem() {
    _functionName = "fcv_fft";
    _description = "Fast Fourier Transform accelerated by FastCV (input: 8U grayscale); returns log-magnitude spectrum";
    _category = "fastcv";
    _params = {};
    _example = "fcv_fft()";
    _returnType = "mat";
    _tags = {"fastcv", "fft", "frequency", "transform"};
}

ExecutionResult FastFFTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat dst;
    cv::fastcv::FFT(src, dst);

    if (dst.channels() == 2) {
        std::vector<cv::Mat> planes;
        cv::split(dst, planes);
        cv::Mat mag;
        cv::magnitude(planes[0], planes[1], mag);
        mag += cv::Scalar::all(1);
        cv::log(mag, mag);
        cv::normalize(mag, mag, 0, 255, cv::NORM_MINMAX);
        mag.convertTo(mag, CV_8U);
        // Shift quadrants for centered visualization
        int cx = mag.cols / 2, cy = mag.rows / 2;
        cv::Mat q0(mag, cv::Rect(0,  0,  cx, cy)), q1(mag, cv::Rect(cx, 0,  cx, cy));
        cv::Mat q2(mag, cv::Rect(0,  cy, cx, cy)), q3(mag, cv::Rect(cx, cy, cx, cy));
        cv::Mat tmp; q0.copyTo(tmp); q3.copyTo(q0); tmp.copyTo(q3);
                     q1.copyTo(tmp); q2.copyTo(q1); tmp.copyTo(q2);
        return ExecutionResult::ok(mag);
    }
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastIFFTItem
// ============================================================================

FastIFFTItem::FastIFFTItem() {
    _functionName = "fcv_ifft";
    _description = "Inverse FFT accelerated by FastCV (input: 2-channel CV_32F output of fcv_fft raw)";
    _category = "fastcv";
    _params = {};
    _example = "fcv_ifft()";
    _returnType = "mat";
    _tags = {"fastcv", "ifft", "frequency", "transform"};
}

ExecutionResult FastIFFTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    if (ctx.currentMat.type() != CV_32FC2) {
        return ExecutionResult::fail("fcv_ifft expects 2-channel CV_32F input (raw output of fcv_fft)");
    }
    cv::Mat dst;
    cv::fastcv::IFFT(ctx.currentMat, dst);
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastHoughLinesItem
// ============================================================================

FastHoughLinesItem::FastHoughLinesItem() {
    _functionName = "fcv_hough_lines";
    _description = "Hough line detection accelerated by FastCV (input: grayscale or edge image); draws lines on output";
    _category = "fastcv";
    _params = {
        ParamDef::optional("threshold", BaseType::FLOAT, "Normalized quality threshold [0,1]", 0.25),
        ParamDef::optional("color", BaseType::STRING, "Line draw color (green, red, blue, white)", "green")
    };
    _example = "fcv_hough_lines(0.25, \"green\")";
    _returnType = "mat";
    _tags = {"fastcv", "hough", "lines", "detect"};
}

ExecutionResult FastHoughLinesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double threshold  = args.size() > 0 ? args[0].asNumber() : 0.25;
    std::string colorName = args.size() > 1 ? args[1].asString() : "green";

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    // Width must be aligned to 8 for FastCV
    if (src.cols % 8 != 0) {
        int bpix = ((src.cols & 0xfffffff8) + 8) - src.cols;
        cv::copyMakeBorder(src, src, 0, 0, 0, bpix, cv::BORDER_REFLECT101);
    }

    std::vector<cv::Vec4f> lines;
    cv::fastcv::houghLines(src, lines, threshold);

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);

    cv::Scalar color(0, 255, 0);
    if (colorName == "red")   color = cv::Scalar(0, 0, 255);
    else if (colorName == "blue")  color = cv::Scalar(255, 0, 0);
    else if (colorName == "white") color = cv::Scalar(255, 255, 255);

    for (const cv::Vec4f& l : lines) {
        cv::line(output, {(int)l[0], (int)l[1]}, {(int)l[2], (int)l[3]}, color, 2);
    }
    return ExecutionResult::ok(output);
}

// ============================================================================
// FastMomentsItem
// ============================================================================

FastMomentsItem::FastMomentsItem() {
    _functionName = "fcv_moments";
    _description = "Image moments accelerated by FastCV; prints m00, m10, m01, centroid to console";
    _category = "fastcv";
    _params = {
        ParamDef::optional("binary", BaseType::BOOL, "Treat image as binary before computing moments", false)
    };
    _example = "fcv_moments(false)";
    _returnType = "mat";
    _tags = {"fastcv", "moments", "statistics"};
}

ExecutionResult FastMomentsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool binary = args.size() > 0 ? args[0].asBool() : false;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Moments m = cv::fastcv::moments(src, binary);
    double cx = m.m00 != 0.0 ? m.m10 / m.m00 : 0.0;
    double cy = m.m00 != 0.0 ? m.m01 / m.m00 : 0.0;
    std::cout << "[fcv_moments] m00=" << m.m00
              << " m10=" << m.m10 << " m01=" << m.m01
              << " centroid=(" << cx << ", " << cy << ")" << std::endl;
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// FastThresholdRangeItem
// ============================================================================

FastThresholdRangeItem::FastThresholdRangeItem() {
    _functionName = "fcv_threshold_range";
    _description = "Range threshold (FastCV): pixels in [low_thresh, high_thresh] become true_value, others become false_value";
    _category = "fastcv";
    _params = {
        ParamDef::required("low_thresh",  BaseType::INT, "Lower threshold (inclusive)"),
        ParamDef::required("high_thresh", BaseType::INT, "Upper threshold (inclusive)"),
        ParamDef::optional("true_value",  BaseType::INT, "Value assigned to in-range pixels", 255),
        ParamDef::optional("false_value", BaseType::INT, "Value assigned to out-of-range pixels", 0)
    };
    _example = "fcv_threshold_range(100, 200, 255, 0)";
    _returnType = "mat";
    _tags = {"fastcv", "threshold", "range", "binary"};
}

ExecutionResult FastThresholdRangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int loTh = static_cast<int>(args[0].asNumber());
    int hiTh = static_cast<int>(args[1].asNumber());
    int tv   = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 255;
    int fv   = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat dst;
    cv::fastcv::thresholdRange(src, dst, loTh, hiTh, tv, fv);
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastBilateralFilterItem
// ============================================================================

FastBilateralFilterItem::FastBilateralFilterItem() {
    _functionName = "fcv_bilateral_filter";
    _description = "Bilateral filter accelerated by FastCV (kernel d: 5, 7, or 9; 8U grayscale input)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("d",           BaseType::INT,   "Kernel diameter (5, 7, or 9)", 5),
        ParamDef::optional("sigma_color", BaseType::FLOAT, "Color sigma (typical: 50)", 50.0),
        ParamDef::optional("sigma_space", BaseType::FLOAT, "Space sigma (typical: 1)", 1.0)
    };
    _example = "fcv_bilateral_filter(5, 50.0, 1.0)";
    _returnType = "mat";
    _tags = {"fastcv", "bilateral", "filter", "edge-preserving"};
}

ExecutionResult FastBilateralFilterItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int d            = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    float sigmaColor = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 50.0f;
    float sigmaSpace = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 1.0f;

    if (d != 5 && d != 7 && d != 9) d = 5;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat dst;
    cv::fastcv::bilateralFilter(src, dst, d, sigmaColor, sigmaSpace);
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastBilateralRecursiveItem
// ============================================================================

FastBilateralRecursiveItem::FastBilateralRecursiveItem() {
    _functionName = "fcv_bilateral_recursive";
    _description = "Recursive bilateral filter accelerated by FastCV (fast edge-preserving; 8U grayscale)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("sigma_color", BaseType::FLOAT, "Color sigma in [0,1] (typical: 0.03)", 0.03),
        ParamDef::optional("sigma_space", BaseType::FLOAT, "Space sigma in [0,1] (typical: 0.1)", 0.1)
    };
    _example = "fcv_bilateral_recursive(0.03, 0.1)";
    _returnType = "mat";
    _tags = {"fastcv", "bilateral", "recursive", "filter", "edge-preserving"};
}

ExecutionResult FastBilateralRecursiveItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float sigmaColor = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 0.03f;
    float sigmaSpace = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.1f;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat dst;
    cv::fastcv::bilateralRecursive(src, dst, sigmaColor, sigmaSpace);
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastSobelItem
// ============================================================================

FastSobelItem::FastSobelItem() {
    _functionName = "fcv_sobel";
    _description = "Sobel edge detection accelerated by FastCV; stores dx and dy to cache as <id>_dx and <id>_dy; returns magnitude";
    _category = "fastcv";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT,    "Kernel size (3, 5, or 7)", 3),
        ParamDef::optional("cache_id",    BaseType::STRING, "Cache prefix for dx/dy gradient outputs", "sobel")
    };
    _example = "fcv_sobel(3, \"sobel\")";
    _returnType = "mat";
    _tags = {"fastcv", "sobel", "edge", "gradient"};
}

ExecutionResult FastSobelItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize      = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 3;
    std::string id = args.size() > 1 ? args[1].asString() : "sobel";

    if (ksize != 3 && ksize != 5 && ksize != 7) ksize = 3;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat dx, dy;
    cv::fastcv::sobel(src, dx, dy, ksize, cv::BORDER_CONSTANT, 0);

    if (ctx.cacheManager) {
        ctx.cacheManager->set(id + "_dx", dx, false);
        ctx.cacheManager->set(id + "_dy", dy, false);
    }

    cv::Mat fdx, fdy, mag;
    dx.convertTo(fdx, CV_32F);
    dy.convertTo(fdy, CV_32F);
    cv::magnitude(fdx, fdy, mag);
    cv::normalize(mag, mag, 0, 255, cv::NORM_MINMAX);
    mag.convertTo(mag, CV_8U);
    return ExecutionResult::ok(mag);
}

// ============================================================================
// FastResizeDownItem
// ============================================================================

FastResizeDownItem::FastResizeDownItem() {
    _functionName = "fcv_resize_down";
    _description = "Optimized downscale resize accelerated by FastCV; specify either target size or scale factors";
    _category = "fastcv";
    _params = {
        ParamDef::optional("width",      BaseType::INT,   "Target width (0 = use scale)", 0),
        ParamDef::optional("height",     BaseType::INT,   "Target height (0 = use scale)", 0),
        ParamDef::optional("inv_scale_x", BaseType::FLOAT, "Horizontal inverse scale (e.g., 0.5 = half width)", 0.5),
        ParamDef::optional("inv_scale_y", BaseType::FLOAT, "Vertical inverse scale", 0.5)
    };
    _example = "fcv_resize_down(640, 480, 0, 0)";
    _returnType = "mat";
    _tags = {"fastcv", "resize", "scale", "downsample"};
}

ExecutionResult FastResizeDownItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int w   = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 0;
    int h   = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    double sx = args.size() > 2 ? args[2].asNumber() : 0.5;
    double sy = args.size() > 3 ? args[3].asNumber() : 0.5;

    cv::Size dsize(w, h);
    double invScaleX = (w > 0 || h > 0) ? 0.0 : sx;
    double invScaleY = (w > 0 || h > 0) ? 0.0 : sy;

    cv::Mat dst;
    cv::fastcv::resizeDown(ctx.currentMat, dst, dsize, invScaleX, invScaleY);
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastFilter2DItem
// ============================================================================

FastFilter2DItem::FastFilter2DItem() {
    _functionName = "fcv_filter2d";
    _description = "2D convolution filter accelerated by FastCV (kernel sizes 3-11; 8U grayscale input)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT,    "Kernel size (3, 5, 7, 9, or 11)", 3),
        ParamDef::optional("ddepth",      BaseType::STRING, "Output depth: u8, s16, f32", "u8")
    };
    _example = "fcv_filter2d(3, \"u8\")";
    _returnType = "mat";
    _tags = {"fastcv", "filter", "convolution", "2d"};
}

ExecutionResult FastFilter2DItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize        = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 3;
    std::string depthStr = args.size() > 1 ? args[1].asString() : "u8";

    int ddepth = CV_8U;
    if (depthStr == "s16") ddepth = CV_16S;
    else if (depthStr == "f32") ddepth = CV_32F;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat kernel;
    if (ddepth == CV_32F) {
        kernel = cv::Mat::ones(ksize, ksize, CV_32F) / static_cast<float>(ksize * ksize);
    } else {
        kernel = cv::Mat::ones(ksize, ksize, CV_8S);
    }

    cv::Mat dst;
    cv::fastcv::filter2D(src, dst, ddepth, kernel);

    if (dst.depth() != CV_8U) {
        cv::normalize(dst, dst, 0, 255, cv::NORM_MINMAX);
        dst.convertTo(dst, CV_8U);
    }
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastMSERItem
// ============================================================================

FastMSERItem::FastMSERItem() {
    _functionName = "fcv_mser";
    _description = "MSER region detector accelerated by FastCV; draws colored contours and bounding boxes on output";
    _category = "fastcv";
    _params = {
        ParamDef::optional("min_area",        BaseType::INT,   "Minimum region area in pixels", 256),
        ParamDef::optional("max_area_ratio",  BaseType::FLOAT, "Maximum region area as fraction of image", 0.25),
        ParamDef::optional("delta",           BaseType::INT,   "MSER delta parameter", 2)
    };
    _example = "fcv_mser(256, 0.25, 2)";
    _returnType = "mat";
    _tags = {"fastcv", "mser", "region", "detect", "feature"};
}

ExecutionResult FastMSERItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int minArea        = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 256;
    float maxAreaRatio = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.25f;
    uint32_t delta     = args.size() > 2 ? static_cast<uint32_t>(args[2].asNumber()) : 2u;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    uint32_t maxArea = static_cast<uint32_t>(src.total() * maxAreaRatio);
    auto mser = cv::fastcv::FCVMSER::create(src.size(), 4, delta,
                    static_cast<uint32_t>(minArea), maxArea, 0.15f, 0.2f);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Rect> bboxes;
    mser->detect(src, contours, bboxes);

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);

    for (size_t i = 0; i < contours.size(); i++) {
        cv::Scalar color(rand() % 200 + 55, rand() % 200 + 55, rand() % 200 + 55);
        for (const auto& pt : contours[i]) {
            output.at<cv::Vec3b>(pt.y, pt.x) = cv::Vec3b(
                static_cast<uint8_t>(color[0]),
                static_cast<uint8_t>(color[1]),
                static_cast<uint8_t>(color[2]));
        }
        if (!bboxes.empty()) cv::rectangle(output, bboxes[i], cv::Scalar(0, 255, 0), 1);
    }
    return ExecutionResult::ok(output);
}

// ============================================================================
// FastWarpPerspectiveItem
// ============================================================================

FastWarpPerspectiveItem::FastWarpPerspectiveItem() {
    _functionName = "fcv_warp_perspective";
    _description = "Perspective warp accelerated by FastCV; 3x3 CV_32F matrix loaded from cache";
    _category = "fastcv";
    _params = {
        ParamDef::required("matrix_cache", BaseType::STRING, "Cache ID holding the 3x3 CV_32F perspective matrix"),
        ParamDef::optional("width",  BaseType::INT, "Output width  (0 = same as input)", 0),
        ParamDef::optional("height", BaseType::INT, "Output height (0 = same as input)", 0)
    };
    _example = "fcv_warp_perspective(\"M\", 640, 480)";
    _returnType = "mat";
    _tags = {"fastcv", "warp", "perspective", "transform"};
}

ExecutionResult FastWarpPerspectiveItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string matId = args[0].asString();
    int w = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    int h = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;

    if (!ctx.cacheManager) return ExecutionResult::fail("No cache manager available");

    cv::Mat M = ctx.cacheManager->get(matId);
    if (M.empty()) return ExecutionResult::fail("Perspective matrix not found in cache: " + matId);
    if (M.type() != CV_32F) M.convertTo(M, CV_32F);

    cv::Size dsize = (w > 0 && h > 0) ? cv::Size(w, h) : ctx.currentMat.size();
    cv::Mat dst;
    cv::fastcv::warpPerspective(ctx.currentMat, dst, M, dsize,
                                cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastWarpAffineItem
// ============================================================================

FastWarpAffineItem::FastWarpAffineItem() {
    _functionName = "fcv_warp_affine";
    _description = "Affine warp accelerated by FastCV; 2x3 CV_32F matrix loaded from cache";
    _category = "fastcv";
    _params = {
        ParamDef::required("matrix_cache", BaseType::STRING, "Cache ID holding the 2x3 CV_32F affine matrix"),
        ParamDef::optional("width",  BaseType::INT, "Output width  (0 = same as input)", 0),
        ParamDef::optional("height", BaseType::INT, "Output height (0 = same as input)", 0)
    };
    _example = "fcv_warp_affine(\"A\", 640, 480)";
    _returnType = "mat";
    _tags = {"fastcv", "warp", "affine", "transform"};
}

ExecutionResult FastWarpAffineItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string matId = args[0].asString();
    int w = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    int h = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;

    if (!ctx.cacheManager) return ExecutionResult::fail("No cache manager available");

    cv::Mat M = ctx.cacheManager->get(matId);
    if (M.empty()) return ExecutionResult::fail("Affine matrix not found in cache: " + matId);
    if (M.type() != CV_32F) M.convertTo(M, CV_32F);

    cv::Size dsize = (w > 0 && h > 0) ? cv::Size(w, h) : ctx.currentMat.size();
    cv::Mat dst;
    cv::fastcv::warpAffine(ctx.currentMat, dst, M, dsize);
    return ExecutionResult::ok(dst);
}

// ============================================================================
// FastMeanShiftItem
// ============================================================================

FastMeanShiftItem::FastMeanShiftItem() {
    _functionName = "fcv_mean_shift";
    _description = "Mean shift tracking accelerated by FastCV; ROI rect [x,y,w,h] stored as 1x4 CV_32F Mat in cache";
    _category = "fastcv";
    _params = {
        ParamDef::required("track_cache", BaseType::STRING, "Cache ID holding ROI rect (1x4 CV_32F [x,y,w,h])"),
        ParamDef::optional("max_iter", BaseType::INT,   "Maximum iterations", 10),
        ParamDef::optional("epsilon",  BaseType::FLOAT, "Convergence epsilon in pixels", 1.0)
    };
    _example = "fcv_mean_shift(\"roi\", 10, 1.0)";
    _returnType = "mat";
    _tags = {"fastcv", "mean-shift", "tracking"};
}

ExecutionResult FastMeanShiftItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    int maxIter  = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 10;
    double eps   = args.size() > 2 ? args[2].asNumber() : 1.0;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Rect rect;
    if (ctx.cacheManager) {
        cv::Mat roiMat = ctx.cacheManager->get(cacheId);
        if (!roiMat.empty() && roiMat.total() >= 4) {
            float* p = roiMat.ptr<float>();
            rect = cv::Rect((int)p[0], (int)p[1], (int)p[2], (int)p[3]);
        }
    }
    if (rect.empty()) {
        rect = cv::Rect(src.cols/4, src.rows/4, src.cols/2, src.rows/2);
    }
    rect &= cv::Rect(0, 0, src.cols, src.rows);

    cv::TermCriteria termCrit(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, maxIter, eps);
    cv::fastcv::meanShift(src, rect, termCrit);

    if (ctx.cacheManager) {
        cv::Mat roiMat(1, 4, CV_32F);
        float* p = roiMat.ptr<float>();
        p[0] = (float)rect.x; p[1] = (float)rect.y;
        p[2] = (float)rect.width; p[3] = (float)rect.height;
        ctx.cacheManager->set(cacheId, roiMat, false);
    }

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    cv::rectangle(output, rect, cv::Scalar(0, 255, 255), 2);
    return ExecutionResult::ok(output);
}

// ============================================================================
// FastTrackOpticalFlowLKItem
// ============================================================================

FastTrackOpticalFlowLKItem::FastTrackOpticalFlowLKItem() {
    _functionName = "fcv_track_lk";
    _description = "Lucas-Kanade sparse optical flow accelerated by FastCV; "
                   "tracks FAST10 corners between consecutive frames; re-detects corners when all are lost";
    _category = "fastcv";
    _params = {
        ParamDef::optional("max_corners", BaseType::INT, "Maximum number of corners to track", 200),
        ParamDef::optional("win_size",    BaseType::INT, "LK search window size (odd number, e.g. 7)", 7),
        ParamDef::optional("levels",      BaseType::INT, "Number of pyramid levels", 3)
    };
    _example = "fcv_track_lk(200, 7, 3)";
    _returnType = "mat";
    _tags = {"fastcv", "optical-flow", "tracking", "lk", "lucas-kanade"};
}

ExecutionResult FastTrackOpticalFlowLKItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int maxCorners = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 200;
    int winSz      = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 7;
    int pyrLevels  = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 3;

    if (winSz % 2 == 0) winSz++;

    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);

    auto detectCorners = [&](const cv::Mat& img, int limit) {
        std::vector<cv::Point2f> pts;
        std::vector<int> coords, scores;
        cv::fastcv::FAST10(img, cv::noArray(), coords, scores, 10, 4, true);
        for (size_t i = 0; i < coords.size() / 2 && (int)pts.size() < limit; i++) {
            pts.push_back({(float)coords[2*i], (float)coords[2*i+1]});
        }
        return pts;
    };

    if (_prevFrame.empty()) {
        _prevFrame = gray.clone();
        _prevPts   = detectCorners(gray, maxCorners);
        return ExecutionResult::ok(output);
    }

    if (_prevPts.empty()) {
        _prevFrame = gray.clone();
        _prevPts   = detectCorners(gray, maxCorners);
        return ExecutionResult::ok(output);
    }

    std::vector<cv::Mat> prevPyr, currPyr;
    cv::fastcv::buildPyramid(_prevFrame, prevPyr, pyrLevels, true);
    cv::fastcv::buildPyramid(gray, currPyr, pyrLevels, true);

    cv::Mat ptsInMat((int)_prevPts.size(), 1, CV_32FC2, _prevPts.data());
    cv::Mat ptsOutMat, statusMat;
    cv::TermCriteria termCrit(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, 7, 0.03f * 0.03f);

    cv::fastcv::trackOpticalFlowLK(
        _prevFrame, gray, prevPyr, currPyr,
        ptsInMat, ptsOutMat, cv::noArray(), statusMat,
        cv::Size(winSz, winSz), termCrit);

    std::vector<cv::Point2f> nextPts;
    if (!ptsOutMat.empty()) {
        for (int i = 0; i < ptsOutMat.rows; i++) {
            int status = statusMat.empty() ? 1 : statusMat.at<int>(i);
            if (status == 0) continue;
            cv::Point2f prev = _prevPts[i];
            cv::Point2f next = ptsOutMat.at<cv::Point2f>(i);
            cv::line(output, prev, next, cv::Scalar(0, 255, 0), 1);
            cv::circle(output, next, 3, cv::Scalar(0, 0, 255), -1);
            nextPts.push_back(next);
        }
    }

    _prevFrame = gray.clone();
    _prevPts   = nextPts.empty() ? detectCorners(gray, maxCorners) : nextPts;
    return ExecutionResult::ok(output);
}

// ============================================================================
// FastCalcHistItem
// ============================================================================

FastCalcHistItem::FastCalcHistItem() {
    _functionName = "fcv_calc_hist";
    _description = "Grayscale histogram accelerated by FastCV; returns a 256-wide histogram bar chart";
    _category = "fastcv";
    _params = {
        ParamDef::optional("height", BaseType::INT, "Height of the histogram visualization in pixels", 128)
    };
    _example = "fcv_calc_hist(128)";
    _returnType = "mat";
    _tags = {"fastcv", "histogram", "statistics"};
}

ExecutionResult FastCalcHistItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int visHeight = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 128;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat hist;
    cv::fastcv::calcHist(src, hist);  // 1x256 CV_32S

    cv::Mat vis(visHeight, 256, CV_8UC3, cv::Scalar(0, 0, 0));
    double maxVal = 0;
    cv::minMaxLoc(hist, nullptr, &maxVal);
    if (maxVal > 0) {
        for (int i = 0; i < 256; i++) {
            int binH = static_cast<int>(hist.at<int32_t>(0, i) / maxVal * visHeight);
            cv::line(vis, {i, visHeight}, {i, visHeight - binH}, cv::Scalar(255, 255, 255));
        }
    }
    return ExecutionResult::ok(vis);
}

// ============================================================================
// FastNormalizeLocalBoxItem
// ============================================================================

FastNormalizeLocalBoxItem::FastNormalizeLocalBoxItem() {
    _functionName = "fcv_normalize_local_box";
    _description = "Local box contrast normalization accelerated by FastCV (8U grayscale input)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("box_width",  BaseType::INT,  "Local box width", 5),
        ParamDef::optional("box_height", BaseType::INT,  "Local box height", 5),
        ParamDef::optional("use_stddev", BaseType::BOOL, "Normalize by std deviation (true) or mean (false)", true)
    };
    _example = "fcv_normalize_local_box(5, 5, true)";
    _returnType = "mat";
    _tags = {"fastcv", "normalize", "local", "contrast"};
}

ExecutionResult FastNormalizeLocalBoxItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int bw      = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    int bh      = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 5;
    bool useStd = args.size() > 2 ? args[2].asBool() : true;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);

    cv::Mat dst;
    cv::fastcv::normalizeLocalBox(src, dst, cv::Size(bw, bh), useStd);

    if (dst.depth() != CV_8U) {
        cv::normalize(dst, dst, 0, 255, cv::NORM_MINMAX);
        dst.convertTo(dst, CV_8U);
    }
    return ExecutionResult::ok(dst);
}

} // namespace visionpipe

#endif // VISIONPIPE_FASTCV_ENABLED

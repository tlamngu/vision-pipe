#include "interpreter/builtin_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace visionpipe {

// ============================================================================
// Registration
// ============================================================================

void registerBuiltinItems(ItemRegistry& registry) {
    // Video I/O
    registry.add<VideoCaptureItem>();
    registry.add<VideoWriteItem>();
    registry.add<SaveImageItem>();
    registry.add<LoadImageItem>();
    
    // Image Processing
    registry.add<ResizeItem>();
    registry.add<ColorConvertItem>();
    registry.add<GaussianBlurItem>();
    registry.add<MedianBlurItem>();
    registry.add<BilateralFilterItem>();
    registry.add<CannyEdgeItem>();
    registry.add<ThresholdItem>();
    registry.add<AdaptiveThresholdItem>();
    registry.add<MorphologyItem>();
    registry.add<EqualizeHistItem>();
    
    // Stereo Vision
    registry.add<UndistortItem>();
    registry.add<StereoSGBMItem>();
    registry.add<StereoBMItem>();
    registry.add<ApplyColormapItem>();
    
    // Display & Debug
    registry.add<DisplayItem>();
    registry.add<WaitKeyItem>();
    registry.add<PrintItem>();
    registry.add<DebugMatItem>();
    
    // Cache & Control
    registry.add<UseItem>();
    registry.add<CacheItem>();
    registry.add<PassItem>();
    registry.add<SleepItem>();
    
    // Drawing
    registry.add<DrawTextItem>();
    registry.add<DrawRectItem>();
    registry.add<DrawCircleItem>();
    registry.add<DrawLineItem>();
    
    // Frame Manipulation
    registry.add<HStackItem>();
    registry.add<VStackItem>();
    registry.add<CropItem>();
    registry.add<RotateItem>();
    registry.add<FlipItem>();
    
    // IPC
#ifdef VISIONPIPE_ICEORYX2_ENABLED
    registry.add<PublishFrameItem>();
    registry.add<SubscribeFrameItem>();
#endif

    // FastCV
#ifdef VISIONPIPE_FASTCV_ENABLED
    registry.add<FastGaussianBlurItem>();
    registry.add<FastCornerDetectItem>();
    registry.add<FastPyramidItem>();
#endif
}

// ============================================================================
// Video I/O Items
// ============================================================================

VideoCaptureItem::VideoCaptureItem() {
    _functionName = "video_cap";
    _description = "Captures video frames from a source (camera, file, or URL)";
    _category = "video_io";
    _params = {
        ParamDef::required("input_id", BaseType::ANY, 
            "Source identifier: device index (0, 1), file path, or URL")
    };
    _example = "video_cap(0)";
    _returnType = "mat";
    _tags = {"video", "capture", "input"};
}

ExecutionResult VideoCaptureItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId;
    
    if (args[0].isNumeric()) {
        sourceId = std::to_string(static_cast<int>(args[0].asNumber()));
    } else {
        sourceId = args[0].asString();
    }
    
    // Get or create capture
    if (_captures.find(sourceId) == _captures.end()) {
        auto cap = std::make_shared<cv::VideoCapture>();
        
        if (args[0].isNumeric()) {
            cap->open(static_cast<int>(args[0].asNumber()));
        } else {
            cap->open(args[0].asString());
        }
        
        if (!cap->isOpened()) {
            return ExecutionResult::fail("Failed to open video source: " + sourceId);
        }
        
        _captures[sourceId] = cap;
    }
    
    cv::Mat frame;
    if (!_captures[sourceId]->read(frame)) {
        return ExecutionResult::fail("Failed to read frame from: " + sourceId);
    }
    
    return ExecutionResult::ok(frame);
}

VideoWriteItem::VideoWriteItem() {
    _functionName = "video_write";
    _description = "Writes frames to a video file";
    _category = "video_io";
    _params = {
        ParamDef::required("filename", BaseType::STRING, "Output video file path"),
        ParamDef::optional("fps", BaseType::FLOAT, "Frames per second", 30.0),
        ParamDef::optional("fourcc", BaseType::STRING, "FourCC codec code", "MJPG")
    };
    _example = "video_write(\"output.avi\", 30, \"MJPG\")";
    _returnType = "mat";
    _tags = {"video", "output", "write"};
}

ExecutionResult VideoWriteItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string filename = args[0].asString();
    double fps = args.size() > 1 ? args[1].asNumber() : 30.0;
    std::string fourcc = args.size() > 2 ? args[2].asString() : "MJPG";
    
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("No frame to write");
    }
    
    // Get or create writer
    if (_writers.find(filename) == _writers.end()) {
        auto writer = std::make_shared<cv::VideoWriter>();
        
        int codecCode = cv::VideoWriter::fourcc(
            fourcc[0], fourcc[1], fourcc[2], fourcc[3]
        );
        
        writer->open(filename, codecCode, fps, ctx.currentMat.size(), 
                    ctx.currentMat.channels() == 3);
        
        if (!writer->isOpened()) {
            return ExecutionResult::fail("Failed to open video writer: " + filename);
        }
        
        _writers[filename] = writer;
    }
    
    _writers[filename]->write(ctx.currentMat);
    return ExecutionResult::ok(ctx.currentMat);
}

SaveImageItem::SaveImageItem() {
    _functionName = "save_image";
    _description = "Saves the current frame to an image file";
    _category = "video_io";
    _params = {
        ParamDef::required("filename", BaseType::STRING, "Output image file path")
    };
    _example = "save_image(\"frame.png\")";
    _returnType = "mat";
    _tags = {"image", "output", "save"};
}

ExecutionResult SaveImageItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string filename = args[0].asString();
    
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("No frame to save");
    }
    
    if (!cv::imwrite(filename, ctx.currentMat)) {
        return ExecutionResult::fail("Failed to save image: " + filename);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

LoadImageItem::LoadImageItem() {
    _functionName = "load_image";
    _description = "Loads an image from a file";
    _category = "video_io";
    _params = {
        ParamDef::required("filename", BaseType::STRING, "Input image file path"),
        ParamDef::optional("flags", BaseType::INT, "imread flags (-1=unchanged, 0=grayscale, 1=color)", 1)
    };
    _example = "load_image(\"input.png\")";
    _returnType = "mat";
    _tags = {"image", "input", "load"};
}

ExecutionResult LoadImageItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string filename = args[0].asString();
    int flags = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : cv::IMREAD_COLOR;
    
    cv::Mat image = cv::imread(filename, flags);
    if (image.empty()) {
        return ExecutionResult::fail("Failed to load image: " + filename);
    }
    
    return ExecutionResult::ok(image);
}

// ============================================================================
// Image Processing Items
// ============================================================================

ResizeItem::ResizeItem() {
    _functionName = "resize";
    _description = "Resizes the current frame";
    _category = "image_processing";
    _params = {
        ParamDef::required("width", BaseType::INT, "Target width in pixels"),
        ParamDef::required("height", BaseType::INT, "Target height in pixels"),
        ParamDef::optional("interpolation", BaseType::STRING, 
            "Interpolation method (nearest, linear, cubic, lanczos)", "linear")
    };
    _example = "resize(640, 480)";
    _returnType = "mat";
    _tags = {"resize", "transform"};
}

ExecutionResult ResizeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int width = static_cast<int>(args[0].asNumber());
    int height = static_cast<int>(args[1].asNumber());
    
    int interp = cv::INTER_LINEAR;
    if (args.size() > 2) {
        std::string method = args[2].asString();
        if (method == "nearest") interp = cv::INTER_NEAREST;
        else if (method == "cubic") interp = cv::INTER_CUBIC;
        else if (method == "lanczos") interp = cv::INTER_LANCZOS4;
    }
    
    cv::Mat resized;
    cv::resize(ctx.currentMat, resized, cv::Size(width, height), 0, 0, interp);
    
    return ExecutionResult::ok(resized);
}

ColorConvertItem::ColorConvertItem() {
    _functionName = "cvt_color";
    _description = "Converts the color space of the current frame";
    _category = "image_processing";
    _params = {
        ParamDef::required("code", BaseType::STRING, 
            "Conversion code: bgr2gray, bgr2rgb, gray2bgr, bgr2hsv, hsv2bgr, etc.")
    };
    _example = "cvt_color(\"bgr2gray\")";
    _returnType = "mat";
    _tags = {"color", "convert"};
}

ExecutionResult ColorConvertItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string code = args[0].asString();
    
    // Map string to cv::ColorConversionCodes
    int cvCode = -1;
    if (code == "bgr2gray") cvCode = cv::COLOR_BGR2GRAY;
    else if (code == "gray2bgr") cvCode = cv::COLOR_GRAY2BGR;
    else if (code == "bgr2rgb") cvCode = cv::COLOR_BGR2RGB;
    else if (code == "rgb2bgr") cvCode = cv::COLOR_RGB2BGR;
    else if (code == "bgr2hsv") cvCode = cv::COLOR_BGR2HSV;
    else if (code == "hsv2bgr") cvCode = cv::COLOR_HSV2BGR;
    else if (code == "bgr2lab") cvCode = cv::COLOR_BGR2Lab;
    else if (code == "lab2bgr") cvCode = cv::COLOR_Lab2BGR;
    else if (code == "bgr2yuv") cvCode = cv::COLOR_BGR2YUV;
    else if (code == "yuv2bgr") cvCode = cv::COLOR_YUV2BGR;
    else {
        return ExecutionResult::fail("Unknown color conversion code: " + code);
    }
    
    cv::Mat converted;
    cv::cvtColor(ctx.currentMat, converted, cvCode);
    
    return ExecutionResult::ok(converted);
}

GaussianBlurItem::GaussianBlurItem() {
    _functionName = "gaussian_blur";
    _description = "Applies Gaussian blur to the current frame";
    _category = "image_processing";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT, "Blur kernel size (odd number)", 5),
        ParamDef::optional("sigma", BaseType::FLOAT, "Gaussian sigma (0 = auto)", 0.0)
    };
    _example = "gaussian_blur(5)";
    _returnType = "mat";
    _tags = {"blur", "filter", "smooth"};
}

ExecutionResult GaussianBlurItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    double sigma = args.size() > 1 ? args[1].asNumber() : 0.0;
    
    if (ksize % 2 == 0) ksize++;  // Ensure odd
    
    cv::Mat blurred;
    cv::GaussianBlur(ctx.currentMat, blurred, cv::Size(ksize, ksize), sigma);
    
    return ExecutionResult::ok(blurred);
}

MedianBlurItem::MedianBlurItem() {
    _functionName = "median_blur";
    _description = "Applies median blur (good for salt-and-pepper noise)";
    _category = "image_processing";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT, "Blur kernel size (odd number)", 5)
    };
    _example = "median_blur(5)";
    _returnType = "mat";
    _tags = {"blur", "filter", "denoise"};
}

ExecutionResult MedianBlurItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    if (ksize % 2 == 0) ksize++;
    
    cv::Mat blurred;
    cv::medianBlur(ctx.currentMat, blurred, ksize);
    
    return ExecutionResult::ok(blurred);
}

BilateralFilterItem::BilateralFilterItem() {
    _functionName = "bilateral_filter";
    _description = "Applies bilateral filter (edge-preserving smoothing)";
    _category = "image_processing";
    _params = {
        ParamDef::optional("d", BaseType::INT, "Diameter of pixel neighborhood", 9),
        ParamDef::optional("sigma_color", BaseType::FLOAT, "Color sigma", 75.0),
        ParamDef::optional("sigma_space", BaseType::FLOAT, "Space sigma", 75.0)
    };
    _example = "bilateral_filter(9, 75, 75)";
    _returnType = "mat";
    _tags = {"blur", "filter", "edge-preserving"};
}

ExecutionResult BilateralFilterItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int d = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 9;
    double sigmaColor = args.size() > 1 ? args[1].asNumber() : 75.0;
    double sigmaSpace = args.size() > 2 ? args[2].asNumber() : 75.0;
    
    cv::Mat filtered;
    cv::bilateralFilter(ctx.currentMat, filtered, d, sigmaColor, sigmaSpace);
    
    return ExecutionResult::ok(filtered);
}

CannyEdgeItem::CannyEdgeItem() {
    _functionName = "canny";
    _description = "Detects edges using Canny algorithm";
    _category = "image_processing";
    _params = {
        ParamDef::required("threshold1", BaseType::FLOAT, "Lower threshold"),
        ParamDef::required("threshold2", BaseType::FLOAT, "Upper threshold")
    };
    _example = "canny(50, 150)";
    _returnType = "mat";
    _tags = {"edge", "detect", "canny"};
}

ExecutionResult CannyEdgeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double thresh1 = args[0].asNumber();
    double thresh2 = args[1].asNumber();
    
    cv::Mat edges;
    cv::Canny(ctx.currentMat, edges, thresh1, thresh2);
    
    return ExecutionResult::ok(edges);
}

ThresholdItem::ThresholdItem() {
    _functionName = "threshold";
    _description = "Applies threshold to the image";
    _category = "image_processing";
    _params = {
        ParamDef::required("thresh", BaseType::FLOAT, "Threshold value"),
        ParamDef::optional("max_val", BaseType::FLOAT, "Maximum value", 255.0),
        ParamDef::optional("type", BaseType::STRING, 
            "Threshold type: binary, binary_inv, trunc, tozero, tozero_inv", "binary")
    };
    _example = "threshold(127, 255, \"binary\")";
    _returnType = "mat";
    _tags = {"threshold", "binary"};
}

ExecutionResult ThresholdItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double thresh = args[0].asNumber();
    double maxVal = args.size() > 1 ? args[1].asNumber() : 255.0;
    
    int threshType = cv::THRESH_BINARY;
    if (args.size() > 2) {
        std::string type = args[2].asString();
        if (type == "binary_inv") threshType = cv::THRESH_BINARY_INV;
        else if (type == "trunc") threshType = cv::THRESH_TRUNC;
        else if (type == "tozero") threshType = cv::THRESH_TOZERO;
        else if (type == "tozero_inv") threshType = cv::THRESH_TOZERO_INV;
    }
    
    cv::Mat thresholded;
    cv::threshold(ctx.currentMat, thresholded, thresh, maxVal, threshType);
    
    return ExecutionResult::ok(thresholded);
}

AdaptiveThresholdItem::AdaptiveThresholdItem() {
    _functionName = "adaptive_threshold";
    _description = "Applies adaptive threshold";
    _category = "image_processing";
    _params = {
        ParamDef::optional("max_val", BaseType::FLOAT, "Maximum value", 255.0),
        ParamDef::optional("block_size", BaseType::INT, "Block size (odd)", 11),
        ParamDef::optional("c", BaseType::FLOAT, "Constant subtracted from mean", 2.0)
    };
    _example = "adaptive_threshold(255, 11, 2)";
    _returnType = "mat";
    _tags = {"threshold", "adaptive"};
}

ExecutionResult AdaptiveThresholdItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double maxVal = args.size() > 0 ? args[0].asNumber() : 255.0;
    int blockSize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 11;
    double C = args.size() > 2 ? args[2].asNumber() : 2.0;
    
    if (blockSize % 2 == 0) blockSize++;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat thresholded;
    cv::adaptiveThreshold(input, thresholded, maxVal, 
        cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, blockSize, C);
    
    return ExecutionResult::ok(thresholded);
}

MorphologyItem::MorphologyItem() {
    _functionName = "morphology";
    _description = "Applies morphological operations";
    _category = "image_processing";
    _params = {
        ParamDef::required("op", BaseType::STRING, 
            "Operation: erode, dilate, open, close, gradient, tophat, blackhat"),
        ParamDef::optional("kernel_size", BaseType::INT, "Kernel size", 3),
        ParamDef::optional("iterations", BaseType::INT, "Number of iterations", 1)
    };
    _example = "morphology(\"dilate\", 3, 1)";
    _returnType = "mat";
    _tags = {"morphology", "erode", "dilate"};
}

ExecutionResult MorphologyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string op = args[0].asString();
    int ksize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    int iterations = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 1;
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(ksize, ksize));
    cv::Mat result;
    
    if (op == "erode") {
        cv::erode(ctx.currentMat, result, kernel, cv::Point(-1,-1), iterations);
    } else if (op == "dilate") {
        cv::dilate(ctx.currentMat, result, kernel, cv::Point(-1,-1), iterations);
    } else if (op == "open") {
        cv::morphologyEx(ctx.currentMat, result, cv::MORPH_OPEN, kernel, cv::Point(-1,-1), iterations);
    } else if (op == "close") {
        cv::morphologyEx(ctx.currentMat, result, cv::MORPH_CLOSE, kernel, cv::Point(-1,-1), iterations);
    } else if (op == "gradient") {
        cv::morphologyEx(ctx.currentMat, result, cv::MORPH_GRADIENT, kernel, cv::Point(-1,-1), iterations);
    } else if (op == "tophat") {
        cv::morphologyEx(ctx.currentMat, result, cv::MORPH_TOPHAT, kernel, cv::Point(-1,-1), iterations);
    } else if (op == "blackhat") {
        cv::morphologyEx(ctx.currentMat, result, cv::MORPH_BLACKHAT, kernel, cv::Point(-1,-1), iterations);
    } else {
        return ExecutionResult::fail("Unknown morphology operation: " + op);
    }
    
    return ExecutionResult::ok(result);
}

EqualizeHistItem::EqualizeHistItem() {
    _functionName = "equalize_hist";
    _description = "Applies histogram equalization";
    _category = "image_processing";
    _params = {};
    _example = "equalize_hist()";
    _returnType = "mat";
    _tags = {"histogram", "equalize", "contrast"};
}

ExecutionResult EqualizeHistItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat equalized;
    cv::equalizeHist(input, equalized);
    
    return ExecutionResult::ok(equalized);
}

// ============================================================================
// Stereo Vision Items
// ============================================================================

UndistortItem::UndistortItem() {
    _functionName = "undistort";
    _description = "Undistorts/rectifies an image using calibration parameters";
    _category = "stereo";
    _params = {
        ParamDef::required("calib_file", BaseType::STRING, 
            "Path to calibration file (YAML/XML with camera_matrix, dist_coeffs)")
    };
    _example = "undistort(\"camera_calib.yaml\")";
    _returnType = "mat";
    _tags = {"undistort", "rectify", "calibration", "stereo"};
}

ExecutionResult UndistortItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string calibFile = args[0].asString();
    
    // Load or get cached rectification maps
    if (_rectifyMaps.find(calibFile) == _rectifyMaps.end()) {
        cv::FileStorage fs(calibFile, cv::FileStorage::READ);
        if (!fs.isOpened()) {
            return ExecutionResult::fail("Cannot open calibration file: " + calibFile);
        }
        
        cv::Mat cameraMatrix, distCoeffs;
        fs["camera_matrix"] >> cameraMatrix;
        fs["distortion_coefficients"] >> distCoeffs;
        
        if (cameraMatrix.empty() || distCoeffs.empty()) {
            // Try alternative names
            fs["cameraMatrix"] >> cameraMatrix;
            fs["distCoeffs"] >> distCoeffs;
        }
        
        if (cameraMatrix.empty()) {
            return ExecutionResult::fail("Cannot read camera matrix from: " + calibFile);
        }
        
        // Initialize rectification maps
        cv::Mat map1, map2;
        cv::Size imageSize = ctx.currentMat.size();
        
        cv::initUndistortRectifyMap(
            cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix,
            imageSize, CV_16SC2, map1, map2
        );
        
        _rectifyMaps[calibFile] = std::make_pair(map1, map2);
    }
    
    const auto& maps = _rectifyMaps[calibFile];
    cv::Mat rectified;
    cv::remap(ctx.currentMat, rectified, maps.first, maps.second, cv::INTER_LINEAR);
    
    return ExecutionResult::ok(rectified);
}

StereoSGBMItem::StereoSGBMItem() {
    _functionName = "stereo_sgbm";
    _description = "Computes stereo disparity using Semi-Global Block Matching";
    _category = "stereo";
    _params = {
        ParamDef::required("left_cache", BaseType::STRING, "Cache ID of left image"),
        ParamDef::required("right_cache", BaseType::STRING, "Cache ID of right image"),
        ParamDef::optional("num_disparities", BaseType::INT, "Number of disparities (multiple of 16)", 96),
        ParamDef::optional("block_size", BaseType::INT, "Block size (odd, 1-11)", 5)
    };
    _example = "stereo_sgbm(\"left\", \"right\", 96, 5)";
    _returnType = "mat";
    _tags = {"stereo", "disparity", "sgbm"};
}

ExecutionResult StereoSGBMItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string leftCache = args[0].asString();
    std::string rightCache = args[1].asString();
    int numDisparities = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 96;
    int blockSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 5;
    
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager available");
    }
    
    cv::Mat left = ctx.cacheManager->get(leftCache);
    cv::Mat right = ctx.cacheManager->get(rightCache);
    
    if (left.empty() || right.empty()) {
        return ExecutionResult::fail("Left or right image not found in cache");
    }
    
    // Convert to grayscale if needed
    if (left.channels() > 1) cv::cvtColor(left, left, cv::COLOR_BGR2GRAY);
    if (right.channels() > 1) cv::cvtColor(right, right, cv::COLOR_BGR2GRAY);
    
    // Create/update SGBM
    if (!_sgbm) {
        _sgbm = cv::StereoSGBM::create(
            0, numDisparities, blockSize,
            8 * blockSize * blockSize,
            32 * blockSize * blockSize,
            1, 63, 10, 100, 32,
            cv::StereoSGBM::MODE_SGBM_3WAY
        );
    }
    
    cv::Mat disparity;
    _sgbm->compute(left, right, disparity);
    
    return ExecutionResult::ok(disparity);
}

StereoBMItem::StereoBMItem() {
    _functionName = "stereo_bm";
    _description = "Computes stereo disparity using Block Matching (faster, less accurate)";
    _category = "stereo";
    _params = {
        ParamDef::required("left_cache", BaseType::STRING, "Cache ID of left image"),
        ParamDef::required("right_cache", BaseType::STRING, "Cache ID of right image"),
        ParamDef::optional("num_disparities", BaseType::INT, "Number of disparities (multiple of 16)", 64),
        ParamDef::optional("block_size", BaseType::INT, "Block size (odd, 5+)", 15)
    };
    _example = "stereo_bm(\"left\", \"right\")";
    _returnType = "mat";
    _tags = {"stereo", "disparity", "bm"};
}

ExecutionResult StereoBMItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string leftCache = args[0].asString();
    std::string rightCache = args[1].asString();
    int numDisparities = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 64;
    int blockSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 15;
    
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager available");
    }
    
    cv::Mat left = ctx.cacheManager->get(leftCache);
    cv::Mat right = ctx.cacheManager->get(rightCache);
    
    if (left.empty() || right.empty()) {
        return ExecutionResult::fail("Left or right image not found in cache");
    }
    
    if (left.channels() > 1) cv::cvtColor(left, left, cv::COLOR_BGR2GRAY);
    if (right.channels() > 1) cv::cvtColor(right, right, cv::COLOR_BGR2GRAY);
    
    if (!_bm) {
        _bm = cv::StereoBM::create(numDisparities, blockSize);
    }
    
    cv::Mat disparity;
    _bm->compute(left, right, disparity);
    
    return ExecutionResult::ok(disparity);
}

ApplyColormapItem::ApplyColormapItem() {
    _functionName = "apply_colormap";
    _description = "Applies a colormap for visualization (e.g., for disparity maps)";
    _category = "stereo";
    _params = {
        ParamDef::optional("colormap", BaseType::STRING, 
            "Colormap: jet, hot, cool, rainbow, ocean, bone, hsv, turbo", "jet")
    };
    _example = "apply_colormap(\"jet\")";
    _returnType = "mat";
    _tags = {"colormap", "visualization", "stereo"};
}

ExecutionResult ApplyColormapItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string mapName = args.size() > 0 ? args[0].asString() : "jet";
    
    int colormap = cv::COLORMAP_JET;
    if (mapName == "hot") colormap = cv::COLORMAP_HOT;
    else if (mapName == "cool") colormap = cv::COLORMAP_COOL;
    else if (mapName == "rainbow") colormap = cv::COLORMAP_RAINBOW;
    else if (mapName == "ocean") colormap = cv::COLORMAP_OCEAN;
    else if (mapName == "bone") colormap = cv::COLORMAP_BONE;
    else if (mapName == "hsv") colormap = cv::COLORMAP_HSV;
    else if (mapName == "turbo") colormap = cv::COLORMAP_TURBO;
    
    cv::Mat input = ctx.currentMat;
    
    // Normalize to 8-bit if needed
    if (input.type() != CV_8U) {
        double minVal, maxVal;
        cv::minMaxLoc(input, &minVal, &maxVal);
        input.convertTo(input, CV_8U, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
    }
    
    cv::Mat colored;
    cv::applyColorMap(input, colored, colormap);
    
    return ExecutionResult::ok(colored);
}

// ============================================================================
// Display & Debug Items
// ============================================================================

DisplayItem::DisplayItem() {
    _functionName = "display";
    _description = "Displays the current frame in a window";
    _category = "display";
    _params = {
        ParamDef::optional("window_name", BaseType::STRING, "Window title", "VisionPipe")
    };
    _example = "display(\"Output\")";
    _returnType = "mat";
    _tags = {"display", "window", "show"};
}

ExecutionResult DisplayItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string windowName = args.size() > 0 ? args[0].asString() : "VisionPipe";
    
    if (!ctx.currentMat.empty()) {
        cv::imshow(windowName, ctx.currentMat);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

WaitKeyItem::WaitKeyItem() {
    _functionName = "wait_key";
    _description = "Waits for a key press";
    _category = "display";
    _params = {
        ParamDef::optional("delay", BaseType::INT, "Delay in ms (0 = wait forever)", 1)
    };
    _example = "wait_key(1)";
    _returnType = "int";
    _tags = {"wait", "key", "input"};
}

ExecutionResult WaitKeyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int delay = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 1;
    int key = cv::waitKey(delay);
    (void)key;
    return ExecutionResult::ok(ctx.currentMat);
}

PrintItem::PrintItem() {
    _functionName = "print";
    _description = "Prints a message to console";
    _category = "debug";
    _params = {
        ParamDef::required("message", BaseType::ANY, "Message to print")
    };
    _example = "print(\"Processing frame...\")";
    _returnType = "void";
    _tags = {"print", "debug", "log"};
}

ExecutionResult PrintItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::cout << args[0].asString() << std::endl;
    return ExecutionResult::ok(ctx.currentMat);
}

DebugMatItem::DebugMatItem() {
    _functionName = "debug_mat";
    _description = "Prints Mat properties for debugging";
    _category = "debug";
    _params = {
        ParamDef::optional("label", BaseType::STRING, "Label for output", "Mat")
    };
    _example = "debug_mat(\"current\")";
    _returnType = "mat";
    _tags = {"debug", "mat", "info"};
}

ExecutionResult DebugMatItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string label = args.size() > 0 ? args[0].asString() : "Mat";
    
    const cv::Mat& mat = ctx.currentMat;
    std::cout << label << ": "
              << mat.cols << "x" << mat.rows
              << " type=" << mat.type()
              << " channels=" << mat.channels()
              << " depth=" << mat.depth()
              << " continuous=" << mat.isContinuous()
              << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Cache & Control Items
// ============================================================================

UseItem::UseItem() {
    _functionName = "use";
    _description = "Loads a cached Mat as the current frame";
    _category = "cache";
    _params = {
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier")
    };
    _example = "use(\"rect_left\")";
    _returnType = "mat";
    _tags = {"cache", "load", "use"};
}

ExecutionResult UseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager available");
    }
    
    cv::Mat mat = ctx.cacheManager->get(cacheId);
    if (mat.empty()) {
        return ExecutionResult::fail("Cache entry not found: " + cacheId);
    }
    
    return ExecutionResult::ok(mat);
}

CacheItem::CacheItem() {
    _functionName = "cache";
    _description = "Caches the current Mat with an identifier";
    _category = "cache";
    _params = {
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier"),
        ParamDef::optional("global", BaseType::BOOL, "Store as global cache", false)
    };
    _example = "cache(\"processed\")";
    _returnType = "mat";
    _tags = {"cache", "store", "save"};
}

ExecutionResult CacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    bool isGlobal = args.size() > 1 ? args[1].asBool() : false;
    
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager available");
    }
    
    ctx.cacheManager->set(cacheId, ctx.currentMat, isGlobal);
    
    return ExecutionResult::ok(ctx.currentMat);
}

PassItem::PassItem() {
    _functionName = "pass";
    _description = "Pass-through operation (no-op)";
    _category = "control";
    _params = {};
    _example = "pass()";
    _returnType = "mat";
    _tags = {"pass", "noop"};
}

ExecutionResult PassItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    return ExecutionResult::ok(ctx.currentMat);
}

SleepItem::SleepItem() {
    _functionName = "sleep";
    _description = "Pauses execution for specified milliseconds";
    _category = "control";
    _params = {
        ParamDef::required("ms", BaseType::INT, "Milliseconds to sleep")
    };
    _example = "sleep(100)";
    _returnType = "void";
    _tags = {"sleep", "delay", "wait"};
}

ExecutionResult SleepItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int ms = static_cast<int>(args[0].asNumber());
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Drawing Items
// ============================================================================

DrawTextItem::DrawTextItem() {
    _functionName = "draw_text";
    _description = "Draws text on the image";
    _category = "drawing";
    _params = {
        ParamDef::required("text", BaseType::STRING, "Text to draw"),
        ParamDef::required("x", BaseType::INT, "X position"),
        ParamDef::required("y", BaseType::INT, "Y position"),
        ParamDef::optional("scale", BaseType::FLOAT, "Font scale", 1.0),
        ParamDef::optional("color", BaseType::STRING, "Color (white, red, green, blue, etc.)", "white")
    };
    _example = "draw_text(\"FPS: 30\", 10, 30)";
    _returnType = "mat";
    _tags = {"draw", "text", "annotate"};
}

ExecutionResult DrawTextItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string text = args[0].asString();
    int x = static_cast<int>(args[1].asNumber());
    int y = static_cast<int>(args[2].asNumber());
    double scale = args.size() > 3 ? args[3].asNumber() : 1.0;
    std::string colorName = args.size() > 4 ? args[4].asString() : "white";
    
    cv::Scalar color(255, 255, 255);
    if (colorName == "red") color = cv::Scalar(0, 0, 255);
    else if (colorName == "green") color = cv::Scalar(0, 255, 0);
    else if (colorName == "blue") color = cv::Scalar(255, 0, 0);
    else if (colorName == "yellow") color = cv::Scalar(0, 255, 255);
    else if (colorName == "black") color = cv::Scalar(0, 0, 0);
    
    cv::Mat output = ctx.currentMat.clone();
    cv::putText(output, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, scale, color, 1);
    
    return ExecutionResult::ok(output);
}

DrawRectItem::DrawRectItem() {
    _functionName = "draw_rect";
    _description = "Draws a rectangle on the image";
    _category = "drawing";
    _params = {
        ParamDef::required("x", BaseType::INT, "X position"),
        ParamDef::required("y", BaseType::INT, "Y position"),
        ParamDef::required("width", BaseType::INT, "Width"),
        ParamDef::required("height", BaseType::INT, "Height"),
        ParamDef::optional("color", BaseType::STRING, "Color name", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "draw_rect(100, 100, 50, 50, \"green\", 2)";
    _returnType = "mat";
    _tags = {"draw", "rectangle", "annotate"};
}

ExecutionResult DrawRectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int w = static_cast<int>(args[2].asNumber());
    int h = static_cast<int>(args[3].asNumber());
    std::string colorName = args.size() > 4 ? args[4].asString() : "green";
    int thickness = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorName == "red") color = cv::Scalar(0, 0, 255);
    else if (colorName == "blue") color = cv::Scalar(255, 0, 0);
    else if (colorName == "white") color = cv::Scalar(255, 255, 255);
    else if (colorName == "yellow") color = cv::Scalar(0, 255, 255);
    
    cv::Mat output = ctx.currentMat.clone();
    cv::rectangle(output, cv::Rect(x, y, w, h), color, thickness);
    
    return ExecutionResult::ok(output);
}

DrawCircleItem::DrawCircleItem() {
    _functionName = "draw_circle";
    _description = "Draws a circle on the image";
    _category = "drawing";
    _params = {
        ParamDef::required("x", BaseType::INT, "Center X"),
        ParamDef::required("y", BaseType::INT, "Center Y"),
        ParamDef::required("radius", BaseType::INT, "Radius"),
        ParamDef::optional("color", BaseType::STRING, "Color name", "red"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness (-1 for filled)", 2)
    };
    _example = "draw_circle(320, 240, 50, \"red\", 2)";
    _returnType = "mat";
    _tags = {"draw", "circle", "annotate"};
}

ExecutionResult DrawCircleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int radius = static_cast<int>(args[2].asNumber());
    std::string colorName = args.size() > 3 ? args[3].asString() : "red";
    int thickness = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 2;
    
    cv::Scalar color(0, 0, 255);
    if (colorName == "green") color = cv::Scalar(0, 255, 0);
    else if (colorName == "blue") color = cv::Scalar(255, 0, 0);
    else if (colorName == "white") color = cv::Scalar(255, 255, 255);
    
    cv::Mat output = ctx.currentMat.clone();
    cv::circle(output, cv::Point(x, y), radius, color, thickness);
    
    return ExecutionResult::ok(output);
}

DrawLineItem::DrawLineItem() {
    _functionName = "draw_line";
    _description = "Draws a line on the image";
    _category = "drawing";
    _params = {
        ParamDef::required("x1", BaseType::INT, "Start X"),
        ParamDef::required("y1", BaseType::INT, "Start Y"),
        ParamDef::required("x2", BaseType::INT, "End X"),
        ParamDef::required("y2", BaseType::INT, "End Y"),
        ParamDef::optional("color", BaseType::STRING, "Color name", "green"),
        ParamDef::optional("thickness", BaseType::INT, "Line thickness", 2)
    };
    _example = "draw_line(0, 0, 100, 100, \"green\", 2)";
    _returnType = "mat";
    _tags = {"draw", "line", "annotate"};
}

ExecutionResult DrawLineItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x1 = static_cast<int>(args[0].asNumber());
    int y1 = static_cast<int>(args[1].asNumber());
    int x2 = static_cast<int>(args[2].asNumber());
    int y2 = static_cast<int>(args[3].asNumber());
    std::string colorName = args.size() > 4 ? args[4].asString() : "green";
    int thickness = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 2;
    
    cv::Scalar color(0, 255, 0);
    if (colorName == "red") color = cv::Scalar(0, 0, 255);
    else if (colorName == "blue") color = cv::Scalar(255, 0, 0);
    else if (colorName == "white") color = cv::Scalar(255, 255, 255);
    
    cv::Mat output = ctx.currentMat.clone();
    cv::line(output, cv::Point(x1, y1), cv::Point(x2, y2), color, thickness);
    
    return ExecutionResult::ok(output);
}

// ============================================================================
// Frame Manipulation Items
// ============================================================================

HStackItem::HStackItem() {
    _functionName = "hstack";
    _description = "Horizontally stacks frames from cache";
    _category = "frame";
    _params = {
        ParamDef::required("left_cache", BaseType::STRING, "Left frame cache ID"),
        ParamDef::required("right_cache", BaseType::STRING, "Right frame cache ID")
    };
    _example = "hstack(\"left\", \"right\")";
    _returnType = "mat";
    _tags = {"stack", "concat", "horizontal"};
}

ExecutionResult HStackItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager");
    }
    
    cv::Mat left = ctx.cacheManager->get(args[0].asString());
    cv::Mat right = ctx.cacheManager->get(args[1].asString());
    
    if (left.empty() || right.empty()) {
        return ExecutionResult::fail("Could not load frames from cache");
    }
    
    // Ensure same type
    if (left.type() != right.type()) {
        if (left.channels() > right.channels()) {
            cv::cvtColor(right, right, cv::COLOR_GRAY2BGR);
        } else if (right.channels() > left.channels()) {
            cv::cvtColor(left, left, cv::COLOR_GRAY2BGR);
        }
    }
    
    // Ensure same height
    if (left.rows != right.rows) {
        double scale = static_cast<double>(left.rows) / right.rows;
        cv::resize(right, right, cv::Size(), scale, scale);
    }
    
    cv::Mat result;
    cv::hconcat(left, right, result);
    
    return ExecutionResult::ok(result);
}

VStackItem::VStackItem() {
    _functionName = "vstack";
    _description = "Vertically stacks frames from cache";
    _category = "frame";
    _params = {
        ParamDef::required("top_cache", BaseType::STRING, "Top frame cache ID"),
        ParamDef::required("bottom_cache", BaseType::STRING, "Bottom frame cache ID")
    };
    _example = "vstack(\"top\", \"bottom\")";
    _returnType = "mat";
    _tags = {"stack", "concat", "vertical"};
}

ExecutionResult VStackItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager");
    }
    
    cv::Mat top = ctx.cacheManager->get(args[0].asString());
    cv::Mat bottom = ctx.cacheManager->get(args[1].asString());
    
    if (top.empty() || bottom.empty()) {
        return ExecutionResult::fail("Could not load frames from cache");
    }
    
    // Ensure same type
    if (top.type() != bottom.type()) {
        if (top.channels() > bottom.channels()) {
            cv::cvtColor(bottom, bottom, cv::COLOR_GRAY2BGR);
        } else if (bottom.channels() > top.channels()) {
            cv::cvtColor(top, top, cv::COLOR_GRAY2BGR);
        }
    }
    
    // Ensure same width
    if (top.cols != bottom.cols) {
        double scale = static_cast<double>(top.cols) / bottom.cols;
        cv::resize(bottom, bottom, cv::Size(), scale, scale);
    }
    
    cv::Mat result;
    cv::vconcat(top, bottom, result);
    
    return ExecutionResult::ok(result);
}

CropItem::CropItem() {
    _functionName = "crop";
    _description = "Crops a region of interest";
    _category = "frame";
    _params = {
        ParamDef::required("x", BaseType::INT, "X position"),
        ParamDef::required("y", BaseType::INT, "Y position"),
        ParamDef::required("width", BaseType::INT, "Width"),
        ParamDef::required("height", BaseType::INT, "Height")
    };
    _example = "crop(100, 100, 200, 200)";
    _returnType = "mat";
    _tags = {"crop", "roi", "region"};
}

ExecutionResult CropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int w = static_cast<int>(args[2].asNumber());
    int h = static_cast<int>(args[3].asNumber());
    
    cv::Rect roi(x, y, w, h);
    roi &= cv::Rect(0, 0, ctx.currentMat.cols, ctx.currentMat.rows);
    
    cv::Mat cropped = ctx.currentMat(roi).clone();
    return ExecutionResult::ok(cropped);
}

RotateItem::RotateItem() {
    _functionName = "rotate";
    _description = "Rotates the image";
    _category = "frame";
    _params = {
        ParamDef::required("angle", BaseType::FLOAT, "Rotation angle in degrees"),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0)
    };
    _example = "rotate(90)";
    _returnType = "mat";
    _tags = {"rotate", "transform"};
}

ExecutionResult RotateItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double angle = args[0].asNumber();
    double scale = args.size() > 1 ? args[1].asNumber() : 1.0;
    
    cv::Point2f center(ctx.currentMat.cols / 2.0f, ctx.currentMat.rows / 2.0f);
    cv::Mat rot = cv::getRotationMatrix2D(center, angle, scale);
    
    cv::Mat rotated;
    cv::warpAffine(ctx.currentMat, rotated, rot, ctx.currentMat.size());
    
    return ExecutionResult::ok(rotated);
}

FlipItem::FlipItem() {
    _functionName = "flip";
    _description = "Flips the image";
    _category = "frame";
    _params = {
        ParamDef::required("mode", BaseType::STRING, "Flip mode: horizontal, vertical, both")
    };
    _example = "flip(\"horizontal\")";
    _returnType = "mat";
    _tags = {"flip", "mirror", "transform"};
}

ExecutionResult FlipItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string mode = args[0].asString();
    
    int flipCode = 1;  // horizontal
    if (mode == "vertical") flipCode = 0;
    else if (mode == "both") flipCode = -1;
    
    cv::Mat flipped;
    cv::flip(ctx.currentMat, flipped, flipCode);
    
    return ExecutionResult::ok(flipped);
}

} // namespace visionpipe

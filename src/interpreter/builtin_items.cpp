#include "interpreter/builtin_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef VISIONPIPE_FASTCV_ENABLED
#include <opencv2/fastcv.hpp>
#endif

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

// ============================================================================
// FastCV Items (Qualcomm FastCV accelerated operations)
// ============================================================================

#ifdef VISIONPIPE_FASTCV_ENABLED

FastGaussianBlurItem::FastGaussianBlurItem() {
    _functionName = "fcv_gaussian_blur";
    _description = "Gaussian blur accelerated by FastCV (supports 3x3 and 5x5 kernels, 8U/16S/32S)";
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
    int ksize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    bool blurBorder = args.size() > 1 ? args[1].asBool() : true;

    // FastCV supports kernel sizes 3 and 5
    if (ksize != 3 && ksize != 5) ksize = 5;

    cv::Mat dst;
    cv::fastcv::gaussianBlur(ctx.currentMat, dst, ksize, blurBorder);
    return ExecutionResult::ok(dst);
}

FastCornerDetectItem::FastCornerDetectItem() {
    _functionName = "fcv_fast10";
    _description = "FAST10 corner detection accelerated by FastCV";
    _category = "fastcv";
    _params = {
        ParamDef::optional("barrier", BaseType::INT, "Intensity barrier threshold for corner detection", 10),
        ParamDef::optional("border", BaseType::INT, "Border width to ignore around image edges", 4),
        ParamDef::optional("nms", BaseType::BOOL, "Enable non-maximum suppression", true)
    };
    _example = "fcv_fast10(10, 4, true)";
    _returnType = "mat";
    _tags = {"fastcv", "corner", "feature", "detect", "fast10"};
}

ExecutionResult FastCornerDetectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int barrier    = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 10;
    int border     = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 4;
    bool nmsEnabled = args.size() > 2 ? args[2].asBool() : true;

    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }

    std::vector<int> coords, scores;
    cv::fastcv::FAST10(gray, cv::noArray(), coords, scores, barrier, border, nmsEnabled);

    // Draw detected corners on color image
    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) {
        cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    }
    for (size_t i = 0; i < coords.size() / 2; i++) {
        cv::circle(output, cv::Point(coords[2*i], coords[2*i+1]), 3, cv::Scalar(0, 255, 0), -1);
    }

    return ExecutionResult::ok(output);
}

FastPyramidItem::FastPyramidItem() {
    _functionName = "fcv_pyramid";
    _description = "Image pyramid accelerated by FastCV; stores each level by name in cache";
    _category = "fastcv";
    _params = {
        ParamDef::optional("levels", BaseType::INT, "Number of pyramid levels", 4),
        ParamDef::optional("scale_by2", BaseType::BOOL, "Scale by 2 (true) or ORB factor (false)", true),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache key prefix for pyramid levels", "pyr")
    };
    _example = "fcv_pyramid(4, true, \"pyr\")";
    _returnType = "mat";
    _tags = {"fastcv", "pyramid", "scale", "multi-scale"};
}

ExecutionResult FastPyramidItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int nLevels   = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 4;
    bool scaleBy2 = args.size() > 1 ? args[1].asBool() : true;
    std::string prefix = args.size() > 2 ? args[2].asString() : "pyr";

    std::vector<cv::Mat> pyr;
    cv::fastcv::buildPyramid(ctx.currentMat, pyr, nLevels, scaleBy2);

    if (!pyr.empty() && ctx.cacheManager) {
        for (int i = 0; i < (int)pyr.size(); i++) {
            ctx.cacheManager->set(prefix + "_" + std::to_string(i), pyr[i], false);
        }
    }

    // Return level 0 (full-res approximation) as current frame
    cv::Mat out = pyr.empty() ? ctx.currentMat : pyr[0];
    return ExecutionResult::ok(out);
}

FastFFTItem::FastFFTItem() {
    _functionName = "fcv_fft";
    _description = "Fast Fourier Transform accelerated by FastCV (input: 8U grayscale)";
    _category = "fastcv";
    _params = {};
    _example = "fcv_fft()";
    _returnType = "mat";
    _tags = {"fastcv", "fft", "frequency", "transform"};
}

ExecutionResult FastFFTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Mat dst;
    cv::fastcv::FFT(src, dst);

    // Visualize magnitude spectrum (dst is 2-channel CV_32F)
    if (dst.channels() == 2) {
        std::vector<cv::Mat> planes;
        cv::split(dst, planes);
        cv::Mat mag;
        cv::magnitude(planes[0], planes[1], mag);
        mag += cv::Scalar::all(1);
        cv::log(mag, mag);
        cv::normalize(mag, mag, 0, 255, cv::NORM_MINMAX);
        mag.convertTo(mag, CV_8U);
        // Shift quadrants for visualization
        int cx = mag.cols / 2, cy = mag.rows / 2;
        cv::Mat q0(mag, cv::Rect(0, 0, cx, cy));
        cv::Mat q1(mag, cv::Rect(cx, 0, cx, cy));
        cv::Mat q2(mag, cv::Rect(0, cy, cx, cy));
        cv::Mat q3(mag, cv::Rect(cx, cy, cx, cy));
        cv::Mat tmp;
        q0.copyTo(tmp); q3.copyTo(q0); tmp.copyTo(q3);
        q1.copyTo(tmp); q2.copyTo(q1); tmp.copyTo(q2);
        return ExecutionResult::ok(mag);
    }

    return ExecutionResult::ok(dst);
}

FastIFFTItem::FastIFFTItem() {
    _functionName = "fcv_ifft";
    _description = "Inverse FFT accelerated by FastCV (input: 2-channel CV_32F from fcv_fft)";
    _category = "fastcv";
    _params = {};
    _example = "fcv_ifft()";
    _returnType = "mat";
    _tags = {"fastcv", "ifft", "frequency", "transform"};
}

ExecutionResult FastIFFTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    cv::Mat src = ctx.currentMat;

    // IFFT expects 2-channel float
    if (src.type() != CV_32FC2) {
        return ExecutionResult::fail("fcv_ifft expects a 2-channel CV_32F input (output of fcv_fft raw)");
    }

    cv::Mat dst;
    cv::fastcv::IFFT(src, dst);
    return ExecutionResult::ok(dst);
}

FastHoughLinesItem::FastHoughLinesItem() {
    _functionName = "fcv_hough_lines";
    _description = "Hough line detection accelerated by FastCV (input: grayscale/binary edge image)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("threshold", BaseType::FLOAT, "Normalized threshold [0,1] for line quality", 0.25),
        ParamDef::optional("draw_color", BaseType::STRING, "Line color (green, red, blue, white)", "green")
    };
    _example = "fcv_hough_lines(0.25, \"green\")";
    _returnType = "mat";
    _tags = {"fastcv", "hough", "lines", "detect"};
}

ExecutionResult FastHoughLinesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double threshold  = args.size() > 0 ? args[0].asNumber() : 0.25;
    std::string colorName = args.size() > 1 ? args[1].asString() : "green";

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    // Width must be aligned to 8
    int bpix = 0;
    if (src.cols % 8 != 0) {
        bpix = ((src.cols & 0xfffffff8) + 8) - src.cols;
        cv::copyMakeBorder(src, src, 0, 0, 0, bpix, cv::BORDER_REFLECT101);
    }

    std::vector<cv::Vec4f> lines;
    cv::fastcv::houghLines(src, lines, threshold);

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) {
        cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    }

    cv::Scalar color(0, 255, 0);
    if (colorName == "red") color = cv::Scalar(0, 0, 255);
    else if (colorName == "blue") color = cv::Scalar(255, 0, 0);
    else if (colorName == "white") color = cv::Scalar(255, 255, 255);

    for (const cv::Vec4f& l : lines) {
        cv::line(output, cv::Point((int)l[0], (int)l[1]), cv::Point((int)l[2], (int)l[3]), color, 2);
    }

    return ExecutionResult::ok(output);
}

FastMomentsItem::FastMomentsItem() {
    _functionName = "fcv_moments";
    _description = "Image moments accelerated by FastCV; prints moments to console";
    _category = "fastcv";
    _params = {
        ParamDef::optional("binary", BaseType::BOOL, "Treat image as binary", false)
    };
    _example = "fcv_moments(false)";
    _returnType = "mat";
    _tags = {"fastcv", "moments", "statistics"};
}

ExecutionResult FastMomentsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool binary = args.size() > 0 ? args[0].asBool() : false;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Moments m = cv::fastcv::moments(src, binary);
    std::cout << "FastCV Moments: m00=" << m.m00
              << " m10=" << m.m10 << " m01=" << m.m01
              << " cx=" << (m.m00 != 0.0 ? m.m10/m.m00 : 0.0)
              << " cy=" << (m.m00 != 0.0 ? m.m01/m.m00 : 0.0)
              << std::endl;

    return ExecutionResult::ok(ctx.currentMat);
}

FastThresholdRangeItem::FastThresholdRangeItem() {
    _functionName = "fcv_threshold_range";
    _description = "Range threshold: pixels in [low, high] -> trueValue, others -> falseValue";
    _category = "fastcv";
    _params = {
        ParamDef::required("low_thresh", BaseType::INT, "Lower threshold (inclusive)"),
        ParamDef::required("high_thresh", BaseType::INT, "Upper threshold (inclusive)"),
        ParamDef::optional("true_value", BaseType::INT, "Value for in-range pixels", 255),
        ParamDef::optional("false_value", BaseType::INT, "Value for out-of-range pixels", 0)
    };
    _example = "fcv_threshold_range(100, 200, 255, 0)";
    _returnType = "mat";
    _tags = {"fastcv", "threshold", "range", "binary"};
}

ExecutionResult FastThresholdRangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int lowThresh  = static_cast<int>(args[0].asNumber());
    int highThresh = static_cast<int>(args[1].asNumber());
    int trueValue  = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 255;
    int falseValue = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Mat dst;
    cv::fastcv::thresholdRange(src, dst, lowThresh, highThresh, trueValue, falseValue);
    return ExecutionResult::ok(dst);
}

FastBilateralFilterItem::FastBilateralFilterItem() {
    _functionName = "fcv_bilateral_filter";
    _description = "Bilateral filter accelerated by FastCV (kernel: 5, 7, or 9; input: 8U grayscale)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("d", BaseType::INT, "Kernel diameter (5, 7, or 9)", 5),
        ParamDef::optional("sigma_color", BaseType::FLOAT, "Color sigma", 50.0),
        ParamDef::optional("sigma_space", BaseType::FLOAT, "Space sigma", 1.0)
    };
    _example = "fcv_bilateral_filter(5, 50.0, 1.0)";
    _returnType = "mat";
    _tags = {"fastcv", "bilateral", "filter", "edge-preserving"};
}

ExecutionResult FastBilateralFilterItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int d             = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    float sigmaColor  = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 50.0f;
    float sigmaSpace  = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 1.0f;

    // FastCV supports d = 5, 7, 9
    if (d != 5 && d != 7 && d != 9) d = 5;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Mat dst;
    cv::fastcv::bilateralFilter(src, dst, d, sigmaColor, sigmaSpace);
    return ExecutionResult::ok(dst);
}

FastBilateralRecursiveItem::FastBilateralRecursiveItem() {
    _functionName = "fcv_bilateral_recursive";
    _description = "Recursive bilateral filter by FastCV (fast edge-preserving; 8U grayscale)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("sigma_color", BaseType::FLOAT, "Color sigma (0..1)", 0.03),
        ParamDef::optional("sigma_space", BaseType::FLOAT, "Space sigma (0..1)", 0.1)
    };
    _example = "fcv_bilateral_recursive(0.03, 0.1)";
    _returnType = "mat";
    _tags = {"fastcv", "bilateral", "recursive", "filter", "edge-preserving"};
}

ExecutionResult FastBilateralRecursiveItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float sigmaColor = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 0.03f;
    float sigmaSpace = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.1f;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Mat dst;
    cv::fastcv::bilateralRecursive(src, dst, sigmaColor, sigmaSpace);
    return ExecutionResult::ok(dst);
}

FastSobelItem::FastSobelItem() {
    _functionName = "fcv_sobel";
    _description = "Sobel edge detection accelerated by FastCV; dx cached as <cache_id>_dx, dy as <cache_id>_dy";
    _category = "fastcv";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT, "Kernel size (3, 5, or 7)", 3),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache prefix for dx/dy gradients", "sobel")
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
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Mat dx, dy;
    cv::fastcv::sobel(src, dx, dy, ksize, cv::BORDER_CONSTANT, 0);

    if (ctx.cacheManager) {
        ctx.cacheManager->set(id + "_dx", dx, false);
        ctx.cacheManager->set(id + "_dy", dy, false);
    }

    // Return magnitude as visualization
    cv::Mat fdx, fdy, mag;
    dx.convertTo(fdx, CV_32F);
    dy.convertTo(fdy, CV_32F);
    cv::magnitude(fdx, fdy, mag);
    cv::normalize(mag, mag, 0, 255, cv::NORM_MINMAX);
    mag.convertTo(mag, CV_8U);
    return ExecutionResult::ok(mag);
}

FastResizeDownItem::FastResizeDownItem() {
    _functionName = "fcv_resize_down";
    _description = "Optimized downscale resize by FastCV";
    _category = "fastcv";
    _params = {
        ParamDef::optional("width", BaseType::INT, "Target width (0 = use scale)", 0),
        ParamDef::optional("height", BaseType::INT, "Target height (0 = use scale)", 0),
        ParamDef::optional("inv_scale_x", BaseType::FLOAT, "Horizontal inverse scale (e.g., 0.5 = half)", 0.5),
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

FastFilter2DItem::FastFilter2DItem() {
    _functionName = "fcv_filter2d";
    _description = "2D convolution filter accelerated by FastCV";
    _category = "fastcv";
    _params = {
        ParamDef::optional("kernel_size", BaseType::INT, "Kernel size (3,5,7,9,11)", 3),
        ParamDef::optional("ddepth", BaseType::STRING, "Output depth: u8, s16, f32", "u8")
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
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    // Build a box kernel of type matching ddepth
    cv::Mat kernel;
    if (ddepth == CV_32F) {
        kernel = cv::Mat::ones(ksize, ksize, CV_32F) / static_cast<float>(ksize * ksize);
    } else {
        kernel = cv::Mat::ones(ksize, ksize, CV_8S);
    }

    cv::Mat dst;
    cv::fastcv::filter2D(src, dst, ddepth, kernel);

    // Normalize for display if needed
    if (dst.depth() != CV_8U) {
        cv::normalize(dst, dst, 0, 255, cv::NORM_MINMAX);
        dst.convertTo(dst, CV_8U);
    }
    return ExecutionResult::ok(dst);
}

FastMSERItem::FastMSERItem() {
    _functionName = "fcv_mser";
    _description = "MSER region detection accelerated by FastCV; draws contours on output";
    _category = "fastcv";
    _params = {
        ParamDef::optional("min_area", BaseType::INT, "Minimum region area", 256),
        ParamDef::optional("max_area_ratio", BaseType::FLOAT, "Max area as fraction of image", 0.25),
        ParamDef::optional("delta", BaseType::INT, "MSER delta parameter", 2)
    };
    _example = "fcv_mser(256, 0.25, 2)";
    _returnType = "mat";
    _tags = {"fastcv", "mser", "region", "detect", "feature"};
}

ExecutionResult FastMSERItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int minArea           = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 256;
    float maxAreaRatio    = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.25f;
    uint32_t delta        = args.size() > 2 ? static_cast<uint32_t>(args[2].asNumber()) : 2;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    uint32_t maxArea = static_cast<uint32_t>(src.total() * maxAreaRatio);

    auto mser = cv::fastcv::FCVMSER::create(
        src.size(), 4, delta,
        static_cast<uint32_t>(minArea), maxArea,
        0.15f, 0.2f);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Rect> bboxes;
    mser->detect(src, contours, bboxes);

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) {
        cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    }
    for (size_t i = 0; i < contours.size(); i++) {
        cv::Scalar color(rand() % 200 + 55, rand() % 200 + 55, rand() % 200 + 55);
        for (const auto& pt : contours[i]) {
            output.at<cv::Vec3b>(pt.y, pt.x) = cv::Vec3b(
                static_cast<uint8_t>(color[0]),
                static_cast<uint8_t>(color[1]),
                static_cast<uint8_t>(color[2]));
        }
        if (!bboxes.empty()) {
            cv::rectangle(output, bboxes[i], cv::Scalar(0, 255, 0), 1);
        }
    }
    return ExecutionResult::ok(output);
}

FastWarpPerspectiveItem::FastWarpPerspectiveItem() {
    _functionName = "fcv_warp_perspective";
    _description = "Perspective warp accelerated by FastCV; matrix (3x3 CV_32F) loaded from cache";
    _category = "fastcv";
    _params = {
        ParamDef::required("matrix_cache", BaseType::STRING, "Cache ID of 3x3 CV_32F perspective matrix"),
        ParamDef::optional("width", BaseType::INT, "Output width (0 = same as input)", 0),
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

    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager available");
    }

    cv::Mat M = ctx.cacheManager->get(matId);
    if (M.empty()) {
        return ExecutionResult::fail("Perspective matrix not found in cache: " + matId);
    }
    if (M.type() != CV_32F) {
        M.convertTo(M, CV_32F);
    }

    cv::Size dsize = (w > 0 && h > 0) ? cv::Size(w, h) : ctx.currentMat.size();

    cv::Mat dst;
    cv::fastcv::warpPerspective(ctx.currentMat, dst, M, dsize, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
    return ExecutionResult::ok(dst);
}

FastWarpAffineItem::FastWarpAffineItem() {
    _functionName = "fcv_warp_affine";
    _description = "Affine warp accelerated by FastCV; matrix (2x3 CV_32F) loaded from cache";
    _category = "fastcv";
    _params = {
        ParamDef::required("matrix_cache", BaseType::STRING, "Cache ID of 2x3 CV_32F affine matrix"),
        ParamDef::optional("width", BaseType::INT, "Output width (0 = same as input)", 0),
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

    if (!ctx.cacheManager) {
        return ExecutionResult::fail("No cache manager available");
    }

    cv::Mat M = ctx.cacheManager->get(matId);
    if (M.empty()) {
        return ExecutionResult::fail("Affine matrix not found in cache: " + matId);
    }
    if (M.type() != CV_32F) {
        M.convertTo(M, CV_32F);
    }

    cv::Size dsize = (w > 0 && h > 0) ? cv::Size(w, h) : ctx.currentMat.size();

    cv::Mat dst;
    cv::fastcv::warpAffine(ctx.currentMat, dst, M, dsize);
    return ExecutionResult::ok(dst);
}

FastMeanShiftItem::FastMeanShiftItem() {
    _functionName = "fcv_mean_shift";
    _description = "Mean shift tracking accelerated by FastCV; tracks ROI loaded from cache";
    _category = "fastcv";
    _params = {
        ParamDef::required("track_cache", BaseType::STRING, "Cache ID of ROI rect stored as 1x4 Mat [x,y,w,h]"),
        ParamDef::optional("max_iter", BaseType::INT, "Max iterations", 10),
        ParamDef::optional("epsilon", BaseType::FLOAT, "Convergence epsilon", 1.0)
    };
    _example = "fcv_mean_shift(\"roi\", 10, 1.0)";
    _returnType = "mat";
    _tags = {"fastcv", "mean-shift", "tracking"};
}

ExecutionResult FastMeanShiftItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    int maxIter   = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 10;
    double eps    = args.size() > 2 ? args[2].asNumber() : 1.0;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    // Get or initialize ROI rect
    cv::Rect rect;
    if (ctx.cacheManager) {
        cv::Mat roiMat = ctx.cacheManager->get(cacheId);
        if (!roiMat.empty() && roiMat.total() >= 4) {
            float* p = roiMat.ptr<float>();
            rect = cv::Rect(static_cast<int>(p[0]), static_cast<int>(p[1]),
                            static_cast<int>(p[2]), static_cast<int>(p[3]));
        }
    }
    if (rect.empty()) {
        // Default to center quarter
        rect = cv::Rect(src.cols/4, src.rows/4, src.cols/2, src.rows/2);
    }

    // Clamp to image bounds
    rect &= cv::Rect(0, 0, src.cols, src.rows);

    cv::TermCriteria termCrit(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, maxIter, eps);
    cv::fastcv::meanShift(src, rect, termCrit);

    // Store updated rect
    if (ctx.cacheManager) {
        cv::Mat roiMat(1, 4, CV_32F);
        float* p = roiMat.ptr<float>();
        p[0] = static_cast<float>(rect.x);
        p[1] = static_cast<float>(rect.y);
        p[2] = static_cast<float>(rect.width);
        p[3] = static_cast<float>(rect.height);
        ctx.cacheManager->set(cacheId, roiMat, false);
    }

    // Draw result on output
    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) {
        cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    }
    cv::rectangle(output, rect, cv::Scalar(0, 255, 255), 2);
    return ExecutionResult::ok(output);
}

FastTrackOpticalFlowLKItem::FastTrackOpticalFlowLKItem() {
    _functionName = "fcv_track_lk";
    _description = "Lucas-Kanade sparse optical flow accelerated by FastCV "
                   "(tracks FAST corners between consecutive frames)";
    _category = "fastcv";
    _params = {
        ParamDef::optional("max_corners", BaseType::INT, "Max number of corners to track", 200),
        ParamDef::optional("win_size", BaseType::INT, "LK window size (odd)", 7),
        ParamDef::optional("levels", BaseType::INT, "Pyramid levels", 3)
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
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }

    cv::Mat output = ctx.currentMat.clone();
    if (output.channels() == 1) {
        cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    }

    if (_prevFrame.empty()) {
        // First frame: detect corners
        _prevFrame = gray.clone();
        std::vector<int> coords, scores;
        cv::fastcv::FAST10(gray, cv::noArray(), coords, scores, 10, 4, true);
        _prevPts.clear();
        for (size_t i = 0; i < coords.size() / 2 && (int)_prevPts.size() < maxCorners; i++) {
            _prevPts.push_back(cv::Point2f(static_cast<float>(coords[2*i]),
                                           static_cast<float>(coords[2*i+1])));
        }
        return ExecutionResult::ok(output);
    }

    if (_prevPts.empty()) {
        _prevFrame = gray.clone();
        return ExecutionResult::ok(output);
    }

    // Build pyramids
    std::vector<cv::Mat> prevPyr, currPyr;
    cv::fastcv::buildPyramid(_prevFrame, prevPyr, pyrLevels, true);
    cv::fastcv::buildPyramid(gray, currPyr, pyrLevels, true);

    // Track
    cv::Mat ptsInMat(static_cast<int>(_prevPts.size()), 1, CV_32FC2, _prevPts.data());
    cv::Mat ptsOutMat, statusMat;

    cv::TermCriteria termCrit(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, 7, 0.03f * 0.03f);
    cv::fastcv::trackOpticalFlowLK(
        _prevFrame, gray,
        prevPyr, currPyr,
        ptsInMat, ptsOutMat, cv::noArray(),
        statusMat,
        cv::Size(winSz, winSz), termCrit);

    // Draw tracks
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
    if (!nextPts.empty()) {
        _prevPts = nextPts;
    } else {
        // Re-detect corners since we lost all tracks
        std::vector<int> coords, scores;
        cv::fastcv::FAST10(gray, cv::noArray(), coords, scores, 10, 4, true);
        _prevPts.clear();
        for (size_t i = 0; i < coords.size() / 2 && (int)_prevPts.size() < maxCorners; i++) {
            _prevPts.push_back(cv::Point2f(static_cast<float>(coords[2*i]),
                                           static_cast<float>(coords[2*i+1])));
        }
    }

    return ExecutionResult::ok(output);
}

FastCalcHistItem::FastCalcHistItem() {
    _functionName = "fcv_calc_hist";
    _description = "Grayscale histogram calculation accelerated by FastCV; returns histogram visualization";
    _category = "fastcv";
    _params = {
        ParamDef::optional("height", BaseType::INT, "Histogram visualization height", 128)
    };
    _example = "fcv_calc_hist(128)";
    _returnType = "mat";
    _tags = {"fastcv", "histogram", "statistics"};
}

ExecutionResult FastCalcHistItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int visHeight = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 128;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Mat hist;
    cv::fastcv::calcHist(src, hist);  // outputs 1x256 CV_32S

    // Normalize and draw
    cv::Mat histVis(visHeight, 256, CV_8UC3, cv::Scalar(0, 0, 0));
    double maxVal = 0;
    cv::minMaxLoc(hist, nullptr, &maxVal);
    if (maxVal > 0) {
        for (int i = 0; i < 256; i++) {
            int binH = static_cast<int>(static_cast<double>(hist.at<int32_t>(0, i)) / maxVal * visHeight);
            cv::line(histVis,
                     cv::Point(i, visHeight),
                     cv::Point(i, visHeight - binH),
                     cv::Scalar(255, 255, 255));
        }
    }
    return ExecutionResult::ok(histVis);
}

FastNormalizeLocalBoxItem::FastNormalizeLocalBoxItem() {
    _functionName = "fcv_normalize_local_box";
    _description = "Local box normalization (contrast normalization) accelerated by FastCV";
    _category = "fastcv";
    _params = {
        ParamDef::optional("box_width", BaseType::INT, "Local box width", 5),
        ParamDef::optional("box_height", BaseType::INT, "Local box height", 5),
        ParamDef::optional("use_stddev", BaseType::BOOL, "Normalize by std deviation (true) or mean (false)", true)
    };
    _example = "fcv_normalize_local_box(5, 5, true)";
    _returnType = "mat";
    _tags = {"fastcv", "normalize", "local", "contrast"};
}

ExecutionResult FastNormalizeLocalBoxItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int bw       = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 5;
    int bh       = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 5;
    bool useStd  = args.size() > 2 ? args[2].asBool() : true;

    cv::Mat src = ctx.currentMat;
    if (src.channels() > 1) {
        cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
    }

    cv::Mat dst;
    cv::fastcv::normalizeLocalBox(src, dst, cv::Size(bw, bh), useStd);

    // Normalize to 8U for display
    if (dst.depth() != CV_8U) {
        cv::normalize(dst, dst, 0, 255, cv::NORM_MINMAX);
        dst.convertTo(dst, CV_8U);
    }
    return ExecutionResult::ok(dst);
}

#endif // VISIONPIPE_FASTCV_ENABLED

} // namespace visionpipe

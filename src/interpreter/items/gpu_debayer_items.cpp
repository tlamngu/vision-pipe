#include "interpreter/items/gpu_debayer_items.h"
#include "utils/camera_device_manager.h"
#include <iostream>
#include <algorithm>

#ifdef VISIONPIPE_OPENCL_ENABLED
#include <opencv2/core/ocl.hpp>
#endif

namespace visionpipe {

void registerGpuDebayerItems(ItemRegistry& registry) {
    registry.add<GpuDebayerItem>();
}

// ============================================================================
// Helper: resolve Bayer conversion code
// ============================================================================

static int resolveBayerCode(const std::string& pattern,
                            const std::string& algorithm,
                            const std::string& output)
{
    // Build conversion code from pattern + algorithm + output
    // OpenCV naming convention:
    //   RGGB -> BayerRG  (named by the pixel at position (0,0) then (0,1))
    //   BGGR -> BayerBG
    //   GBRG -> BayerGB
    //   GRBG -> BayerGR

    if (output == "bgr") {
        if (algorithm == "bilinear") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR;
        } else if (algorithm == "vng") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR_VNG;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR_VNG;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR_VNG;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR_VNG;
        } else if (algorithm == "ea") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR_EA;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR_EA;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR_EA;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR_EA;
        }
    } else if (output == "rgb") {
        if (algorithm == "bilinear") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2RGB;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2RGB;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2RGB;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2RGB;
        } else if (algorithm == "vng") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2RGB_VNG;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2RGB_VNG;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2RGB_VNG;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2RGB_VNG;
        } else if (algorithm == "ea") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2RGB_EA;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2RGB_EA;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2RGB_EA;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2RGB_EA;
        }
    }
    return -1;
}

// ============================================================================
// GpuDebayerItem
// ============================================================================

GpuDebayerItem::GpuDebayerItem() {
    _functionName = "gpu_debayer";
    _description = "GPU-accelerated Bayer demosaicing using OpenCL. "
                   "Falls back to CPU when OpenCL is unavailable. "
                   "Supports bilinear, VNG, and EA algorithms.";
    _category = "color";
    _params = {
        ParamDef::optional("pattern",   BaseType::STRING,
            "Bayer tile pattern: RGGB, BGGR, GBRG, GRBG, or \"auto\"", "RGGB"),
        ParamDef::optional("algorithm", BaseType::STRING,
            "Demosaic algorithm: bilinear, vng, ea", "ea"),
        ParamDef::optional("output",    BaseType::STRING,
            "Output format: bgr or rgb", "bgr"),
        ParamDef::optional("bit_shift", BaseType::INT,
            "Right-shift for 10/12-bit packed inputs (e.g. 2 for 10-bit in 16-bit container)", 0)
    };
    _example = R"(gpu_debayer("auto", "ea") | gpu_debayer("RGGB", "vng", "bgr", 2))";
    _returnType = "mat";
    _tags = {"gpu", "opencl", "debayer", "demosaic", "bayer", "color", "acceleration"};
}

ExecutionResult GpuDebayerItem::execute(const std::vector<RuntimeValue>& args,
                                        ExecutionContext& ctx)
{
    if (ctx.currentMat.empty()) {
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ---- Parse parameters ----
    std::string pattern   = args.size() > 0 ? args[0].asString() : "RGGB";
    std::string algorithm = args.size() > 1 ? args[1].asString() : "ea";
    std::string output    = args.size() > 2 ? args[2].asString() : "bgr";
    int         bitShift  = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;

    // Normalise
    std::transform(pattern.begin(),   pattern.end(),   pattern.begin(),   ::toupper);
    std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(), ::tolower);
    std::transform(output.begin(),    output.end(),    output.begin(),    ::tolower);

    // ---- Auto-detect pattern from V4L2 / libcamera ----
    if (pattern == "AUTO") {
        if (ctx.lastSourceId.empty()) {
            return ExecutionResult::fail(
                "gpu_debayer(\"auto\"): no source ID in context — "
                "run video_cap or v4l2_setup before gpu_debayer");
        }
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
        pattern = CameraDeviceManager::instance().getV4L2BayerPattern(ctx.lastSourceId);
#endif
        if (pattern.empty()) {
            pattern = CameraDeviceManager::instance().getBayerPattern(ctx.lastSourceId);
        }
        if (pattern.empty()) {
            return ExecutionResult::fail(
                "gpu_debayer(\"auto\"): could not detect Bayer pattern for source '"
                + ctx.lastSourceId + "'");
        }
        if (ctx.verbose) {
            std::cout << "[GPU-DEBAYER] auto-detected pattern=" << pattern
                      << " for source=" << ctx.lastSourceId << std::endl;
        }
    }

    // ---- Prepare input ----
    cv::Mat input = ctx.currentMat;

    // Bit-shift for packed raw data
    if (bitShift > 0 && input.depth() == CV_16U) {
        input = input.clone();
        input = input / (1 << bitShift);
        input.convertTo(input, CV_8U);
    } else if (input.depth() == CV_16U) {
        input = input.clone();
        double minVal, maxVal;
        cv::minMaxLoc(input, &minVal, &maxVal);
        if (maxVal > 0) {
            input.convertTo(input, CV_8U, 255.0 / maxVal);
        }
    }

    if (input.channels() != 1) {
        return ExecutionResult::fail(
            "gpu_debayer: input must be single-channel (got "
            + std::to_string(input.channels()) + " channels)");
    }

    // ---- Resolve conversion code ----
    int code = resolveBayerCode(pattern, algorithm, output);
    if (code < 0) {
        return ExecutionResult::fail(
            "gpu_debayer: invalid combination pattern=\"" + pattern
            + "\" algorithm=\"" + algorithm + "\" output=\"" + output + "\"");
    }

    // ---- GPU (OpenCL) path ----
#ifdef VISIONPIPE_OPENCL_ENABLED
    bool useGpu = false;
    try {
        useGpu = cv::ocl::useOpenCL() && cv::ocl::haveOpenCL();
    } catch (...) {
        useGpu = false;
    }

    if (useGpu) {
        try {
            cv::UMat srcUMat, dstUMat;
            input.copyTo(srcUMat);

            cv::cvtColor(srcUMat, dstUMat, code);

            cv::Mat result;
            dstUMat.copyTo(result);

            if (ctx.verbose) {
                std::cout << "[GPU-DEBAYER] OpenCL path: pattern=" << pattern
                          << " algorithm=" << algorithm
                          << " " << input.cols << "x" << input.rows
                          << std::endl;
            }
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            // OpenCL kernel may not exist for certain algorithm combos —
            // fall through to CPU
            if (ctx.verbose) {
                std::cout << "[GPU-DEBAYER] OpenCL fallback to CPU: "
                          << e.what() << std::endl;
            }
        }
    }
#endif // VISIONPIPE_OPENCL_ENABLED

    // ---- CPU fallback ----
    try {
        cv::Mat result;
        cv::cvtColor(input, result, code);
        if (ctx.verbose) {
            std::cout << "[GPU-DEBAYER] CPU fallback: pattern=" << pattern
                      << " algorithm=" << algorithm
                      << " " << input.cols << "x" << input.rows
                      << std::endl;
        }
        return ExecutionResult::ok(result);
    } catch (const cv::Exception& e) {
        return ExecutionResult::fail(
            "gpu_debayer: OpenCV error: " + std::string(e.what()));
    }
}

} // namespace visionpipe

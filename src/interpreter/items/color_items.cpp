#include "interpreter/items/color_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>

namespace visionpipe {

void registerColorItems(ItemRegistry& registry) {
    registry.add<ColorConvertItem>();
    registry.add<ExtractChannelItem>();
    registry.add<SplitChannelsItem>();
    registry.add<MergeChannelsItem>();
    registry.add<MixChannelsItem>();
    registry.add<EqualizeHistItem>();
    registry.add<ClaheItem>();
    registry.add<CalcHistItem>();
    registry.add<CompareHistItem>();
    registry.add<CalcBackProjectItem>();
    registry.add<BrightnessContrastItem>();
    registry.add<GammaItem>();
    registry.add<ApplyColormapItem>();
    registry.add<LutItem>();
    registry.add<InRangeItem>();
    registry.add<WeightedGrayItem>();
}

// ============================================================================
// ColorConvertItem
// ============================================================================

ColorConvertItem::ColorConvertItem() {
    _functionName = "cvt_color";
    _description = "Converts color space of the current frame";
    _category = "color";
    _params = {
        ParamDef::required("code", BaseType::STRING, 
            "Conversion code: bgr2gray, gray2bgr, bgr2rgb, rgb2bgr, bgr2hsv, hsv2bgr, "
            "bgr2hls, hls2bgr, bgr2lab, lab2bgr, bgr2luv, luv2bgr, bgr2xyz, xyz2bgr, "
            "bgr2ycrcb, ycrcb2bgr, bgr2yuv, yuv2bgr")
    };
    _example = "cvt_color(\"bgr2gray\") | cvt_color(\"bgr2hsv\")";
    _returnType = "mat";
    _tags = {"color", "convert", "space"};
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
    else if (code == "bgr2hsv_full") cvCode = cv::COLOR_BGR2HSV_FULL;
    else if (code == "hsv2bgr_full") cvCode = cv::COLOR_HSV2BGR_FULL;
    else if (code == "bgr2hls") cvCode = cv::COLOR_BGR2HLS;
    else if (code == "hls2bgr") cvCode = cv::COLOR_HLS2BGR;
    else if (code == "bgr2lab") cvCode = cv::COLOR_BGR2Lab;
    else if (code == "lab2bgr") cvCode = cv::COLOR_Lab2BGR;
    else if (code == "bgr2luv") cvCode = cv::COLOR_BGR2Luv;
    else if (code == "luv2bgr") cvCode = cv::COLOR_Luv2BGR;
    else if (code == "bgr2xyz") cvCode = cv::COLOR_BGR2XYZ;
    else if (code == "xyz2bgr") cvCode = cv::COLOR_XYZ2BGR;
    else if (code == "bgr2ycrcb") cvCode = cv::COLOR_BGR2YCrCb;
    else if (code == "ycrcb2bgr") cvCode = cv::COLOR_YCrCb2BGR;
    else if (code == "bgr2yuv") cvCode = cv::COLOR_BGR2YUV;
    else if (code == "yuv2bgr") cvCode = cv::COLOR_YUV2BGR;
    else if (code == "rgb2gray") cvCode = cv::COLOR_RGB2GRAY;
    else if (code == "gray2rgb") cvCode = cv::COLOR_GRAY2RGB;
    else if (code == "bgra2rgba") cvCode = cv::COLOR_BGRA2RGBA;
    else if (code == "rgba2bgra") cvCode = cv::COLOR_RGBA2BGRA;
    else if (code == "bgr2bgra") cvCode = cv::COLOR_BGR2BGRA;
    else if (code == "bgra2bgr") cvCode = cv::COLOR_BGRA2BGR;
    else {
        return ExecutionResult::fail("Unknown color conversion code: " + code);
    }
    
    cv::Mat result;
    cv::cvtColor(ctx.currentMat, result, cvCode);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ExtractChannelItem
// ============================================================================

ExtractChannelItem::ExtractChannelItem() {
    _functionName = "extract_channel";
    _description = "Extracts a single channel from the image";
    _category = "color";
    _params = {
        ParamDef::required("channel", BaseType::INT, "Channel index (0, 1, 2, ...)")
    };
    _example = "extract_channel(0)  // Extract blue channel";
    _returnType = "mat";
    _tags = {"color", "channel", "extract"};
}

ExecutionResult ExtractChannelItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int channel = static_cast<int>(args[0].asNumber());
    
    if (channel < 0 || channel >= ctx.currentMat.channels()) {
        return ExecutionResult::fail("Channel index out of range");
    }
    
    cv::Mat result;
    cv::extractChannel(ctx.currentMat, result, channel);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SplitChannelsItem
// ============================================================================

SplitChannelsItem::SplitChannelsItem() {
    _functionName = "split_channels";
    _description = "Splits image into separate channels and caches them";
    _category = "color";
    _params = {
        ParamDef::optional("prefix", BaseType::STRING, "Cache prefix for channels", "ch")
    };
    _example = "split_channels(\"bgr\")  // Creates cache: bgr_0, bgr_1, bgr_2";
    _returnType = "mat";
    _tags = {"color", "channel", "split"};
}

ExecutionResult SplitChannelsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args.size() > 0 ? args[0].asString() : "ch";
    
    std::vector<cv::Mat> channels;
    cv::split(ctx.currentMat, channels);
    
    // Cache each channel
    for (size_t i = 0; i < channels.size(); i++) {
        std::string cacheId = prefix + "_" + std::to_string(i);
        ctx.cacheManager->set(cacheId, channels[i]);
    }
    
    // Return first channel
    return ExecutionResult::ok(channels.empty() ? ctx.currentMat : channels[0]);
}

// ============================================================================
// MergeChannelsItem
// ============================================================================

MergeChannelsItem::MergeChannelsItem() {
    _functionName = "merge_channels";
    _description = "Merges separate channels into a multi-channel image";
    _category = "color";
    _params = {
        ParamDef::required("channels", BaseType::ARRAY, "Array of cache IDs for channels")
    };
    _example = "merge_channels([\"ch_0\", \"ch_1\", \"ch_2\"])";
    _returnType = "mat";
    _tags = {"color", "channel", "merge"};
}

ExecutionResult MergeChannelsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // This needs array support - for now use current mat
    // Full implementation would iterate over cache IDs
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// MixChannelsItem
// ============================================================================

MixChannelsItem::MixChannelsItem() {
    _functionName = "mix_channels";
    _description = "Copies specified channels from input to output";
    _category = "color";
    _params = {
        ParamDef::required("from_to", BaseType::ARRAY, "Pairs of [src_channel, dst_channel]"),
        ParamDef::optional("output_channels", BaseType::INT, "Number of output channels", 3)
    };
    _example = "mix_channels([0, 2, 2, 0])  // Swap B and R";
    _returnType = "mat";
    _tags = {"color", "channel", "mix", "copy"};
}

ExecutionResult MixChannelsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Simplified - swap red and blue
    if (ctx.currentMat.channels() >= 3) {
        cv::Mat result;
        cv::cvtColor(ctx.currentMat, result, cv::COLOR_BGR2RGB);
        return ExecutionResult::ok(result);
    }
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// EqualizeHistItem
// ============================================================================

EqualizeHistItem::EqualizeHistItem() {
    _functionName = "equalize_hist";
    _description = "Equalizes histogram of grayscale image or Y channel of color";
    _category = "color";
    _params = {};
    _example = "equalize_hist()";
    _returnType = "mat";
    _tags = {"histogram", "equalize", "contrast"};
}

ExecutionResult EqualizeHistItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat result;
    
    if (ctx.currentMat.channels() == 1) {
        cv::equalizeHist(ctx.currentMat, result);
    } else {
        // Convert to YCrCb, equalize Y channel, convert back
        cv::Mat ycrcb;
        cv::cvtColor(ctx.currentMat, ycrcb, cv::COLOR_BGR2YCrCb);
        
        std::vector<cv::Mat> channels;
        cv::split(ycrcb, channels);
        cv::equalizeHist(channels[0], channels[0]);
        cv::merge(channels, ycrcb);
        
        cv::cvtColor(ycrcb, result, cv::COLOR_YCrCb2BGR);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ClaheItem
// ============================================================================

ClaheItem::ClaheItem() {
    _functionName = "clahe";
    _description = "Applies CLAHE (Contrast Limited Adaptive Histogram Equalization)";
    _category = "color";
    _params = {
        ParamDef::optional("clip_limit", BaseType::FLOAT, "Contrast limit threshold", 2.0),
        ParamDef::optional("tile_grid_size", BaseType::INT, "Size of grid for tiles", 8)
    };
    _example = "clahe(2.0, 8)";
    _returnType = "mat";
    _tags = {"histogram", "clahe", "contrast", "adaptive"};
}

ExecutionResult ClaheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double clipLimit = args.size() > 0 ? args[0].asNumber() : 2.0;
    int tileSize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 8;
    
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clipLimit, cv::Size(tileSize, tileSize));
    
    cv::Mat result;
    if (ctx.currentMat.channels() == 1) {
        clahe->apply(ctx.currentMat, result);
    } else {
        // Apply to L channel of LAB
        cv::Mat lab;
        cv::cvtColor(ctx.currentMat, lab, cv::COLOR_BGR2Lab);
        
        std::vector<cv::Mat> channels;
        cv::split(lab, channels);
        clahe->apply(channels[0], channels[0]);
        cv::merge(channels, lab);
        
        cv::cvtColor(lab, result, cv::COLOR_Lab2BGR);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// CalcHistItem
// ============================================================================

CalcHistItem::CalcHistItem() {
    _functionName = "calc_hist";
    _description = "Calculates histogram of the image";
    _category = "color";
    _params = {
        ParamDef::optional("channel", BaseType::INT, "Channel to calculate (default: 0)", 0),
        ParamDef::optional("hist_size", BaseType::INT, "Number of histogram bins", 256),
        ParamDef::optional("range_min", BaseType::FLOAT, "Minimum range value", 0.0),
        ParamDef::optional("range_max", BaseType::FLOAT, "Maximum range value", 256.0)
    };
    _example = "calc_hist(0, 256)";
    _returnType = "mat";
    _tags = {"histogram", "calculate", "analyze"};
}

ExecutionResult CalcHistItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int channel = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 0;
    int histSize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 256;
    float rangeMin = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 0.0f;
    float rangeMax = args.size() > 3 ? static_cast<float>(args[3].asNumber()) : 256.0f;
    
    std::vector<cv::Mat> bgr_planes;
    cv::split(ctx.currentMat, bgr_planes);
    
    if (channel >= static_cast<int>(bgr_planes.size())) {
        channel = 0;
    }
    
    float range[] = { rangeMin, rangeMax };
    const float* histRange = { range };
    
    cv::Mat hist;
    cv::calcHist(&bgr_planes[channel], 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
    
    return ExecutionResult::ok(hist);
}

// ============================================================================
// CompareHistItem
// ============================================================================

CompareHistItem::CompareHistItem() {
    _functionName = "compare_hist";
    _description = "Compares two histograms";
    _category = "color";
    _params = {
        ParamDef::required("other", BaseType::STRING, "Cache ID of other histogram"),
        ParamDef::optional("method", BaseType::STRING, "Comparison method: correlation, chi_square, intersection, bhattacharyya, kl_div", "correlation")
    };
    _example = "compare_hist(\"hist2\", \"correlation\")";
    _returnType = "float";
    _tags = {"histogram", "compare", "similarity"};
}

ExecutionResult CompareHistItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string otherId = args[0].asString();
    std::string method = args.size() > 1 ? args[1].asString() : "correlation";
    
    cv::Mat other = ctx.cacheManager->get(otherId);
    if (other.empty()) {
        return ExecutionResult::fail("Cached histogram not found: " + otherId);
    }
    
    int compMethod = cv::HISTCMP_CORREL;
    if (method == "chi_square") compMethod = cv::HISTCMP_CHISQR;
    else if (method == "intersection") compMethod = cv::HISTCMP_INTERSECT;
    else if (method == "bhattacharyya") compMethod = cv::HISTCMP_BHATTACHARYYA;
    else if (method == "kl_div") compMethod = cv::HISTCMP_KL_DIV;
    
    double result = cv::compareHist(ctx.currentMat, other, compMethod);
    
    // Store result as a 1x1 Mat and return current mat unchanged
    ctx.cacheManager->set("hist_compare_result", cv::Mat(1, 1, CV_64F, cv::Scalar(result)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CalcBackProjectItem
// ============================================================================

CalcBackProjectItem::CalcBackProjectItem() {
    _functionName = "calc_back_project";
    _description = "Calculates back projection of a histogram";
    _category = "color";
    _params = {
        ParamDef::required("hist", BaseType::STRING, "Cache ID of histogram"),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0)
    };
    _example = "calc_back_project(\"hist\")";
    _returnType = "mat";
    _tags = {"histogram", "backproject", "probability"};
}

ExecutionResult CalcBackProjectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string histId = args[0].asString();
    double scale = args.size() > 1 ? args[1].asNumber() : 1.0;
    
    cv::Mat hist = ctx.cacheManager->get(histId);
    if (hist.empty()) {
        return ExecutionResult::fail("Histogram not found: " + histId);
    }
    
    float range[] = { 0, 256 };
    const float* histRange = { range };
    int channels[] = { 0 };
    
    cv::Mat result;
    cv::calcBackProject(&ctx.currentMat, 1, channels, hist, result, &histRange, scale);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BrightnessContrastItem
// ============================================================================

BrightnessContrastItem::BrightnessContrastItem() {
    _functionName = "brightness_contrast";
    _description = "Adjusts brightness and contrast";
    _category = "color";
    _params = {
        ParamDef::optional("brightness", BaseType::FLOAT, "Brightness adjustment (-100 to 100)", 0.0),
        ParamDef::optional("contrast", BaseType::FLOAT, "Contrast multiplier (0.0 to 3.0)", 1.0)
    };
    _example = "brightness_contrast(10, 1.2)";
    _returnType = "mat";
    _tags = {"brightness", "contrast", "adjust"};
}

ExecutionResult BrightnessContrastItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double brightness = args.size() > 0 ? args[0].asNumber() : 0.0;
    double contrast = args.size() > 1 ? args[1].asNumber() : 1.0;
    
    cv::Mat result;
    ctx.currentMat.convertTo(result, -1, contrast, brightness);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// GammaItem
// ============================================================================

GammaItem::GammaItem() {
    _functionName = "gamma";
    _description = "Applies gamma correction";
    _category = "color";
    _params = {
        ParamDef::optional("gamma", BaseType::FLOAT, "Gamma value (>1 darkens, <1 brightens)", 1.0)
    };
    _example = "gamma(0.5)  // Brighten | gamma(2.0)  // Darken";
    _returnType = "mat";
    _tags = {"gamma", "correction", "adjust"};
}

ExecutionResult GammaItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double gamma = args.size() > 0 ? args[0].asNumber() : 1.0;
    
    if (gamma == 1.0) {
        return ExecutionResult::ok(ctx.currentMat);
    }
    
    // Build lookup table
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);
    }
    
    cv::Mat result;
    cv::LUT(ctx.currentMat, lookUpTable, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ApplyColormapItem
// ============================================================================

ApplyColormapItem::ApplyColormapItem() {
    _functionName = "apply_colormap";
    _description = "Applies a colormap to grayscale image";
    _category = "color";
    _params = {
        ParamDef::optional("colormap", BaseType::STRING, 
            "Colormap: autumn, bone, jet, winter, rainbow, ocean, summer, spring, cool, "
            "hsv, pink, hot, parula, magma, inferno, plasma, viridis, cividis, twilight, "
            "twilight_shifted, turbo, deepgreen", "jet")
    };
    _example = "apply_colormap(\"jet\") | apply_colormap(\"viridis\")";
    _returnType = "mat";
    _tags = {"colormap", "visualization", "pseudocolor"};
}

ExecutionResult ApplyColormapItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string mapName = args.size() > 0 ? args[0].asString() : "jet";
    
    int colormap = cv::COLORMAP_JET;
    if (mapName == "autumn") colormap = cv::COLORMAP_AUTUMN;
    else if (mapName == "bone") colormap = cv::COLORMAP_BONE;
    else if (mapName == "winter") colormap = cv::COLORMAP_WINTER;
    else if (mapName == "rainbow") colormap = cv::COLORMAP_RAINBOW;
    else if (mapName == "ocean") colormap = cv::COLORMAP_OCEAN;
    else if (mapName == "summer") colormap = cv::COLORMAP_SUMMER;
    else if (mapName == "spring") colormap = cv::COLORMAP_SPRING;
    else if (mapName == "cool") colormap = cv::COLORMAP_COOL;
    else if (mapName == "hsv") colormap = cv::COLORMAP_HSV;
    else if (mapName == "pink") colormap = cv::COLORMAP_PINK;
    else if (mapName == "hot") colormap = cv::COLORMAP_HOT;
    else if (mapName == "parula") colormap = cv::COLORMAP_PARULA;
    else if (mapName == "magma") colormap = cv::COLORMAP_MAGMA;
    else if (mapName == "inferno") colormap = cv::COLORMAP_INFERNO;
    else if (mapName == "plasma") colormap = cv::COLORMAP_PLASMA;
    else if (mapName == "viridis") colormap = cv::COLORMAP_VIRIDIS;
    else if (mapName == "cividis") colormap = cv::COLORMAP_CIVIDIS;
    else if (mapName == "twilight") colormap = cv::COLORMAP_TWILIGHT;
    else if (mapName == "twilight_shifted") colormap = cv::COLORMAP_TWILIGHT_SHIFTED;
    else if (mapName == "turbo") colormap = cv::COLORMAP_TURBO;
    else if (mapName == "deepgreen") colormap = cv::COLORMAP_DEEPGREEN;
    
    cv::Mat input = ctx.currentMat;
    if (input.channels() > 1) {
        cv::cvtColor(input, input, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat result;
    cv::applyColorMap(input, result, colormap);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// LutItem
// ============================================================================

LutItem::LutItem() {
    _functionName = "lut";
    _description = "Applies lookup table transformation";
    _category = "color";
    _params = {
        ParamDef::required("lut", BaseType::STRING, "Cache ID of lookup table (256x1 or 1x256)")
    };
    _example = "lut(\"my_lut\")";
    _returnType = "mat";
    _tags = {"lut", "lookup", "transform"};
}

ExecutionResult LutItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string lutId = args[0].asString();
    
    cv::Mat lut = ctx.cacheManager->get(lutId);
    if (lut.empty()) {
        return ExecutionResult::fail("LUT not found: " + lutId);
    }
    
    cv::Mat result;
    cv::LUT(ctx.currentMat, lut, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// InRangeItem
// ============================================================================

InRangeItem::InRangeItem() {
    _functionName = "in_range";
    _description = "Checks if array elements lie between two values";
    _category = "color";
    _params = {
        ParamDef::required("lower", BaseType::ARRAY, "Lower bound [B, G, R] or scalar"),
        ParamDef::required("upper", BaseType::ARRAY, "Upper bound [B, G, R] or scalar")
    };
    _example = "in_range([100, 100, 100], [255, 255, 255])";
    _returnType = "mat";
    _tags = {"range", "threshold", "mask"};
}

ExecutionResult InRangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Simplified version - needs full array support
    cv::Scalar lower(0, 0, 0);
    cv::Scalar upper(255, 255, 255);
    
    if (args.size() >= 2 && args[0].isNumeric() && args[1].isNumeric()) {
        lower = cv::Scalar::all(args[0].asNumber());
        upper = cv::Scalar::all(args[1].asNumber());
    }
    
    cv::Mat result;
    cv::inRange(ctx.currentMat, lower, upper, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// WeightedGrayItem
// ============================================================================

WeightedGrayItem::WeightedGrayItem() {
    _functionName = "weighted_gray";
    _description = "Converts to grayscale with custom channel weights";
    _category = "color";
    _params = {
        ParamDef::optional("w_b", BaseType::FLOAT, "Blue channel weight", 0.114),
        ParamDef::optional("w_g", BaseType::FLOAT, "Green channel weight", 0.587),
        ParamDef::optional("w_r", BaseType::FLOAT, "Red channel weight", 0.299)
    };
    _example = "weighted_gray(0.11, 0.59, 0.30)";
    _returnType = "mat";
    _tags = {"grayscale", "convert", "weighted"};
}

ExecutionResult WeightedGrayItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double wB = args.size() > 0 ? args[0].asNumber() : 0.114;
    double wG = args.size() > 1 ? args[1].asNumber() : 0.587;
    double wR = args.size() > 2 ? args[2].asNumber() : 0.299;
    
    if (ctx.currentMat.channels() != 3) {
        return ExecutionResult::ok(ctx.currentMat);
    }
    
    std::vector<cv::Mat> channels;
    cv::split(ctx.currentMat, channels);
    
    cv::Mat result;
    result = channels[0] * wB + channels[1] * wG + channels[2] * wR;
    result.convertTo(result, CV_8U);
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

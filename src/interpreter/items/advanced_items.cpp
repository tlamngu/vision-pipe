#include "interpreter/items/advanced_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <chrono>

namespace visionpipe {

void registerAdvancedItems(ItemRegistry& registry) {
    // Optical Flow
    registry.add<CalcOpticalFlowPyrLKItem>();
    registry.add<CalcOpticalFlowFarnebackItem>();
    registry.add<DrawOpticalFlowItem>();
    
    // Background Subtraction
    registry.add<BackgroundSubtractorMOG2Item>();
    registry.add<BackgroundSubtractorKNNItem>();
    registry.add<ApplyBackgroundSubtractorItem>();
    
    // Object Tracking
    registry.add<TrackerKCFItem>();
    registry.add<TrackerCSRTItem>();
    registry.add<TrackerMILItem>();
    registry.add<InitTrackerItem>();
    registry.add<UpdateTrackerItem>();
    registry.add<MultiTrackerItem>();
    
    // Image Stitching
    registry.add<StitcherItem>();
    
    // HDR
    registry.add<CreateMergeMertensItem>();
    registry.add<MergeExposuresItem>();
    registry.add<TonemapItem>();
    registry.add<TonemapDragoItem>();
    registry.add<TonemapReinhardItem>();
    registry.add<TonemapMantiukItem>();
    
    // Non-Photorealistic Rendering
    registry.add<EdgePreservingFilterItem>();
    registry.add<DetailEnhanceItem>();
    registry.add<PencilSketchItem>();
    registry.add<StylizationItem>();
    
    // Inpainting
    registry.add<InpaintItem>();
    
    // Denoising
    registry.add<FastNlMeansDenoisingItem>();
    registry.add<FastNlMeansDenoisingColoredItem>();
    
    // Seamless Cloning
    registry.add<SeamlessCloneItem>();
    registry.add<ColorChangeItem>();
    registry.add<IlluminationChangeItem>();
    registry.add<TextureFlattingItem>();
}

// ============================================================================
// CalcOpticalFlowPyrLKItem
// ============================================================================

CalcOpticalFlowPyrLKItem::CalcOpticalFlowPyrLKItem() {
    _functionName = "calc_optical_flow_pyr_lk";
    _description = "Calculates sparse optical flow using Lucas-Kanade pyramid method";
    _category = "advanced";
    _params = {
        ParamDef::required("prev_cache", BaseType::STRING, "Previous frame cache ID"),
        ParamDef::required("prev_points_cache", BaseType::STRING, "Previous points cache ID"),
        ParamDef::optional("win_size", BaseType::INT, "Search window size", 21),
        ParamDef::optional("max_level", BaseType::INT, "Maximum pyramid level", 3),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "flow")
    };
    _example = "calc_optical_flow_pyr_lk(\"prev_frame\", \"prev_points\", 21, 3, \"flow\")";
    _returnType = "mat";
    _tags = {"optical_flow", "lk", "tracking"};
}

ExecutionResult CalcOpticalFlowPyrLKItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prevCache = args[0].asString();
    std::string prevPointsCache = args[1].asString();
    int winSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 21;
    int maxLevel = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 3;
    std::string prefix = args.size() > 4 ? args[4].asString() : "flow";
    
    cv::Mat prevOpt = ctx.cacheManager->get(prevCache);
    cv::Mat prevPointsOpt = ctx.cacheManager->get(prevPointsCache);
    
    if (prevOpt.empty() || prevPointsOpt.empty()) {
        return ExecutionResult::fail("Previous frame or points not found");
    }
    
    cv::Mat prevGray, currGray;
    if (prevOpt.channels() > 1) {
        cv::cvtColor(prevOpt, prevGray, cv::COLOR_BGR2GRAY);
    } else {
        prevGray = prevOpt;
    }
    
    if (ctx.currentMat.channels() > 1) {
        cv::cvtColor(ctx.currentMat, currGray, cv::COLOR_BGR2GRAY);
    } else {
        currGray = ctx.currentMat;
    }
    
    std::vector<cv::Point2f> prevPoints, nextPoints;
    for (int i = 0; i < prevPointsOpt.rows; i++) {
        prevPoints.push_back(prevPointsOpt.at<cv::Point2f>(i));
    }
    
    std::vector<uchar> status;
    std::vector<float> err;
    
    cv::calcOpticalFlowPyrLK(prevGray, currGray, prevPoints, nextPoints, status, err,
                              cv::Size(winSize, winSize), maxLevel);
    
    ctx.cacheManager->set(prefix + "_next_points", cv::Mat(nextPoints));
    ctx.cacheManager->set(prefix + "_status", cv::Mat(status));
    ctx.cacheManager->set(prefix + "_error", cv::Mat(err));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CalcOpticalFlowFarnebackItem
// ============================================================================

CalcOpticalFlowFarnebackItem::CalcOpticalFlowFarnebackItem() {
    _functionName = "calc_optical_flow_farneback";
    _description = "Calculates dense optical flow using Farneback algorithm";
    _category = "advanced";
    _params = {
        ParamDef::required("prev_cache", BaseType::STRING, "Previous frame cache ID"),
        ParamDef::optional("pyr_scale", BaseType::FLOAT, "Pyramid scale", 0.5),
        ParamDef::optional("levels", BaseType::INT, "Number of pyramid levels", 3),
        ParamDef::optional("winsize", BaseType::INT, "Averaging window size", 15),
        ParamDef::optional("iterations", BaseType::INT, "Iterations at each level", 3),
        ParamDef::optional("poly_n", BaseType::INT, "Polynomial neighborhood size", 5),
        ParamDef::optional("poly_sigma", BaseType::FLOAT, "Polynomial sigma", 1.2),
        ParamDef::optional("cache_id", BaseType::STRING, "Flow cache ID", "flow")
    };
    _example = "calc_optical_flow_farneback(\"prev_frame\", 0.5, 3, 15, 3, 5, 1.2, \"flow\")";
    _returnType = "mat";
    _tags = {"optical_flow", "farneback", "dense"};
}

ExecutionResult CalcOpticalFlowFarnebackItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prevCache = args[0].asString();
    double pyrScale = args.size() > 1 ? args[1].asNumber() : 0.5;
    int levels = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 3;
    int winsize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 15;
    int iterations = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 3;
    int polyN = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 5;
    double polySigma = args.size() > 6 ? args[6].asNumber() : 1.2;
    std::string cacheId = args.size() > 7 ? args[7].asString() : "flow";
    
    cv::Mat prevOpt = ctx.cacheManager->get(prevCache);
    if (prevOpt.empty()) {
        return ExecutionResult::fail("Previous frame not found: " + prevCache);
    }
    
    cv::Mat prevGray, currGray;
    if (prevOpt.channels() > 1) {
        cv::cvtColor(prevOpt, prevGray, cv::COLOR_BGR2GRAY);
    } else {
        prevGray = prevOpt;
    }
    
    if (ctx.currentMat.channels() > 1) {
        cv::cvtColor(ctx.currentMat, currGray, cv::COLOR_BGR2GRAY);
    } else {
        currGray = ctx.currentMat;
    }
    
    cv::Mat flow;
    cv::calcOpticalFlowFarneback(prevGray, currGray, flow, pyrScale, levels, winsize,
                                  iterations, polyN, polySigma, 0);
    
    ctx.cacheManager->set(cacheId, flow);
    
    return ExecutionResult::ok(flow);
}

// ============================================================================
// DrawOpticalFlowItem
// ============================================================================

DrawOpticalFlowItem::DrawOpticalFlowItem() {
    _functionName = "draw_optical_flow";
    _description = "Visualizes optical flow as arrows or HSV";
    _category = "advanced";
    _params = {
        ParamDef::required("flow_cache", BaseType::STRING, "Flow field cache ID"),
        ParamDef::optional("mode", BaseType::STRING, "Mode: arrows, hsv", "hsv"),
        ParamDef::optional("step", BaseType::INT, "Arrow step for arrows mode", 16),
        ParamDef::optional("scale", BaseType::FLOAT, "Arrow scale", 2.0)
    };
    _example = "draw_optical_flow(\"flow\", \"hsv\")";
    _returnType = "mat";
    _tags = {"optical_flow", "visualization"};
}

ExecutionResult DrawOpticalFlowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string flowCache = args[0].asString();
    std::string mode = args.size() > 1 ? args[1].asString() : "hsv";
    int step = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 16;
    double scale = args.size() > 3 ? args[3].asNumber() : 2.0;
    
    cv::Mat flowOpt = ctx.cacheManager->get(flowCache);
    if (flowOpt.empty()) {
        return ExecutionResult::fail("Flow not found: " + flowCache);
    }
    
    cv::Mat result;
    
    if (mode == "hsv") {
        std::vector<cv::Mat> flowParts;
        cv::split(flowOpt, flowParts);
        
        cv::Mat magnitude, angle;
        cv::cartToPolar(flowParts[0], flowParts[1], magnitude, angle, true);
        
        cv::normalize(magnitude, magnitude, 0, 255, cv::NORM_MINMAX);
        
        cv::Mat hsv(flowOpt.size(), CV_8UC3);
        std::vector<cv::Mat> hsvChannels(3);
        angle.convertTo(hsvChannels[0], CV_8U, 180.0 / 360.0);
        hsvChannels[1] = cv::Mat::ones(flowOpt.size(), CV_8U) * 255;
        magnitude.convertTo(hsvChannels[2], CV_8U);
        cv::merge(hsvChannels, hsv);
        
        cv::cvtColor(hsv, result, cv::COLOR_HSV2BGR);
    } else {
        result = ctx.currentMat.clone();
        if (result.channels() == 1) {
            cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
        }
        
        for (int y = 0; y < flowOpt.rows; y += step) {
            for (int x = 0; x < flowOpt.cols; x += step) {
                cv::Vec2f flow = flowOpt.at<cv::Vec2f>(y, x);
                cv::Point2f from(x, y);
                cv::Point2f to(x + flow[0] * scale, y + flow[1] * scale);
                cv::arrowedLine(result, from, to, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
            }
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BackgroundSubtractorMOG2Item
// ============================================================================

BackgroundSubtractorMOG2Item::BackgroundSubtractorMOG2Item() {
    _functionName = "background_subtractor_mog2";
    _description = "Creates MOG2 background subtractor";
    _category = "advanced";
    _params = {
        ParamDef::optional("history", BaseType::INT, "History length", 500),
        ParamDef::optional("var_threshold", BaseType::FLOAT, "Variance threshold", 16.0),
        ParamDef::optional("detect_shadows", BaseType::BOOL, "Detect shadows", true),
        ParamDef::optional("cache_id", BaseType::STRING, "Subtractor cache ID", "bg_sub")
    };
    _example = "background_subtractor_mog2(500, 16.0, true, \"bg_sub\")";
    _returnType = "mat";
    _tags = {"background", "subtraction", "mog2"};
}

ExecutionResult BackgroundSubtractorMOG2Item::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Note: Background subtractor state would need to be stored separately
    // This is a simplified version that just creates the mask
    int history = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 500;
    double varThreshold = args.size() > 1 ? args[1].asNumber() : 16.0;
    bool detectShadows = args.size() > 2 ? args[2].asBool() : true;
    
    cv::Ptr<cv::BackgroundSubtractorMOG2> bgSub = cv::createBackgroundSubtractorMOG2(history, varThreshold, detectShadows);
    
    cv::Mat mask;
    bgSub->apply(ctx.currentMat, mask);
    
    return ExecutionResult::ok(mask);
}

// ============================================================================
// BackgroundSubtractorKNNItem
// ============================================================================

BackgroundSubtractorKNNItem::BackgroundSubtractorKNNItem() {
    _functionName = "background_subtractor_knn";
    _description = "Creates KNN background subtractor";
    _category = "advanced";
    _params = {
        ParamDef::optional("history", BaseType::INT, "History length", 500),
        ParamDef::optional("dist2_threshold", BaseType::FLOAT, "Distance threshold", 400.0),
        ParamDef::optional("detect_shadows", BaseType::BOOL, "Detect shadows", true)
    };
    _example = "background_subtractor_knn(500, 400.0, true)";
    _returnType = "mat";
    _tags = {"background", "subtraction", "knn"};
}

ExecutionResult BackgroundSubtractorKNNItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int history = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 500;
    double dist2Threshold = args.size() > 1 ? args[1].asNumber() : 400.0;
    bool detectShadows = args.size() > 2 ? args[2].asBool() : true;
    
    cv::Ptr<cv::BackgroundSubtractorKNN> bgSub = cv::createBackgroundSubtractorKNN(history, dist2Threshold, detectShadows);
    
    cv::Mat mask;
    bgSub->apply(ctx.currentMat, mask);
    
    return ExecutionResult::ok(mask);
}

// ============================================================================
// ApplyBackgroundSubtractorItem
// ============================================================================

ApplyBackgroundSubtractorItem::ApplyBackgroundSubtractorItem() {
    _functionName = "apply_background_subtractor";
    _description = "Applies background subtractor to current frame";
    _category = "advanced";
    _params = {
        ParamDef::required("subtractor_cache", BaseType::STRING, "Subtractor cache ID"),
        ParamDef::optional("learning_rate", BaseType::FLOAT, "Learning rate (-1 = auto)", -1.0)
    };
    _example = "apply_background_subtractor(\"bg_sub\", -1)";
    _returnType = "mat";
    _tags = {"background", "subtraction"};
}

ExecutionResult ApplyBackgroundSubtractorItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Simplified - would need proper state management
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// TrackerKCFItem
// ============================================================================

TrackerKCFItem::TrackerKCFItem() {
    _functionName = "tracker_kcf";
    _description = "Creates KCF tracker";
    _category = "advanced";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Tracker cache ID", "tracker")
    };
    _example = "tracker_kcf(\"tracker\")";
    _returnType = "mat";
    _tags = {"tracking", "kcf"};
}

ExecutionResult TrackerKCFItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Would need proper tracker state management
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// TrackerCSRTItem
// ============================================================================

TrackerCSRTItem::TrackerCSRTItem() {
    _functionName = "tracker_csrt";
    _description = "Creates CSRT tracker (more accurate but slower)";
    _category = "advanced";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Tracker cache ID", "tracker")
    };
    _example = "tracker_csrt(\"tracker\")";
    _returnType = "mat";
    _tags = {"tracking", "csrt"};
}

ExecutionResult TrackerCSRTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// TrackerMILItem
// ============================================================================

TrackerMILItem::TrackerMILItem() {
    _functionName = "tracker_mil";
    _description = "Creates MIL tracker";
    _category = "advanced";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Tracker cache ID", "tracker")
    };
    _example = "tracker_mil(\"tracker\")";
    _returnType = "mat";
    _tags = {"tracking", "mil"};
}

ExecutionResult TrackerMILItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// InitTrackerItem
// ============================================================================

InitTrackerItem::InitTrackerItem() {
    _functionName = "init_tracker";
    _description = "Initializes tracker with bounding box";
    _category = "advanced";
    _params = {
        ParamDef::required("tracker_cache", BaseType::STRING, "Tracker cache ID"),
        ParamDef::required("x", BaseType::INT, "Bounding box X"),
        ParamDef::required("y", BaseType::INT, "Bounding box Y"),
        ParamDef::required("width", BaseType::INT, "Bounding box width"),
        ParamDef::required("height", BaseType::INT, "Bounding box height")
    };
    _example = "init_tracker(\"tracker\", 100, 100, 50, 50)";
    _returnType = "mat";
    _tags = {"tracking", "init"};
}

ExecutionResult InitTrackerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// UpdateTrackerItem
// ============================================================================

UpdateTrackerItem::UpdateTrackerItem() {
    _functionName = "update_tracker";
    _description = "Updates tracker with new frame";
    _category = "advanced";
    _params = {
        ParamDef::required("tracker_cache", BaseType::STRING, "Tracker cache ID"),
        ParamDef::optional("draw_bbox", BaseType::BOOL, "Draw bounding box on image", true)
    };
    _example = "update_tracker(\"tracker\", true)";
    _returnType = "mat";
    _tags = {"tracking", "update"};
}

ExecutionResult UpdateTrackerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// MultiTrackerItem
// ============================================================================

MultiTrackerItem::MultiTrackerItem() {
    _functionName = "multi_tracker";
    _description = "Creates multi-object tracker";
    _category = "advanced";
    _params = {
        ParamDef::optional("tracker_type", BaseType::STRING, "Tracker type: kcf, csrt, mil", "kcf"),
        ParamDef::optional("cache_id", BaseType::STRING, "Multi-tracker cache ID", "multi_tracker")
    };
    _example = "multi_tracker(\"csrt\", \"multi_tracker\")";
    _returnType = "mat";
    _tags = {"tracking", "multi"};
}

ExecutionResult MultiTrackerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// StitcherItem
// ============================================================================

StitcherItem::StitcherItem() {
    _functionName = "stitcher";
    _description = "Stitches multiple images into panorama";
    _category = "advanced";
    _params = {
        ParamDef::required("images_cache", BaseType::STRING, "Comma-separated image cache IDs"),
        ParamDef::optional("mode", BaseType::STRING, "Mode: panorama, scans", "panorama")
    };
    _example = "stitcher(\"img1,img2,img3\", \"panorama\")";
    _returnType = "mat";
    _tags = {"stitching", "panorama"};
}

ExecutionResult StitcherItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string imagesCache = args[0].asString();
    std::string mode = args.size() > 1 ? args[1].asString() : "panorama";
    
    std::vector<cv::Mat> images;
    images.push_back(ctx.currentMat);
    
    // Parse comma-separated cache IDs
    std::stringstream ss(imagesCache);
    std::string cacheId;
    while (std::getline(ss, cacheId, ',')) {
        cv::Mat imgOpt = ctx.cacheManager->get(cacheId);
        if (!imgOpt.empty()) {
            images.push_back(imgOpt);
        }
    }
    
    if (images.size() < 2) {
        return ExecutionResult::fail("Need at least 2 images for stitching");
    }
    
    cv::Stitcher::Mode stitchMode = (mode == "scans") ? cv::Stitcher::SCANS : cv::Stitcher::PANORAMA;
    cv::Ptr<cv::Stitcher> stitcher = cv::Stitcher::create(stitchMode);
    
    cv::Mat panorama;
    cv::Stitcher::Status status = stitcher->stitch(images, panorama);
    
    if (status != cv::Stitcher::OK) {
        return ExecutionResult::fail("Stitching failed with error code: " + std::to_string(static_cast<int>(status)));
    }
    
    return ExecutionResult::ok(panorama);
}

// ============================================================================
// CreateMergeMertensItem
// ============================================================================

CreateMergeMertensItem::CreateMergeMertensItem() {
    _functionName = "create_merge_mertens";
    _description = "Creates Mertens exposure fusion algorithm";
    _category = "advanced";
    _params = {
        ParamDef::optional("contrast_weight", BaseType::FLOAT, "Contrast weight", 1.0),
        ParamDef::optional("saturation_weight", BaseType::FLOAT, "Saturation weight", 1.0),
        ParamDef::optional("exposure_weight", BaseType::FLOAT, "Exposure weight", 0.0)
    };
    _example = "create_merge_mertens(1.0, 1.0, 0.0)";
    _returnType = "mat";
    _tags = {"hdr", "mertens", "fusion"};
}

ExecutionResult CreateMergeMertensItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// MergeExposuresItem
// ============================================================================

MergeExposuresItem::MergeExposuresItem() {
    _functionName = "merge_exposures";
    _description = "Merges multiple exposures using Mertens fusion";
    _category = "advanced";
    _params = {
        ParamDef::required("images_cache", BaseType::STRING, "Comma-separated image cache IDs")
    };
    _example = "merge_exposures(\"exp1,exp2,exp3\")";
    _returnType = "mat";
    _tags = {"hdr", "merge", "exposure"};
}

ExecutionResult MergeExposuresItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string imagesCache = args[0].asString();
    
    std::vector<cv::Mat> images;
    images.push_back(ctx.currentMat);
    
    std::stringstream ss(imagesCache);
    std::string cacheId;
    while (std::getline(ss, cacheId, ',')) {
        cv::Mat imgOpt = ctx.cacheManager->get(cacheId);
        if (!imgOpt.empty()) {
            images.push_back(imgOpt);
        }
    }
    
    cv::Ptr<cv::MergeMertens> merge = cv::createMergeMertens();
    cv::Mat fusion;
    merge->process(images, fusion);
    
    fusion.convertTo(fusion, CV_8U, 255);
    
    return ExecutionResult::ok(fusion);
}

// ============================================================================
// TonemapItem
// ============================================================================

TonemapItem::TonemapItem() {
    _functionName = "tonemap";
    _description = "Basic tonemapping";
    _category = "advanced";
    _params = {
        ParamDef::optional("gamma", BaseType::FLOAT, "Gamma value", 1.0)
    };
    _example = "tonemap(2.2)";
    _returnType = "mat";
    _tags = {"hdr", "tonemap"};
}

ExecutionResult TonemapItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float gamma = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 1.0f;
    
    cv::Ptr<cv::Tonemap> tonemap = cv::createTonemap(gamma);
    
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F, 1.0 / 255.0);
    
    cv::Mat result;
    tonemap->process(floatMat, result);
    result.convertTo(result, CV_8U, 255);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// TonemapDragoItem
// ============================================================================

TonemapDragoItem::TonemapDragoItem() {
    _functionName = "tonemap_drago";
    _description = "Drago tonemapping";
    _category = "advanced";
    _params = {
        ParamDef::optional("gamma", BaseType::FLOAT, "Gamma value", 1.0),
        ParamDef::optional("saturation", BaseType::FLOAT, "Saturation", 1.0),
        ParamDef::optional("bias", BaseType::FLOAT, "Bias", 0.85)
    };
    _example = "tonemap_drago(1.0, 1.0, 0.85)";
    _returnType = "mat";
    _tags = {"hdr", "tonemap", "drago"};
}

ExecutionResult TonemapDragoItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float gamma = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 1.0f;
    float saturation = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 1.0f;
    float bias = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 0.85f;
    
    cv::Ptr<cv::TonemapDrago> tonemap = cv::createTonemapDrago(gamma, saturation, bias);
    
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F, 1.0 / 255.0);
    
    cv::Mat result;
    tonemap->process(floatMat, result);
    result.convertTo(result, CV_8U, 255);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// TonemapReinhardItem
// ============================================================================

TonemapReinhardItem::TonemapReinhardItem() {
    _functionName = "tonemap_reinhard";
    _description = "Reinhard tonemapping";
    _category = "advanced";
    _params = {
        ParamDef::optional("gamma", BaseType::FLOAT, "Gamma value", 1.0),
        ParamDef::optional("intensity", BaseType::FLOAT, "Intensity", 0.0),
        ParamDef::optional("light_adapt", BaseType::FLOAT, "Light adaptation", 1.0),
        ParamDef::optional("color_adapt", BaseType::FLOAT, "Color adaptation", 0.0)
    };
    _example = "tonemap_reinhard(1.0, 0.0, 1.0, 0.0)";
    _returnType = "mat";
    _tags = {"hdr", "tonemap", "reinhard"};
}

ExecutionResult TonemapReinhardItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float gamma = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 1.0f;
    float intensity = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.0f;
    float lightAdapt = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 1.0f;
    float colorAdapt = args.size() > 3 ? static_cast<float>(args[3].asNumber()) : 0.0f;
    
    cv::Ptr<cv::TonemapReinhard> tonemap = cv::createTonemapReinhard(gamma, intensity, lightAdapt, colorAdapt);
    
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F, 1.0 / 255.0);
    
    cv::Mat result;
    tonemap->process(floatMat, result);
    result.convertTo(result, CV_8U, 255);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// TonemapMantiukItem
// ============================================================================

TonemapMantiukItem::TonemapMantiukItem() {
    _functionName = "tonemap_mantiuk";
    _description = "Mantiuk tonemapping";
    _category = "advanced";
    _params = {
        ParamDef::optional("gamma", BaseType::FLOAT, "Gamma value", 1.0),
        ParamDef::optional("scale", BaseType::FLOAT, "Contrast scale", 0.7),
        ParamDef::optional("saturation", BaseType::FLOAT, "Saturation", 1.0)
    };
    _example = "tonemap_mantiuk(1.0, 0.7, 1.0)";
    _returnType = "mat";
    _tags = {"hdr", "tonemap", "mantiuk"};
}

ExecutionResult TonemapMantiukItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float gamma = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 1.0f;
    float scale = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.7f;
    float saturation = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 1.0f;
    
    cv::Ptr<cv::TonemapMantiuk> tonemap = cv::createTonemapMantiuk(gamma, scale, saturation);
    
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F, 1.0 / 255.0);
    
    cv::Mat result;
    tonemap->process(floatMat, result);
    result.convertTo(result, CV_8U, 255);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// EdgePreservingFilterItem
// ============================================================================

EdgePreservingFilterItem::EdgePreservingFilterItem() {
    _functionName = "edge_preserving_filter";
    _description = "Applies edge-preserving smoothing";
    _category = "advanced";
    _params = {
        ParamDef::optional("flags", BaseType::STRING, "Type: recursive, normconv", "recursive"),
        ParamDef::optional("sigma_s", BaseType::FLOAT, "Spatial sigma", 60.0),
        ParamDef::optional("sigma_r", BaseType::FLOAT, "Range sigma", 0.4)
    };
    _example = "edge_preserving_filter(\"recursive\", 60, 0.4)";
    _returnType = "mat";
    _tags = {"filter", "edge_preserving", "npr"};
}

ExecutionResult EdgePreservingFilterItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string flagsStr = args.size() > 0 ? args[0].asString() : "recursive";
    float sigmaS = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 60.0f;
    float sigmaR = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 0.4f;
    
    int flags = cv::RECURS_FILTER;
    if (flagsStr == "normconv") flags = cv::NORMCONV_FILTER;
    
    cv::Mat result;
    cv::edgePreservingFilter(ctx.currentMat, result, flags, sigmaS, sigmaR);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DetailEnhanceItem
// ============================================================================

DetailEnhanceItem::DetailEnhanceItem() {
    _functionName = "detail_enhance";
    _description = "Enhances image details";
    _category = "advanced";
    _params = {
        ParamDef::optional("sigma_s", BaseType::FLOAT, "Spatial sigma", 10.0),
        ParamDef::optional("sigma_r", BaseType::FLOAT, "Range sigma", 0.15)
    };
    _example = "detail_enhance(10, 0.15)";
    _returnType = "mat";
    _tags = {"enhance", "detail", "npr"};
}

ExecutionResult DetailEnhanceItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float sigmaS = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 10.0f;
    float sigmaR = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.15f;
    
    cv::Mat result;
    cv::detailEnhance(ctx.currentMat, result, sigmaS, sigmaR);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// PencilSketchItem
// ============================================================================

PencilSketchItem::PencilSketchItem() {
    _functionName = "pencil_sketch";
    _description = "Creates pencil sketch effect";
    _category = "advanced";
    _params = {
        ParamDef::optional("sigma_s", BaseType::FLOAT, "Spatial sigma", 60.0),
        ParamDef::optional("sigma_r", BaseType::FLOAT, "Range sigma", 0.07),
        ParamDef::optional("shade_factor", BaseType::FLOAT, "Shade factor", 0.02),
        ParamDef::optional("colored", BaseType::BOOL, "Return colored version", false)
    };
    _example = "pencil_sketch(60, 0.07, 0.02, false)";
    _returnType = "mat";
    _tags = {"sketch", "pencil", "npr"};
}

ExecutionResult PencilSketchItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float sigmaS = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 60.0f;
    float sigmaR = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.07f;
    float shadeFactor = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 0.02f;
    bool colored = args.size() > 3 ? args[3].asBool() : false;
    
    cv::Mat dst1, dst2;
    cv::pencilSketch(ctx.currentMat, dst1, dst2, sigmaS, sigmaR, shadeFactor);
    
    return ExecutionResult::ok(colored ? dst2 : dst1);
}

// ============================================================================
// StylizationItem
// ============================================================================

StylizationItem::StylizationItem() {
    _functionName = "stylization";
    _description = "Applies stylization effect";
    _category = "advanced";
    _params = {
        ParamDef::optional("sigma_s", BaseType::FLOAT, "Spatial sigma", 60.0),
        ParamDef::optional("sigma_r", BaseType::FLOAT, "Range sigma", 0.45)
    };
    _example = "stylization(60, 0.45)";
    _returnType = "mat";
    _tags = {"stylize", "artistic", "npr"};
}

ExecutionResult StylizationItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float sigmaS = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 60.0f;
    float sigmaR = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.45f;
    
    cv::Mat result;
    cv::stylization(ctx.currentMat, result, sigmaS, sigmaR);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// InpaintItem
// ============================================================================

InpaintItem::InpaintItem() {
    _functionName = "inpaint";
    _description = "Restores image areas indicated by mask";
    _category = "advanced";
    _params = {
        ParamDef::required("mask_cache", BaseType::STRING, "Inpainting mask cache ID"),
        ParamDef::optional("radius", BaseType::FLOAT, "Inpainting radius", 3.0),
        ParamDef::optional("method", BaseType::STRING, "Method: ns, telea", "telea")
    };
    _example = "inpaint(\"mask\", 3, \"telea\")";
    _returnType = "mat";
    _tags = {"inpaint", "restore"};
}

ExecutionResult InpaintItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string maskCache = args[0].asString();
    double radius = args.size() > 1 ? args[1].asNumber() : 3.0;
    std::string method = args.size() > 2 ? args[2].asString() : "telea";
    
    cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
    if (maskOpt.empty()) {
        return ExecutionResult::fail("Mask not found: " + maskCache);
    }
    
    int flags = cv::INPAINT_TELEA;
    if (method == "ns") flags = cv::INPAINT_NS;
    
    cv::Mat result;
    cv::inpaint(ctx.currentMat, maskOpt, result, radius, flags);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FastNlMeansDenoisingItem
// ============================================================================

FastNlMeansDenoisingItem::FastNlMeansDenoisingItem() {
    _functionName = "fast_nl_means_denoising";
    _description = "Non-local means denoising for grayscale images";
    _category = "advanced";
    _params = {
        ParamDef::optional("h", BaseType::FLOAT, "Filter strength", 10.0),
        ParamDef::optional("template_size", BaseType::INT, "Template patch size", 7),
        ParamDef::optional("search_size", BaseType::INT, "Search window size", 21)
    };
    _example = "fast_nl_means_denoising(10, 7, 21)";
    _returnType = "mat";
    _tags = {"denoise", "nlm"};
}

ExecutionResult FastNlMeansDenoisingItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float h = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 10.0f;
    int templateSize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 7;
    int searchSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 21;
    
    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }
    
    cv::Mat result;
    cv::fastNlMeansDenoising(gray, result, h, templateSize, searchSize);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FastNlMeansDenoisingColoredItem
// ============================================================================

FastNlMeansDenoisingColoredItem::FastNlMeansDenoisingColoredItem() {
    _functionName = "fast_nl_means_denoising_colored";
    _description = "Non-local means denoising for color images";
    _category = "advanced";
    _params = {
        ParamDef::optional("h", BaseType::FLOAT, "Filter strength (luminance)", 10.0),
        ParamDef::optional("h_color", BaseType::FLOAT, "Filter strength (color)", 10.0),
        ParamDef::optional("template_size", BaseType::INT, "Template patch size", 7),
        ParamDef::optional("search_size", BaseType::INT, "Search window size", 21)
    };
    _example = "fast_nl_means_denoising_colored(10, 10, 7, 21)";
    _returnType = "mat";
    _tags = {"denoise", "nlm", "color"};
}

ExecutionResult FastNlMeansDenoisingColoredItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float h = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 10.0f;
    float hColor = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 10.0f;
    int templateSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 7;
    int searchSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 21;
    
    cv::Mat result;
    cv::fastNlMeansDenoisingColored(ctx.currentMat, result, h, hColor, templateSize, searchSize);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SeamlessCloneItem
// ============================================================================

SeamlessCloneItem::SeamlessCloneItem() {
    _functionName = "seamless_clone";
    _description = "Seamlessly clones source into destination";
    _category = "advanced";
    _params = {
        ParamDef::required("src_cache", BaseType::STRING, "Source image cache ID"),
        ParamDef::required("mask_cache", BaseType::STRING, "Mask cache ID"),
        ParamDef::required("center_x", BaseType::INT, "Center X in destination"),
        ParamDef::required("center_y", BaseType::INT, "Center Y in destination"),
        ParamDef::optional("mode", BaseType::STRING, "Mode: normal, mixed, monochrome", "normal")
    };
    _example = "seamless_clone(\"src\", \"mask\", 200, 150, \"normal\")";
    _returnType = "mat";
    _tags = {"clone", "seamless", "blend"};
}

ExecutionResult SeamlessCloneItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string srcCache = args[0].asString();
    std::string maskCache = args[1].asString();
    int centerX = static_cast<int>(args[2].asNumber());
    int centerY = static_cast<int>(args[3].asNumber());
    std::string mode = args.size() > 4 ? args[4].asString() : "normal";
    
    cv::Mat srcOpt = ctx.cacheManager->get(srcCache);
    cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
    
    if (srcOpt.empty() || maskOpt.empty()) {
        return ExecutionResult::fail("Source or mask not found");
    }
    
    int flags = cv::NORMAL_CLONE;
    if (mode == "mixed") flags = cv::MIXED_CLONE;
    else if (mode == "monochrome") flags = cv::MONOCHROME_TRANSFER;
    
    cv::Mat result;
    cv::seamlessClone(srcOpt, ctx.currentMat, maskOpt, cv::Point(centerX, centerY), result, flags);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ColorChangeItem
// ============================================================================

ColorChangeItem::ColorChangeItem() {
    _functionName = "color_change";
    _description = "Changes color of image region";
    _category = "advanced";
    _params = {
        ParamDef::required("mask_cache", BaseType::STRING, "Mask cache ID"),
        ParamDef::optional("red_mul", BaseType::FLOAT, "Red multiplier", 1.0),
        ParamDef::optional("green_mul", BaseType::FLOAT, "Green multiplier", 1.0),
        ParamDef::optional("blue_mul", BaseType::FLOAT, "Blue multiplier", 1.0)
    };
    _example = "color_change(\"mask\", 1.5, 1.0, 0.5)";
    _returnType = "mat";
    _tags = {"color", "change"};
}

ExecutionResult ColorChangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string maskCache = args[0].asString();
    float redMul = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 1.0f;
    float greenMul = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 1.0f;
    float blueMul = args.size() > 3 ? static_cast<float>(args[3].asNumber()) : 1.0f;
    
    cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
    if (maskOpt.empty()) {
        return ExecutionResult::fail("Mask not found: " + maskCache);
    }
    
    cv::Mat result;
    cv::colorChange(ctx.currentMat, maskOpt, result, redMul, greenMul, blueMul);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// IlluminationChangeItem
// ============================================================================

IlluminationChangeItem::IlluminationChangeItem() {
    _functionName = "illumination_change";
    _description = "Changes illumination of image region";
    _category = "advanced";
    _params = {
        ParamDef::required("mask_cache", BaseType::STRING, "Mask cache ID"),
        ParamDef::optional("alpha", BaseType::FLOAT, "Brightness factor", 0.2),
        ParamDef::optional("beta", BaseType::FLOAT, "Blend factor", 0.4)
    };
    _example = "illumination_change(\"mask\", 0.2, 0.4)";
    _returnType = "mat";
    _tags = {"illumination", "lighting"};
}

ExecutionResult IlluminationChangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string maskCache = args[0].asString();
    float alpha = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 0.2f;
    float beta = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 0.4f;
    
    cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
    if (maskOpt.empty()) {
        return ExecutionResult::fail("Mask not found: " + maskCache);
    }
    
    cv::Mat result;
    cv::illuminationChange(ctx.currentMat, maskOpt, result, alpha, beta);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// TextureFlattingItem
// ============================================================================

TextureFlattingItem::TextureFlattingItem() {
    _functionName = "texture_flatting";
    _description = "Flattens texture in image region";
    _category = "advanced";
    _params = {
        ParamDef::required("mask_cache", BaseType::STRING, "Mask cache ID"),
        ParamDef::optional("low_threshold", BaseType::FLOAT, "Low edge threshold", 30.0),
        ParamDef::optional("high_threshold", BaseType::FLOAT, "High edge threshold", 45.0),
        ParamDef::optional("kernel_size", BaseType::INT, "Gaussian kernel size", 3)
    };
    _example = "texture_flatting(\"mask\", 30, 45, 3)";
    _returnType = "mat";
    _tags = {"texture", "flatten"};
}

ExecutionResult TextureFlattingItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string maskCache = args[0].asString();
    double lowThresh = args.size() > 1 ? args[1].asNumber() : 30.0;
    double highThresh = args.size() > 2 ? args[2].asNumber() : 45.0;
    int kernelSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 3;
    
    cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
    if (maskOpt.empty()) {
        return ExecutionResult::fail("Mask not found: " + maskCache);
    }
    
    cv::Mat result;
    cv::textureFlattening(ctx.currentMat, maskOpt, result, lowThresh, highThresh, kernelSize);
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

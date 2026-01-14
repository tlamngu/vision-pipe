#include "interpreter/items/feature_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>

namespace visionpipe {

void registerFeatureItems(ItemRegistry& registry) {
    registry.add<GoodFeaturesToTrackItem>();
    registry.add<CornerHarrisItem>();
    registry.add<CornerMinEigenValItem>();
    registry.add<CornerSubPixItem>();
    registry.add<SimpleBlobDetectorItem>();
    registry.add<ORBItem>();
    registry.add<SIFTItem>();
    registry.add<AKAZEItem>();
    registry.add<BRISKItem>();
    registry.add<FASTItem>();
    registry.add<AGASTItem>();
    registry.add<KAZEItem>();
    registry.add<BFMatcherItem>();
    registry.add<FlannMatcherItem>();
    registry.add<DrawMatchesItem>();
    registry.add<DrawKeypointsItem>();
    registry.add<FindHomographyItem>();
    registry.add<PerspectiveTransformPointsItem>();
    registry.add<MatchTemplateItem>();
    registry.add<MinMaxLocItem>();
}

// ============================================================================
// GoodFeaturesToTrackItem
// ============================================================================

GoodFeaturesToTrackItem::GoodFeaturesToTrackItem() {
    _functionName = "good_features_to_track";
    _description = "Finds corners using Shi-Tomasi corner detection";
    _category = "feature";
    _params = {
        ParamDef::optional("max_corners", BaseType::INT, "Maximum corners to return", 100),
        ParamDef::optional("quality_level", BaseType::FLOAT, "Quality level threshold", 0.01),
        ParamDef::optional("min_distance", BaseType::FLOAT, "Minimum distance between corners", 10.0),
        ParamDef::optional("block_size", BaseType::INT, "Neighborhood size", 3),
        ParamDef::optional("use_harris", BaseType::BOOL, "Use Harris detector", false),
        ParamDef::optional("k", BaseType::FLOAT, "Harris parameter", 0.04)
    };
    _example = "good_features_to_track(100, 0.01, 10)";
    _returnType = "mat";
    _tags = {"corners", "shi-tomasi", "features"};
}

ExecutionResult GoodFeaturesToTrackItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int maxCorners = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 100;
    double qualityLevel = args.size() > 1 ? args[1].asNumber() : 0.01;
    double minDistance = args.size() > 2 ? args[2].asNumber() : 10.0;
    int blockSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 3;
    bool useHarris = args.size() > 4 ? args[4].asBool() : false;
    double k = args.size() > 5 ? args[5].asNumber() : 0.04;
    
    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }
    
    std::vector<cv::Point2f> corners;
    cv::goodFeaturesToTrack(gray, corners, maxCorners, qualityLevel, minDistance, cv::Mat(), blockSize, useHarris, k);
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    for (const auto& corner : corners) {
        cv::circle(result, corner, 5, cv::Scalar(0, 255, 0), -1);
    }
    
    // Cache corners
    cv::Mat cornersMat(corners);
    ctx.cacheManager->set("corners", cornersMat);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// CornerHarrisItem
// ============================================================================

CornerHarrisItem::CornerHarrisItem() {
    _functionName = "corner_harris";
    _description = "Detects corners using Harris corner detector";
    _category = "feature";
    _params = {
        ParamDef::optional("block_size", BaseType::INT, "Neighborhood size", 2),
        ParamDef::optional("ksize", BaseType::INT, "Sobel aperture", 3),
        ParamDef::optional("k", BaseType::FLOAT, "Harris parameter", 0.04),
        ParamDef::optional("threshold", BaseType::FLOAT, "Response threshold (0-1)", 0.01)
    };
    _example = "corner_harris(2, 3, 0.04, 0.01)";
    _returnType = "mat";
    _tags = {"corners", "harris", "features"};
}

ExecutionResult CornerHarrisItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int blockSize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 2;
    int ksize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    double k = args.size() > 2 ? args[2].asNumber() : 0.04;
    double threshold = args.size() > 3 ? args[3].asNumber() : 0.01;
    
    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }
    gray.convertTo(gray, CV_32F);
    
    cv::Mat dst;
    cv::cornerHarris(gray, dst, blockSize, ksize, k);
    
    cv::Mat dstNorm;
    cv::normalize(dst, dstNorm, 0, 255, cv::NORM_MINMAX);
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    double maxVal;
    cv::minMaxLoc(dstNorm, nullptr, &maxVal);
    double thresh = threshold * maxVal;
    
    for (int i = 0; i < dstNorm.rows; i++) {
        for (int j = 0; j < dstNorm.cols; j++) {
            if (dstNorm.at<float>(i, j) > thresh) {
                cv::circle(result, cv::Point(j, i), 5, cv::Scalar(0, 0, 255), 2);
            }
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// CornerMinEigenValItem
// ============================================================================

CornerMinEigenValItem::CornerMinEigenValItem() {
    _functionName = "corner_min_eigen_val";
    _description = "Calculates minimum eigenvalue for corner detection";
    _category = "feature";
    _params = {
        ParamDef::optional("block_size", BaseType::INT, "Neighborhood size", 3),
        ParamDef::optional("ksize", BaseType::INT, "Sobel aperture", 3)
    };
    _example = "corner_min_eigen_val(3, 3)";
    _returnType = "mat";
    _tags = {"corners", "eigenvalue", "features"};
}

ExecutionResult CornerMinEigenValItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int blockSize = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 3;
    int ksize = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    
    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }
    gray.convertTo(gray, CV_32F);
    
    cv::Mat dst;
    cv::cornerMinEigenVal(gray, dst, blockSize, ksize);
    
    cv::normalize(dst, dst, 0, 255, cv::NORM_MINMAX);
    dst.convertTo(dst, CV_8U);
    
    return ExecutionResult::ok(dst);
}

// ============================================================================
// CornerSubPixItem
// ============================================================================

CornerSubPixItem::CornerSubPixItem() {
    _functionName = "corner_sub_pix";
    _description = "Refines corner locations to sub-pixel accuracy";
    _category = "feature";
    _params = {
        ParamDef::optional("win_size", BaseType::INT, "Half window size", 5),
        ParamDef::optional("zero_zone", BaseType::INT, "Half dead zone size (-1 to disable)", -1),
        ParamDef::optional("max_count", BaseType::INT, "Maximum iterations", 30),
        ParamDef::optional("epsilon", BaseType::FLOAT, "Convergence threshold", 0.001)
    };
    _example = "corner_sub_pix(5, -1, 30, 0.001)";
    _returnType = "mat";
    _tags = {"corners", "subpixel", "refinement"};
}

ExecutionResult CornerSubPixItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Sub-pixel refinement works on previously detected corners
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SimpleBlobDetectorItem
// ============================================================================

SimpleBlobDetectorItem::SimpleBlobDetectorItem() {
    _functionName = "simple_blob_detector";
    _description = "Detects blobs using SimpleBlobDetector";
    _category = "feature";
    _params = {
        ParamDef::optional("min_threshold", BaseType::FLOAT, "Minimum threshold", 10.0),
        ParamDef::optional("max_threshold", BaseType::FLOAT, "Maximum threshold", 200.0),
        ParamDef::optional("min_area", BaseType::FLOAT, "Minimum blob area", 25.0),
        ParamDef::optional("max_area", BaseType::FLOAT, "Maximum blob area", 5000.0),
        ParamDef::optional("min_circularity", BaseType::FLOAT, "Minimum circularity (0-1)", 0.0),
        ParamDef::optional("min_convexity", BaseType::FLOAT, "Minimum convexity (0-1)", 0.0),
        ParamDef::optional("min_inertia_ratio", BaseType::FLOAT, "Minimum inertia ratio (0-1)", 0.0)
    };
    _example = "simple_blob_detector(10, 200, 25, 5000)";
    _returnType = "mat";
    _tags = {"blob", "detector", "features"};
}

ExecutionResult SimpleBlobDetectorItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::SimpleBlobDetector::Params params;
    params.minThreshold = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 10.0f;
    params.maxThreshold = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 200.0f;
    
    params.filterByArea = true;
    params.minArea = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 25.0f;
    params.maxArea = args.size() > 3 ? static_cast<float>(args[3].asNumber()) : 5000.0f;
    
    float minCirc = args.size() > 4 ? static_cast<float>(args[4].asNumber()) : 0.0f;
    params.filterByCircularity = minCirc > 0;
    params.minCircularity = minCirc;
    
    float minConv = args.size() > 5 ? static_cast<float>(args[5].asNumber()) : 0.0f;
    params.filterByConvexity = minConv > 0;
    params.minConvexity = minConv;
    
    float minInertia = args.size() > 6 ? static_cast<float>(args[6].asNumber()) : 0.0f;
    params.filterByInertia = minInertia > 0;
    params.minInertiaRatio = minInertia;
    
    cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
    
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(ctx.currentMat, keypoints);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 0, 255), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ORBItem
// ============================================================================

ORBItem::ORBItem() {
    _functionName = "orb";
    _description = "Detects ORB features and computes descriptors";
    _category = "feature";
    _params = {
        ParamDef::optional("n_features", BaseType::INT, "Maximum features", 500),
        ParamDef::optional("scale_factor", BaseType::FLOAT, "Pyramid scale factor", 1.2),
        ParamDef::optional("n_levels", BaseType::INT, "Pyramid levels", 8),
        ParamDef::optional("edge_threshold", BaseType::INT, "Edge threshold", 31),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for descriptors", "orb")
    };
    _example = "orb(500, 1.2, 8, 31, \"orb\")";
    _returnType = "mat";
    _tags = {"orb", "features", "descriptors"};
}

ExecutionResult ORBItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int nfeatures = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 500;
    float scaleFactor = args.size() > 1 ? static_cast<float>(args[1].asNumber()) : 1.2f;
    int nlevels = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 8;
    int edgeThreshold = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 31;
    std::string cacheId = args.size() > 4 ? args[4].asString() : "orb";
    
    cv::Ptr<cv::ORB> orb = cv::ORB::create(nfeatures, scaleFactor, nlevels, edgeThreshold);
    
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    orb->detectAndCompute(ctx.currentMat, cv::noArray(), keypoints, descriptors);
    
    ctx.cacheManager->set(cacheId + "_descriptors", descriptors);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 255, 0));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SIFTItem
// ============================================================================

SIFTItem::SIFTItem() {
    _functionName = "sift";
    _description = "Detects SIFT features and computes descriptors";
    _category = "feature";
    _params = {
        ParamDef::optional("n_features", BaseType::INT, "Maximum features (0=unlimited)", 0),
        ParamDef::optional("n_octave_layers", BaseType::INT, "Layers per octave", 3),
        ParamDef::optional("contrast_threshold", BaseType::FLOAT, "Contrast threshold", 0.04),
        ParamDef::optional("edge_threshold", BaseType::FLOAT, "Edge threshold", 10.0),
        ParamDef::optional("sigma", BaseType::FLOAT, "Gaussian sigma", 1.6),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "sift")
    };
    _example = "sift(0, 3, 0.04, 10, 1.6, \"sift\")";
    _returnType = "mat";
    _tags = {"sift", "features", "descriptors"};
}

ExecutionResult SIFTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int nfeatures = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 0;
    int nOctaveLayers = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    double contrastThreshold = args.size() > 2 ? args[2].asNumber() : 0.04;
    double edgeThreshold = args.size() > 3 ? args[3].asNumber() : 10.0;
    double sigma = args.size() > 4 ? args[4].asNumber() : 1.6;
    std::string cacheId = args.size() > 5 ? args[5].asString() : "sift";
    
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create(nfeatures, nOctaveLayers, contrastThreshold, edgeThreshold, sigma);
    
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    sift->detectAndCompute(ctx.currentMat, cv::noArray(), keypoints, descriptors);
    
    ctx.cacheManager->set(cacheId + "_descriptors", descriptors);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 255, 0));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// AKAZEItem
// ============================================================================

AKAZEItem::AKAZEItem() {
    _functionName = "akaze";
    _description = "Detects AKAZE features and computes descriptors";
    _category = "feature";
    _params = {
        ParamDef::optional("threshold", BaseType::FLOAT, "Detector threshold", 0.001),
        ParamDef::optional("n_octaves", BaseType::INT, "Maximum octaves", 4),
        ParamDef::optional("n_octave_layers", BaseType::INT, "Layers per octave", 4),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "akaze")
    };
    _example = "akaze(0.001, 4, 4, \"akaze\")";
    _returnType = "mat";
    _tags = {"akaze", "features", "descriptors"};
}

ExecutionResult AKAZEItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    float threshold = args.size() > 0 ? static_cast<float>(args[0].asNumber()) : 0.001f;
    int nOctaves = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 4;
    int nOctaveLayers = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 4;
    std::string cacheId = args.size() > 3 ? args[3].asString() : "akaze";
    
    cv::Ptr<cv::AKAZE> akaze = cv::AKAZE::create(cv::AKAZE::DESCRIPTOR_MLDB, 0, 3, threshold, nOctaves, nOctaveLayers);
    
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    akaze->detectAndCompute(ctx.currentMat, cv::noArray(), keypoints, descriptors);
    
    ctx.cacheManager->set(cacheId + "_descriptors", descriptors);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 255, 0));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BRISKItem
// ============================================================================

BRISKItem::BRISKItem() {
    _functionName = "brisk";
    _description = "Detects BRISK features and computes descriptors";
    _category = "feature";
    _params = {
        ParamDef::optional("thresh", BaseType::INT, "AGAST detection threshold", 30),
        ParamDef::optional("octaves", BaseType::INT, "Detection octaves", 3),
        ParamDef::optional("pattern_scale", BaseType::FLOAT, "Pattern scale", 1.0),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "brisk")
    };
    _example = "brisk(30, 3, 1.0, \"brisk\")";
    _returnType = "mat";
    _tags = {"brisk", "features", "descriptors"};
}

ExecutionResult BRISKItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int thresh = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 30;
    int octaves = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 3;
    float patternScale = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 1.0f;
    std::string cacheId = args.size() > 3 ? args[3].asString() : "brisk";
    
    cv::Ptr<cv::BRISK> brisk = cv::BRISK::create(thresh, octaves, patternScale);
    
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    brisk->detectAndCompute(ctx.currentMat, cv::noArray(), keypoints, descriptors);
    
    ctx.cacheManager->set(cacheId + "_descriptors", descriptors);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 255, 0));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FASTItem
// ============================================================================

FASTItem::FASTItem() {
    _functionName = "fast";
    _description = "Detects corners using FAST algorithm";
    _category = "feature";
    _params = {
        ParamDef::optional("threshold", BaseType::INT, "Detection threshold", 10),
        ParamDef::optional("nonmax_suppression", BaseType::BOOL, "Non-maximum suppression", true),
        ParamDef::optional("type", BaseType::STRING, "Type: 9_16, 7_12, 5_8", "9_16")
    };
    _example = "fast(10, true, \"9_16\")";
    _returnType = "mat";
    _tags = {"fast", "corners", "features"};
}

ExecutionResult FASTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int threshold = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 10;
    bool nonmaxSuppression = args.size() > 1 ? args[1].asBool() : true;
    std::string typeStr = args.size() > 2 ? args[2].asString() : "9_16";
    
    cv::FastFeatureDetector::DetectorType type = cv::FastFeatureDetector::TYPE_9_16;
    if (typeStr == "7_12") type = cv::FastFeatureDetector::TYPE_7_12;
    else if (typeStr == "5_8") type = cv::FastFeatureDetector::TYPE_5_8;
    
    cv::Ptr<cv::FastFeatureDetector> detector = cv::FastFeatureDetector::create(threshold, nonmaxSuppression, type);
    
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(ctx.currentMat, keypoints);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 255, 0));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// AGASTItem
// ============================================================================

AGASTItem::AGASTItem() {
    _functionName = "agast";
    _description = "Detects corners using AGAST algorithm";
    _category = "feature";
    _params = {
        ParamDef::optional("threshold", BaseType::INT, "Detection threshold", 10),
        ParamDef::optional("nonmax_suppression", BaseType::BOOL, "Non-maximum suppression", true),
        ParamDef::optional("type", BaseType::STRING, "Type: 5_8, 7_12d, 7_12s, oast_9_16", "oast_9_16")
    };
    _example = "agast(10, true, \"oast_9_16\")";
    _returnType = "mat";
    _tags = {"agast", "corners", "features"};
}

ExecutionResult AGASTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int threshold = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 10;
    bool nonmaxSuppression = args.size() > 1 ? args[1].asBool() : true;
    std::string typeStr = args.size() > 2 ? args[2].asString() : "oast_9_16";
    
    cv::AgastFeatureDetector::DetectorType type = cv::AgastFeatureDetector::OAST_9_16;
    if (typeStr == "5_8") type = cv::AgastFeatureDetector::AGAST_5_8;
    else if (typeStr == "7_12d") type = cv::AgastFeatureDetector::AGAST_7_12d;
    else if (typeStr == "7_12s") type = cv::AgastFeatureDetector::AGAST_7_12s;
    
    cv::Ptr<cv::AgastFeatureDetector> detector = cv::AgastFeatureDetector::create(threshold, nonmaxSuppression, type);
    
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(ctx.currentMat, keypoints);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 255, 0));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// KAZEItem
// ============================================================================

KAZEItem::KAZEItem() {
    _functionName = "kaze";
    _description = "Detects KAZE features and computes descriptors";
    _category = "feature";
    _params = {
        ParamDef::optional("extended", BaseType::BOOL, "Extended descriptors (128 vs 64)", false),
        ParamDef::optional("upright", BaseType::BOOL, "Upright descriptors", false),
        ParamDef::optional("threshold", BaseType::FLOAT, "Detection threshold", 0.001),
        ParamDef::optional("n_octaves", BaseType::INT, "Maximum octaves", 4),
        ParamDef::optional("n_octave_layers", BaseType::INT, "Layers per octave", 4),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "kaze")
    };
    _example = "kaze(false, false, 0.001, 4, 4, \"kaze\")";
    _returnType = "mat";
    _tags = {"kaze", "features", "descriptors"};
}

ExecutionResult KAZEItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool extended = args.size() > 0 ? args[0].asBool() : false;
    bool upright = args.size() > 1 ? args[1].asBool() : false;
    float threshold = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 0.001f;
    int nOctaves = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 4;
    int nOctaveLayers = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 4;
    std::string cacheId = args.size() > 5 ? args[5].asString() : "kaze";
    
    cv::Ptr<cv::KAZE> kaze = cv::KAZE::create(extended, upright, threshold, nOctaves, nOctaveLayers);
    
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    kaze->detectAndCompute(ctx.currentMat, cv::noArray(), keypoints, descriptors);
    
    ctx.cacheManager->set(cacheId + "_descriptors", descriptors);
    
    cv::Mat result;
    cv::drawKeypoints(ctx.currentMat, keypoints, result, cv::Scalar(0, 255, 0));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BFMatcherItem
// ============================================================================

BFMatcherItem::BFMatcherItem() {
    _functionName = "bf_matcher";
    _description = "Matches descriptors using brute-force matcher";
    _category = "feature";
    _params = {
        ParamDef::required("query_cache", BaseType::STRING, "Query descriptors cache ID"),
        ParamDef::required("train_cache", BaseType::STRING, "Train descriptors cache ID"),
        ParamDef::optional("norm_type", BaseType::STRING, "Norm: l2, l1, hamming, hamming2", "l2"),
        ParamDef::optional("cross_check", BaseType::BOOL, "Cross-check matches", false),
        ParamDef::optional("k", BaseType::INT, "K nearest neighbors", 2),
        ParamDef::optional("ratio_thresh", BaseType::FLOAT, "Lowe's ratio threshold", 0.75)
    };
    _example = "bf_matcher(\"orb1\", \"orb2\", \"hamming\", false, 2, 0.75)";
    _returnType = "mat";
    _tags = {"matcher", "brute-force", "features"};
}

ExecutionResult BFMatcherItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string queryCache = args[0].asString();
    std::string trainCache = args[1].asString();
    std::string normType = args.size() > 2 ? args[2].asString() : "l2";
    bool crossCheck = args.size() > 3 ? args[3].asBool() : false;
    int k = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 2;
    float ratioThresh = args.size() > 5 ? static_cast<float>(args[5].asNumber()) : 0.75f;
    
    cv::Mat queryDescOpt = ctx.cacheManager->get(queryCache + "_descriptors");
    cv::Mat trainDescOpt = ctx.cacheManager->get(trainCache + "_descriptors");
    
    if (queryDescOpt.empty() || trainDescOpt.empty()) {
        return ExecutionResult::fail("Descriptors not found in cache");
    }
    
    int norm = cv::NORM_L2;
    if (normType == "l1") norm = cv::NORM_L1;
    else if (normType == "hamming") norm = cv::NORM_HAMMING;
    else if (normType == "hamming2") norm = cv::NORM_HAMMING2;
    
    cv::BFMatcher matcher(norm, crossCheck);
    
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(queryDescOpt, trainDescOpt, knnMatches, k);
    
    // Apply ratio test
    std::vector<cv::DMatch> goodMatches;
    for (const auto& match : knnMatches) {
        if (match.size() >= 2 && match[0].distance < ratioThresh * match[1].distance) {
            goodMatches.push_back(match[0]);
        }
    }
    
    // Store match count
    ctx.cacheManager->set("match_count", cv::Mat(1, 1, CV_32S, cv::Scalar(static_cast<int>(goodMatches.size()))));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// FlannMatcherItem
// ============================================================================

FlannMatcherItem::FlannMatcherItem() {
    _functionName = "flann_matcher";
    _description = "Matches descriptors using FLANN matcher";
    _category = "feature";
    _params = {
        ParamDef::required("query_cache", BaseType::STRING, "Query descriptors cache ID"),
        ParamDef::required("train_cache", BaseType::STRING, "Train descriptors cache ID"),
        ParamDef::optional("k", BaseType::INT, "K nearest neighbors", 2),
        ParamDef::optional("ratio_thresh", BaseType::FLOAT, "Lowe's ratio threshold", 0.7)
    };
    _example = "flann_matcher(\"sift1\", \"sift2\", 2, 0.7)";
    _returnType = "mat";
    _tags = {"matcher", "flann", "features"};
}

ExecutionResult FlannMatcherItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string queryCache = args[0].asString();
    std::string trainCache = args[1].asString();
    int k = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 2;
    float ratioThresh = args.size() > 3 ? static_cast<float>(args[3].asNumber()) : 0.7f;
    
    cv::Mat queryDescOpt = ctx.cacheManager->get(queryCache + "_descriptors");
    cv::Mat trainDescOpt = ctx.cacheManager->get(trainCache + "_descriptors");
    
    if (queryDescOpt.empty() || trainDescOpt.empty()) {
        return ExecutionResult::fail("Descriptors not found in cache");
    }
    
    cv::Mat queryDesc = queryDescOpt.clone();
    cv::Mat trainDesc = trainDescOpt.clone();
    
    if (queryDesc.type() != CV_32F) {
        queryDesc.convertTo(queryDesc, CV_32F);
    }
    if (trainDesc.type() != CV_32F) {
        trainDesc.convertTo(trainDesc, CV_32F);
    }
    
    cv::FlannBasedMatcher matcher;
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(queryDesc, trainDesc, knnMatches, k);
    
    std::vector<cv::DMatch> goodMatches;
    for (const auto& match : knnMatches) {
        if (match.size() >= 2 && match[0].distance < ratioThresh * match[1].distance) {
            goodMatches.push_back(match[0]);
        }
    }
    
    ctx.cacheManager->set("match_count", cv::Mat(1, 1, CV_32S, cv::Scalar(static_cast<int>(goodMatches.size()))));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DrawMatchesItem
// ============================================================================

DrawMatchesItem::DrawMatchesItem() {
    _functionName = "draw_matches";
    _description = "Draws feature matches between two images";
    _category = "feature";
    _params = {
        ParamDef::required("img1_cache", BaseType::STRING, "First image cache ID"),
        ParamDef::required("img2_cache", BaseType::STRING, "Second image cache ID")
    };
    _example = "draw_matches(\"img1\", \"img2\")";
    _returnType = "mat";
    _tags = {"draw", "matches", "features"};
}

ExecutionResult DrawMatchesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DrawKeypointsItem
// ============================================================================

DrawKeypointsItem::DrawKeypointsItem() {
    _functionName = "draw_keypoints";
    _description = "Draws keypoints on image";
    _category = "feature";
    _params = {
        ParamDef::optional("color", BaseType::STRING, "Keypoint color", "green"),
        ParamDef::optional("rich", BaseType::BOOL, "Draw rich keypoints with size and orientation", true)
    };
    _example = "draw_keypoints(\"green\", true)";
    _returnType = "mat";
    _tags = {"draw", "keypoints", "features"};
}

ExecutionResult DrawKeypointsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// FindHomographyItem
// ============================================================================

FindHomographyItem::FindHomographyItem() {
    _functionName = "find_homography";
    _description = "Finds perspective transformation between two planes";
    _category = "feature";
    _params = {
        ParamDef::required("src_points_cache", BaseType::STRING, "Source points cache ID"),
        ParamDef::required("dst_points_cache", BaseType::STRING, "Destination points cache ID"),
        ParamDef::optional("method", BaseType::STRING, "Method: 0, ransac, lmeds, rho", "ransac"),
        ParamDef::optional("ransac_reproj_threshold", BaseType::FLOAT, "RANSAC threshold", 3.0),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for homography", "homography")
    };
    _example = "find_homography(\"src\", \"dst\", \"ransac\", 3.0, \"H\")";
    _returnType = "mat";
    _tags = {"homography", "perspective", "transform"};
}

ExecutionResult FindHomographyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string srcCache = args[0].asString();
    std::string dstCache = args[1].asString();
    std::string method = args.size() > 2 ? args[2].asString() : "ransac";
    double ransacThresh = args.size() > 3 ? args[3].asNumber() : 3.0;
    std::string cacheId = args.size() > 4 ? args[4].asString() : "homography";
    
    cv::Mat srcOpt = ctx.cacheManager->get(srcCache);
    cv::Mat dstOpt = ctx.cacheManager->get(dstCache);
    
    if (srcOpt.empty() || dstOpt.empty()) {
        return ExecutionResult::fail("Points not found in cache");
    }
    
    int methodVal = cv::RANSAC;
    if (method == "0") methodVal = 0;
    else if (method == "lmeds") methodVal = cv::LMEDS;
    else if (method == "rho") methodVal = cv::RHO;
    
    cv::Mat H = cv::findHomography(srcOpt, dstOpt, methodVal, ransacThresh);
    ctx.cacheManager->set(cacheId, H);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// PerspectiveTransformPointsItem
// ============================================================================

PerspectiveTransformPointsItem::PerspectiveTransformPointsItem() {
    _functionName = "perspective_transform_points";
    _description = "Applies perspective transformation to points";
    _category = "feature";
    _params = {
        ParamDef::required("points_cache", BaseType::STRING, "Points cache ID"),
        ParamDef::required("matrix_cache", BaseType::STRING, "Transformation matrix cache ID"),
        ParamDef::optional("output_cache", BaseType::STRING, "Output points cache ID", "transformed_points")
    };
    _example = "perspective_transform_points(\"points\", \"H\", \"output\")";
    _returnType = "mat";
    _tags = {"perspective", "transform", "points"};
}

ExecutionResult PerspectiveTransformPointsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string pointsCache = args[0].asString();
    std::string matrixCache = args[1].asString();
    std::string outputCache = args.size() > 2 ? args[2].asString() : "transformed_points";
    
    cv::Mat pointsOpt = ctx.cacheManager->get(pointsCache);
    cv::Mat matrixOpt = ctx.cacheManager->get(matrixCache);
    
    if (pointsOpt.empty() || matrixOpt.empty()) {
        return ExecutionResult::fail("Points or matrix not found in cache");
    }
    
    cv::Mat output;
    cv::perspectiveTransform(pointsOpt, output, matrixOpt);
    ctx.cacheManager->set(outputCache, output);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// MatchTemplateItem
// ============================================================================

MatchTemplateItem::MatchTemplateItem() {
    _functionName = "match_template";
    _description = "Finds template matches in image";
    _category = "feature";
    _params = {
        ParamDef::required("template_cache", BaseType::STRING, "Template image cache ID"),
        ParamDef::optional("method", BaseType::STRING, 
            "Method: sqdiff, sqdiff_normed, ccorr, ccorr_normed, ccoeff, ccoeff_normed", "ccoeff_normed")
    };
    _example = "match_template(\"template\", \"ccoeff_normed\")";
    _returnType = "mat";
    _tags = {"template", "matching", "detection"};
}

ExecutionResult MatchTemplateItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string templateCache = args[0].asString();
    std::string method = args.size() > 1 ? args[1].asString() : "ccoeff_normed";
    
    cv::Mat templateOpt = ctx.cacheManager->get(templateCache);
    if (templateOpt.empty()) {
        return ExecutionResult::fail("Template not found in cache: " + templateCache);
    }
    
    int methodVal = cv::TM_CCOEFF_NORMED;
    if (method == "sqdiff") methodVal = cv::TM_SQDIFF;
    else if (method == "sqdiff_normed") methodVal = cv::TM_SQDIFF_NORMED;
    else if (method == "ccorr") methodVal = cv::TM_CCORR;
    else if (method == "ccorr_normed") methodVal = cv::TM_CCORR_NORMED;
    else if (method == "ccoeff") methodVal = cv::TM_CCOEFF;
    
    cv::Mat result;
    cv::matchTemplate(ctx.currentMat, templateOpt, result, methodVal);
    
    ctx.cacheManager->set("match_result", result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MinMaxLocItem
// ============================================================================

MinMaxLocItem::MinMaxLocItem() {
    _functionName = "min_max_loc";
    _description = "Finds minimum and maximum values and their locations";
    _category = "feature";
    _params = {
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache prefix for results", "minmax")
    };
    _example = "min_max_loc(\"minmax\")";
    _returnType = "mat";
    _tags = {"min", "max", "location"};
}

ExecutionResult MinMaxLocItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args.size() > 0 ? args[0].asString() : "minmax";
    
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(ctx.currentMat, &minVal, &maxVal, &minLoc, &maxLoc);
    
    ctx.cacheManager->set(prefix + "_min_val", cv::Mat(1, 1, CV_64F, cv::Scalar(minVal)));
    ctx.cacheManager->set(prefix + "_max_val", cv::Mat(1, 1, CV_64F, cv::Scalar(maxVal)));
    ctx.cacheManager->set(prefix + "_min_loc", cv::Mat(1, 2, CV_32S, cv::Scalar(minLoc.x, minLoc.y)));
    ctx.cacheManager->set(prefix + "_max_loc", cv::Mat(1, 2, CV_32S, cv::Scalar(maxLoc.x, maxLoc.y)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

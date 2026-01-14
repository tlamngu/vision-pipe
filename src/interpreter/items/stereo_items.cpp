#include "interpreter/items/stereo_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>

namespace visionpipe {

void registerStereoItems(ItemRegistry& registry) {
    registry.add<StereoBMItem>();
    registry.add<StereoSGBMItem>();
    registry.add<ReprojectImageTo3DItem>();
    registry.add<FindChessboardCornersItem>();
    registry.add<FindCirclesGridItem>();
    registry.add<DrawChessboardCornersItem>();
    registry.add<CalibrateCameraItem>();
    registry.add<StereoCalibrateItem>();
    registry.add<StereoRectifyItem>();
    registry.add<InitUndistortRectifyMapItem>();
    registry.add<UndistortItem>();
    registry.add<UndistortPointsItem>();
    registry.add<DisparityWLSFilterItem>();
    registry.add<NormalizeDisparityItem>();
    registry.add<DisparityToDepthItem>();
    registry.add<FindFundamentalMatItem>();
    registry.add<FindEssentialMatItem>();
    registry.add<RecoverPoseItem>();
    registry.add<TriangulatePointsItem>();
    registry.add<SolvePnPItem>();
    registry.add<SolvePnPRansacItem>();
    registry.add<ProjectPointsItem>();
}

// ============================================================================
// StereoBMItem
// ============================================================================

StereoBMItem::StereoBMItem() {
    _functionName = "stereo_bm";
    _description = "Computes disparity using Block Matching algorithm";
    _category = "stereo";
    _params = {
        ParamDef::required("right_cache", BaseType::STRING, "Right image cache ID"),
        ParamDef::optional("num_disparities", BaseType::INT, "Number of disparities (multiple of 16)", 64),
        ParamDef::optional("block_size", BaseType::INT, "Block size (odd number)", 15),
        ParamDef::optional("prefilter_type", BaseType::STRING, "Type: normalized_response, xsobel", "normalized_response"),
        ParamDef::optional("prefilter_size", BaseType::INT, "Prefilter size (5-255, odd)", 9),
        ParamDef::optional("prefilter_cap", BaseType::INT, "Prefilter cap (1-63)", 31),
        ParamDef::optional("texture_threshold", BaseType::INT, "Texture threshold", 10),
        ParamDef::optional("uniqueness_ratio", BaseType::INT, "Uniqueness ratio", 15),
        ParamDef::optional("speckle_window_size", BaseType::INT, "Speckle filter window", 100),
        ParamDef::optional("speckle_range", BaseType::INT, "Speckle filter range", 32),
        ParamDef::optional("disp12_max_diff", BaseType::INT, "Left-right disparity check", 1)
    };
    _example = "stereo_bm(\"right\", 64, 15)";
    _returnType = "mat";
    _tags = {"stereo", "disparity", "bm"};
}

ExecutionResult StereoBMItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string rightCache = args[0].asString();
    int numDisparities = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 64;
    int blockSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 15;
    
    cv::Mat rightOpt = ctx.cacheManager->get(rightCache);
    if (rightOpt.empty()) {
        return ExecutionResult::fail("Right image not found: " + rightCache);
    }
    
    cv::Mat left = ctx.currentMat;
    cv::Mat right = rightOpt;
    
    if (left.channels() > 1) cv::cvtColor(left, left, cv::COLOR_BGR2GRAY);
    if (right.channels() > 1) cv::cvtColor(right, right, cv::COLOR_BGR2GRAY);
    
    numDisparities = (numDisparities / 16) * 16;
    if (blockSize % 2 == 0) blockSize++;
    
    cv::Ptr<cv::StereoBM> stereo = cv::StereoBM::create(numDisparities, blockSize);
    
    if (args.size() > 3) {
        std::string prefilterType = args[3].asString();
        if (prefilterType == "xsobel") {
            stereo->setPreFilterType(cv::StereoBM::PREFILTER_XSOBEL);
        }
    }
    if (args.size() > 4) stereo->setPreFilterSize(static_cast<int>(args[4].asNumber()));
    if (args.size() > 5) stereo->setPreFilterCap(static_cast<int>(args[5].asNumber()));
    if (args.size() > 6) stereo->setTextureThreshold(static_cast<int>(args[6].asNumber()));
    if (args.size() > 7) stereo->setUniquenessRatio(static_cast<int>(args[7].asNumber()));
    if (args.size() > 8) stereo->setSpeckleWindowSize(static_cast<int>(args[8].asNumber()));
    if (args.size() > 9) stereo->setSpeckleRange(static_cast<int>(args[9].asNumber()));
    if (args.size() > 10) stereo->setDisp12MaxDiff(static_cast<int>(args[10].asNumber()));
    
    cv::Mat disparity;
    stereo->compute(left, right, disparity);
    
    ctx.cacheManager->set("disparity_raw", disparity);
    
    return ExecutionResult::ok(disparity);
}

// ============================================================================
// StereoSGBMItem
// ============================================================================

StereoSGBMItem::StereoSGBMItem() {
    _functionName = "stereo_sgbm";
    _description = "Computes disparity using Semi-Global Block Matching";
    _category = "stereo";
    _params = {
        ParamDef::required("right_cache", BaseType::STRING, "Right image cache ID"),
        ParamDef::optional("min_disparity", BaseType::INT, "Minimum disparity", 0),
        ParamDef::optional("num_disparities", BaseType::INT, "Number of disparities (multiple of 16)", 64),
        ParamDef::optional("block_size", BaseType::INT, "Block size (odd number, 1-11)", 5),
        ParamDef::optional("p1", BaseType::INT, "P1 penalty (small disparity change)", 0),
        ParamDef::optional("p2", BaseType::INT, "P2 penalty (large disparity change)", 0),
        ParamDef::optional("disp12_max_diff", BaseType::INT, "Left-right check threshold", 1),
        ParamDef::optional("prefilter_cap", BaseType::INT, "Prefilter cap", 63),
        ParamDef::optional("uniqueness_ratio", BaseType::INT, "Uniqueness ratio", 10),
        ParamDef::optional("speckle_window_size", BaseType::INT, "Speckle filter window", 100),
        ParamDef::optional("speckle_range", BaseType::INT, "Speckle filter range", 32),
        ParamDef::optional("mode", BaseType::STRING, "Mode: sgbm, hh, sgbm_3way, hh4", "sgbm")
    };
    _example = "stereo_sgbm(\"right\", 0, 64, 5)";
    _returnType = "mat";
    _tags = {"stereo", "disparity", "sgbm"};
}

ExecutionResult StereoSGBMItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string rightCache = args[0].asString();
    int minDisparity = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    int numDisparities = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 64;
    int blockSize = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 5;
    
    cv::Mat rightOpt = ctx.cacheManager->get(rightCache);
    if (rightOpt.empty()) {
        return ExecutionResult::fail("Right image not found: " + rightCache);
    }
    
    cv::Mat left = ctx.currentMat;
    cv::Mat right = rightOpt;
    
    numDisparities = (numDisparities / 16) * 16;
    if (blockSize % 2 == 0) blockSize++;
    
    int cn = left.channels();
    int p1 = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 8 * cn * blockSize * blockSize;
    int p2 = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 32 * cn * blockSize * blockSize;
    int disp12MaxDiff = args.size() > 6 ? static_cast<int>(args[6].asNumber()) : 1;
    int preFilterCap = args.size() > 7 ? static_cast<int>(args[7].asNumber()) : 63;
    int uniquenessRatio = args.size() > 8 ? static_cast<int>(args[8].asNumber()) : 10;
    int speckleWindowSize = args.size() > 9 ? static_cast<int>(args[9].asNumber()) : 100;
    int speckleRange = args.size() > 10 ? static_cast<int>(args[10].asNumber()) : 32;
    
    int mode = cv::StereoSGBM::MODE_SGBM;
    if (args.size() > 11) {
        std::string modeStr = args[11].asString();
        if (modeStr == "hh") mode = cv::StereoSGBM::MODE_HH;
        else if (modeStr == "sgbm_3way") mode = cv::StereoSGBM::MODE_SGBM_3WAY;
        else if (modeStr == "hh4") mode = cv::StereoSGBM::MODE_HH4;
    }
    
    cv::Ptr<cv::StereoSGBM> stereo = cv::StereoSGBM::create(
        minDisparity, numDisparities, blockSize, p1, p2, disp12MaxDiff,
        preFilterCap, uniquenessRatio, speckleWindowSize, speckleRange, mode);
    
    cv::Mat disparity;
    stereo->compute(left, right, disparity);
    
    ctx.cacheManager->set("disparity_raw", disparity);
    
    return ExecutionResult::ok(disparity);
}

// ============================================================================
// ReprojectImageTo3DItem
// ============================================================================

ReprojectImageTo3DItem::ReprojectImageTo3DItem() {
    _functionName = "reproject_image_to_3d";
    _description = "Reprojects disparity image to 3D space";
    _category = "stereo";
    _params = {
        ParamDef::required("q_cache", BaseType::STRING, "Q matrix cache ID"),
        ParamDef::optional("handle_missing", BaseType::BOOL, "Handle missing values", false),
        ParamDef::optional("ddepth", BaseType::STRING, "Output depth: 32f, 16s", "32f")
    };
    _example = "reproject_image_to_3d(\"Q\", true)";
    _returnType = "mat";
    _tags = {"stereo", "3d", "reproject"};
}

ExecutionResult ReprojectImageTo3DItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string qCache = args[0].asString();
    bool handleMissing = args.size() > 1 ? args[1].asBool() : false;
    std::string ddepthStr = args.size() > 2 ? args[2].asString() : "32f";
    
    cv::Mat qOpt = ctx.cacheManager->get(qCache);
    if (qOpt.empty()) {
        return ExecutionResult::fail("Q matrix not found: " + qCache);
    }
    
    int ddepth = CV_32F;
    if (ddepthStr == "16s") ddepth = CV_16S;
    
    cv::Mat result;
    cv::reprojectImageTo3D(ctx.currentMat, result, qOpt, handleMissing, ddepth);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// FindChessboardCornersItem
// ============================================================================

FindChessboardCornersItem::FindChessboardCornersItem() {
    _functionName = "find_chessboard_corners";
    _description = "Finds chessboard corners for calibration";
    _category = "stereo";
    _params = {
        ParamDef::required("pattern_width", BaseType::INT, "Number of inner corners per row"),
        ParamDef::required("pattern_height", BaseType::INT, "Number of inner corners per column"),
        ParamDef::optional("flags", BaseType::STRING, "Flags: adaptive_thresh, normalize_image, filter_quads, fast_check", "adaptive_thresh+normalize_image"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for corners", "corners")
    };
    _example = "find_chessboard_corners(9, 6, \"adaptive_thresh+normalize_image\", \"corners\")";
    _returnType = "mat";
    _tags = {"calibration", "chessboard", "corners"};
}

ExecutionResult FindChessboardCornersItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int patternW = static_cast<int>(args[0].asNumber());
    int patternH = static_cast<int>(args[1].asNumber());
    std::string flagsStr = args.size() > 2 ? args[2].asString() : "adaptive_thresh+normalize_image";
    std::string cacheId = args.size() > 3 ? args[3].asString() : "corners";
    
    int flags = 0;
    if (flagsStr.find("adaptive_thresh") != std::string::npos) flags |= cv::CALIB_CB_ADAPTIVE_THRESH;
    if (flagsStr.find("normalize_image") != std::string::npos) flags |= cv::CALIB_CB_NORMALIZE_IMAGE;
    if (flagsStr.find("filter_quads") != std::string::npos) flags |= cv::CALIB_CB_FILTER_QUADS;
    if (flagsStr.find("fast_check") != std::string::npos) flags |= cv::CALIB_CB_FAST_CHECK;
    
    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }
    
    std::vector<cv::Point2f> corners;
    bool found = cv::findChessboardCorners(gray, cv::Size(patternW, patternH), corners, flags);
    
    if (found) {
        cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.001));
        cv::Mat cornersMat(corners);
        ctx.cacheManager->set(cacheId, cornersMat);
    }
    
    ctx.cacheManager->set(cacheId + "_found", cv::Mat(1, 1, CV_8U, cv::Scalar(found ? 1 : 0)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// FindCirclesGridItem
// ============================================================================

FindCirclesGridItem::FindCirclesGridItem() {
    _functionName = "find_circles_grid";
    _description = "Finds circle grid centers for calibration";
    _category = "stereo";
    _params = {
        ParamDef::required("pattern_width", BaseType::INT, "Number of circles per row"),
        ParamDef::required("pattern_height", BaseType::INT, "Number of circles per column"),
        ParamDef::optional("symmetric", BaseType::BOOL, "Symmetric grid pattern", true),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for centers", "centers")
    };
    _example = "find_circles_grid(7, 5, true, \"centers\")";
    _returnType = "mat";
    _tags = {"calibration", "circles", "grid"};
}

ExecutionResult FindCirclesGridItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int patternW = static_cast<int>(args[0].asNumber());
    int patternH = static_cast<int>(args[1].asNumber());
    bool symmetric = args.size() > 2 ? args[2].asBool() : true;
    std::string cacheId = args.size() > 3 ? args[3].asString() : "centers";
    
    int flags = symmetric ? cv::CALIB_CB_SYMMETRIC_GRID : cv::CALIB_CB_ASYMMETRIC_GRID;
    
    std::vector<cv::Point2f> centers;
    bool found = cv::findCirclesGrid(ctx.currentMat, cv::Size(patternW, patternH), centers, flags);
    
    if (found) {
        cv::Mat centersMat(centers);
        ctx.cacheManager->set(cacheId, centersMat);
    }
    
    ctx.cacheManager->set(cacheId + "_found", cv::Mat(1, 1, CV_8U, cv::Scalar(found ? 1 : 0)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DrawChessboardCornersItem
// ============================================================================

DrawChessboardCornersItem::DrawChessboardCornersItem() {
    _functionName = "draw_chessboard_corners";
    _description = "Draws detected chessboard corners on image";
    _category = "stereo";
    _params = {
        ParamDef::required("pattern_width", BaseType::INT, "Pattern width"),
        ParamDef::required("pattern_height", BaseType::INT, "Pattern height"),
        ParamDef::optional("corners_cache", BaseType::STRING, "Corners cache ID", "corners")
    };
    _example = "draw_chessboard_corners(9, 6, \"corners\")";
    _returnType = "mat";
    _tags = {"calibration", "chessboard", "draw"};
}

ExecutionResult DrawChessboardCornersItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int patternW = static_cast<int>(args[0].asNumber());
    int patternH = static_cast<int>(args[1].asNumber());
    std::string cornersCache = args.size() > 2 ? args[2].asString() : "corners";
    
    cv::Mat cornersOpt = ctx.cacheManager->get(cornersCache);
    cv::Mat foundOpt = ctx.cacheManager->get(cornersCache + "_found");
    
    if (cornersOpt.empty()) {
        return ExecutionResult::fail("Corners not found: " + cornersCache);
    }
    
    bool found = true;
    if (!foundOpt.empty()) {
        found = foundOpt.at<uchar>(0, 0) > 0;
    }
    
    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }
    
    std::vector<cv::Point2f> corners;
    for (int i = 0; i < cornersOpt.rows; i++) {
        corners.push_back(cornersOpt.at<cv::Point2f>(i));
    }
    
    cv::drawChessboardCorners(result, cv::Size(patternW, patternH), corners, found);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// CalibrateCameraItem
// ============================================================================

CalibrateCameraItem::CalibrateCameraItem() {
    _functionName = "calibrate_camera";
    _description = "Calibrates camera from object/image points";
    _category = "stereo";
    _params = {
        ParamDef::required("object_points_cache", BaseType::STRING, "Object points cache"),
        ParamDef::required("image_points_cache", BaseType::STRING, "Image points cache"),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache prefix for results", "calib")
    };
    _example = "calibrate_camera(\"obj_pts\", \"img_pts\", \"calib\")";
    _returnType = "mat";
    _tags = {"calibration", "camera", "intrinsics"};
}

ExecutionResult CalibrateCameraItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Simplified - full implementation would manage point lists
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// StereoCalibrateItem
// ============================================================================

StereoCalibrateItem::StereoCalibrateItem() {
    _functionName = "stereo_calibrate";
    _description = "Calibrates stereo camera setup";
    _category = "stereo";
    _params = {
        ParamDef::required("left_cache", BaseType::STRING, "Left camera matrix cache"),
        ParamDef::required("right_cache", BaseType::STRING, "Right camera matrix cache"),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "stereo")
    };
    _example = "stereo_calibrate(\"left_cam\", \"right_cam\", \"stereo\")";
    _returnType = "mat";
    _tags = {"stereo", "calibration"};
}

ExecutionResult StereoCalibrateItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// StereoRectifyItem
// ============================================================================

StereoRectifyItem::StereoRectifyItem() {
    _functionName = "stereo_rectify";
    _description = "Computes rectification transforms for stereo cameras";
    _category = "stereo";
    _params = {
        ParamDef::required("cam_matrix1", BaseType::STRING, "Left camera matrix cache"),
        ParamDef::required("dist_coeffs1", BaseType::STRING, "Left distortion coeffs cache"),
        ParamDef::required("cam_matrix2", BaseType::STRING, "Right camera matrix cache"),
        ParamDef::required("dist_coeffs2", BaseType::STRING, "Right distortion coeffs cache"),
        ParamDef::required("R", BaseType::STRING, "Rotation matrix cache"),
        ParamDef::required("T", BaseType::STRING, "Translation vector cache"),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "rect")
    };
    _example = "stereo_rectify(\"K1\", \"D1\", \"K2\", \"D2\", \"R\", \"T\", \"rect\")";
    _returnType = "mat";
    _tags = {"stereo", "rectify"};
}

ExecutionResult StereoRectifyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// InitUndistortRectifyMapItem
// ============================================================================

InitUndistortRectifyMapItem::InitUndistortRectifyMapItem() {
    _functionName = "init_undistort_rectify_map";
    _description = "Computes undistortion and rectification maps";
    _category = "stereo";
    _params = {
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::required("dist_coeffs", BaseType::STRING, "Distortion coeffs cache"),
        ParamDef::required("R", BaseType::STRING, "Rectification transform cache"),
        ParamDef::required("new_cam_matrix", BaseType::STRING, "New camera matrix cache"),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output maps cache prefix", "map")
    };
    _example = "init_undistort_rectify_map(\"K\", \"D\", \"R\", \"P\", \"map\")";
    _returnType = "mat";
    _tags = {"undistort", "rectify", "map"};
}

ExecutionResult InitUndistortRectifyMapItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string camMatCache = args[0].asString();
    std::string distCache = args[1].asString();
    std::string rCache = args[2].asString();
    std::string newCamCache = args[3].asString();
    std::string prefix = args.size() > 4 ? args[4].asString() : "map";
    
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    cv::Mat distOpt = ctx.cacheManager->get(distCache);
    cv::Mat rOpt = ctx.cacheManager->get(rCache);
    cv::Mat newCamOpt = ctx.cacheManager->get(newCamCache);
    
    if (camMatOpt.empty() || distOpt.empty() || rOpt.empty() || newCamOpt.empty()) {
        return ExecutionResult::fail("Required matrices not found in cache");
    }
    
    cv::Mat map1, map2;
    cv::initUndistortRectifyMap(camMatOpt, distOpt, rOpt, newCamOpt, 
                                 ctx.currentMat.size(), CV_32FC1, map1, map2);
    
    ctx.cacheManager->set(prefix + "1", map1);
    ctx.cacheManager->set(prefix + "2", map2);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// UndistortItem
// ============================================================================

UndistortItem::UndistortItem() {
    _functionName = "undistort";
    _description = "Undistorts image using camera parameters";
    _category = "stereo";
    _params = {
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::required("dist_coeffs", BaseType::STRING, "Distortion coeffs cache"),
        ParamDef::optional("new_cam_matrix", BaseType::STRING, "New camera matrix cache", "")
    };
    _example = "undistort(\"K\", \"D\")";
    _returnType = "mat";
    _tags = {"undistort", "calibration"};
}

ExecutionResult UndistortItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string camMatCache = args[0].asString();
    std::string distCache = args[1].asString();
    std::string newCamCache = args.size() > 2 ? args[2].asString() : "";
    
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    cv::Mat distOpt = ctx.cacheManager->get(distCache);
    
    if (camMatOpt.empty() || distOpt.empty()) {
        return ExecutionResult::fail("Camera matrix or distortion coeffs not found");
    }
    
    cv::Mat newCamMatrix;
    if (!newCamCache.empty()) {
        cv::Mat newCamOpt = ctx.cacheManager->get(newCamCache);
        if (!newCamOpt.empty()) {
            newCamMatrix = newCamOpt;
        }
    }
    
    cv::Mat result;
    cv::undistort(ctx.currentMat, result, camMatOpt, distOpt, newCamMatrix);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// UndistortPointsItem
// ============================================================================

UndistortPointsItem::UndistortPointsItem() {
    _functionName = "undistort_points";
    _description = "Undistorts 2D points";
    _category = "stereo";
    _params = {
        ParamDef::required("points_cache", BaseType::STRING, "Points cache"),
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::required("dist_coeffs", BaseType::STRING, "Distortion coeffs cache"),
        ParamDef::optional("output_cache", BaseType::STRING, "Output points cache", "undist_points")
    };
    _example = "undistort_points(\"pts\", \"K\", \"D\", \"undist_pts\")";
    _returnType = "mat";
    _tags = {"undistort", "points"};
}

ExecutionResult UndistortPointsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string pointsCache = args[0].asString();
    std::string camMatCache = args[1].asString();
    std::string distCache = args[2].asString();
    std::string outputCache = args.size() > 3 ? args[3].asString() : "undist_points";
    
    cv::Mat pointsOpt = ctx.cacheManager->get(pointsCache);
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    cv::Mat distOpt = ctx.cacheManager->get(distCache);
    
    if (pointsOpt.empty() || camMatOpt.empty() || distOpt.empty()) {
        return ExecutionResult::fail("Required data not found in cache");
    }
    
    cv::Mat undistorted;
    cv::undistortPoints(pointsOpt, undistorted, camMatOpt, distOpt);
    
    ctx.cacheManager->set(outputCache, undistorted);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DisparityWLSFilterItem
// ============================================================================

DisparityWLSFilterItem::DisparityWLSFilterItem() {
    _functionName = "disparity_wls_filter";
    _description = "Applies WLS filter to disparity map (requires ximgproc)";
    _category = "stereo";
    _params = {
        ParamDef::optional("lambda", BaseType::FLOAT, "Regularization parameter", 8000.0),
        ParamDef::optional("sigma", BaseType::FLOAT, "Sigma color parameter", 1.5)
    };
    _example = "disparity_wls_filter(8000, 1.5)";
    _returnType = "mat";
    _tags = {"disparity", "filter", "wls"};
}

ExecutionResult DisparityWLSFilterItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // WLS filter requires ximgproc contrib module
    // For now, just normalize the disparity
    cv::Mat result;
    cv::normalize(ctx.currentMat, result, 0, 255, cv::NORM_MINMAX, CV_8U);
    return ExecutionResult::ok(result);
}

// ============================================================================
// NormalizeDisparityItem
// ============================================================================

NormalizeDisparityItem::NormalizeDisparityItem() {
    _functionName = "normalize_disparity";
    _description = "Normalizes disparity map for visualization";
    _category = "stereo";
    _params = {
        ParamDef::optional("min_val", BaseType::FLOAT, "Minimum value", 0.0),
        ParamDef::optional("max_val", BaseType::FLOAT, "Maximum value", 255.0)
    };
    _example = "normalize_disparity(0, 255)";
    _returnType = "mat";
    _tags = {"disparity", "normalize", "visualization"};
}

ExecutionResult NormalizeDisparityItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double minVal = args.size() > 0 ? args[0].asNumber() : 0.0;
    double maxVal = args.size() > 1 ? args[1].asNumber() : 255.0;
    
    cv::Mat result;
    cv::normalize(ctx.currentMat, result, minVal, maxVal, cv::NORM_MINMAX, CV_8U);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DisparityToDepthItem
// ============================================================================

DisparityToDepthItem::DisparityToDepthItem() {
    _functionName = "disparity_to_depth";
    _description = "Converts disparity to depth using baseline and focal length";
    _category = "stereo";
    _params = {
        ParamDef::required("baseline", BaseType::FLOAT, "Stereo baseline (distance between cameras)"),
        ParamDef::required("focal_length", BaseType::FLOAT, "Camera focal length in pixels")
    };
    _example = "disparity_to_depth(0.12, 700)";
    _returnType = "mat";
    _tags = {"disparity", "depth", "stereo"};
}

ExecutionResult DisparityToDepthItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double baseline = args[0].asNumber();
    double focalLength = args[1].asNumber();
    
    cv::Mat disparity;
    ctx.currentMat.convertTo(disparity, CV_32F);
    
    // Divide by 16 if disparity is from StereoBM/SGBM (they multiply by 16)
    if (ctx.currentMat.type() == CV_16S) {
        disparity /= 16.0;
    }
    
    cv::Mat depth = baseline * focalLength / disparity;
    
    // Handle invalid disparities
    depth.setTo(0, disparity <= 0);
    
    return ExecutionResult::ok(depth);
}

// ============================================================================
// FindFundamentalMatItem
// ============================================================================

FindFundamentalMatItem::FindFundamentalMatItem() {
    _functionName = "find_fundamental_mat";
    _description = "Calculates fundamental matrix from point correspondences";
    _category = "stereo";
    _params = {
        ParamDef::required("points1_cache", BaseType::STRING, "First image points cache"),
        ParamDef::required("points2_cache", BaseType::STRING, "Second image points cache"),
        ParamDef::optional("method", BaseType::STRING, "Method: 7point, 8point, ransac, lmeds", "ransac"),
        ParamDef::optional("ransac_reproj_threshold", BaseType::FLOAT, "RANSAC threshold", 3.0),
        ParamDef::optional("confidence", BaseType::FLOAT, "Confidence level", 0.99),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for F matrix", "F")
    };
    _example = "find_fundamental_mat(\"pts1\", \"pts2\", \"ransac\", 3.0, 0.99, \"F\")";
    _returnType = "mat";
    _tags = {"fundamental", "epipolar", "geometry"};
}

ExecutionResult FindFundamentalMatItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string pts1Cache = args[0].asString();
    std::string pts2Cache = args[1].asString();
    std::string method = args.size() > 2 ? args[2].asString() : "ransac";
    double ransacThresh = args.size() > 3 ? args[3].asNumber() : 3.0;
    double confidence = args.size() > 4 ? args[4].asNumber() : 0.99;
    std::string cacheId = args.size() > 5 ? args[5].asString() : "F";
    
    cv::Mat pts1Opt = ctx.cacheManager->get(pts1Cache);
    cv::Mat pts2Opt = ctx.cacheManager->get(pts2Cache);
    
    if (pts1Opt.empty() || pts2Opt.empty()) {
        return ExecutionResult::fail("Points not found in cache");
    }
    
    int methodVal = cv::FM_RANSAC;
    if (method == "7point") methodVal = cv::FM_7POINT;
    else if (method == "8point") methodVal = cv::FM_8POINT;
    else if (method == "lmeds") methodVal = cv::FM_LMEDS;
    
    cv::Mat F = cv::findFundamentalMat(pts1Opt, pts2Opt, methodVal, ransacThresh, confidence);
    ctx.cacheManager->set(cacheId, F);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// FindEssentialMatItem
// ============================================================================

FindEssentialMatItem::FindEssentialMatItem() {
    _functionName = "find_essential_mat";
    _description = "Calculates essential matrix from point correspondences";
    _category = "stereo";
    _params = {
        ParamDef::required("points1_cache", BaseType::STRING, "First image points cache"),
        ParamDef::required("points2_cache", BaseType::STRING, "Second image points cache"),
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::optional("method", BaseType::STRING, "Method: ransac, lmeds", "ransac"),
        ParamDef::optional("prob", BaseType::FLOAT, "Confidence probability", 0.999),
        ParamDef::optional("threshold", BaseType::FLOAT, "RANSAC threshold", 1.0),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for E matrix", "E")
    };
    _example = "find_essential_mat(\"pts1\", \"pts2\", \"K\", \"ransac\", 0.999, 1.0, \"E\")";
    _returnType = "mat";
    _tags = {"essential", "epipolar", "geometry"};
}

ExecutionResult FindEssentialMatItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string pts1Cache = args[0].asString();
    std::string pts2Cache = args[1].asString();
    std::string camMatCache = args[2].asString();
    std::string method = args.size() > 3 ? args[3].asString() : "ransac";
    double prob = args.size() > 4 ? args[4].asNumber() : 0.999;
    double threshold = args.size() > 5 ? args[5].asNumber() : 1.0;
    std::string cacheId = args.size() > 6 ? args[6].asString() : "E";
    
    cv::Mat pts1Opt = ctx.cacheManager->get(pts1Cache);
    cv::Mat pts2Opt = ctx.cacheManager->get(pts2Cache);
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    
    if (pts1Opt.empty() || pts2Opt.empty() || camMatOpt.empty()) {
        return ExecutionResult::fail("Required data not found in cache");
    }
    
    int methodVal = cv::RANSAC;
    if (method == "lmeds") methodVal = cv::LMEDS;
    
    cv::Mat E = cv::findEssentialMat(pts1Opt, pts2Opt, camMatOpt, methodVal, prob, threshold);
    ctx.cacheManager->set(cacheId, E);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// RecoverPoseItem
// ============================================================================

RecoverPoseItem::RecoverPoseItem() {
    _functionName = "recover_pose";
    _description = "Recovers rotation and translation from essential matrix";
    _category = "stereo";
    _params = {
        ParamDef::required("E_cache", BaseType::STRING, "Essential matrix cache"),
        ParamDef::required("points1_cache", BaseType::STRING, "First image points cache"),
        ParamDef::required("points2_cache", BaseType::STRING, "Second image points cache"),
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "pose")
    };
    _example = "recover_pose(\"E\", \"pts1\", \"pts2\", \"K\", \"pose\")";
    _returnType = "mat";
    _tags = {"pose", "rotation", "translation"};
}

ExecutionResult RecoverPoseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string eCache = args[0].asString();
    std::string pts1Cache = args[1].asString();
    std::string pts2Cache = args[2].asString();
    std::string camMatCache = args[3].asString();
    std::string prefix = args.size() > 4 ? args[4].asString() : "pose";
    
    cv::Mat eOpt = ctx.cacheManager->get(eCache);
    cv::Mat pts1Opt = ctx.cacheManager->get(pts1Cache);
    cv::Mat pts2Opt = ctx.cacheManager->get(pts2Cache);
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    
    if (eOpt.empty() || pts1Opt.empty() || pts2Opt.empty() || camMatOpt.empty()) {
        return ExecutionResult::fail("Required data not found in cache");
    }
    
    cv::Mat R, t;
    cv::recoverPose(eOpt, pts1Opt, pts2Opt, camMatOpt, R, t);
    
    ctx.cacheManager->set(prefix + "_R", R);
    ctx.cacheManager->set(prefix + "_t", t);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// TriangulatePointsItem
// ============================================================================

TriangulatePointsItem::TriangulatePointsItem() {
    _functionName = "triangulate_points";
    _description = "Triangulates 3D points from 2D correspondences";
    _category = "stereo";
    _params = {
        ParamDef::required("proj_mat1", BaseType::STRING, "First projection matrix cache"),
        ParamDef::required("proj_mat2", BaseType::STRING, "Second projection matrix cache"),
        ParamDef::required("points1", BaseType::STRING, "First image points cache"),
        ParamDef::required("points2", BaseType::STRING, "Second image points cache"),
        ParamDef::optional("cache_id", BaseType::STRING, "Output 4D points cache", "points4d")
    };
    _example = "triangulate_points(\"P1\", \"P2\", \"pts1\", \"pts2\", \"pts4d\")";
    _returnType = "mat";
    _tags = {"triangulate", "3d", "reconstruction"};
}

ExecutionResult TriangulatePointsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string p1Cache = args[0].asString();
    std::string p2Cache = args[1].asString();
    std::string pts1Cache = args[2].asString();
    std::string pts2Cache = args[3].asString();
    std::string cacheId = args.size() > 4 ? args[4].asString() : "points4d";
    
    cv::Mat p1Opt = ctx.cacheManager->get(p1Cache);
    cv::Mat p2Opt = ctx.cacheManager->get(p2Cache);
    cv::Mat pts1Opt = ctx.cacheManager->get(pts1Cache);
    cv::Mat pts2Opt = ctx.cacheManager->get(pts2Cache);
    
    if (p1Opt.empty() || p2Opt.empty() || pts1Opt.empty() || pts2Opt.empty()) {
        return ExecutionResult::fail("Required data not found in cache");
    }
    
    cv::Mat points4D;
    cv::triangulatePoints(p1Opt, p2Opt, pts1Opt, pts2Opt, points4D);
    
    ctx.cacheManager->set(cacheId, points4D);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SolvePnPItem
// ============================================================================

SolvePnPItem::SolvePnPItem() {
    _functionName = "solve_pnp";
    _description = "Finds object pose from 3D-2D correspondences";
    _category = "stereo";
    _params = {
        ParamDef::required("object_points", BaseType::STRING, "3D object points cache"),
        ParamDef::required("image_points", BaseType::STRING, "2D image points cache"),
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::required("dist_coeffs", BaseType::STRING, "Distortion coeffs cache"),
        ParamDef::optional("method", BaseType::STRING, "Method: iterative, p3p, ap3p, epnp, ippe, ippe_square, sqpnp", "iterative"),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "pnp")
    };
    _example = "solve_pnp(\"obj_pts\", \"img_pts\", \"K\", \"D\", \"iterative\", \"pnp\")";
    _returnType = "mat";
    _tags = {"pnp", "pose", "estimation"};
}

ExecutionResult SolvePnPItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string objPtsCache = args[0].asString();
    std::string imgPtsCache = args[1].asString();
    std::string camMatCache = args[2].asString();
    std::string distCache = args[3].asString();
    std::string method = args.size() > 4 ? args[4].asString() : "iterative";
    std::string prefix = args.size() > 5 ? args[5].asString() : "pnp";
    
    cv::Mat objPtsOpt = ctx.cacheManager->get(objPtsCache);
    cv::Mat imgPtsOpt = ctx.cacheManager->get(imgPtsCache);
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    cv::Mat distOpt = ctx.cacheManager->get(distCache);
    
    if (objPtsOpt.empty() || imgPtsOpt.empty() || camMatOpt.empty() || distOpt.empty()) {
        return ExecutionResult::fail("Required data not found in cache");
    }
    
    int flag = cv::SOLVEPNP_ITERATIVE;
    if (method == "p3p") flag = cv::SOLVEPNP_P3P;
    else if (method == "ap3p") flag = cv::SOLVEPNP_AP3P;
    else if (method == "epnp") flag = cv::SOLVEPNP_EPNP;
    else if (method == "ippe") flag = cv::SOLVEPNP_IPPE;
    else if (method == "ippe_square") flag = cv::SOLVEPNP_IPPE_SQUARE;
    else if (method == "sqpnp") flag = cv::SOLVEPNP_SQPNP;
    
    cv::Mat rvec, tvec;
    cv::solvePnP(objPtsOpt, imgPtsOpt, camMatOpt, distOpt, rvec, tvec, false, flag);
    
    ctx.cacheManager->set(prefix + "_rvec", rvec);
    ctx.cacheManager->set(prefix + "_tvec", tvec);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SolvePnPRansacItem
// ============================================================================

SolvePnPRansacItem::SolvePnPRansacItem() {
    _functionName = "solve_pnp_ransac";
    _description = "Finds object pose using RANSAC";
    _category = "stereo";
    _params = {
        ParamDef::required("object_points", BaseType::STRING, "3D object points cache"),
        ParamDef::required("image_points", BaseType::STRING, "2D image points cache"),
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::required("dist_coeffs", BaseType::STRING, "Distortion coeffs cache"),
        ParamDef::optional("iterations", BaseType::INT, "RANSAC iterations", 100),
        ParamDef::optional("reproj_error", BaseType::FLOAT, "Reprojection error threshold", 8.0),
        ParamDef::optional("confidence", BaseType::FLOAT, "Confidence", 0.99),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "pnp_ransac")
    };
    _example = "solve_pnp_ransac(\"obj_pts\", \"img_pts\", \"K\", \"D\", 100, 8.0, 0.99, \"pnp\")";
    _returnType = "mat";
    _tags = {"pnp", "pose", "ransac"};
}

ExecutionResult SolvePnPRansacItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string objPtsCache = args[0].asString();
    std::string imgPtsCache = args[1].asString();
    std::string camMatCache = args[2].asString();
    std::string distCache = args[3].asString();
    int iterations = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 100;
    float reprojError = args.size() > 5 ? static_cast<float>(args[5].asNumber()) : 8.0f;
    double confidence = args.size() > 6 ? args[6].asNumber() : 0.99;
    std::string prefix = args.size() > 7 ? args[7].asString() : "pnp_ransac";
    
    cv::Mat objPtsOpt = ctx.cacheManager->get(objPtsCache);
    cv::Mat imgPtsOpt = ctx.cacheManager->get(imgPtsCache);
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    cv::Mat distOpt = ctx.cacheManager->get(distCache);
    
    if (objPtsOpt.empty() || imgPtsOpt.empty() || camMatOpt.empty() || distOpt.empty()) {
        return ExecutionResult::fail("Required data not found in cache");
    }
    
    cv::Mat rvec, tvec, inliers;
    cv::solvePnPRansac(objPtsOpt, imgPtsOpt, camMatOpt, distOpt, rvec, tvec, false, iterations, reprojError, confidence, inliers);
    
    ctx.cacheManager->set(prefix + "_rvec", rvec);
    ctx.cacheManager->set(prefix + "_tvec", tvec);
    ctx.cacheManager->set(prefix + "_inliers", inliers);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ProjectPointsItem
// ============================================================================

ProjectPointsItem::ProjectPointsItem() {
    _functionName = "project_points";
    _description = "Projects 3D points to image plane";
    _category = "stereo";
    _params = {
        ParamDef::required("object_points", BaseType::STRING, "3D object points cache"),
        ParamDef::required("rvec", BaseType::STRING, "Rotation vector cache"),
        ParamDef::required("tvec", BaseType::STRING, "Translation vector cache"),
        ParamDef::required("cam_matrix", BaseType::STRING, "Camera matrix cache"),
        ParamDef::required("dist_coeffs", BaseType::STRING, "Distortion coeffs cache"),
        ParamDef::optional("cache_id", BaseType::STRING, "Output 2D points cache", "proj_points")
    };
    _example = "project_points(\"obj_pts\", \"rvec\", \"tvec\", \"K\", \"D\", \"proj_pts\")";
    _returnType = "mat";
    _tags = {"project", "3d", "2d"};
}

ExecutionResult ProjectPointsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string objPtsCache = args[0].asString();
    std::string rvecCache = args[1].asString();
    std::string tvecCache = args[2].asString();
    std::string camMatCache = args[3].asString();
    std::string distCache = args[4].asString();
    std::string cacheId = args.size() > 5 ? args[5].asString() : "proj_points";
    
    cv::Mat objPtsOpt = ctx.cacheManager->get(objPtsCache);
    cv::Mat rvecOpt = ctx.cacheManager->get(rvecCache);
    cv::Mat tvecOpt = ctx.cacheManager->get(tvecCache);
    cv::Mat camMatOpt = ctx.cacheManager->get(camMatCache);
    cv::Mat distOpt = ctx.cacheManager->get(distCache);
    
    if (objPtsOpt.empty() || rvecOpt.empty() || tvecOpt.empty() || camMatOpt.empty() || distOpt.empty()) {
        return ExecutionResult::fail("Required data not found in cache");
    }
    
    std::vector<cv::Point2f> imagePoints;
    cv::projectPoints(objPtsOpt, rvecOpt, tvecOpt, camMatOpt, distOpt, imagePoints);
    
    cv::Mat pointsMat(imagePoints);
    ctx.cacheManager->set(cacheId, pointsMat);
    
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

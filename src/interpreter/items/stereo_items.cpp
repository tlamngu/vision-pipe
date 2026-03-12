#include "interpreter/items/stereo_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <opencv2/objdetect/charuco_detector.hpp>

namespace visionpipe {

void registerStereoItems(ItemRegistry& registry) {
    registry.add<StereoBMItem>();
    registry.add<StereoSGBMItem>();
    registry.add<ReprojectImageTo3DItem>();
    registry.add<FindChessboardCornersItem>();
    registry.add<FindCirclesGridItem>();
    registry.add<DrawChessboardCornersItem>();
    registry.add<ChessboardDetectedSaveItem>();
    registry.add<CharucoDetectItem>();
    registry.add<DrawCharucoItem>();
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
    _description = "Finds chessboard corners for calibration. Uses the robust saddle-point-based detector (findChessboardCornersSB) by default.";
    _category = "stereo";
    _params = {
        ParamDef::required("pattern_width",  BaseType::INT,    "Number of inner corners per row"),
        ParamDef::required("pattern_height", BaseType::INT,    "Number of inner corners per column"),
        ParamDef::optional("flags",          BaseType::STRING, "Flags for classic detector: adaptive_thresh, normalize_image, filter_quads, fast_check", "adaptive_thresh+normalize_image"),
        ParamDef::optional("cache_id",       BaseType::STRING, "Cache ID for corners", "corners"),
        ParamDef::optional("use_sb",         BaseType::BOOL,   "Use robust saddle-point detector (findChessboardCornersSB)", true),
        ParamDef::optional("sb_marker_size", BaseType::FLOAT,  "SB: marker size relative to square (0.1–0.9)", 0.5),
        ParamDef::optional("sb_exhaustive",  BaseType::BOOL,   "SB: exhaustive search (slower but more robust)", false),
        ParamDef::optional("subpix_win",     BaseType::INT,    "Sub-pixel refinement window half-size (classic mode)", 11)
    };
    _example = "find_chessboard_corners(9, 6)";
    _returnType = "mat";
    _tags = {"calibration", "chessboard", "corners"};
}

ExecutionResult FindChessboardCornersItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int patternW      = static_cast<int>(args[0].asNumber());
    int patternH      = static_cast<int>(args[1].asNumber());
    std::string flagsStr = args.size() > 2 ? args[2].asString() : "adaptive_thresh+normalize_image";
    std::string cacheId  = args.size() > 3 ? args[3].asString() : "corners";
    bool useSB        = args.size() > 4 ? args[4].asBool()   : true;
    (void)             (args.size() > 5 ? args[5].asNumber() : 0.5); // sb_marker_size reserved
    bool sbExhaustive = args.size() > 6 ? args[6].asBool()   : false;
    int subpixWin     = args.size() > 7 ? static_cast<int>(args[7].asNumber()) : 11;

    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }

    std::vector<cv::Point2f> corners;
    bool found = false;

    if (useSB) {
        int sbFlags = cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE * (sbExhaustive ? 1 : 0);
        found = cv::findChessboardCornersSB(gray, cv::Size(patternW, patternH), corners, sbFlags);
        if (!found && !sbExhaustive) {
            // Retry with exhaustive search as fallback
            found = cv::findChessboardCornersSB(gray, cv::Size(patternW, patternH), corners,
                                                cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_EXHAUSTIVE);
        }
    } else {
        int flags = 0;
        if (flagsStr.find("adaptive_thresh")  != std::string::npos) flags |= cv::CALIB_CB_ADAPTIVE_THRESH;
        if (flagsStr.find("normalize_image")  != std::string::npos) flags |= cv::CALIB_CB_NORMALIZE_IMAGE;
        if (flagsStr.find("filter_quads")     != std::string::npos) flags |= cv::CALIB_CB_FILTER_QUADS;
        if (flagsStr.find("fast_check")       != std::string::npos) flags |= cv::CALIB_CB_FAST_CHECK;
        found = cv::findChessboardCorners(gray, cv::Size(patternW, patternH), corners, flags);
        if (found) {
            // Measure minimum spacing from the actual detected corners
            if(ctx.verbose){
                std::cout << "[FindChessboardCornersItem] Detected corners:" << "\n";
                std::cout << std::string(50, '-') << "\n";
                std::cout << "| Index | X Coordinate | Y Coordinate |" << "\n";
                std::cout << std::string(50, '-') << "\n";
                for (size_t i = 0; i < corners.size(); i++) {
                    printf("| %5zu | %12.2f | %12.2f |\n", i, corners[i].x, corners[i].y);
                }
                std::cout << std::string(50, '-') << "\n";
                std::cout << "[FindChessboardCornersItem] Calculating sub-pixel corners with window half-size " << subpixWin << std::endl;
            }
            float minDist = cv::norm(corners[1] - corners[0]);
            for (size_t i = 1; i < corners.size(); i++)
                minDist = std::min(minDist, (float)cv::norm(corners[i] - corners[i-1]));

            int hw = std::min(std::max(3, subpixWin), std::max(3, (int)(minDist * 0.45f)));
            cv::cornerSubPix(gray, corners, cv::Size(hw, hw), cv::Size(-1, -1),
                            cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 40, 0.0001));
            if(ctx.verbose){
                std::cout << "[FindChessboardCornersItem] Subpixel refined corners:" << "\n";
                std::cout << std::string(50, '-') << "\n";
                std::cout << "| Index | X Coordinate | Y Coordinate |" << "\n";
                std::cout << std::string(50, '-') << "\n";
                for (size_t i = 0; i < corners.size(); i++) {
                    printf("| %5zu | %12.2f | %12.2f |\n", i, corners[i].x, corners[i].y);
                }
                std::cout << std::string(50, '-') << std::endl;
            }
        }
    }

    if (found) {
        cv::Mat cornersMat;
        cv::Mat(corners).copyTo(cornersMat);
        ctx.cacheManager->set(cacheId, cornersMat);
    } else {
        // Clear stale cache so draw_chessboard_corners shows nothing
        ctx.cacheManager->set(cacheId, cv::Mat());
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
        cv::Mat centersMat;
        cv::Mat(centers).copyTo(centersMat);
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

    // Nothing to draw if corners were cleared (detection failed)
    if (cornersOpt.empty()) {
        return ExecutionResult::ok(ctx.currentMat);
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
    
    if(ctx.verbose){
        std::cout << "[DrawChessboardCornersItem] Drawing corners (found=" << found << "):" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "| Index | X Coordinate | Y Coordinate |" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        for (size_t i = 0; i < corners.size(); i++) {
            printf("| %5zu | %12.2f | %12.2f |\n", i, corners[i].x, corners[i].y);
        }
        std::cout << std::string(50, '-') << std::endl;
    }
    
    cv::drawChessboardCorners(result, cv::Size(patternW, patternH), corners, found);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ChessboardDetectedSaveItem
// ============================================================================

ChessboardDetectedSaveItem::ChessboardDetectedSaveItem() {
    _functionName = "chessboard_detected_save";
    _description = "Saves detected chessboard corners to a JSON file in the given directory (only when found)";
    _category = "stereo";
    _params = {
        ParamDef::required("file_dir", BaseType::STRING, "Directory to save JSON files into"),
        ParamDef::optional("corners_cache", BaseType::STRING, "Cache ID for detected corners", "corners"),
        ParamDef::optional("pattern_width", BaseType::INT, "Pattern inner-corner columns (for metadata)", 0),
        ParamDef::optional("pattern_height", BaseType::INT, "Pattern inner-corner rows (for metadata)", 0)
    };
    _example = "chessboard_detected_save(\"/tmp/calib\") | chessboard_detected_save(\"/tmp/calib\", \"corners\", 9, 6)";
    _returnType = "mat";
    _tags = {"calibration", "chessboard", "save", "json"};
}

ExecutionResult ChessboardDetectedSaveItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string fileDir   = args[0].asString();
    std::string cacheId   = args.size() > 1 ? args[1].asString() : "corners";
    int patternW          = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;
    int patternH          = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;

    // Only save when the chessboard was successfully detected.
    cv::Mat foundMat = ctx.cacheManager->get(cacheId + "_found");
    bool found = !foundMat.empty() && foundMat.at<uchar>(0, 0) > 0;
    if (!found) {
        return ExecutionResult::ok(ctx.currentMat);
    }

    cv::Mat cornersMat = ctx.cacheManager->get(cacheId);
    if (cornersMat.empty()) {
        return ExecutionResult::fail("Corners not found in cache: " + cacheId);
    }

    // Auto-increment save counter per directory.
    const std::string counterKey = "__chessboard_save_count:" + fileDir;
    cv::Mat counterMat = ctx.cacheManager->get(counterKey);
    int count = 0;
    if (!counterMat.empty()) {
        count = counterMat.at<int>(0, 0);
    }
    ctx.cacheManager->set(counterKey, cv::Mat(1, 1, CV_32S, cv::Scalar(count + 1)));

    // Ensure the output directory exists.
    std::system(("mkdir -p \"" + fileDir + "\"").c_str());

    // Build numbered file path.
    char fname[64];
    std::snprintf(fname, sizeof(fname), "chessboard_%04d.json", count);
    std::string filePath = fileDir + "/" + fname;

    // Write JSON via cv::FileStorage.
    cv::FileStorage fs(filePath, cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
    if (!fs.isOpened()) {
        return ExecutionResult::fail("Failed to open file for writing: " + filePath);
    }

    fs << "found" << true;
    if (patternW > 0) fs << "pattern_width"  << patternW;
    if (patternH > 0) fs << "pattern_height" << patternH;
    fs << "corner_count" << cornersMat.rows;
    fs << "corners" << "[";
    for (int i = 0; i < cornersMat.rows; i++) {
        cv::Point2f pt = cornersMat.at<cv::Point2f>(i);
        fs << "{:" << "x" << pt.x << "y" << pt.y << "}";
    }
    fs << "]";
    fs.release();

    std::cout << "Chessboard detection saved to: " << filePath << std::endl;
    return ExecutionResult::ok(ctx.currentMat);
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
    
    cv::Mat pointsMat;
    cv::Mat(imagePoints).copyTo(pointsMat);
    ctx.cacheManager->set(cacheId, pointsMat);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CharucoDetectItem
// ============================================================================

CharucoDetectItem::CharucoDetectItem() {
    _functionName = "charuco_detect";
    _description = "Detects ChArUco board corners (ArUco markers embedded in a chessboard grid)";
    _category = "stereo";
    _params = {
        ParamDef::optional("squares_x",          BaseType::INT,    "Chessboard squares in X",                         10),
        ParamDef::optional("squares_y",          BaseType::INT,    "Chessboard squares in Y",                         7),
        ParamDef::optional("square_length",      BaseType::FLOAT,  "Square side length (any consistent unit)",         0.04),
        ParamDef::optional("marker_length",      BaseType::FLOAT,  "Marker side length (same unit as square_length)", 0.02),
        ParamDef::optional("dict",               BaseType::STRING, "ArUco dictionary name",                           "4x4_50"),
        ParamDef::optional("min_markers",        BaseType::INT,    "Min adjacent markers per corner (0 = any)",        1),
        ParamDef::optional("refine",             BaseType::BOOL,   "Try to refine marker positions",                  false),
        ParamDef::optional("corners_cache",      BaseType::STRING, "Cache ID for ChArUco corners",                    "charuco_corners"),
        ParamDef::optional("ids_cache",          BaseType::STRING, "Cache ID for ChArUco corner IDs",                 "charuco_ids"),
        // ArUco detector tuning (useful for fisheye / low-res images)
        ParamDef::optional("min_marker_perim",   BaseType::FLOAT,  "Min marker perimeter rate (0–1, default 0.03)",    0.03),
        ParamDef::optional("max_marker_perim",   BaseType::FLOAT,  "Max marker perimeter rate (default 4.0)",          4.0),
        ParamDef::optional("thresh_constant",    BaseType::INT,    "Adaptive threshold constant (default 7)",          7),
        ParamDef::optional("error_correction",   BaseType::FLOAT,  "Marker bit error correction rate (0–1)",           0.6),
        ParamDef::optional("verbose",            BaseType::BOOL,   "Print marker/corner counts on each frame",         false),
        ParamDef::optional("first_marker_id",    BaseType::INT,    "Starting marker ID for board layout (-1 = default)",-1)
    };
    _example = "charuco_detect(10, 7, 0.04, 0.02, \"4x4_50\")";
    _returnType = "mat";
    _tags = {"calibration", "charuco", "aruco", "chessboard", "detection"};
}

static cv::aruco::PredefinedDictionaryType parseDictType(const std::string& name) {
    if (name == "4x4_50")       return cv::aruco::DICT_4X4_50;
    if (name == "4x4_100")      return cv::aruco::DICT_4X4_100;
    if (name == "4x4_250")      return cv::aruco::DICT_4X4_250;
    if (name == "4x4_1000")     return cv::aruco::DICT_4X4_1000;
    if (name == "5x5_50")       return cv::aruco::DICT_5X5_50;
    if (name == "5x5_100")      return cv::aruco::DICT_5X5_100;
    if (name == "5x5_250")      return cv::aruco::DICT_5X5_250;
    if (name == "5x5_1000")     return cv::aruco::DICT_5X5_1000;
    if (name == "6x6_50")       return cv::aruco::DICT_6X6_50;
    if (name == "6x6_100")      return cv::aruco::DICT_6X6_100;
    if (name == "6x6_250")      return cv::aruco::DICT_6X6_250;
    if (name == "6x6_1000")     return cv::aruco::DICT_6X6_1000;
    if (name == "7x7_50")       return cv::aruco::DICT_7X7_50;
    if (name == "7x7_100")      return cv::aruco::DICT_7X7_100;
    if (name == "7x7_250")      return cv::aruco::DICT_7X7_250;
    if (name == "7x7_1000")     return cv::aruco::DICT_7X7_1000;
    if (name == "aruco_original") return cv::aruco::DICT_ARUCO_ORIGINAL;
    return cv::aruco::DICT_4X4_50; // default
}

ExecutionResult CharucoDetectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int squaresX        = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 10;
    int squaresY        = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 7;
    float squareLength  = args.size() > 2 ? static_cast<float>(args[2].asNumber()) : 0.04f;
    float markerLength  = args.size() > 3 ? static_cast<float>(args[3].asNumber()) : 0.02f;
    std::string dictName      = args.size() > 4 ? args[4].asString() : "4x4_50";
    int minMarkers            = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 1;
    bool refine               = args.size() > 6 ? args[6].asBool() : false;
    std::string cornersCache  = args.size() > 7 ? args[7].asString() : "charuco_corners";
    std::string idsCache      = args.size() > 8 ? args[8].asString() : "charuco_ids";
    double minMarkerPerim     = args.size() > 9  ? args[9].asNumber()  : 0.03;
    double maxMarkerPerim     = args.size() > 10 ? args[10].asNumber() : 4.0;
    int threshConstant        = args.size() > 11 ? static_cast<int>(args[11].asNumber()) : 7;
    double errorCorrection    = args.size() > 12 ? args[12].asNumber() : 0.6;
    bool verbose              = args.size() > 13 ? args[13].asBool() : false;
    int firstMarkerId         = args.size() > 14 ? static_cast<int>(args[14].asNumber()) : -1;

    // Validate image
    if (ctx.currentMat.empty()) {
        return ExecutionResult::ok(ctx.currentMat);
    }
    // Ensure 8-bit (CharucoDetector can handle gray or BGR, but not float/double)
    cv::Mat img = ctx.currentMat;
    if (img.depth() != CV_8U) {
        img.convertTo(img, CV_8U, img.depth() == CV_16U ? 1.0/256.0 : 255.0);
    }

    // Build dictionary and board
    cv::aruco::Dictionary dict = cv::aruco::getPredefinedDictionary(parseDictType(dictName));
    cv::aruco::CharucoBoard board(cv::Size(squaresX, squaresY), squareLength, markerLength, dict);
    if (firstMarkerId >= 0) {
        const auto markerCount = static_cast<int>(board.getIds().size());
        const int dictCapacity = dict.bytesList.rows;
        const long long endExclusive = static_cast<long long>(firstMarkerId) + markerCount;
        if (firstMarkerId < 0 || firstMarkerId >= dictCapacity || endExclusive > dictCapacity) {
            return ExecutionResult::fail(
                "charuco_detect: first_marker_id out of dictionary range. "
                "dict='" + dictName + "' supports IDs [0.." + std::to_string(dictCapacity - 1) +
                "], board needs " + std::to_string(markerCount) + " contiguous IDs starting from " +
                std::to_string(firstMarkerId) + ".");
        }

        std::vector<int> customIds;
        customIds.reserve(markerCount);
        for (int i = 0; i < markerCount; ++i) {
            customIds.push_back(firstMarkerId + i);
        }
        board = cv::aruco::CharucoBoard(cv::Size(squaresX, squaresY), squareLength, markerLength, dict, customIds);
    }

    // Configure ArUco detector parameters
    cv::aruco::DetectorParameters detParams;
    detParams.minMarkerPerimeterRate  = static_cast<float>(minMarkerPerim);
    detParams.maxMarkerPerimeterRate  = static_cast<float>(maxMarkerPerim);
    detParams.adaptiveThreshConstant  = threshConstant;
    detParams.errorCorrectionRate     = static_cast<float>(errorCorrection);

    // Configure charuco parameters
    cv::aruco::CharucoParameters charucoParams;
    charucoParams.minMarkers       = minMarkers;
    charucoParams.tryRefineMarkers = refine;

    cv::aruco::CharucoDetector detector(board, charucoParams, detParams);

    // Detect — also capture intermediate ArUco marker detections for diagnostics
    std::vector<cv::Point2f> charucoCorners;
    std::vector<int>         charucoIds;
    std::vector<std::vector<cv::Point2f>> markerCorners;
    std::vector<int>                      markerIds;
    detector.detectBoard(img, charucoCorners, charucoIds, markerCorners, markerIds);

    // Auto-retry: if markers were found but none of them produced corners
    // (e.g. because the board is partially visible and minMarkers > 1),
    // retry with minMarkers=1 before giving up.
    if (!markerIds.empty() && charucoCorners.empty() && minMarkers > 1) {
        cv::aruco::CharucoParameters retryParams = charucoParams;
        retryParams.minMarkers = 1;
        cv::aruco::CharucoDetector retryDetector(board, retryParams, detParams);
        retryDetector.detectBoard(img, charucoCorners, charucoIds, markerCorners, markerIds);
    }

    bool found = !charucoCorners.empty();

    if (verbose || (!found && markerIds.empty())) {
        // Rate-limit the "0 markers found" warning to avoid console spam.
        // Verbose mode always prints every frame.
        static thread_local int s_noMarkerCount = 0;
        bool shouldPrint = verbose || (++s_noMarkerCount == 1) ||
                           (s_noMarkerCount % 300 == 0);
        if (shouldPrint) {
            std::cerr << "[charuco_detect] markers=" << markerIds.size()
                      << " charuco_corners=" << charucoCorners.size()
                      << " dict=" << dictName
                      << " board=" << squaresX << "x" << squaresY
                      << " first_marker_id=" << (firstMarkerId >= 0 ? std::to_string(firstMarkerId) : std::string("default"))
                      << " min_markers=" << minMarkers
                      << " min_marker_perim=" << minMarkerPerim
                      << (markerIds.empty() ? " ← check dict/image quality" : "")
                      << ((!markerIds.empty() && charucoCorners.empty())
                          ? " ← markers found but no corners: try lower min_markers" : "")
                      << "\n";
        }
        if (found) s_noMarkerCount = 0;  // reset on success
    }

    // Store corners in cache as Nx1 CV_32FC2 (matches find_chessboard_corners layout)
    if (found) {
        cv::Mat cornersMat(static_cast<int>(charucoCorners.size()), 1, CV_32FC2);
        for (int i = 0; i < static_cast<int>(charucoCorners.size()); ++i) {
            cornersMat.at<cv::Point2f>(i) = charucoCorners[i];
        }
        ctx.cacheManager->set(cornersCache, cornersMat);

        cv::Mat idsMat(static_cast<int>(charucoIds.size()), 1, CV_32S);
        for (int i = 0; i < static_cast<int>(charucoIds.size()); ++i) {
            idsMat.at<int>(i) = charucoIds[i];
        }
        ctx.cacheManager->set(idsCache, idsMat);
    } else {
        // Explicitly clear cache on failure — consistent with find_chessboard_corners,
        // ensures cache_ready("corners") correctly returns false this frame.
        ctx.cacheManager->set(cornersCache, cv::Mat());
        ctx.cacheManager->set(idsCache,     cv::Mat());
    }

    ctx.cacheManager->set(cornersCache + "_found",
                          cv::Mat(1, 1, CV_8U, cv::Scalar(found ? 1 : 0)));

    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DrawCharucoItem
// ============================================================================

DrawCharucoItem::DrawCharucoItem() {
    _functionName = "draw_charuco";
    _description  = "Draws detected ChArUco corners on the image";
    _category     = "stereo";
    _params = {
        ParamDef::optional("corners_cache", BaseType::STRING, "Cache ID for ChArUco corners",  "charuco_corners"),
        ParamDef::optional("ids_cache",     BaseType::STRING, "Cache ID for ChArUco IDs",      "charuco_ids"),
        ParamDef::optional("color_r",       BaseType::INT,    "Red channel of corner color",   0),
        ParamDef::optional("color_g",       BaseType::INT,    "Green channel of corner color", 255),
        ParamDef::optional("color_b",       BaseType::INT,    "Blue channel of corner color",  0)
    };
    _example  = "draw_charuco(\"charuco_corners\", \"charuco_ids\")";
    _returnType = "mat";
    _tags = {"calibration", "charuco", "aruco", "draw"};
}

ExecutionResult DrawCharucoItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cornersCache = args.size() > 0 ? args[0].asString() : "charuco_corners";
    std::string idsCache     = args.size() > 1 ? args[1].asString() : "charuco_ids";
    int colorR = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;
    int colorG = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 255;
    int colorB = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 0;

    cv::Mat cornersMat = ctx.cacheManager->get(cornersCache);
    cv::Mat idsMat     = ctx.cacheManager->get(idsCache);

    if (cornersMat.empty()) {
        // Nothing detected – return image unchanged
        return ExecutionResult::ok(ctx.currentMat);
    }

    cv::Mat result = ctx.currentMat.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }

    cv::Scalar color(colorB, colorG, colorR);

    for (int i = 0; i < cornersMat.rows; ++i) {
        cv::Point2f pt = cornersMat.at<cv::Point2f>(i);
        cv::circle(result, pt, 6, color, 2);

        if (!idsMat.empty() && i < idsMat.rows) {
            int id = idsMat.at<int>(i);
            cv::putText(result, std::to_string(id),
                        cv::Point(static_cast<int>(pt.x) + 8, static_cast<int>(pt.y) - 8),
                        cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1);
        }
    }

    return ExecutionResult::ok(result);
}

} // namespace visionpipe

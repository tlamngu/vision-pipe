#ifndef VISIONPIPE_STEREO_ITEMS_H
#define VISIONPIPE_STEREO_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/objdetect/charuco_detector.hpp>

namespace visionpipe {

void registerStereoItems(ItemRegistry& registry);

// ============================================================================
// Stereo Matching
// ============================================================================

/**
 * @brief Block matching stereo correspondence algorithm
 * 
 * Parameters:
 * - num_disparities: Number of disparities (must be divisible by 16; default: 64)
 * - block_size: Block size (odd number; default: 21)
 * - pre_filter_type: "normalized_response" or "xsobel" (default: "normalized_response")
 * - pre_filter_size: Pre-filter size (5-255; default: 9)
 * - pre_filter_cap: Pre-filter cap (1-63; default: 31)
 * - texture_threshold: Texture threshold (default: 10)
 * - uniqueness_ratio: Uniqueness ratio (default: 15)
 * - speckle_window_size: Speckle window size (default: 100)
 * - speckle_range: Speckle range (default: 32)
 * - disp12_max_diff: Max allowed difference in left-right check (default: 1)
 * - min_disparity: Minimum disparity (default: 0)
 */
class StereoBMItem : public InterpreterItem {
public:
    StereoBMItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Semi-global block matching stereo algorithm
 * 
 * Parameters:
 * - num_disparities: Number of disparities (divisible by 16; default: 64)
 * - block_size: Block size (odd 1-11; default: 3)
 * - min_disparity: Minimum disparity (default: 0)
 * - P1: First smoothness penalty (default: 8*channels*block_size^2)
 * - P2: Second smoothness penalty (default: 32*channels*block_size^2)
 * - disp12_max_diff: Max allowed difference in left-right check (default: 1)
 * - pre_filter_cap: Truncation value for prefiltered pixels (default: 63)
 * - uniqueness_ratio: Margin in percentage (default: 10)
 * - speckle_window_size: Max speckle size (default: 100)
 * - speckle_range: Max disparity variation within speckle (default: 32)
 * - mode: "sgbm", "hh", "sgbm_3way", "hh4" (default: "sgbm")
 */
class StereoSGBMItem : public InterpreterItem {
public:
    StereoSGBMItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Reprojects disparity map to 3D
 * 
 * Parameters:
 * - Q: Disparity-to-depth mapping matrix (4x4)
 * - handle_missing: Handle missing values (default: false)
 * - ddepth: Output depth (-1 for CV_32F; default: CV_32F)
 */
class ReprojectImageTo3DItem : public InterpreterItem {
public:
    ReprojectImageTo3DItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Camera Calibration
// ============================================================================

/**
 * @brief Finds chessboard corners for calibration
 *
 * Uses the robust saddle-point detector (findChessboardCornersSB) by default.
 * Falls back to the classic detector when use_sb=false.
 *
 * Parameters:
 * - pattern_width:  Inner corners per row
 * - pattern_height: Inner corners per column
 * - flags:          Classic-mode flags: adaptive_thresh, normalize_image,
 *                   filter_quads, fast_check (only used when use_sb=false)
 * - cache_id:       Cache key for corners (default: "corners")
 * - use_sb:         Use findChessboardCornersSB (default: true)
 * - sb_marker_size: SB marker size relative to square 0.1–0.9 (default: 0.5)
 * - sb_exhaustive:  SB exhaustive search – slower but more robust (default: false)
 * - subpix_win:     Sub-pixel window half-size for classic mode (default: 11)
 */
class FindChessboardCornersItem : public InterpreterItem {
public:
    FindChessboardCornersItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Finds circular grid pattern
 * 
 * Parameters:
 * - pattern_size: [cols, rows] of circles
 * - flags: "symmetric_grid" or "asymmetric_grid", "clustering"
 */
class FindCirclesGridItem : public InterpreterItem {
public:
    FindCirclesGridItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws detected chessboard corners
 * 
 * Parameters:
 * - pattern_size: [cols, rows]
 * - corners: Corner points
 * - pattern_was_found: Whether pattern was found
 */
class DrawChessboardCornersItem : public InterpreterItem {
public:
    DrawChessboardCornersItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Saves detected chessboard corners to a JSON file in the specified directory.
 * Only saves when the chessboard was successfully found. Files are auto-numbered
 * (chessboard_0000.json, chessboard_0001.json, …) inside file_dir.
 *
 * Parameters:
 * - file_dir: Directory path where JSON files will be written
 * - corners_cache: Cache ID for corners (default: "corners")
 * - pattern_width: Inner corner columns for metadata (default: 0 = omit)
 * - pattern_height: Inner corner rows for metadata (default: 0 = omit)
 */
class ChessboardDetectedSaveItem : public InterpreterItem {
public:
    ChessboardDetectedSaveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Detects a ChArUco board (ArUco markers + chessboard grid)
 *
 * Parameters:
 * - squares_x:      Number of chessboard squares in X direction (default: 10)
 * - squares_y:      Number of chessboard squares in Y direction (default: 7)
 * - square_length:  Size of a chessboard square in meters (default: 0.04)
 * - marker_length:  Size of embedded ArUco markers in meters (default: 0.02)
 * - dict:           ArUco dictionary: "4x4_50", "4x4_100", "4x4_250",
 *                   "5x5_50", "5x5_100", "5x5_250", "6x6_50", "6x6_250",
 *                   "7x7_50", "7x7_250", "aruco_original" (default: "4x4_50")
 * - min_markers:    Minimum adjacent markers to accept a corner (default: 2)
 * - refine:         Try sub-pixel refinement of marker positions (default: false)
 * - corners_cache:  Cache ID for detected ChArUco corners (default: "charuco_corners")
 * - ids_cache:      Cache ID for detected ChArUco corner IDs (default: "charuco_ids")
 * - first_marker_id:First marker ID in board layout (default: -1 = OpenCV default)
 */
class CharucoDetectItem : public InterpreterItem {
public:
    CharucoDetectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws detected ChArUco corners on the current image
 *
 * Parameters:
 * - corners_cache:  Cache ID for corners (default: "charuco_corners")
 * - ids_cache:      Cache ID for IDs (default: "charuco_ids")
 * - color_r:        Red channel of corner marker color (default: 0)
 * - color_g:        Green channel (default: 255)
 * - color_b:        Blue channel (default: 0)
 */
class DrawCharucoItem : public InterpreterItem {
public:
    DrawCharucoItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calibrates camera from chessboard images
 * 
 * Parameters:
 * - object_points: 3D points in world coordinates
 * - image_points: Corresponding 2D points
 * - image_size: [width, height] of calibration images
 * - flags: Calibration flags
 */
class CalibrateCameraItem : public InterpreterItem {
public:
    CalibrateCameraItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calibrates stereo camera pair
 * 
 * Parameters:
 * - object_points: 3D object points
 * - image_points_left: 2D points in left images
 * - image_points_right: 2D points in right images
 * - camera_matrix_left/right: Intrinsic matrices
 * - dist_coeffs_left/right: Distortion coefficients
 * - image_size: Image size [width, height]
 * - flags: Stereo calibration flags
 */
class StereoCalibrateItem : public InterpreterItem {
public:
    StereoCalibrateItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Rectification
// ============================================================================

/**
 * @brief Computes rectification transforms for stereo camera
 * 
 * Parameters:
 * - camera_matrix_left/right: Intrinsic matrices
 * - dist_coeffs_left/right: Distortion coefficients
 * - image_size: Image size
 * - R: Rotation between cameras
 * - T: Translation between cameras
 * - flags: "zero_disparity" (default)
 * - alpha: Free scaling parameter (0-1; default: -1)
 * - new_image_size: New image size (default: same as input)
 */
class StereoRectifyItem : public InterpreterItem {
public:
    StereoRectifyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Computes undistortion and rectification maps
 * 
 * Parameters:
 * - camera_matrix: Intrinsic camera matrix
 * - dist_coeffs: Distortion coefficients
 * - R: Optional rectification transform
 * - new_camera_matrix: New camera matrix
 * - size: Undistorted image size
 * - m1type: Type of first output map (CV_16SC2 or CV_32FC1)
 */
class InitUndistortRectifyMapItem : public InterpreterItem {
public:
    InitUndistortRectifyMapItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Undistorts image using camera matrix and distortion coefficients
 * 
 * Parameters:
 * - camera_matrix: Intrinsic camera matrix
 * - dist_coeffs: Distortion coefficients
 * - new_camera_matrix: Optional new camera matrix (default: same as input)
 */
class UndistortItem : public InterpreterItem {
public:
    UndistortItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Undistorts points using camera parameters
 * 
 * Parameters:
 * - camera_matrix: Intrinsic matrix
 * - dist_coeffs: Distortion coefficients
 * - R: Optional rectification transform
 * - P: Optional new projection matrix
 */
class UndistortPointsItem : public InterpreterItem {
public:
    UndistortPointsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Depth Estimation
// ============================================================================

/**
 * @brief Filters disparity map using WLS filter
 * 
 * Parameters:
 * - left_disparity: Left disparity map
 * - right_disparity: Right disparity map (optional for guided filter)
 * - left_image: Left image for guidance
 * - lambda: Regularization parameter (default: 8000)
 * - sigma_color: Color sensitivity (default: 1.5)
 * - lrc_thresh: Left-right consistency threshold (default: 24)
 * - depth_radius: Depth discontinuity radius (default: 1)
 */
class DisparityWLSFilterItem : public InterpreterItem {
public:
    DisparityWLSFilterItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Normalizes disparity map for visualization
 * 
 * Parameters:
 * - min_disparity: Minimum disparity value
 * - num_disparities: Number of disparities
 */
class NormalizeDisparityItem : public InterpreterItem {
public:
    NormalizeDisparityItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Converts disparity to depth using baseline and focal length
 * 
 * Parameters:
 * - baseline: Distance between cameras (in same units as desired depth)
 * - focal_length: Focal length in pixels
 * - min_depth: Minimum valid depth (default: 0)
 * - max_depth: Maximum valid depth (default: infinity)
 */
class DisparityToDepthItem : public InterpreterItem {
public:
    DisparityToDepthItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Essential & Fundamental Matrix
// ============================================================================

/**
 * @brief Finds fundamental matrix from point correspondences
 * 
 * Parameters:
 * - method: "7point", "8point", "ransac", "lmeds" (default: "ransac")
 * - ransac_reproj_threshold: RANSAC threshold (default: 3.0)
 * - confidence: Confidence level (default: 0.99)
 * - max_iters: Maximum RANSAC iterations (default: 1000)
 */
class FindFundamentalMatItem : public InterpreterItem {
public:
    FindFundamentalMatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Finds essential matrix from point correspondences
 * 
 * Parameters:
 * - method: "ransac", "lmeds" (default: "ransac")
 * - prob: Confidence level (default: 0.999)
 * - threshold: RANSAC threshold (default: 1.0)
 */
class FindEssentialMatItem : public InterpreterItem {
public:
    FindEssentialMatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Recovers relative camera rotation and translation from essential matrix
 */
class RecoverPoseItem : public InterpreterItem {
public:
    RecoverPoseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Triangulates points from two camera views
 * 
 * Parameters:
 * - proj_matrix_1: First camera projection matrix
 * - proj_matrix_2: Second camera projection matrix
 * - points_1: Points in first image
 * - points_2: Corresponding points in second image
 */
class TriangulatePointsItem : public InterpreterItem {
public:
    TriangulatePointsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Pose Estimation
// ============================================================================

/**
 * @brief Solves PnP problem (object pose from point correspondences)
 * 
 * Parameters:
 * - object_points: 3D object points
 * - image_points: 2D image points
 * - camera_matrix: Intrinsic matrix
 * - dist_coeffs: Distortion coefficients
 * - method: "iterative", "p3p", "ap3p", "epnp", "ippe", "ippe_square" (default: "iterative")
 * - use_extrinsic_guess: Use initial guess (default: false)
 */
class SolvePnPItem : public InterpreterItem {
public:
    SolvePnPItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Solves PnP with RANSAC for robust estimation
 * 
 * Parameters:
 * - Same as solvePnP plus:
 * - iterations_count: RANSAC iterations (default: 100)
 * - reprojection_error: RANSAC threshold (default: 8.0)
 * - confidence: RANSAC confidence (default: 0.99)
 */
class SolvePnPRansacItem : public InterpreterItem {
public:
    SolvePnPRansacItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Projects 3D points to image plane
 * 
 * Parameters:
 * - object_points: 3D points
 * - rvec: Rotation vector
 * - tvec: Translation vector
 * - camera_matrix: Intrinsic matrix
 * - dist_coeffs: Distortion coefficients
 */
class ProjectPointsItem : public InterpreterItem {
public:
    ProjectPointsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_STEREO_ITEMS_H

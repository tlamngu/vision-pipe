#ifndef VISIONPIPE_FEATURE_ITEMS_H
#define VISIONPIPE_FEATURE_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>

namespace visionpipe {

void registerFeatureItems(ItemRegistry& registry);

// ============================================================================
// Corner Detection
// ============================================================================

/**
 * @brief Detects corners using Shi-Tomasi method
 * 
 * Parameters:
 * - max_corners: Maximum number of corners (default: 100)
 * - quality_level: Minimal accepted quality (default: 0.01)
 * - min_distance: Minimum distance between corners (default: 10)
 * - block_size: Averaging block size (default: 3)
 * - use_harris: Use Harris detector (default: false)
 * - k: Harris detector free parameter (default: 0.04)
 * - mask: Optional mask image
 */
class GoodFeaturesToTrackItem : public InterpreterItem {
public:
    GoodFeaturesToTrackItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Harris corner detector
 * 
 * Parameters:
 * - block_size: Neighborhood size (default: 2)
 * - ksize: Aperture parameter for Sobel (default: 3)
 * - k: Harris detector free parameter (default: 0.04)
 * - border_type: Border extrapolation type
 */
class CornerHarrisItem : public InterpreterItem {
public:
    CornerHarrisItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Minimum eigenvalue corner detector
 * 
 * Parameters:
 * - block_size: Neighborhood size (default: 3)
 * - ksize: Aperture size for Sobel (default: 3)
 * - border_type: Border extrapolation type
 */
class CornerMinEigenValItem : public InterpreterItem {
public:
    CornerMinEigenValItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Refines corner locations to sub-pixel accuracy
 * 
 * Parameters:
 * - win_size: Half of the side length of the search window
 * - zero_zone: Half of size of dead region in the search
 * - max_iter: Maximum iterations (default: 40)
 * - epsilon: Required accuracy (default: 0.001)
 */
class CornerSubPixItem : public InterpreterItem {
public:
    CornerSubPixItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Blob Detection
// ============================================================================

/**
 * @brief Simple blob detector
 * 
 * Parameters:
 * - min_threshold: Min threshold for binary (default: 10)
 * - max_threshold: Max threshold (default: 200)
 * - threshold_step: Step between thresholds (default: 10)
 * - min_repeatability: Minimum repeatability (default: 2)
 * - min_dist_between_blobs: Minimum distance (default: 10)
 * - filter_by_color: Filter by color (default: true)
 * - blob_color: 0 for dark, 255 for light (default: 0)
 * - filter_by_area: Filter by area (default: true)
 * - min_area: Minimum blob area (default: 25)
 * - max_area: Maximum blob area (default: 5000)
 * - filter_by_circularity: (default: false)
 * - min_circularity: (default: 0.8)
 * - max_circularity: (default: 1.0)
 * - filter_by_convexity: (default: false)
 * - min_convexity: (default: 0.95)
 * - max_convexity: (default: 1.0)
 * - filter_by_inertia: (default: false)
 * - min_inertia_ratio: (default: 0.1)
 * - max_inertia_ratio: (default: 1.0)
 */
class SimpleBlobDetectorItem : public InterpreterItem {
public:
    SimpleBlobDetectorItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Feature Detection & Description
// ============================================================================

/**
 * @brief ORB (Oriented FAST and Rotated BRIEF) feature detector
 * 
 * Parameters:
 * - n_features: Maximum number of features (default: 500)
 * - scale_factor: Pyramid decimation ratio (default: 1.2)
 * - n_levels: Number of pyramid levels (default: 8)
 * - edge_threshold: Border threshold (default: 31)
 * - first_level: First pyramid level (default: 0)
 * - WTA_K: Points for BRIEF descriptor (default: 2)
 * - score_type: "harris" or "fast" (default: "harris")
 * - patch_size: Size of patch for descriptor (default: 31)
 * - fast_threshold: FAST threshold (default: 20)
 */
class ORBItem : public InterpreterItem {
public:
    ORBItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief SIFT feature detector (requires opencv-contrib)
 * 
 * Parameters:
 * - n_features: Number of best features to retain (default: 0 = all)
 * - n_octave_layers: Number of layers per octave (default: 3)
 * - contrast_threshold: Contrast threshold (default: 0.04)
 * - edge_threshold: Edge response threshold (default: 10)
 * - sigma: Gaussian sigma (default: 1.6)
 */
class SIFTItem : public InterpreterItem {
public:
    SIFTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief AKAZE feature detector
 * 
 * Parameters:
 * - descriptor_type: "kaze", "kaze_upright", "mldb", "mldb_upright" (default: "mldb")
 * - descriptor_size: Size of descriptor (default: 0)
 * - descriptor_channels: Channels per pxel (default: 3)
 * - threshold: Detector response threshold (default: 0.001)
 * - n_octaves: Number of octaves (default: 4)
 * - n_octave_layers: Number of layers per octave (default: 4)
 * - diffusivity: "pm_g1", "pm_g2", "weickert", "charbonnier" (default: "pm_g2")
 */
class AKAZEItem : public InterpreterItem {
public:
    AKAZEItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief BRISK feature detector
 * 
 * Parameters:
 * - thresh: AGAST detection threshold (default: 30)
 * - octaves: Number of octaves (default: 3)
 * - pattern_scale: Pattern scale (default: 1.0)
 */
class BRISKItem : public InterpreterItem {
public:
    BRISKItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FAST corner detector
 * 
 * Parameters:
 * - threshold: Detection threshold (default: 10)
 * - nonmax_suppression: Apply non-max suppression (default: true)
 * - type: "type_9_16", "type_7_12", "type_5_8" (default: "type_9_16")
 */
class FASTItem : public InterpreterItem {
public:
    FASTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief AGAST corner detector
 * 
 * Parameters:
 * - threshold: Detection threshold (default: 10)
 * - nonmax_suppression: Apply non-max suppression (default: true)
 * - type: "5_8", "7_12d", "7_12s", "oast_9_16" (default: "oast_9_16")
 */
class AGASTItem : public InterpreterItem {
public:
    AGASTItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief KAZE feature detector
 * 
 * Parameters:
 * - extended: Use extended descriptor (default: false)
 * - upright: Use upright (non-rotation-invariant) version (default: false)
 * - threshold: Detector response threshold (default: 0.001)
 * - n_octaves: Number of octaves (default: 4)
 * - n_octave_layers: Number of sublayers per octave (default: 4)
 * - diffusivity: Diffusivity type (default: "pm_g2")
 */
class KAZEItem : public InterpreterItem {
public:
    KAZEItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Feature Matching
// ============================================================================

/**
 * @brief Brute-force descriptor matcher
 * 
 * Parameters:
 * - norm_type: "l1", "l2", "hamming", "hamming2" (default: "l2")
 * - cross_check: Enable cross-check filtering (default: false)
 */
class BFMatcherItem : public InterpreterItem {
public:
    BFMatcherItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief FLANN-based descriptor matcher
 * 
 * Parameters:
 * - index_type: "kdtree", "kmeans", "composite", "lsh" (default: "kdtree")
 * - trees: Number of trees for KD-tree (default: 5)
 * - checks: Number of checks for search (default: 50)
 */
class FlannMatcherItem : public InterpreterItem {
public:
    FlannMatcherItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws feature matches between two images
 * 
 * Parameters:
 * - match_color: Color for matches (default: random)
 * - single_point_color: Color for keypoints (default: random)
 * - flags: "default", "draw_over", "not_draw_single" (default: "default")
 */
class DrawMatchesItem : public InterpreterItem {
public:
    DrawMatchesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Draws keypoints on image
 * 
 * Parameters:
 * - color: Keypoint color (default: random)
 * - flags: "default", "draw_rich_keypoints", "not_draw_single_points" (default: "default")
 */
class DrawKeypointsItem : public InterpreterItem {
public:
    DrawKeypointsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Homography & Perspective
// ============================================================================

/**
 * @brief Finds homography matrix between two sets of points
 * 
 * Parameters:
 * - method: "0" (regular), "ransac", "lmeds", "rho" (default: "0")
 * - ransac_reproj_threshold: Maximum reprojection error (default: 3.0)
 * - max_iters: Maximum RANSAC iterations (default: 2000)
 * - confidence: Confidence level (default: 0.995)
 */
class FindHomographyItem : public InterpreterItem {
public:
    FindHomographyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies perspective transformation
 * 
 * Uses homography matrix to transform points
 */
class PerspectiveTransformItem : public InterpreterItem {
public:
    PerspectiveTransformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Applies perspective transformation to points from cache
 * 
 * Parameters:
 * - points_cache: Cache ID containing points
 * - matrix_cache: Cache ID containing transformation matrix
 * - output_cache: Cache ID for output (default: "transformed_points")
 */
class PerspectiveTransformPointsItem : public InterpreterItem {
public:
    PerspectiveTransformPointsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Template Matching
// ============================================================================

/**
 * @brief Template matching
 * 
 * Parameters:
 * - method: "sqdiff", "sqdiff_normed", "ccorr", "ccorr_normed", 
 *           "ccoeff", "ccoeff_normed" (default: "ccoeff_normed")
 * - mask: Optional mask for template
 */
class MatchTemplateItem : public InterpreterItem {
public:
    MatchTemplateItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Finds location of minimum and maximum values
 * 
 * Returns [min_val, max_val, min_loc_x, min_loc_y, max_loc_x, max_loc_y]
 */
class MinMaxLocItem : public InterpreterItem {
public:
    MinMaxLocItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_FEATURE_ITEMS_H

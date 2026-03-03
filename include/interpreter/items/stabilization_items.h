#ifndef VISIONPIPE_STABILIZATION_ITEMS_H
#define VISIONPIPE_STABILIZATION_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <opencv2/video.hpp>

namespace visionpipe {

void registerStabilizationItems(ItemRegistry& registry);

// ============================================================================
// Horizon Lock / Video Stabilization
// ============================================================================

/**
 * @brief Horizon lock — keeps the frame steady against rotational motion.
 *
 * Tracks Shi-Tomasi feature points across consecutive frames via Lucas-Kanade
 * optical flow, estimates the per-frame rotation with a RANSAC partial-affine
 * fit, then applies a corrective counter-rotation so the horizon stays level.
 *
 * Algorithm
 * ---------
 *  1. Convert frame to grayscale; detect Shi-Tomasi corners on first frame
 *     (and re-detect whenever the tracked set drops below 25 % of max_points).
 *  2. Track corners from the previous frame with calcOpticalFlowPyrLK.
 *  3. Estimate partial-affine (RANSAC) → extract per-frame Δθ.
 *  4. Smooth the raw Δθ with an EMA (parameter `delta_smooth`) to reduce
 *     measurement noise **without introducing correction lag**:
 *       smoothed_delta = delta_smooth × smoothed_delta + (1−delta_smooth) × Δθ
 *  5. Integrate into the accumulated angle:
 *       accumulated += smoothed_delta
 *  6. Apply a gentle per-frame decay so very slow, intentional tilts are
 *     eventually accepted instead of endlessly fought:
 *       accumulated *= drift_decay   (e.g. 0.997 per frame at 30 fps)
 *  7. Apply the **full** counter-rotation  correction = −accumulated  to the
 *     frame (no trajectory-smoothing lag, no overshoot).
 *  8. Optionally scale the result to fill the frame (no black borders).
 *
 * Parameters
 * ----------
 * - delta_smooth  FLOAT  EMA coefficient for per-frame delta noise filtering
 *                        ∈ [0, 1).  Higher = smoother deltas, slower initial
 *                        response (default 0.5).
 * - drift_decay   FLOAT  Per-frame decay of the accumulated angle ∈ (0, 1].
 *                        1.0 = hard lock (never accepts tilt).
 *                        ~0.997 = soft lock (accepts very slow tilts over
 *                        ~10 s at 30 fps). (default 0.997)
 * - max_points    INT    Maximum Shi-Tomasi corner count (default 200).
 * - quality       FLOAT  Shi-Tomasi quality level (default 0.01).
 * - min_distance  FLOAT  Minimum distance between corners in px (default 10).
 * - crop          BOOL   Scale-crop to hide rotation borders (default true).
 * - reset         BOOL   Clear accumulated state and restart (default false).
 *
 * Example
 * -------
 *   video_cap("/dev/video0") | horizon_lock()
 *   video_cap("/dev/video0") | horizon_lock(0.5, 0.997, 200, 0.01, 10, true)
 */
class HorizonLockItem : public InterpreterItem {
public:
    HorizonLockItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;

private:
    cv::Mat  _prevGray;                    ///< Grayscale of the previous frame
    std::vector<cv::Point2f> _prevPts;     ///< Feature points from previous frame

    double   _accumulatedAngle = 0.0;      ///< Integrated rotation since start (radians)
    double   _smoothedDelta    = 0.0;      ///< EMA of per-frame delta (noise filter)
    bool     _initialised      = false;    ///< True after the first frame pair

    static std::vector<cv::Point2f> detectPoints(const cv::Mat& gray,
                                                  int maxPoints,
                                                  double quality,
                                                  double minDist);
};

// ============================================================================
// Video Stabilizer (full 2-D: translation + rotation)
// ============================================================================

/**
 * @brief Full 2-D video stabilizer — cancels both translational shake and
 *        rotational jitter in a single warp.
 *
 * Uses the same Lucas-Kanade / RANSAC pipeline as horizon_lock, but estimates
 * a full 4-DOF partial-affine transform (Δx, Δy, Δθ) each frame.
 * Each component is noise-filtered with its own EMA, integrated into an
 * accumulated trajectory, and a per-frame drift decay lets slow intentional
 * camera motion through.
 *
 * Algorithm
 * ---------
 *  1. Shi-Tomasi feature detection + Lucas-Kanade PyrLK tracking.
 *  2. estimateAffinePartial2D (RANSAC) → extract Δx, Δy, Δθ.
 *  3. EMA noise-filter each delta independently.
 *  4. Accumulate: accX += smoothedDx, accY += smoothedDy, accA += smoothedDa.
 *  5. Decay:      acc* *= drift_decay.
 *  6. Apply full counter-transform (−accX, −accY, −accA) as a single warpAffine.
 *  7. Optionally crop/scale to hide borders.
 *
 * Parameters
 * ----------
 * - delta_smooth  FLOAT  EMA coefficient for per-frame delta noise [0,1) (default 0.5).
 * - drift_decay   FLOAT  Per-frame decay ∈ (0,1]. 1.0 = hard lock (default 0.997).
 * - max_points    INT    Max Shi-Tomasi corners (default 200).
 * - quality       FLOAT  Shi-Tomasi quality level (default 0.01).
 * - min_distance  FLOAT  Min corner distance in px (default 10).
 * - crop          BOOL   Scale-crop to hide borders (default true).
 * - reset         BOOL   Clear state and restart (default false).
 *
 * Example
 * -------
 *   video_cap("/dev/video0") | video_stabilize()
 *   video_cap("/dev/video0") | video_stabilize(0.5, 0.997)
 *   // hard lock (no drift allowed):
 *   video_cap("/dev/video0") | video_stabilize(0.5, 1.0)
 */
class VideoStabilizeItem : public InterpreterItem {
public:
    VideoStabilizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;

private:
    cv::Mat  _prevGray;
    std::vector<cv::Point2f> _prevPts;

    // Accumulated trajectory (what we correct against)
    double _accX = 0.0;   ///< accumulated X translation (px)
    double _accY = 0.0;   ///< accumulated Y translation (px)
    double _accA = 0.0;   ///< accumulated rotation     (rad)

    // EMA-smoothed per-frame deltas (noise filter)
    double _smoothDx = 0.0;
    double _smoothDy = 0.0;
    double _smoothDa = 0.0;

    bool _initialised = false;

    static std::vector<cv::Point2f> detectPoints(const cv::Mat& gray,
                                                  int maxPoints,
                                                  double quality,
                                                  double minDist);
};

// ============================================================================
// Stereo Video Stabilizer (synchronised 2-D stabilization on two frames)
// ============================================================================

/**
 * @brief Stereo-synchronised 2-D video stabilizer — tracks feature points in
 *        **two** frames (left = currentMat, right = cache), estimates a single
 *        shared correction transform, and applies it identically to both views.
 *
 * This keeps the stereo pair geometrically consistent: because both frames
 * receive the exact same counter-transform, the epipolar geometry is
 * preserved, so downstream stereo matching / depth estimation is unaffected.
 *
 * Algorithm
 * ---------
 *  1. Read left frame from currentMat, right frame from `right_cache`.
 *  2. Convert both to grayscale; detect Shi-Tomasi corners on first frame
 *     (re-detect when the tracked set drops below 25 % of max_points).
 *  3. Track corners from both previous frames with calcOpticalFlowPyrLK.
 *  4. Pool good matches from left and right, then estimateAffinePartial2D
 *     (RANSAC) on the combined set → shared Δx, Δy, Δθ.
 *  5. EMA noise-filter each delta independently.
 *  6. Accumulate and decay (same as video_stabilize).
 *  7. Apply the **same** counter-transform to both left and right frames.
 *  8. Output stabilised left → currentMat, stabilised right → `output_cache`.
 *
 * Parameters
 * ----------
 * - right_cache    STRING Cache key of the right frame (required).
 * - output_cache   STRING Cache key to store the stabilised right frame
 *                         (default: "stabilized_right").
 * - delta_smooth   FLOAT  EMA coefficient for per-frame delta noise [0,1)
 *                         (default 0.5).
 * - drift_decay    FLOAT  Per-frame decay ∈ (0,1]. 1.0 = hard lock
 *                         (default 0.997).
 * - max_points     INT    Max Shi-Tomasi corners **per view** (default 200).
 * - quality        FLOAT  Shi-Tomasi quality level (default 0.01).
 * - min_distance   FLOAT  Min corner distance in px (default 10).
 * - crop           BOOL   Scale-crop to hide borders (default true).
 * - reset          BOOL   Clear state and restart (default false).
 *
 * Example
 * -------
 *   // In a stereo pipeline:
 *   use("rect_l")
 *   stereo_stabilize("rect_r", "stab_r")
 *   // currentMat = stabilised left, cache "stab_r" = stabilised right
 */
class StereoStabilizeItem : public InterpreterItem {
public:
    StereoStabilizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;

private:
    // Per-view tracking state
    cv::Mat  _prevGrayL;
    cv::Mat  _prevGrayR;
    std::vector<cv::Point2f> _prevPtsL;
    std::vector<cv::Point2f> _prevPtsR;

    // Accumulated trajectory (shared by both views)
    double _accX = 0.0;
    double _accY = 0.0;
    double _accA = 0.0;

    // EMA-smoothed per-frame deltas
    double _smoothDx = 0.0;
    double _smoothDy = 0.0;
    double _smoothDa = 0.0;

    bool _initialised = false;

    static std::vector<cv::Point2f> detectPoints(const cv::Mat& gray,
                                                  int maxPoints,
                                                  double quality,
                                                  double minDist);

    /// Build the 2×3 counter-transform matrix for the current accumulated state.
    cv::Mat buildCorrectionMatrix(double corrA, double corrX, double corrY,
                                  double W, double H, bool crop) const;
};

} // namespace visionpipe

#endif // VISIONPIPE_STABILIZATION_ITEMS_H

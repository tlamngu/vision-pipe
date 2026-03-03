#ifndef VISIONPIPE_GPU_STABILIZATION_ITEMS_H
#define VISIONPIPE_GPU_STABILIZATION_ITEMS_H

/**
 * @file gpu_stabilization_items.h
 * @brief OpenCL-accelerated horizon lock and full 2-D video stabilization.
 *
 * Two items are provided:
 *
 *  gpu_horizon_lock(...)
 *    Rotation-only stabilizer.  Same algorithm as horizon_lock() but with
 *    the per-frame feature detection, optical flow, and warp dispatched via
 *    OpenCL through OpenCV T-API (cv::UMat) and an optional native warp
 *    kernel for Mali/embedded SoCs.
 *
 *  gpu_video_stabilize(...)
 *    Full 2-D (translation + rotation) stabilizer.  Same algorithm as
 *    video_stabilize() with the same GPU acceleration strategy.
 *
 * GPU dispatch strategy (identical for both items):
 *   - goodFeaturesToTrack → T-API OpenCL via UMat
 *   - calcOpticalFlowPyrLK → T-API OpenCL via UMat
 *   - estimateAffinePartial2D → CPU (tiny point set, negligible cost)
 *   - warpAffine → **native OpenCL kernel** (primary) or T-API fallback
 *
 * When VISIONPIPE_OPENCL_ENABLED is not defined or OpenCL is unavailable at
 * runtime, both items fall back cleanly to the CPU path (semantically
 * identical to horizon_lock / video_stabilize).
 */

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <opencv2/video.hpp>

#ifdef VISIONPIPE_OPENCL_ENABLED
#include <opencv2/core/ocl.hpp>
#endif

#include <mutex>
#include <vector>

namespace visionpipe {

void registerGpuStabilizationItems(ItemRegistry& registry);

// ============================================================================
// GpuHorizonLockItem — rotation-only, OpenCL-accelerated
// ============================================================================

/**
 * @brief GPU-accelerated horizon lock (rotation-only stabilization).
 *
 * Parameters (identical to horizon_lock):
 * - delta_smooth  FLOAT  EMA for per-frame delta noise [0,1) (default 0.5)
 * - drift_decay   FLOAT  Per-frame accumulated-angle decay (0,1] (default 0.997)
 * - max_points    INT    Max Shi-Tomasi corners (default 200)
 * - quality       FLOAT  Shi-Tomasi quality (default 0.01)
 * - min_distance  FLOAT  Min corner distance in px (default 10)
 * - crop          BOOL   Scale-crop to hide borders (default true)
 * - reset         BOOL   Reset state (default false)
 */
class GpuHorizonLockItem : public InterpreterItem {
public:
    GpuHorizonLockItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }

private:
    // Tracking state
    cv::Mat _prevGray;
    std::vector<cv::Point2f> _prevPts;
    double _accumulatedAngle = 0.0;
    double _smoothedDelta    = 0.0;
    bool   _initialised      = false;

#ifdef VISIONPIPE_OPENCL_ENABLED
    // OpenCL program & persistent UMat buffers
    cv::ocl::Program getWarpProgram(bool verbose);
    cv::ocl::Program _warpProgram;
    std::mutex       _warpMutex;
    bool             _warpBuilt{false};
    bool             _warpOk{false};

    cv::UMat _srcU;   ///< Persistent input buffer
    cv::UMat _dstU;   ///< Persistent output buffer
    cv::UMat _prevGrayU;
    cv::UMat _currGrayU;
#endif

    static std::vector<cv::Point2f> detectPoints(const cv::Mat& gray,
                                                  int maxPoints,
                                                  double quality,
                                                  double minDist);
};

// ============================================================================
// GpuVideoStabilizeItem — translation + rotation, OpenCL-accelerated
// ============================================================================

/**
 * @brief GPU-accelerated full 2-D video stabilizer (translation + rotation).
 *
 * Parameters (identical to video_stabilize):
 * - delta_smooth  FLOAT  EMA for per-frame delta noise [0,1) (default 0.5)
 * - drift_decay   FLOAT  Per-frame decay (0,1] (default 0.997)
 * - max_points    INT    Max Shi-Tomasi corners (default 200)
 * - quality       FLOAT  Shi-Tomasi quality (default 0.01)
 * - min_distance  FLOAT  Min corner distance in px (default 10)
 * - crop          BOOL   Scale-crop to hide borders (default true)
 * - reset         BOOL   Reset state (default false)
 */
class GpuVideoStabilizeItem : public InterpreterItem {
public:
    GpuVideoStabilizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }

private:
    // Tracking state
    cv::Mat _prevGray;
    std::vector<cv::Point2f> _prevPts;
    double _accX = 0.0, _accY = 0.0, _accA = 0.0;
    double _smoothDx = 0.0, _smoothDy = 0.0, _smoothDa = 0.0;
    bool   _initialised = false;

#ifdef VISIONPIPE_OPENCL_ENABLED
    cv::ocl::Program getWarpProgram(bool verbose);
    cv::ocl::Program _warpProgram;
    std::mutex       _warpMutex;
    bool             _warpBuilt{false};
    bool             _warpOk{false};

    cv::UMat _srcU;
    cv::UMat _dstU;
    cv::UMat _prevGrayU;
    cv::UMat _currGrayU;
#endif

    static std::vector<cv::Point2f> detectPoints(const cv::Mat& gray,
                                                  int maxPoints,
                                                  double quality,
                                                  double minDist);
};

} // namespace visionpipe

#endif // VISIONPIPE_GPU_STABILIZATION_ITEMS_H

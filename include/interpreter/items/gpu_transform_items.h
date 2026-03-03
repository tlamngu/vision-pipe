#ifndef VISIONPIPE_GPU_TRANSFORM_ITEMS_H
#define VISIONPIPE_GPU_TRANSFORM_ITEMS_H

/**
 * @file gpu_transform_items.h
 * @brief OpenCL-accelerated color/channel transform and matrix multiplication.
 *
 * Two items are provided:
 *
 *  gpu_transform(matrix_cache, [depth])
 *    Per-pixel color/channel linear transform.  For each pixel vector p,
 *    computes:   dst_pixel = M * [p | 1]   (bias column) or M * p (no bias)
 *    where M is the float matrix stored under `matrix_cache`.
 *
 *    Primary path: native OpenCL kernel (float32 pipeline, tiled).
 *    Fallback:     cv::UMat + cv::transform() (OpenCV T-API OpenCL).
 *    CPU fallback: cv::transform().
 *
 *  gpu_matrix_mul(src2_cache, [alpha, src3_cache, beta, flags])
 *    General matrix multiplication:  dst = alpha * A * B + beta * C
 *    where A = currentMat, B = src2_cache, C = src3_cache (optional).
 *
 *    Primary path: native OpenCL tiled GEMM kernel (float32).
 *    Fallback:     cv::UMat + cv::gemm() (OpenCV T-API OpenCL).
 *    CPU fallback: cv::gemm().
 */

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

#ifdef VISIONPIPE_OPENCL_ENABLED
#include <opencv2/core/ocl.hpp>
#endif

#include <mutex>
#include <string>

namespace visionpipe {

void registerGpuTransformItems(ItemRegistry& registry);

// ============================================================================
// GpuTransformItem — per-pixel OpenCL color/channel transform
// ============================================================================

/**
 * @brief GPU-accelerated per-pixel channel transform via a cached matrix.
 *
 * Equivalent to `cv::transform(src, dst, M)` but dispatched through a native
 * OpenCL kernel when available.
 *
 * The transform matrix M (loaded from cache) must be:
 *   rows = dst_channels  (1–4)
 *   cols = src_channels or src_channels+1 (last column is bias / offset)
 *   depth = CV_32F
 *
 * Typical uses:
 *   - 3×3 colour correction matrix   (BGR→corrected BGR)
 *   - 3×4 colour correction + offset
 *   - 1×3 luminance weighting        (BGR→grayscale)
 *   - 4×3 or 4×4 with alpha channel
 *
 * Parameters:
 *   matrix_cache  : string — cache key of the float transform matrix (required)
 *   depth         : string — output depth: "8u", "16u", "32f", or "same"
 *                            (default: "same" = match input depth)
 *
 * Returns: MAT — transformed image
 */
class GpuTransformItem : public InterpreterItem {
public:
    GpuTransformItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }

private:
#ifdef VISIONPIPE_OPENCL_ENABLED
    cv::ocl::Program getMalvarProgram(bool verbose); // reuses program slot
    cv::ocl::Program getTransformProgram(bool verbose);
    cv::ocl::Program _program;
    std::mutex       _programMutex;
    bool             _programBuilt{false};
    bool             _programOk{false};

    // Persistent GPU buffers — reused across frames to avoid per-frame
    // clCreateBuffer/clReleaseMemObject overhead (critical on Mali SoC).
    cv::UMat _srcU;              ///< Input buffer (reused across frames)
    cv::UMat _dstU;              ///< Output buffer (reused across frames)
    cv::UMat _MU;                ///< Cached transform matrix on GPU
    std::string _cachedMKey;     ///< Cache key of currently uploaded matrix
    const void* _cachedMData{nullptr}; ///< Data pointer to detect matrix changes
#endif
};

// ============================================================================
// GpuMatrixMulItem — OpenCL tiled GEMM
// ============================================================================

/**
 * @brief GPU-accelerated general matrix multiplication.
 *
 *   dst = alpha * op(A) * op(B) + beta * C
 *
 * where A = currentMat, B = src2, C = src3 (optional).
 * op() is controlled by the flags parameter.
 *
 * Primary path: native OpenCL tiled GEMM kernel (float32).
 * Both A and B are automatically converted to CV_32F and must be 2-D
 * (single-channel) matrices.  The result is always CV_32F.
 *
 * Parameters:
 *   src2_cache   : string — cache key for matrix B (required)
 *   alpha        : float  — scale for A*B            (default: 1.0)
 *   src3_cache   : string — cache key for matrix C   (default: "" = zero)
 *   beta         : float  — scale for C              (default: 0.0)
 *   flags        : string — transpose flags: "none", "at", "bt", "atbt"
 *                           (default: "none")
 *
 * Returns: MAT — CV_32F result matrix
 */
class GpuMatrixMulItem : public InterpreterItem {
public:
    GpuMatrixMulItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }

private:
#ifdef VISIONPIPE_OPENCL_ENABLED
    cv::ocl::Program getGemmProgram(bool verbose);
    cv::ocl::Program _program;
    std::mutex       _programMutex;
    bool             _programBuilt{false};
    bool             _programOk{false};

    // Persistent GPU buffers — reused across frames.
    cv::UMat _AU, _BU, _DU;
#endif
};

} // namespace visionpipe

#endif // VISIONPIPE_GPU_TRANSFORM_ITEMS_H

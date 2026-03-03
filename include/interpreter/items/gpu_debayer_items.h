#ifndef VISIONPIPE_GPU_DEBAYER_ITEMS_H
#define VISIONPIPE_GPU_DEBAYER_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

#ifdef VISIONPIPE_OPENCL_ENABLED
#include <opencv2/core/ocl.hpp>
#endif

#include <string>
#include <mutex>

namespace visionpipe {

void registerGpuDebayerItems(ItemRegistry& registry);

// ============================================================================
// GPU-Accelerated Debayering
// ============================================================================

/**
 * @brief GPU-accelerated Bayer demosaicing using a native OpenCL kernel.
 *
 * The default algorithm ("malvar") runs a self-contained native OpenCL kernel
 * implementing the Malvar-He-Cutler high-quality linear interpolation method
 * (H. S. Malvar, L.-w. He, R. Cutler, 2004), adapted from cldemosaic
 * (https://github.com/nevion/cldemosaic, MIT License).
 *
 * The kernel is compiled once at first call and cached for subsequent frames.
 * Output is always 3-channel BGR (OpenCV channel order).
 *
 * Algorithms:
 *   - "malvar"   Native OpenCL Malvar-He-Cutler kernel (default, highest quality)
 *   - "bilinear" OpenCV cvtColor bilinear (CPU/OpenCL T-API)
 *   - "vng"      OpenCV variable number of gradients (CPU/OpenCL T-API)
 *   - "ea"       OpenCV edge-aware (CPU/OpenCL T-API)
 *
 * When VISIONPIPE_OPENCL_ENABLED is not defined or OpenCL is unavailable at
 * runtime, the item falls back to the CPU cvtColor path automatically.
 *
 * Parameters:
 *   pattern   : "RGGB" | "BGGR" | "GBRG" | "GRBG" | "auto"  (default: "RGGB")
 *   algorithm : "malvar" | "bilinear" | "vng" | "ea"          (default: "malvar")
 *   output    : "bgr" | "rgb"                                  (default: "bgr")
 *   bit_shift : right-shift bits for 10/12-bit packed data     (default: 0)
 *
 * Returns: MAT — 3-channel BGR (or RGB when output="rgb") image.
 */
class GpuDebayerItem : public InterpreterItem {
public:
    GpuDebayerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }

private:
#ifdef VISIONPIPE_OPENCL_ENABLED
    /**
     * Returns a compiled cv::ocl::Program for the Malvar-He-Cutler kernel.
     * The program is lazily compiled and then cached (thread-safe).
     *
     * @param verbose  Print build log on first compilation.
     * @return         A valid program, or an empty program on failure.
     */
    cv::ocl::Program getMalvarProgram(bool verbose);

    cv::ocl::Program _malvarProgram; ///< Cached compiled OCL program (empty until first use)
    std::mutex       _programMutex;  ///< Guards lazy program compilation
    bool             _programBuilt{false};
    bool             _programOk{false};

    // Persistent GPU buffers — reused across frames to avoid per-frame
    // clCreateBuffer/clReleaseMemObject overhead (critical on Mali SoC).
    cv::UMat _srcU;   ///< Input buffer (reallocated only on size change)
    cv::UMat _dstU;   ///< Output buffer (reallocated only on size change)
#endif
};

} // namespace visionpipe

#endif // VISIONPIPE_GPU_DEBAYER_ITEMS_H

#ifndef VISIONPIPE_GPU_DEBAYER_ITEMS_H
#define VISIONPIPE_GPU_DEBAYER_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <string>

namespace visionpipe {

void registerGpuDebayerItems(ItemRegistry& registry);

// ============================================================================
// GPU-Accelerated Debayering
// ============================================================================

/**
 * @brief GPU-accelerated Bayer demosaicing using OpenCL (via cv::UMat).
 *
 * Mirrors the CPU `debayer()` item but transparently uses the OpenCL path
 * when available.  OpenCV's `cvtColor` dispatches to its OpenCL T-API
 * kernels when the source and destination are `cv::UMat`, giving significant
 * speed-ups on platforms with an OpenCL-capable GPU.
 *
 * Supports the same algorithms as CPU debayer:
 *   - bilinear  (fast, default)
 *   - vng       (Variable Number of Gradients — higher quality)
 *   - ea        (Edge-Aware)
 *
 * When VISIONPIPE_OPENCL_ENABLED is **not** defined or OpenCL is unavailable
 * at runtime, the item falls back to the normal CPU path automatically.
 *
 * Parameters:
 * - pattern    : Bayer tile pattern: "RGGB", "BGGR", "GBRG", "GRBG", or "auto" (default: "RGGB")
 * - algorithm  : Demosaic algorithm: "bilinear", "vng", "ea" (default: "ea")
 * - output     : Output color order: "bgr" or "rgb" (default: "bgr")
 * - bit_shift  : Right-shift for 10/12-bit packed data in a 16-bit container (default: 0)
 *
 * Returns: MAT (3-channel BGR/RGB image)
 */
class GpuDebayerItem : public InterpreterItem {
public:
    GpuDebayerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return true; }
};

} // namespace visionpipe

#endif // VISIONPIPE_GPU_DEBAYER_ITEMS_H

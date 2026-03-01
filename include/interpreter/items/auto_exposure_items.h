#ifndef VISIONPIPE_AUTO_EXPOSURE_ITEMS_H
#define VISIONPIPE_AUTO_EXPOSURE_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <deque>
#include <string>

namespace visionpipe {

void registerAutoExposureItems(ItemRegistry& registry);

// ============================================================================
// Software Auto Exposure
// ============================================================================

/**
 * @brief Software auto-exposure: accumulates brightness samples from frames,
 *        then computes an optimal exposure value within [min_exp, max_exp].
 *
 * The item analyses the current frame's luminance (mean brightness) and
 * accumulates readings over a configurable number of samples. Once enough
 * samples are collected the algorithm produces a new exposure value that
 * steers the average brightness towards a configurable target.
 *
 * The computed exposure is returned as a **scalar float** value so that the
 * caller can store it in a variable and feed it to `v4l2_prop` or any other
 * control item:
 *
 *   exp = auto_exposure(50, 10000, 10, 128)   // returns float
 *   v4l2_prop("/dev/video0", "Exposure (Absolute)", exp)
 *
 * Parameters:
 * - min_exp    : Minimum exposure value (float, required)
 * - max_exp    : Maximum exposure value (float, required)
 * - samples    : Number of frames to accumulate before recalculating (default: 10)
 * - target     : Target mean brightness 0-255 (default: 128)
 * - speed      : Convergence speed / gain 0.0-1.0 (default: 0.3)
 * - roi_x      : ROI x-offset as fraction 0.0-1.0 (default: 0.0 = full frame)
 * - roi_y      : ROI y-offset as fraction 0.0-1.0 (default: 0.0)
 * - roi_w      : ROI width  as fraction 0.0-1.0 (default: 1.0)
 * - roi_h      : ROI height as fraction 0.0-1.0 (default: 1.0)
 *
 * Returns: FLOAT  (the calculated exposure value, clamped to [min_exp, max_exp])
 *
 * The current Mat is passed through unchanged.
 */
class AutoExposureItem : public InterpreterItem {
public:
    AutoExposureItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }

private:
    // Per-source state: keeps a sliding window of brightness readings
    struct AEState {
        std::deque<double> brightnessSamples;
        double currentExposure = 0.0;  // last computed exposure
        bool initialised = false;
    };

    // Key = stringified source identifier (can be arbitrary)
    std::map<std::string, AEState> _states;
};

/**
 * @brief Resets accumulated auto-exposure state for a given source or all sources.
 *
 * Parameters:
 * - source (optional): Source identifier to reset. If omitted all sources are reset.
 */
class AutoExposureResetItem : public InterpreterItem {
public:
    AutoExposureResetItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

} // namespace visionpipe

#endif // VISIONPIPE_AUTO_EXPOSURE_ITEMS_H

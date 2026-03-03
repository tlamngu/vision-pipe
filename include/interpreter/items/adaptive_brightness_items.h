#ifndef VISIONPIPE_ADAPTIVE_BRIGHTNESS_ITEMS_H
#define VISIONPIPE_ADAPTIVE_BRIGHTNESS_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <deque>
#include <string>
#include <map>

namespace visionpipe {

void registerAdaptiveBrightnessItems(ItemRegistry& registry);

// ============================================================================
// Adaptive Auto-Brightness (Multi-Axis PID Controller)
// ============================================================================

/**
 * @brief PID-based auto brightness controller that adjusts up to three V4L2
 *        axes: analogue gain, digital gain, and exposure time.
 *
 * The item measures the current frame's luminance (median by default, or mean)
 * and runs a PID controller every frame.  The correction output is distributed
 * across the three control axes according to a configurable priority order:
 *
 *   Priority order (default): exposure → analog_gain → digital_gain
 *
 * Each axis operates within a user-supplied [min, max] range.  When a lower-
 * priority axis is saturated, correction overflows to the next axis in line.
 *
 * V4L2 properties are written directly when prop name strings are provided.
 * Prop names are those accepted by v4l2_prop() / the V4L2DeviceManager.
 * Set a prop name to "" (empty) to skip that axis entirely.
 *
 * Usage example (all positional):
 * ─────────────────────────────────────────────────────────────────────────────
 *   pipeline acquire(let src)
 *       video_cap(src)
 *       adaptive_auto_brightness(
 *           src,                        # source path/index
 *           "exposure",                 # exposure prop name (or "")
 *           100, 10000,                 # exposure [min, max]
 *           "analogue_gain",            # analog gain prop name (or "")
 *           100, 1600,                  # analog gain [min, max]
 *           "digital_gain",             # digital gain prop name (or "")
 *           256, 1024,                  # digital gain [min, max]
 *           128,                        # target brightness (0-255)
 *           "median",                   # brightness mode: "median" or "mean"
 *           0.5, 0.5, 0.5, 0.5,        # focus ROI: x, y, w, h (fractions)
 *           0.8, 0.0, 0.05,            # PID: kp, ki, kd
 *           "exposure,analog,digital"  # priority order (comma-separated)
 *       )
 *   end
 *
 * Usage with named args (only override what you need):
 * ─────────────────────────────────────────────────────────────────────────────
 *   adaptive_auto_brightness(src,
 *       target=200,
 *       exposure_prop="exposure",
 *       min_exposure=50, max_exposure=20000,
 *       analog_gain_prop="",
 *       digital_gain_prop="")
 *
 * Returns: array [exposure_value, analog_gain_value, digital_gain_value]
 *   Each value is the current setpoint for that axis (even when the axis is
 *   disabled, the last value is preserved so callers can use it manually).
 *
 * The current Mat is passed through unchanged.
 */
class AdaptiveAutoBrightnessItem : public InterpreterItem {
public:
    AdaptiveAutoBrightnessItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }

private:
    // Per-source PID state
    struct PIDState {
        // Current setpoints (floating-point, full precision)
        double exposure    = 0.0;
        double analogGain  = 0.0;
        double digitalGain = 0.0;

        // PID integrator / previous error
        double integral     = 0.0;
        double prevError    = 0.0;

        // Last values written to hardware (integer-rounded).
        // Used to skip redundant V4L2 ioctl calls.
        int lastExpVal = -1;
        int lastAnaVal = -1;
        int lastDigVal = -1;

        bool initialised = false;
    };

    std::map<std::string, PIDState> _states;

    // Helpers
    static double measureBrightness(const cv::Mat& frame,
                                    const std::string& mode,
                                    double roiX, double roiY,
                                    double roiW, double roiH,
                                    int subsample = 1);

    static double clampToRange(double value, double lo, double hi) {
        return std::max(lo, std::min(hi, value));
    }
};

/**
 * @brief Resets the PID state for adaptive_auto_brightness.
 *
 * Parameters:
 * - source (optional): Source identifier to reset; omit to reset all.
 */
class AdaptiveAutoBrightnessResetItem : public InterpreterItem {
public:
    AdaptiveAutoBrightnessResetItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }
};

} // namespace visionpipe

#endif // VISIONPIPE_ADAPTIVE_BRIGHTNESS_ITEMS_H

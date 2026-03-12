#include "interpreter/items/adaptive_brightness_items.h"
#include <iostream>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

// V4L2 support for direct hardware writes
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
#include "utils/camera_device_manager.h"
#include "utils/v4l2_device_manager.h"
#endif

namespace visionpipe {

// ============================================================================
// Async V4L2 write queue
// ============================================================================
// Runs V4L2 ioctl writes on a dedicated background thread so the pipeline
// thread is never blocked by driver serialisation during frame acquisition.
// Writes are *coalesced*: if multiple PID iterations queue the same
// (source, prop) before the worker drains, only the latest value is sent.
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
namespace {

struct V4L2AsyncWriter {
    struct Request {
        std::string source;
        std::string prop;
        int         value;
    };

    std::mutex              mtx_;
    std::condition_variable cv_;
    std::vector<Request>    pending_;
    std::atomic<bool>       stop_{false};
    std::thread             worker_;

    V4L2AsyncWriter() {
        worker_ = std::thread([this] { run(); });
    }

    ~V4L2AsyncWriter() {
        stop_.store(true, std::memory_order_relaxed);
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    // Called from pipeline thread — never blocks.
    void enqueue(std::string src, std::string prop, int val) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            // Coalesce: overwrite if same (source, prop) already queued.
            for (auto& r : pending_) {
                if (r.source == src && r.prop == prop) {
                    r.value = val;
                    return;
                }
            }
            pending_.push_back({std::move(src), std::move(prop), val});
        }
        cv_.notify_one();
    }

    void run() {
        while (!stop_.load(std::memory_order_relaxed)) {
            std::vector<Request> batch;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] {
                    return stop_.load(std::memory_order_relaxed) || !pending_.empty();
                });
                if (stop_.load(std::memory_order_relaxed)) break;
                // Brief delay: let the camera driver finish any in-progress
                // DQBUF/QBUF cycle before hammering ioctl controls.
                lk.unlock();
                std::this_thread::sleep_for(std::chrono::microseconds(800));
                lk.lock();
                batch.swap(pending_);
            }
            for (auto& r : batch) {
                CameraDeviceManager::forSource(r.source).setV4L2Control(r.source, r.prop, r.value);
            }
        }
    }
};

V4L2AsyncWriter& asyncWriter() {
    static V4L2AsyncWriter w;
    return w;
}

} // anonymous namespace
#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

void registerAdaptiveBrightnessItems(ItemRegistry& registry) {
    registry.add<AdaptiveAutoBrightnessItem>();
    registry.add<AdaptiveAutoBrightnessResetItem>();
}

// ============================================================================
// Helpers
// ============================================================================

static double computeMedian(cv::Mat& gray8) {
    // Build histogram then find median
    int histSize = 256;
    float range[]    = {0, 256};
    const float* ranges[] = {range};
    cv::Mat hist;
    cv::calcHist(&gray8, 1, nullptr, cv::Mat(), hist, 1, &histSize, ranges);

    double total   = gray8.total();
    double cumsum  = 0.0;
    double half    = total / 2.0;

    for (int i = 0; i < histSize; ++i) {
        cumsum += hist.at<float>(i);
        if (cumsum >= half) {
            return static_cast<double>(i);
        }
    }
    return 127.0;
}

// Fast mean using a pixel step — skips (step²-1)/step² of all pixels.
// Much cheaper than full histogram; accurate enough for PID feedback.
static double computeMeanSubsampled(const cv::Mat& gray8, int step) {
    if (step <= 1) {
        return cv::mean(gray8)[0];
    }
    double sum = 0.0;
    long   cnt = 0;
    for (int y = 0; y < gray8.rows; y += step) {
        const uchar* row = gray8.ptr<uchar>(y);
        for (int x = 0; x < gray8.cols; x += step) {
            sum += row[x];
            ++cnt;
        }
    }
    return cnt > 0 ? sum / cnt : 0.0;
}

/*static*/ double AdaptiveAutoBrightnessItem::measureBrightness(
        const cv::Mat& frame,
        const std::string& mode,
        double roiX, double roiY, double roiW, double roiH,
        int subsample)
{
    if (frame.empty()) return 0.0;

    // Extract ROI
    cv::Mat region;
    {
        int fw = frame.cols, fh = frame.rows;
        int x0 = static_cast<int>(std::clamp(roiX, 0.0, 1.0) * fw);
        int y0 = static_cast<int>(std::clamp(roiY, 0.0, 1.0) * fh);
        int rw = static_cast<int>(std::clamp(roiW, 0.0, 1.0) * fw);
        int rh = static_cast<int>(std::clamp(roiH, 0.0, 1.0) * fh);
        if (x0 + rw > fw) rw = fw - x0;
        if (y0 + rh > fh) rh = fh - y0;
        if (rw <= 0 || rh <= 0) {
            region = frame;
        } else {
            region = frame(cv::Rect(x0, y0, rw, rh));
        }
    }

    // Convert to 8-bit grayscale
    cv::Mat gray;
    if (region.channels() == 1) {
        region.copyTo(gray);
    } else {
        cv::cvtColor(region, gray, cv::COLOR_BGR2GRAY);
    }
    if (gray.depth() == CV_16U) {
        double mn, mx;
        cv::minMaxLoc(gray, &mn, &mx);
        gray.convertTo(gray, CV_8U, mx > 0 ? 255.0 / mx : 1.0);
    } else if (gray.depth() == CV_32F || gray.depth() == CV_64F) {
        gray.convertTo(gray, CV_8U, 255.0);
    } else if (gray.depth() != CV_8U) {
        gray.convertTo(gray, CV_8U);
    }

    // When subsampling is requested, always use fast mean.
    if (subsample > 1 || mode == "mean") {
        return computeMeanSubsampled(gray, std::max(1, subsample));
    }
    // Default accurate path: histogram median.
    return computeMedian(gray);
}

// ============================================================================
// AdaptiveAutoBrightnessItem
// ============================================================================

AdaptiveAutoBrightnessItem::AdaptiveAutoBrightnessItem() {
    _functionName = "adaptive_auto_brightness";
    _description =
        "PID-based automatic brightness controller.  Adjusts up to three V4L2\n"
        "control axes (exposure, analogue gain, digital gain) so that the\n"
        "measured frame brightness converges to a configurable target.\n\n"
        "Control is distributed across axes by priority: the first axis absorbs\n"
        "as much correction as possible; overflow spills to the next axis.\n\n"
        "V4L2 properties are written directly when their prop name is non-empty\n"
        "(requires VISIONPIPE_V4L2_NATIVE_ENABLED).  Set any prop name to \"\"\n"
        "to disable that axis; the item still returns the computed setpoints.\n\n"
        "Returns: array [exposure_setpoint, analog_gain_setpoint, digital_gain_setpoint]";
    _category = "camera";

    _params = {
        // --- Source ---
        ParamDef::required("source", BaseType::ANY,
            "Camera source: device path (\"/dev/video0\") or index"),

        // --- Exposure axis ---
        ParamDef::optional("exposure_prop", BaseType::STRING,
            "V4L2 control name for exposure (e.g. \"exposure\"); \"\" = disabled",
            RuntimeValue(std::string("exposure"))),
        ParamDef::optional("min_exposure", BaseType::FLOAT,
            "Minimum exposure setpoint (device units)", RuntimeValue(100.0)),
        ParamDef::optional("max_exposure", BaseType::FLOAT,
            "Maximum exposure setpoint (device units)", RuntimeValue(10000.0)),

        // --- Analogue gain axis ---
        ParamDef::optional("analog_gain_prop", BaseType::STRING,
            "V4L2 control name for analogue gain (e.g. \"analogue_gain\"); \"\" = disabled",
            RuntimeValue(std::string("analogue_gain"))),
        ParamDef::optional("min_analog_gain", BaseType::FLOAT,
            "Minimum analogue gain (device units)", RuntimeValue(100.0)),
        ParamDef::optional("max_analog_gain", BaseType::FLOAT,
            "Maximum analogue gain (device units)", RuntimeValue(1600.0)),

        // --- Digital gain axis ---
        ParamDef::optional("digital_gain_prop", BaseType::STRING,
            "V4L2 control name for digital gain (e.g. \"digital_gain\"); \"\" = disabled",
            RuntimeValue(std::string(""))),
        ParamDef::optional("min_digital_gain", BaseType::FLOAT,
            "Minimum digital gain (device units)", RuntimeValue(256.0)),
        ParamDef::optional("max_digital_gain", BaseType::FLOAT,
            "Maximum digital gain (device units)", RuntimeValue(1024.0)),

        // --- Brightness target / measurement ---
        ParamDef::optional("target", BaseType::FLOAT,
            "Target brightness 0-255 (default 128)", RuntimeValue(128.0)),
        ParamDef::optional("brightness_mode", BaseType::STRING,
            "Brightness measurement: \"median\" (default) or \"mean\"",
            RuntimeValue(std::string("median"))),

        // --- Focus ROI (fractions of frame size) ---
        ParamDef::optional("roi_x", BaseType::FLOAT,
            "ROI left edge as fraction of frame width (0.0)",  RuntimeValue(0.0)),
        ParamDef::optional("roi_y", BaseType::FLOAT,
            "ROI top edge as fraction of frame height (0.0)", RuntimeValue(0.0)),
        ParamDef::optional("roi_w", BaseType::FLOAT,
            "ROI width as fraction of frame width (1.0 = full)", RuntimeValue(1.0)),
        ParamDef::optional("roi_h", BaseType::FLOAT,
            "ROI height as fraction of frame height (1.0 = full)", RuntimeValue(1.0)),

        // --- PID gains ---
        ParamDef::optional("kp", BaseType::FLOAT,
            "Proportional gain (default 0.8)", RuntimeValue(0.8)),
        ParamDef::optional("ki", BaseType::FLOAT,
            "Integral gain (default 0.0)", RuntimeValue(0.0)),
        ParamDef::optional("kd", BaseType::FLOAT,
            "Derivative gain (default 0.05)", RuntimeValue(0.05)),

        // --- Priority order ---
        ParamDef::optional("priority", BaseType::STRING,
            "Comma-separated axis priority order.\n"
            "  Tokens: \"exposure\", \"analog\" (or \"analogue\"), \"digital\"\n"
            "  Default: \"exposure,analog,digital\"",
            RuntimeValue(std::string("exposure,analog,digital"))),

        // --- Convergence deadband ---
        ParamDef::optional("deadband", BaseType::FLOAT,
            "Skip PID update and V4L2 writes when |error| < deadband (brightness units).\n"
            "  Eliminates micro-adjustments at the cost of small steady-state offset.\n"
            "  Default: 4.0",
            RuntimeValue(4.0)),

        // --- Brightness measurement subsampling ---
        ParamDef::optional("subsample", BaseType::FLOAT,
            "Pixel step for brightness sampling (1 = every pixel, 4 = every 4th pixel).\n"
            "  Values > 1 switch to fast subsampled mean regardless of brightness_mode.\n"
            "  Default: 4 (significant speedup on large frames with negligible accuracy loss)",
            RuntimeValue(4.0)),
    };

    _example = R"vsp(
# Full example with named args – only override what you need:
pipeline measure_brightness
    video_cap("/dev/video0")
    adaptive_auto_brightness("/dev/video0",
        exposure_prop="exposure",
        min_exposure=100, max_exposure=20000,
        analog_gain_prop="analogue_gain",
        min_analog_gain=100, max_analog_gain=1600,
        digital_gain_prop="",
        target=160,
        brightness_mode="median",
        kp=0.9, ki=0.01)
end
exec_loop measure_brightness
)vsp";
    _returnType = "array[float, float, float]";
    _tags = {"auto", "brightness", "exposure", "gain", "pid", "camera", "v4l2", "control"};
}

ExecutionResult AdaptiveAutoBrightnessItem::execute(
        const std::vector<RuntimeValue>& args,
        ExecutionContext& ctx)
{
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("adaptive_auto_brightness: no input frame");
    }

    // ----------------------------------------------------------------
    // Parse parameters
    // ----------------------------------------------------------------
    auto argOr = [&](size_t idx, const RuntimeValue& def) -> RuntimeValue {
        return (idx < args.size() && !args[idx].isVoid()) ? args[idx] : def;
    };

    // Source identifier
    std::string sourceId;
    if (!args.empty()) {
        const auto& a = args[0];
        if (a.isNumeric()) {
            sourceId = "/dev/video" + std::to_string(static_cast<int>(a.asNumber()));
        } else {
            sourceId = a.asString();
        }
    }
    if (sourceId.empty()) sourceId = ctx.lastSourceId.empty() ? "__default__" : ctx.lastSourceId;

    std::string exposureProp     = argOr(1,  RuntimeValue(std::string("exposure"))).asString();
    double      minExposure      = argOr(2,  RuntimeValue(100.0)).asNumber();
    double      maxExposure      = argOr(3,  RuntimeValue(10000.0)).asNumber();

    std::string analogGainProp   = argOr(4,  RuntimeValue(std::string("analogue_gain"))).asString();
    double      minAnalogGain    = argOr(5,  RuntimeValue(100.0)).asNumber();
    double      maxAnalogGain    = argOr(6,  RuntimeValue(1600.0)).asNumber();

    std::string digitalGainProp  = argOr(7,  RuntimeValue(std::string(""))).asString();
    double      minDigitalGain   = argOr(8,  RuntimeValue(256.0)).asNumber();
    double      maxDigitalGain   = argOr(9,  RuntimeValue(1024.0)).asNumber();

    double target                = argOr(10, RuntimeValue(128.0)).asNumber();
    std::string brightnessMode   = argOr(11, RuntimeValue(std::string("median"))).asString();

    double roiX                  = argOr(12, RuntimeValue(0.0)).asNumber();
    double roiY                  = argOr(13, RuntimeValue(0.0)).asNumber();
    double roiW                  = argOr(14, RuntimeValue(1.0)).asNumber();
    double roiH                  = argOr(15, RuntimeValue(1.0)).asNumber();

    double kp                    = argOr(16, RuntimeValue(0.8)).asNumber();
    double ki                    = argOr(17, RuntimeValue(0.0)).asNumber();
    double kd                    = argOr(18, RuntimeValue(0.05)).asNumber();

    std::string priorityStr      = argOr(19, RuntimeValue(std::string("exposure,analog,digital"))).asString();
    double      deadband         = std::max(0.0, argOr(20, RuntimeValue(4.0)).asNumber());
    int         subsample        = std::max(1, static_cast<int>(argOr(21, RuntimeValue(4.0)).asNumber()));

    // Sanity clamps
    target         = std::clamp(target, 1.0, 254.0);
    if (minExposure   > maxExposure)    std::swap(minExposure,   maxExposure);
    if (minAnalogGain > maxAnalogGain)  std::swap(minAnalogGain, maxAnalogGain);
    if (minDigitalGain> maxDigitalGain) std::swap(minDigitalGain,maxDigitalGain);

    // ----------------------------------------------------------------
    // Parse priority order
    // ----------------------------------------------------------------
    // Each entry: 0=exposure, 1=analog, 2=digital
    std::vector<int> priority;
    {
        std::istringstream ss(priorityStr);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            // Trim whitespace
            tok.erase(0, tok.find_first_not_of(" \t"));
            tok.erase(tok.find_last_not_of(" \t") + 1);
            if (tok == "exposure")                                priority.push_back(0);
            else if (tok == "analog" || tok == "analogue")        priority.push_back(1);
            else if (tok == "digital")                            priority.push_back(2);
        }
        if (priority.empty()) { priority = {0, 1, 2}; }
    }

    // ----------------------------------------------------------------
    // Retrieve or initialise PID state
    // ----------------------------------------------------------------
    PIDState& state = _states[sourceId];

    if (!state.initialised) {
        state.exposure    = (minExposure    + maxExposure)    / 2.0;
        state.analogGain  = minAnalogGain;   // start at minimum gain for cleanest image
        state.digitalGain = minDigitalGain;
        state.integral    = 0.0;
        state.prevError   = 0.0;
        state.initialised = true;
    }

    // ----------------------------------------------------------------
    // Measure current brightness  (subsampled fast path when step > 1)
    // ----------------------------------------------------------------
    double brightness = measureBrightness(ctx.currentMat, brightnessMode,
                                          roiX, roiY, roiW, roiH, subsample);

    // ----------------------------------------------------------------
    // Deadband precheck — skip everything if we're close enough
    // ----------------------------------------------------------------
    double error = target - brightness;   // positive = too dark

    if (std::abs(error) < deadband) {
        // Already within tolerance. Return last known setpoints unchanged.
        if (ctx.verbose || ctx.debugDump) {
            std::cout << "[adaptive_auto_brightness] source=" << sourceId
                      << "  brightness=" << brightness << "  target=" << target
                      << "  error=" << error << "  (deadband skip)\n";
        }
        std::vector<RuntimeValue> result;
        result.emplace_back(static_cast<double>(state.lastExpVal >= 0 ? state.lastExpVal
                                                                      : static_cast<int>(std::round(state.exposure))));
        result.emplace_back(static_cast<double>(state.lastAnaVal >= 0 ? state.lastAnaVal
                                                                      : static_cast<int>(std::round(state.analogGain))));
        result.emplace_back(static_cast<double>(state.lastDigVal >= 0 ? state.lastDigVal
                                                                      : static_cast<int>(std::round(state.digitalGain))));
        return ExecutionResult::okWithMat(ctx.currentMat, RuntimeValue(std::move(result)));
    }

    // ----------------------------------------------------------------
    // PID calculation
    // ----------------------------------------------------------------
    state.integral  += error;
    // Anti-windup: clamp integral to ±255
    state.integral   = std::clamp(state.integral, -255.0, 255.0);
    double derivative = error - state.prevError;
    state.prevError   = error;

    // PID output in brightness units (−255 … +255)
    double pidOutput = kp * error + ki * state.integral + kd * derivative;

    // Convert to a normalised correction [−1, +1] for axis distribution
    double nc = pidOutput / 255.0;

    // ----------------------------------------------------------------
    // Distribute correction across axes in priority order with overflow
    // ----------------------------------------------------------------
    struct AxisInfo {
        double* current;
        double  minVal;
        double  maxVal;
        bool    active;   // prop name non-empty
    };

    AxisInfo axes[3] = {
        { &state.exposure,    minExposure,    maxExposure,    !exposureProp.empty()    },
        { &state.analogGain,  minAnalogGain,  maxAnalogGain,  !analogGainProp.empty() },
        { &state.digitalGain, minDigitalGain, maxDigitalGain, !digitalGainProp.empty()},
    };

    double remaining = nc;  // correction still to distribute

    for (int axIdx : priority) {
        if (std::abs(remaining) < 1e-6) break;
        AxisInfo& ax = axes[axIdx];
        if (!ax.active) continue;

        double range    = ax.maxVal - ax.minVal;
        if (range <= 0.0) continue;

        // Current normalised position [0, 1]
        double normCur  = (*ax.current - ax.minVal) / range;
        // Apply correction in normalised space
        double normNew  = std::clamp(normCur + remaining, 0.0, 1.0);
        double applied  = normNew - normCur;   // actual correction applied (after clamp)
        remaining      -= applied;             // overflow to next axis

        *ax.current = ax.minVal + normNew * range;
    }

    // Round to integer (camera controls only accept integers)
    int expVal     = static_cast<int>(std::round(state.exposure));
    int anaVal     = static_cast<int>(std::round(state.analogGain));
    int digVal     = static_cast<int>(std::round(state.digitalGain));

    // ----------------------------------------------------------------
    // Apply to V4L2 hardware — only when value changed; enqueued async
    // so the pipeline thread never stalls on ioctl serialisation.
    // ----------------------------------------------------------------
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
    // NOTE: do NOT call setVerbose(ctx.verbose) here — ctx.verbose is scoped
    // to this interpreter thread and must not mutate the process-wide singleton.
    if (!exposureProp.empty()    && expVal != state.lastExpVal) {
        asyncWriter().enqueue(sourceId, exposureProp,    expVal);
    }
    if (!analogGainProp.empty()  && anaVal != state.lastAnaVal) {
        asyncWriter().enqueue(sourceId, analogGainProp,  anaVal);
    }
    if (!digitalGainProp.empty() && digVal != state.lastDigVal) {
        asyncWriter().enqueue(sourceId, digitalGainProp, digVal);
    }
#else
    // When V4L2 not compiled in, warn once per source if prop names were given
    static std::set<std::string> _warnedSources;
    bool anyProp = !exposureProp.empty() || !analogGainProp.empty() || !digitalGainProp.empty();
    if (anyProp && _warnedSources.find(sourceId) == _warnedSources.end()) {
        std::cerr << "[adaptive_auto_brightness] WARNING: V4L2 not enabled; "
                     "hardware writes skipped for source '" << sourceId << "'.\n";
        _warnedSources.insert(sourceId);
    }
#endif

    // Track last written values to guard against redundant ioctls
    state.lastExpVal = expVal;
    state.lastAnaVal = anaVal;
    state.lastDigVal = digVal;

    if (ctx.verbose || ctx.debugDump) {
        std::cout << "[adaptive_auto_brightness] source=" << sourceId
                  << "  brightness=" << brightness << "  target=" << target
                  << "  error=" << error << "  pidOut=" << pidOutput
                  << "  exposure=" << expVal
                  << "  aGain=" << anaVal
                  << "  dGain=" << digVal
                  << std::endl;
    }

    // ----------------------------------------------------------------
    // Return [exposure, analog_gain, digital_gain] as RuntimeValue array
    // ----------------------------------------------------------------
    std::vector<RuntimeValue> result;
    result.emplace_back(static_cast<double>(expVal));
    result.emplace_back(static_cast<double>(anaVal));
    result.emplace_back(static_cast<double>(digVal));

    return ExecutionResult::okWithMat(ctx.currentMat, RuntimeValue(std::move(result)));
}

// ============================================================================
// AdaptiveAutoBrightnessResetItem
// ============================================================================

AdaptiveAutoBrightnessResetItem::AdaptiveAutoBrightnessResetItem() {
    _functionName = "adaptive_auto_brightness_reset";
    _description  = "Resets the PID state of adaptive_auto_brightness.\n"
                    "Call this when the camera source changes or a manual control adjustment "
                    "has been made externally.";
    _category = "camera";
    _params = {
        ParamDef::optional("source", BaseType::STRING,
            "Source identifier to reset; omit to reset all sources", "")
    };
    _example   = "adaptive_auto_brightness_reset()\n"
                 "adaptive_auto_brightness_reset(\"/dev/video0\")";
    _returnType = "void";
    _tags = {"auto", "brightness", "pid", "reset", "camera"};
}

ExecutionResult AdaptiveAutoBrightnessResetItem::execute(
        const std::vector<RuntimeValue>& args,
        ExecutionContext& ctx)
{
    auto& registry = ItemRegistry::instance();
    auto item = std::dynamic_pointer_cast<AdaptiveAutoBrightnessItem>(
        registry.getItem("adaptive_auto_brightness"));

    if (!item) {
        return ExecutionResult::fail(
            "adaptive_auto_brightness_reset: adaptive_auto_brightness item not registered");
    }

    // Re-register a fresh instance to wipe all state
    registry.registerItem(std::make_shared<AdaptiveAutoBrightnessItem>());

    std::string src = args.size() > 0 ? args[0].asString() : "";
    if (ctx.verbose) {
        std::cout << "[adaptive_auto_brightness] reset"
                  << (src.empty() ? " (all sources)" : (" source=" + src)) << "\n";
    }

    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

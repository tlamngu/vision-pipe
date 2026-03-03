#ifndef VISIONPIPE_V4L2_ITEMS_H
#define VISIONPIPE_V4L2_ITEMS_H

#include "interpreter/item_registry.h"

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED

#include <climits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace visionpipe {

void registerV4L2Items(ItemRegistry& registry);

// ============================================================================
// V4L2 Native Items
// ============================================================================

/**
 * @brief v4l2_setup: Configure and open a V4L2 native device
 *
 * Parameters:
 * - source: Device path (e.g. "/dev/video0") or index
 * - width: Frame width (default: 640)
 * - height: Frame height (default: 480)
 * - pixel_format: Pixel format string (default: "YUYV")
 * - fps: Framerate (default: 30)
 * - buffer_count: Number of mmap buffers (default: 4)
 */
class V4L2SetupItem : public InterpreterItem {
public:
    V4L2SetupItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief v4l2_prop: Set a V4L2 control by name
 *
 * Parameters:
 * - source: Device path
 * - control_name: Control name string (e.g. "Brightness")
 * - value: Control value (float, cast to int)
 */
class V4L2PropItem : public InterpreterItem {
public:
    V4L2PropItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief v4l2_get_prop: Get current value of a V4L2 control
 *
 * Parameters:
 * - source: Device path
 * - control_name: Control name string
 * Returns: INT
 */
class V4L2GetPropItem : public InterpreterItem {
public:
    V4L2GetPropItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief v4l2_list_controls: Print all controls with current values and ranges
 *
 * Parameters:
 * - source: Device path
 */
class V4L2ListControlsItem : public InterpreterItem {
public:
    V4L2ListControlsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief v4l2_list_formats: Print all supported formats and resolutions
 *
 * Parameters:
 * - source: Device path
 */
class V4L2ListFormatsItem : public InterpreterItem {
public:
    V4L2ListFormatsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief v4l2_get_bayer: Return Bayer pattern string from active format
 *
 * Parameters:
 * - source: Device path
 * Returns: STRING ("RGGB", "BGGR", "GBRG", "GRBG", or "")
 */
class V4L2GetBayerItem : public InterpreterItem {
public:
    V4L2GetBayerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief v4l2_enum_devices: Enumerate /dev/video* and their capabilities
 */
class V4L2EnumDevicesItem : public InterpreterItem {
public:
    V4L2EnumDevicesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief video_sync: Copy all V4L2 controls (and OpenCV capture properties)
 *        from a master device to a target device so both cameras run with
 *        identical settings.
 *
 * Parameters:
 * - master : master device path (e.g. "/dev/video3") or index
 * - target : device to synchronise (e.g. "/dev/video9") or index
 * - verbose : (optional bool, default false) log every copied control
 *
 * Returns: number of controls successfully synced (INT)
 *
 * Example:
 *   video_sync("/dev/video3", "/dev/video9")  # sync 9 → 3
 */
class VideoSyncItem : public InterpreterItem {
public:
    VideoSyncItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    bool modifiesMat() const override { return false; }

private:
    // -----------------------------------------------------------------------
    // Per (master, target) sync state – persists across calls so we only
    // enumerate + test-write once (discovery), then only push deltas.
    // -----------------------------------------------------------------------
    struct ControlEntry {
        std::string name;                   // normalized (lowercase, '_' instead of ' ')
        int32_t     lastSynced = INT_MIN;   // last value written to target (INT_MIN = never)
        bool        writable   = false;     // false if target rejected on discovery
    };
    struct SyncState {
        bool                      discovered = false;
        std::vector<ControlEntry> controls;
    };
    std::mutex                                 _stateMutex;
    std::unordered_map<std::string, SyncState> _syncStates; // key = master+"@"+target
};

} // namespace visionpipe

#else // !VISIONPIPE_V4L2_NATIVE_ENABLED

namespace visionpipe {

// Empty stub when V4L2 native is disabled
inline void registerV4L2Items(ItemRegistry&) {}

} // namespace visionpipe

#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

#endif // VISIONPIPE_V4L2_ITEMS_H

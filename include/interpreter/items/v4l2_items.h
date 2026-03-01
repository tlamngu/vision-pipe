#ifndef VISIONPIPE_V4L2_ITEMS_H
#define VISIONPIPE_V4L2_ITEMS_H

#include "interpreter/item_registry.h"

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED

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

} // namespace visionpipe

#else // !VISIONPIPE_V4L2_NATIVE_ENABLED

namespace visionpipe {

// Empty stub when V4L2 native is disabled
inline void registerV4L2Items(ItemRegistry&) {}

} // namespace visionpipe

#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

#endif // VISIONPIPE_V4L2_ITEMS_H

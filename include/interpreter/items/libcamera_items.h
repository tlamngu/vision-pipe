#ifndef VISIONPIPE_LIBCAMERA_ITEMS_H
#define VISIONPIPE_LIBCAMERA_ITEMS_H

#include "interpreter/item_registry.h"

#ifdef VISIONPIPE_LIBCAMERA_ENABLED

namespace visionpipe {

void registerLibCameraItems(ItemRegistry& registry);

// ============================================================================
// LibCamera Items
// ============================================================================

/**
 * @brief libcam_setup: Configure libcamera-specific settings
 * 
 * Parameters:
 * - source: Camera source identifier
 * - width: (optional) Frame width
 * - height: (optional) Frame height
 * - pixel_format: (optional) Pixel format (BGR888, YUYV, MJPEG, etc.)
 * - buffer_count: (optional) Number of buffers (default: 4)
 * - stream_role: (optional) Stream role (VideoRecording, StillCapture, Viewfinder)
 */
class LibCamSetupItem : public InterpreterItem {
public:
    LibCamSetupItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief libcam_prop: Set libcamera control by name or ID
 * 
 * Parameters:
 * - source: Camera source identifier
 * - control: Control name (e.g., "AeEnable", "ExposureTime", "AwbMode")
 * - value: Control value
 */
class LibCamListControlsItem : public InterpreterItem {
public:
    LibCamListControlsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class LibCamPropItem : public InterpreterItem {
public:
    LibCamPropItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief libcam_list: List available libcamera devices
 * 
 * Returns information about all detected libcamera devices.
 */
class LibCamListItem : public InterpreterItem {
public:
    LibCamListItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class LibCamGetBayerItem : public InterpreterItem {
public:
    LibCamGetBayerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class LibCamListFormatsItem : public InterpreterItem {
public:
    LibCamListFormatsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class LibCamDebugConfigItem : public InterpreterItem {
public:
    LibCamDebugConfigItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};




} // namespace visionpipe

#endif // VISIONPIPE_LIBCAMERA_ENABLED

#endif // VISIONPIPE_LIBCAMERA_ITEMS_H

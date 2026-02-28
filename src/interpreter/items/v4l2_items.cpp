#include "interpreter/items/v4l2_items.h"

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED

#include "utils/camera_device_manager.h"
#include "utils/v4l2_device_manager.h"
#include "utils/Logger.h"
#include <iostream>

#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace visionpipe {

void registerV4L2Items(ItemRegistry& registry) {
    registry.add<V4L2SetupItem>();
    registry.add<V4L2PropItem>();
    registry.add<V4L2GetPropItem>();
    registry.add<V4L2ListControlsItem>();
    registry.add<V4L2ListFormatsItem>();
    registry.add<V4L2GetBayerItem>();
    registry.add<V4L2EnumDevicesItem>();
}

// Helper: resolve sourceId from ANY arg
static std::string resolveSource(const RuntimeValue& arg) {
    if (arg.isNumeric()) {
        return "/dev/video" + std::to_string(static_cast<int>(arg.asNumber()));
    }
    return arg.asString();
}

// ============================================================================
// V4L2SetupItem
// ============================================================================

V4L2SetupItem::V4L2SetupItem() {
    _functionName = "v4l2_setup";
    _description = "Configure and open a V4L2 native device";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path (e.g. \"/dev/video0\") or index"),
        ParamDef::optional("width", BaseType::INT, "Frame width", 640),
        ParamDef::optional("height", BaseType::INT, "Frame height", 480),
        ParamDef::optional("pixel_format", BaseType::STRING, "Pixel format: YUYV, MJPG, NV12, SRGGB10, etc.", "YUYV"),
        ParamDef::optional("fps", BaseType::INT, "Framerate", 30),
        ParamDef::optional("buffer_count", BaseType::INT, "Number of mmap buffers", 4)
    };
    _example = "v4l2_setup(\"/dev/video0\", 1920, 1080, \"SRGGB10\", 30)";
    _returnType = "mat";
    _tags = {"v4l2", "camera", "setup", "configuration"};
}

ExecutionResult V4L2SetupItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    V4L2DeviceManager::instance().setVerbose(ctx.verbose);
    std::string sourceId = resolveSource(args[0]);

    int width = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 640;
    int height = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 480;
    std::string pixelFormat = args.size() > 3 ? args[3].asString() : "YUYV";
    int fps = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 30;
    int bufferCount = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 4;

    if (ctx.verbose) {
        std::cout << "[DEBUG] V4L2Items: v4l2_setup source=" << sourceId
                  << " " << width << "x" << height
                  << " fmt=" << pixelFormat
                  << " fps=" << fps
                  << " buffers=" << bufferCount << std::endl;
    }

    V4L2NativeConfig config;
    config.width = width;
    config.height = height;
    config.pixelFormat = pixelFormat;
    config.fps = fps;
    config.bufferCount = bufferCount;

    // Open and configure through CameraDeviceManager delegation
    if (!CameraDeviceManager::instance().setV4L2NativeConfig(sourceId, config)) {
        return ExecutionResult::fail("v4l2_setup: failed to open/configure " + sourceId +
                                    " (" + std::to_string(width) + "x" + std::to_string(height) +
                                    " " + pixelFormat + " @" + std::to_string(fps) + "fps)");
    }

    SystemLogger::info("V4L2Items", "v4l2_setup: Configured " + sourceId + " " +
                       std::to_string(width) + "x" + std::to_string(height) +
                       " " + pixelFormat + " @" + std::to_string(fps) + "fps");

    return ExecutionResult::ok();
}

// ============================================================================
// V4L2PropItem
// ============================================================================

V4L2PropItem::V4L2PropItem() {
    _functionName = "v4l2_prop";
    _description = "Set a V4L2 control by name";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path or index"),
        ParamDef::required("control_name", BaseType::STRING, "Control name (e.g. \"Brightness\", \"ExposureTime\")"),
        ParamDef::required("value", BaseType::FLOAT, "Control value")
    };
    _example = "v4l2_prop(\"/dev/video0\", \"ExposureTime\", 5000)";
    _returnType = "mat";
    _tags = {"v4l2", "camera", "control", "property"};
}

ExecutionResult V4L2PropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    V4L2DeviceManager::instance().setVerbose(ctx.verbose);
    std::string sourceId = resolveSource(args[0]);
    std::string controlName = args[1].asString();
    int value = static_cast<int>(args[2].asNumber());

    if (ctx.verbose) {
        std::cout << "[DEBUG] V4L2Items: v4l2_prop source=" << sourceId
                  << " control='" << controlName << "' value=" << value << std::endl;
    }

    if (!CameraDeviceManager::instance().setV4L2Control(sourceId, controlName, value)) {
        return ExecutionResult::fail("Failed to set V4L2 control " + controlName + " on " + sourceId);
    }

    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// V4L2GetPropItem
// ============================================================================

V4L2GetPropItem::V4L2GetPropItem() {
    _functionName = "v4l2_get_prop";
    _description = "Get current value of a V4L2 control";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path or index"),
        ParamDef::required("control_name", BaseType::STRING, "Control name")
    };
    _example = "v4l2_get_prop(\"/dev/video0\", \"Brightness\")";
    _returnType = "int";
    _tags = {"v4l2", "camera", "control", "get"};
}

ExecutionResult V4L2GetPropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    V4L2DeviceManager::instance().setVerbose(ctx.verbose);
    std::string sourceId = resolveSource(args[0]);
    std::string controlName = args[1].asString();

    if (ctx.verbose) {
        std::cout << "[DEBUG] V4L2Items: v4l2_get_prop source=" << sourceId
                  << " control='" << controlName << "'" << std::endl;
    }

    int value = V4L2DeviceManager::instance().getControl(sourceId, controlName);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_get_prop result=" << value << std::endl;
    return ExecutionResult::ok(static_cast<double>(value));
}

// ============================================================================
// V4L2ListControlsItem
// ============================================================================

V4L2ListControlsItem::V4L2ListControlsItem() {
    _functionName = "v4l2_list_controls";
    _description = "List all V4L2 controls with current values and ranges";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path or index")
    };
    _example = "v4l2_list_controls(\"/dev/video0\")";
    _returnType = "mat";
    _tags = {"v4l2", "camera", "controls", "list"};
}

ExecutionResult V4L2ListControlsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    V4L2DeviceManager::instance().setVerbose(ctx.verbose);
    std::string sourceId = resolveSource(args[0]);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_list_controls source=" << sourceId << std::endl;
    CameraDeviceManager::instance().listV4L2Controls(sourceId);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// V4L2ListFormatsItem
// ============================================================================

V4L2ListFormatsItem::V4L2ListFormatsItem() {
    _functionName = "v4l2_list_formats";
    _description = "List all supported V4L2 formats and resolutions";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path or index")
    };
    _example = "v4l2_list_formats(\"/dev/video0\")";
    _returnType = "mat";
    _tags = {"v4l2", "formats", "resolutions", "enumerate"};
}

ExecutionResult V4L2ListFormatsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    V4L2DeviceManager::instance().setVerbose(ctx.verbose);
    std::string sourceId = resolveSource(args[0]);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_list_formats source=" << sourceId << std::endl;
    CameraDeviceManager::instance().listV4L2Formats(sourceId);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// V4L2GetBayerItem
// ============================================================================

V4L2GetBayerItem::V4L2GetBayerItem() {
    _functionName = "v4l2_get_bayer";
    _description = "Get the Bayer pattern string from the active V4L2 format";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path or index")
    };
    _example = "v4l2_get_bayer(\"/dev/video0\")";
    _returnType = "string";
    _tags = {"v4l2", "bayer", "format"};
}

ExecutionResult V4L2GetBayerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    V4L2DeviceManager::instance().setVerbose(ctx.verbose);
    std::string sourceId = resolveSource(args[0]);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_get_bayer source=" << sourceId << std::endl;
    std::string pattern = CameraDeviceManager::instance().getV4L2BayerPattern(sourceId);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_get_bayer result='" << pattern << "'" << std::endl;
    return ExecutionResult::ok(pattern);
}

// ============================================================================
// V4L2EnumDevicesItem
// ============================================================================

V4L2EnumDevicesItem::V4L2EnumDevicesItem() {
    _functionName = "v4l2_enum_devices";
    _description = "Enumerate /dev/video* devices and their capabilities";
    _category = "video_io";
    _params = {};
    _example = "v4l2_enum_devices()";
    _returnType = "mat";
    _tags = {"v4l2", "enumerate", "devices"};
}

ExecutionResult V4L2EnumDevicesItem::execute(const std::vector<RuntimeValue>&, ExecutionContext& ctx) {
    V4L2DeviceManager::instance().setVerbose(ctx.verbose);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_enum_devices scanning /dev/video0../dev/video31" << std::endl;
    std::cout << "\n=== V4L2 Device Enumeration ===" << std::endl;

    int found = 0;
    for (int i = 0; i < 32; ++i) {
        std::string path = "/dev/video" + std::to_string(i);
        int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK);
        if (fd < 0) continue;

        struct v4l2_capability cap{};
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            std::cout << "  [" << i << "] " << path << std::endl;
            std::cout << "       Card:   " << cap.card << std::endl;
            std::cout << "       Driver: " << cap.driver << std::endl;
            std::cout << "       Bus:    " << cap.bus_info << std::endl;
            std::cout << "       Caps:   ";
            if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) std::cout << "CAPTURE ";
            if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) std::cout << "OUTPUT ";
            if (cap.capabilities & V4L2_CAP_STREAMING) std::cout << "STREAMING ";
            if (cap.capabilities & V4L2_CAP_READWRITE) std::cout << "READWRITE ";
            std::cout << std::endl;
            found++;
        }
        ::close(fd);
    }

    if (found == 0) {
        std::cout << "  No V4L2 devices found." << std::endl;
    }
    std::cout << "================================\n" << std::endl;

    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

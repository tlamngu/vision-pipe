#include "interpreter/items/libcamera_items.h"

#ifdef VISIONPIPE_LIBCAMERA_ENABLED

#include "utils/camera_device_manager.h"
#include "utils/Logger.h"
#include <iostream>

namespace visionpipe {

void registerLibCameraItems(ItemRegistry& registry) {
    registry.add<LibCamSetupItem>();
    registry.add<LibCamPropItem>();
    registry.add<LibCamListItem>();
}

// ============================================================================
// LibCamSetupItem
// ============================================================================

LibCamSetupItem::LibCamSetupItem() {
    _functionName = "libcam-setup";
    _description = "Configure libcamera-specific settings for a camera";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Camera source (index or ID string)"),
        ParamDef::optional("width", BaseType::INT, "Frame width", 640),
        ParamDef::optional("height", BaseType::INT, "Frame height", 480),
        ParamDef::optional("pixel_format", BaseType::STRING, "Pixel format: BGR888, YUYV, MJPEG", "BGR888"),
        ParamDef::optional("buffer_count", BaseType::INT, "Number of buffers", 4),
        ParamDef::optional("stream_role", BaseType::STRING, "Stream role: VideoRecording, StillCapture, Viewfinder", "VideoRecording")
    };
    _example = "libcam-setup(0, 1920, 1080)";
    _returnType = "mat";
    _tags = {"libcamera", "camera", "setup", "configuration"};
}

ExecutionResult LibCamSetupItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext&) {
    std::string sourceId;
    
    if (args[0].isNumeric()) {
        sourceId = std::to_string(static_cast<int>(args[0].asNumber()));
    } else {
        sourceId = args[0].asString();
    }
    
    // Extract configuration parameters
    int width = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 640;
    int height = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 480;
    std::string pixelFormat = args.size() > 3 ? args[3].asString() : "BGR888";
    int bufferCount = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 4;
    std::string streamRole = args.size() > 5 ? args[5].asString() : "VideoRecording";
    
    // Configure camera before acquiring
    LibCameraConfig config;
    config.width = width;
    config.height = height;
    config.pixelFormat = pixelFormat;
    config.bufferCount = bufferCount;
    config.streamRole = streamRole;
    
    CameraDeviceManager::instance().setLibCameraConfig(sourceId, config);
    
    cv::Mat frame;
    if (!CameraDeviceManager::instance().acquireFrame(sourceId, CameraBackend::LIBCAMERA, frame)) {
        return ExecutionResult::fail("Failed to setup libcamera device: " + sourceId);
    }
    
    SystemLogger::info("LibCameraItems", "libcam-setup: Configured camera " + sourceId + " with resolution " + 
                 std::to_string(width) + "x" + std::to_string(height));
    
    return ExecutionResult::ok(frame);
}

// ============================================================================
// LibCamPropItem
// ============================================================================

LibCamPropItem::LibCamPropItem() {
    _functionName = "libcam-prop";
    _description = "Set a libcamera control by name";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::required("control", BaseType::STRING, "Control name (AeEnable, ExposureTime, AwbMode, Brightness, Contrast, Saturation, Sharpness)"),
        ParamDef::required("value", BaseType::ANY, "Control value (number or boolean)")
    };
    _example = "libcam-prop(\"0\", \"AeEnable\", 0)";
    _returnType = "mat";
    _tags = {"libcamera", "camera", "control", "property"};
}

ExecutionResult LibCamPropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string controlName = args[1].asString();
    
    float value = 0.0f;
    if (args[2].isNumeric()) {
        value = static_cast<float>(args[2].asNumber());
    } else if (args[2].isBool()) {
        value = args[2].asBool() ? 1.0f : 0.0f;
    } else {
        // Try parsing string if possible, or support string controls later
        // For now, default to 0
        SystemLogger::warning("LibCameraItems", "libcam-prop: Unsupported value type for control " + controlName);
    }
    
    if (!CameraDeviceManager::instance().setLibCameraControl(sourceId, controlName, value)) {
        return ExecutionResult::fail("Failed to set control " + controlName + " on camera " + sourceId + 
                                     " (camera might not be open or control not supported)");
    }
    
    SystemLogger::info("LibCameraItems", "libcam-prop: Set " + controlName + " to " + std::to_string(value));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LibCamListItem
// ============================================================================

LibCamListItem::LibCamListItem() {
    _functionName = "libcam-list";
    _description = "List available libcamera devices";
    _category = "video_io";
    _params = {};
    _example = "libcam-list()";
    _returnType = "mat";
    _tags = {"libcamera", "camera", "list", "enumerate"};
}

ExecutionResult LibCamListItem::execute(const std::vector<RuntimeValue>&, ExecutionContext& ctx) {
    auto* manager = CameraDeviceManager::instance().getLibCameraManager();
    if (!manager) {
        return ExecutionResult::fail("Failed to initialize libcamera manager");
    }
    
    auto cameras = manager->cameras();
    
    std::cout << "\n=== Available libcamera Devices ===" << std::endl;
    std::cout << "Found " << cameras.size() << " camera(s):" << std::endl;
    
    for (size_t i = 0; i < cameras.size(); ++i) {
        const auto& camera = cameras[i];
        std::cout << "  [" << i << "] " << camera->id() << std::endl;
        
        // Print camera properties if available
        // const auto& props = camera->properties();
        // Note: Accessing properties requires the camera to be acquired
        // For listing, we just show the ID
    }
    
    std::cout << "===================================\n" << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

#endif // VISIONPIPE_LIBCAMERA_ENABLED

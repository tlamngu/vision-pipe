#include "interpreter/items/libcamera_items.h"

#ifdef VISIONPIPE_LIBCAMERA_ENABLED

#include "utils/camera_device_manager.h"
#include "utils/Logger.h"
#include <libcamera/property_ids.h>
#include <iostream>

namespace visionpipe {

void registerLibCameraItems(ItemRegistry& registry) {
    registry.add<LibCamSetupItem>();
    registry.add<LibCamPropItem>();
    registry.add<LibCamListItem>();
    registry.add<LibCamListControlsItem>();
}

// ============================================================================
// LibCamSetupItem
// ============================================================================

LibCamSetupItem::LibCamSetupItem() {
    _functionName = "libcam_setup";
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
    _example = "libcam_setup(0, 1920, 1080)";
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
    LibCameraConfig lcConfig;
    lcConfig.width = width;
    lcConfig.height = height;
    lcConfig.pixelFormat = pixelFormat;
    lcConfig.bufferCount = bufferCount;
    lcConfig.streamRole = streamRole;
    // Set configuration
    CameraDeviceManager::instance().setLibCameraConfig(sourceId, lcConfig);
    
    SystemLogger::info("LibCameraItems", "libcam_setup: Configured camera " + sourceId + " with resolution " + 
                 std::to_string(width) + "x" + std::to_string(height));
    
    return ExecutionResult::ok();
}

// ============================================================================
// LibCamPropItem
// ============================================================================

LibCamPropItem::LibCamPropItem() {
    _functionName = "libcam_prop";
    _description = "Set a libcamera control by name";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Camera source identifier (index or ID)"),
        ParamDef::required("control", BaseType::STRING, "Control name (AeEnable, ExposureTime, AwbMode, Brightness, Contrast, Saturation, Sharpness)"),
        ParamDef::required("value", BaseType::ANY, "Control value (number or boolean)")
    };
    _example = "libcam_prop(0, \"AeEnable\", 0)";
    _returnType = "mat";
    _tags = {"libcamera", "camera", "control", "property"};
}

ExecutionResult LibCamPropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId;
    if (args[0].isNumeric()) {
        sourceId = std::to_string(static_cast<int>(args[0].asNumber()));
    } else {
        sourceId = args[0].asString();
    }
    
    std::string controlName = args[1].asString();
    
    float value = 0.0f;
    if (args[2].isNumeric()) {
        value = static_cast<float>(args[2].asNumber());
    } else if (args[2].isBool()) {
        value = args[2].asBool() ? 1.0f : 0.0f;
    } else {
        // Try parsing string if possible, or support string controls later
        // For now, default to 0
        SystemLogger::warning("LibCameraItems", "libcam_prop: Unsupported value type for control " + controlName);
    }
    
    std::cout << "[DEBUG] libcam_prop: Setting " << controlName << " to " << value << " on camera " << sourceId << std::endl;

    if (!CameraDeviceManager::instance().setLibCameraControl(sourceId, controlName, value)) {
        std::cout << "[ERROR] libcam_prop: Failed to set control " << controlName << std::endl;
        return ExecutionResult::fail("Failed to set control " + controlName + " on camera " + sourceId + 
                                     " (camera might not be open or control not supported)");
    }
    
    std::cout << "[DEBUG] libcam_prop: Success for " << controlName << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LibCamListControlsItem
// ============================================================================

LibCamListControlsItem::LibCamListControlsItem() {
    _functionName = "libcam_list_controls";
    _description = "List all available controls for a libcamera device";
    _category = "video_io";
    _params = {
        ParamDef::optional("source", BaseType::ANY, "Camera source identifier (index or ID)", 0)
    };
    _example = "libcam_list_controls(0)";
    _returnType = "mat";
    _tags = {"libcamera", "camera", "controls", "list"};
}

ExecutionResult LibCamListControlsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId;
    if (args.size() > 0) {
        if (args[0].isNumeric()) {
            sourceId = std::to_string(static_cast<int>(args[0].asNumber()));
        } else {
            sourceId = args[0].asString();
        }
    } else {
        sourceId = "0";
    }

    auto camera = CameraDeviceManager::instance().getLibCamera(sourceId);
    if (!camera) {
        std::cout << "[INFO] Camera " << sourceId << " not open. Attempting to open for control discovery..." << std::endl;
        cv::Mat dummy;
        if (!CameraDeviceManager::instance().acquireFrame(sourceId, CameraBackend::LIBCAMERA, dummy)) {
            return ExecutionResult::fail("libcamera device not found or could not be opened: " + sourceId);
        }
        camera = CameraDeviceManager::instance().getLibCamera(sourceId);
    }

    if (!camera) {
        return ExecutionResult::fail("libcamera device not found or not open: " + sourceId);
    }

    const auto& controls = camera->controls();
    
    std::cout << "\n=== Available Controls for libcamera [" << sourceId << "] ===" << std::endl;
    std::cout << "Found " << controls.size() << " control(s):" << std::endl;
    
    for (const auto& [id, info] : controls) {
        std::cout << id->name()
                  << " type=" << id->type()
                  << " min="  << info.min().toString()
                  << " max="  << info.max().toString()
                  << " def="  << info.def().toString()
                  << std::endl;
    }
    
    std::cout << "=========================================================" << std::endl;

    const auto& properties = camera->properties();
    const auto& propertyIds = libcamera::properties::properties;
    
    std::cout << "\n=== Camera Properties for libcamera [" << sourceId << "] ===" << std::endl;
    for (const auto& it : properties) {
        unsigned int id = it.first;
        const libcamera::ControlValue &value = it.second;
        
        // Try to find the name from the global properties map
        std::string name = "Unknown (" + std::to_string(id) + ")";
        for (const auto& [propId, propPtr] : propertyIds) {
            if (propPtr->id() == id) {
                name = propPtr->name();
                break;
            }
        }
        
        std::cout << std::left << std::setw(35) << name 
                  << " = " << value.toString() << std::endl;
    }
    
    std::cout << "=========================================================\n" << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LibCamListItem
// ============================================================================

LibCamListItem::LibCamListItem() {
    _functionName = "libcam_list";
    _description = "List available libcamera devices";
    _category = "video_io";
    _params = {};
    _example = "libcam_list()";
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

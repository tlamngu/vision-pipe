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
    registry.add<VideoSyncItem>();
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
        ParamDef::optional("buffer_count", BaseType::INT, "Number of mmap buffers", 4),
        ParamDef::optional("subdev", BaseType::STRING,
            "Pin a specific /dev/v4l-subdev* path for control lookup on this device. "
            "Skips media-controller BFS on complex ISP pipelines (e.g. Qualcomm MSM). "
            "Leave empty for auto-discovery.", ""),
        ParamDef::optional("camera_device_manager_id", BaseType::STRING,
            "Bind this camera to a named device manager instance. "
            "Cameras on different managers have independent mutexes, "
            "eliminating cross-device lock contention and enabling "
            "true parallel capture throughput.", "")
    };
    _example = "v4l2_setup(\"/dev/video0\", 1920, 1080, \"SRGGB10\", 30)\n"
               "v4l2_setup(\"/dev/video3\", 1640, 1232, \"SRGGB8\", 30, 4, \"/dev/v4l-subdev28\")";
    _returnType = "mat";
    _tags = {"v4l2", "camera", "setup", "configuration"};
}

ExecutionResult V4L2SetupItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // NOTE: do NOT call V4L2DeviceManager::setVerbose(ctx.verbose) here.
    // ctx.verbose is per-thread (controlled by debug_start/debug_end); pushing it
    // into the process-wide singleton would poison every other concurrent thread.
    std::string sourceId = resolveSource(args[0]);

    int width = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 640;
    int height = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 480;
    std::string pixelFormat = args.size() > 3 ? args[3].asString() : "YUYV";
    int fps = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 30;
    int bufferCount = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : 4;
    std::string subdev = args.size() > 6 ? args[6].asString() : "";
    std::string managerId = args.size() > 7 ? args[7].asString() : "";

    // Optional: bind this source to a named device manager
    if (!managerId.empty()) {
        CameraDeviceManager::bindSource(sourceId, managerId);
    }

    if (ctx.verbose) {
        std::cout << "[DEBUG] V4L2Items: v4l2_setup source=" << sourceId
                  << " " << width << "x" << height
                  << " fmt=" << pixelFormat
                  << " fps=" << fps
                  << " buffers=" << bufferCount;
        if (!subdev.empty()) std::cout << " subdev=" << subdev;
        std::cout << std::endl;
    }

    V4L2NativeConfig config;
    config.width = width;
    config.height = height;
    config.pixelFormat = pixelFormat;
    config.fps = fps;
    config.bufferCount = bufferCount;

    // Open and configure through CameraDeviceManager delegation
    if (!CameraDeviceManager::forSource(sourceId).setV4L2NativeConfig(sourceId, config)) {
        return ExecutionResult::fail("v4l2_setup: failed to open/configure " + sourceId +
                                    " (" + std::to_string(width) + "x" + std::to_string(height) +
                                    " " + pixelFormat + " @" + std::to_string(fps) + "fps)");
    }

    // Pin preferred subdev if provided
    if (!subdev.empty()) {
        CameraDeviceManager::forSource(sourceId).getV4L2DeviceManager().setPreferredSubDev(sourceId, subdev);
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
    _description = "Set a V4L2 control by name.\n"
                   "Works on both /dev/video* (VFE) and /dev/v4l-subdev* paths.\n"
                   "When the control is not found on the video device, the manager "
                   "automatically discovers and tries linked sub-devices via the media controller "
                   "(BFS upstream traversal, sensor first).\n"
                   "Pass an explicit subdev path as the 4th argument to bypass auto-discovery.";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY,
            "Device path (\"/dev/video0\", \"/dev/v4l-subdev28\") or index"),
        ParamDef::required("control_name", BaseType::STRING,
            "Control name, case-insensitive with space/underscore equivalence "
            "(e.g. \"Exposure\", \"vertical_flip\", \"analogue_gain\")"),
        ParamDef::required("value", BaseType::FLOAT, "Control value"),
        ParamDef::optional("subdev", BaseType::STRING,
            "Explicit /dev/v4l-subdev* path to target directly, bypassing auto-discovery. "
            "Useful on ISP pipelines where the video node is an intermediate VFE.", "")
    };
    _example = "v4l2_prop(\"/dev/video0\", \"vertical_flip\", 1)\n"
               "v4l2_prop(\"/dev/video3\", \"vertical_flip\", 1, \"/dev/v4l-subdev28\")\n"
               "v4l2_prop(\"/dev/v4l-subdev28\", \"exposure\", 800)";
    _returnType = "mat";
    _tags = {"v4l2", "camera", "control", "property", "subdev"};
}

ExecutionResult V4L2PropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = resolveSource(args[0]);
    std::string controlName = args[1].asString();
    int value = static_cast<int>(args[2].asNumber());
    std::string explicitSubdev = args.size() > 3 ? args[3].asString() : "";

    if (ctx.verbose) {
        std::cout << "[DEBUG] V4L2Items: v4l2_prop source=" << sourceId
                  << " control='" << controlName << "' value=" << value;
        if (!explicitSubdev.empty()) std::cout << " subdev=" << explicitSubdev;
        std::cout << std::endl;
    }

    // If an explicit subdev is provided, target it directly
    const std::string& target = explicitSubdev.empty() ? sourceId : explicitSubdev;

    if (!CameraDeviceManager::forSource(target).setV4L2Control(target, controlName, value)) {
        return ExecutionResult::fail("Failed to set V4L2 control " + controlName + " on " + sourceId);
    }

    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// V4L2GetPropItem
// ============================================================================

V4L2GetPropItem::V4L2GetPropItem() {
    _functionName = "v4l2_get_prop";
    _description = "Get current value of a V4L2 control.\n"
                   "Works on both /dev/video* and /dev/v4l-subdev* paths.\n"
                   "Auto-discovers linked sub-devices when control not found on primary device.";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path or index"),
        ParamDef::required("control_name", BaseType::STRING,
            "Control name, case-insensitive (e.g. \"exposure\", \"analogue_gain\")")
    };
    _example = "exp = v4l2_get_prop(\"/dev/video0\", \"exposure\")";
    _returnType = "int";
    _tags = {"v4l2", "camera", "control", "get", "subdev"};
}

ExecutionResult V4L2GetPropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = resolveSource(args[0]);
    std::string controlName = args[1].asString();

    if (ctx.verbose) {
        std::cout << "[DEBUG] V4L2Items: v4l2_get_prop source=" << sourceId
                  << " control='" << controlName << "'" << std::endl;
    }

    int value = CameraDeviceManager::forSource(sourceId).getV4L2DeviceManager().getControl(sourceId, controlName);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_get_prop result=" << value << std::endl;
    return ExecutionResult::ok(static_cast<double>(value));
}

// ============================================================================
// V4L2ListControlsItem
// ============================================================================

V4L2ListControlsItem::V4L2ListControlsItem() {
    _functionName = "v4l2_list_controls";
    _description = "List all V4L2 controls (all classes) with ranges and current values.\n"
                   "Automatically includes controls from linked sub-devices (sensors, ISPs) "
                   "discovered via the media controller.";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Device path or index")
    };
    _example = "v4l2_list_controls(\"/dev/video3\") # also shows sensor controls";
    _returnType = "mat";
    _tags = {"v4l2", "camera", "controls", "list", "subdev"};
}

ExecutionResult V4L2ListControlsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = resolveSource(args[0]);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_list_controls source=" << sourceId << std::endl;
    CameraDeviceManager::forSource(sourceId).listV4L2Controls(sourceId);
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
    std::string sourceId = resolveSource(args[0]);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_list_formats source=" << sourceId << std::endl;
    CameraDeviceManager::forSource(sourceId).listV4L2Formats(sourceId);
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
    std::string sourceId = resolveSource(args[0]);
    if (ctx.verbose) std::cout << "[DEBUG] V4L2Items: v4l2_get_bayer source=" << sourceId << std::endl;
    std::string pattern = CameraDeviceManager::forSource(sourceId).getV4L2BayerPattern(sourceId);
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

// ============================================================================
// VideoSyncItem
// ============================================================================

VideoSyncItem::VideoSyncItem() {
    _functionName = "video_sync";
    _description =
        "Copies V4L2 controls from a master camera device to a target device so\n"
        "both cameras run with identical hardware settings.\n\n"
        "Sub-device aware: enumerates controls from the video node AND every linked\n"
        "sub-device (sensor, ISP, etc.) discovered via the media controller, so\n"
        "sensor-level controls such as 'exposure' and 'analogue_gain' that live on\n"
        "/dev/v4l-subdev* are included even when the device path is /dev/video*.\n\n"
        "Cached two-phase operation:\n"
        "  First call  — discovery: enumerate all readable controls on master,\n"
        "                test-write each to target (marks writable/not-writable),\n"
        "                perform initial full sync.\n"
        "  Later calls — delta: read each tracked control from master; write to\n"
        "                target only when the value has changed since the last sync.\n"
        "                Zero ioctls on a stable scene.\n\n"
        "Returns the number of controls written to target on this call.";
    _category = "camera";
    _params = {
        ParamDef::required("master", BaseType::ANY,
            "Master device: path (\"/dev/video3\") or index. Controls are READ from here."),
        ParamDef::required("target", BaseType::ANY,
            "Target device: path (\"/dev/video9\") or index. Controls are WRITTEN here."),
        ParamDef::optional("verbose", BaseType::BOOL,
            "Log each control name and value as it is copied (default: false)",
            RuntimeValue(false)),
    };
    _example =
        "# Sync /dev/video9 to match /dev/video3 (works with MIPI sub-device controls):\n"
        "video_sync(\"/dev/video3\", \"/dev/video9\")\n\n"
        "# With verbose logging:\n"
        "video_sync(\"/dev/video3\", \"/dev/video9\", verbose=true)";
    _returnType = "int";
    _tags = {"v4l2", "camera", "sync", "multi-camera", "control"};
}

ExecutionResult VideoSyncItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx)
{
    if (args.size() < 2) {
        return ExecutionResult::fail("video_sync: requires master and target arguments");
    }

    const std::string masterPath = resolveSource(args[0]);
    const std::string targetPath = resolveSource(args[1]);
    const bool localVerbose = (args.size() > 2 && !args[2].isVoid())
                               ? args[2].asBool() : ctx.verbose;

    if (masterPath == targetPath) {
        return ExecutionResult::fail("video_sync: master and target are the same device");
    }

    // Use the subdev-aware V4L2DeviceManager methods for all reads/writes.
    // This is what makes video_sync work correctly for MIPI cameras where
    // controls like "exposure" and "analogue_gain" live on sub-devices
    // (/dev/v4l-subdev*), not on the main /dev/video* node.
    // Route through the manager bound to the master source for proper isolation.
    V4L2DeviceManager& v4l2 = CameraDeviceManager::forSource(masterPath).getV4L2DeviceManager();

    const std::string stateKey = masterPath + "@" + targetPath;

    std::lock_guard<std::mutex> stateLk(_stateMutex);
    SyncState& state = _syncStates[stateKey];

    int synced = 0;

    if (!state.discovered) {
        // -------------------------------------------------------------------
        // DISCOVERY PHASE (first call only for this master/target pair).
        //
        // 1. Enumerate ALL control names visible on master: video node +
        //    preferred subdev (if pinned) + every cached linked subdev.
        //    This catches sensor controls (exposure, analogue_gain, etc.) on
        //    MIPI cameras whose controls live on /dev/v4l-subdev*, not on
        //    the main capture node.
        // 2. For each readable control, attempt a write to target using the
        //    subdev-aware setControl path.  Mark as writable/not-writable and
        //    cache for future calls.
        // -------------------------------------------------------------------
        if (localVerbose) {
            std::cout << "[video_sync] DISCOVERY  master=" << masterPath
                      << "  target=" << targetPath << "\n";
        }

        std::vector<std::string> names = v4l2.enumerateControlNames(masterPath);
        if (localVerbose) {
            std::cout << "[video_sync]   found " << names.size()
                      << " candidate controls on master\n";
        }

        for (const auto& name : names) {
            int32_t val = v4l2.getControl(masterPath, name);
            if (val == INT_MIN) continue;   // not readable on master — skip

            ControlEntry entry;
            entry.name = name;

            bool ok = v4l2.setControl(targetPath, name, val);
            entry.writable   = ok;
            entry.lastSynced = ok ? val : INT_MIN;

            if (ok) {
                ++synced;
                if (localVerbose)
                    std::cout << "[video_sync]   SYNC  " << name << " = " << val << "\n";
            } else if (localVerbose) {
                std::cout << "[video_sync]   SKIP  " << name
                          << " (target rejected)\n";
            }
            state.controls.push_back(std::move(entry));
        }

        state.discovered = true;

        if (localVerbose || ctx.verbose) {
            std::cout << "[video_sync] discovery done: " << synced
                      << " controls synced ("
                      << state.controls.size() << " discovered, "
                      << (state.controls.size() - synced) << " skipped)\n";
        }
    } else {
        // -------------------------------------------------------------------
        // UPDATE PHASE (every subsequent call).
        //
        // Only push controls whose value on master has changed since the last
        // sync — avoids redundant ioctls and the associated lock contention.
        // -------------------------------------------------------------------
        for (auto& entry : state.controls) {
            if (!entry.writable) continue;

            int32_t cur = v4l2.getControl(masterPath, entry.name);
            if (cur == INT_MIN || cur == entry.lastSynced) continue;

            bool ok = v4l2.setControl(targetPath, entry.name, cur);
            if (ok) {
                if (localVerbose) {
                    std::cout << "[video_sync]   DELTA " << entry.name
                              << "  " << entry.lastSynced << " -> " << cur << "\n";
                }
                entry.lastSynced = cur;
                ++synced;
            }
        }

        if ((localVerbose || ctx.verbose) && synced > 0) {
            std::cout << "[video_sync] update: " << synced
                      << " control(s) changed and synced\n";
        }
    }

    return ExecutionResult::ok(RuntimeValue(static_cast<double>(synced)));
}

} // namespace visionpipe

#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

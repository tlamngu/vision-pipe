#include "interpreter/items/discovery_items.h"
#include "libvisionpipe/discovery.h"
#include <iostream>
#include <sstream>

namespace visionpipe {

// ============================================================================
// Helpers
// ============================================================================

namespace {

// Build a flat RuntimeValue ARRAY for one SinkInfo entry.
// Layout: [session_id, sink_name, shm_name, producer_pid, producer_alive]
RuntimeValue sinkInfoToArray(const SinkInfo& info) {
    return RuntimeValue(std::vector<RuntimeValue>{
        RuntimeValue(info.sessionId),
        RuntimeValue(info.sinkName),
        RuntimeValue(info.shmName),
        RuntimeValue(static_cast<int64_t>(info.producerPid)),
        RuntimeValue(info.producerAlive)
    });
}

// Build a flat RuntimeValue ARRAY for one CaptureFormat entry.
// Layout: [pixel_format, width, height, fps_min, fps_max]
RuntimeValue captureFormatToArray(const CaptureFormat& fmt) {
    return RuntimeValue(std::vector<RuntimeValue>{
        RuntimeValue(fmt.pixelFormat),
        RuntimeValue(static_cast<int64_t>(fmt.width)),
        RuntimeValue(static_cast<int64_t>(fmt.height)),
        RuntimeValue(fmt.fpsMin),
        RuntimeValue(fmt.fpsMax)
    });
}

// Build a flat RuntimeValue ARRAY for a CaptureDevice.
// Layout: [path, device_name, driver_name, bus_info, backend,
//          is_capture, has_controls, format_count,
//          [fmt0], [fmt1], ...]
// The formats are appended after the scalar fields so scripts can read them
// via indexing: dev[8], dev[9], …
RuntimeValue captureDeviceToArray(const CaptureDevice& dev) {
    std::vector<RuntimeValue> arr;
    arr.reserve(8 + dev.formats.size());
    arr.push_back(RuntimeValue(dev.path));
    arr.push_back(RuntimeValue(dev.deviceName));
    arr.push_back(RuntimeValue(dev.driverName));
    arr.push_back(RuntimeValue(dev.busInfo));
    arr.push_back(RuntimeValue(dev.backend));
    arr.push_back(RuntimeValue(dev.isCapture));
    arr.push_back(RuntimeValue(dev.hasControls));
    arr.push_back(RuntimeValue(static_cast<int64_t>(dev.formats.size())));
    for (const auto& fmt : dev.formats) {
        arr.push_back(captureFormatToArray(fmt));
    }
    return RuntimeValue(std::move(arr));
}

void printSinkInfo(const SinkInfo& info) {
    std::cout << "  sink: " << info.sinkName
              << "  session: " << info.sessionId
              << "  pid: " << info.producerPid
              << "  alive: " << (info.producerAlive ? "yes" : "no")
              << "  shm: " << info.shmName << "\n";
}

void printCaptureDevice(const CaptureDevice& dev, bool withFormats) {
    std::cout << "  path: " << dev.path
              << "  name: " << dev.deviceName
              << "  driver: " << dev.driverName
              << "  bus: " << dev.busInfo
              << "  backend: " << dev.backend
              << "  capture: " << (dev.isCapture ? "yes" : "no")
              << "  controls: " << (dev.hasControls ? "yes" : "no") << "\n";
    if (withFormats) {
        for (const auto& fmt : dev.formats) {
            std::cout << "    format: " << fmt.pixelFormat
                      << "  " << fmt.width << "x" << fmt.height
                      << "  fps: " << fmt.fpsMin << "-" << fmt.fpsMax << "\n";
        }
    }
}

} // anonymous namespace

// ============================================================================
// DiscoverSinksItem
// ============================================================================

DiscoverSinksItem::DiscoverSinksItem() {
    _functionName = "discover_sinks";
    _description = "Enumerate all active frame_sink() IPC segments visible to this system.\n\n"
                   "Scans /dev/shm (Linux/macOS) for visionpipe sink shared-memory segments.\n"
                   "Each entry is opened read-only to verify the magic and read the producer PID.\n\n"
                   "Returns an ARRAY where each element is a sub-array:\n"
                   "  [session_id, sink_name, shm_name, producer_pid, producer_alive]\n\n"
                   "Example:\n"
                   "  sinks = discover_sinks()\n"
                   "  # sinks[0][0] = session_id, sinks[0][1] = sink_name, ...";
    _category = "io";
    _params = {};
    _example = "sinks = discover_sinks()";
    _returnType = "array";
    _tags = {"sink", "ipc", "discover", "shm", "libvisionpipe"};
}

ExecutionResult DiscoverSinksItem::execute(const std::vector<RuntimeValue>& /*args*/,
                                            ExecutionContext& ctx) {
    auto sinks = discoverSinks();

    std::cout << "\n=== Active frame_sink() segments === (" << sinks.size() << " found)\n";
    for (const auto& s : sinks) printSinkInfo(s);
    std::cout << "====================================\n" << std::endl;

    std::vector<RuntimeValue> arr;
    arr.reserve(sinks.size());
    for (const auto& s : sinks) arr.push_back(sinkInfoToArray(s));

    return ExecutionResult::ok(RuntimeValue(std::move(arr)));
}

// ============================================================================
// SinkPropertiesItem
// ============================================================================

SinkPropertiesItem::SinkPropertiesItem() {
    _functionName = "sink_properties";
    _description = "Read the current frame properties from an active frame_sink() IPC segment.\n\n"
                   "Opens the shared-memory segment for (session_id, sink_name) read-only,\n"
                   "verifies the magic and returns a snapshot of the frame header.\n\n"
                   "Returns an ARRAY:\n"
                   "  [is_valid, rows, cols, channels, cv_type, step,\n"
                   "   data_size, frame_seq, producer_pid, producer_alive]\n\n"
                   "Indices:\n"
                   "  0  is_valid     (bool)\n"
                   "  1  rows         (int)   frame height\n"
                   "  2  cols         (int)   frame width\n"
                   "  3  channels     (int)   colour channels\n"
                   "  4  cv_type      (int)   OpenCV Mat type constant\n"
                   "  5  step         (int)   bytes per row\n"
                   "  6  data_size    (int)   payload bytes\n"
                   "  7  frame_seq    (int)   current frame counter\n"
                   "  8  producer_pid (int)\n"
                   "  9  producer_alive (bool)";
    _category = "io";
    _params = {
        ParamDef::required("session_id", BaseType::STRING, "Session ID (from discover_sinks or known session)"),
        ParamDef::required("sink_name",  BaseType::STRING, "Sink name passed to frame_sink()")
    };
    _example = "props = sink_properties(\"abc123_def456\", \"output\")\n"
               "# props[0] == true means valid\n"
               "# props[2]/props[1] = cols x rows";
    _returnType = "array";
    _tags = {"sink", "ipc", "properties", "shm", "libvisionpipe"};
}

ExecutionResult SinkPropertiesItem::execute(const std::vector<RuntimeValue>& args,
                                             ExecutionContext& ctx) {
    if (args.size() < 2) {
        return ExecutionResult::fail("sink_properties: requires session_id and sink_name");
    }
    std::string sessionId = args[0].asString();
    std::string sinkName  = args[1].asString();

    SinkProperties props = sinkProperties(sessionId, sinkName);

    if (ctx.verbose) {
        std::cout << "[DEBUG] sink_properties(" << sessionId << ", " << sinkName << ")"
                  << " valid=" << props.isValid
                  << " " << props.cols << "x" << props.rows
                  << " ch=" << props.channels
                  << " type=" << props.cvType
                  << " seq=" << props.frameSeq << "\n";
    }

    std::vector<RuntimeValue> arr = {
        RuntimeValue(props.isValid),
        RuntimeValue(static_cast<int64_t>(props.rows)),
        RuntimeValue(static_cast<int64_t>(props.cols)),
        RuntimeValue(static_cast<int64_t>(props.channels)),
        RuntimeValue(static_cast<int64_t>(props.cvType)),
        RuntimeValue(static_cast<int64_t>(props.step)),
        RuntimeValue(static_cast<int64_t>(props.dataSize)),
        RuntimeValue(static_cast<int64_t>(props.frameSeq)),
        RuntimeValue(static_cast<int64_t>(props.producerPid)),
        RuntimeValue(props.producerAlive)
    };

    return ExecutionResult::ok(RuntimeValue(std::move(arr)));
}

// ============================================================================
// DiscoverCaptureItem
// ============================================================================

DiscoverCaptureItem::DiscoverCaptureItem() {
    _functionName = "discover_capture";
    _description = "Enumerate all available video capture devices on the system.\n\n"
                   "Probes V4L2 (/dev/video0…63) on Linux and libcamera when compiled in.\n"
                   "Each device entry is opened briefly to read capabilities and supported\n"
                   "pixel formats/resolutions/framerates.\n\n"
                   "Returns an ARRAY where each element is a sub-array:\n"
                   "  [path, device_name, driver_name, bus_info, backend,\n"
                   "   is_capture, has_controls, format_count,\n"
                   "   [fmt0_pixel_format, width, height, fps_min, fps_max], ...]\n\n"
                   "Example:\n"
                   "  cameras = discover_capture()\n"
                   "  # cameras[0][0] = path (/dev/video0)\n"
                   "  # cameras[0][7] = number of supported formats";
    _category = "video_io";
    _params = {};
    _example = "cameras = discover_capture()";
    _returnType = "array";
    _tags = {"camera", "capture", "discover", "v4l2", "enumerate"};
}

ExecutionResult DiscoverCaptureItem::execute(const std::vector<RuntimeValue>& /*args*/,
                                              ExecutionContext& ctx) {
    auto devices = discoverCapture();

    std::cout << "\n=== Capture Devices === (" << devices.size() << " found)\n";
    for (const auto& dev : devices) printCaptureDevice(dev, /*withFormats=*/false);
    std::cout << "======================\n" << std::endl;

    std::vector<RuntimeValue> arr;
    arr.reserve(devices.size());
    for (const auto& dev : devices) arr.push_back(captureDeviceToArray(dev));

    return ExecutionResult::ok(RuntimeValue(std::move(arr)));
}

// ============================================================================
// CaptureCapabilitiesItem
// ============================================================================

CaptureCapabilitiesItem::CaptureCapabilitiesItem() {
    _functionName = "capture_capabilities";
    _description = "Query detailed capabilities for a single capture device.\n\n"
                   "Accepts a device path (\"/dev/video0\") or integer index (0).\n"
                   "Opens the device, enumerates all V4L2 pixel formats, and for each\n"
                   "format enumerates supported frame sizes and framerates.\n\n"
                   "Returns an ARRAY:\n"
                   "  [path, device_name, driver_name, bus_info, backend,\n"
                   "   is_capture, has_controls, format_count,\n"
                   "   [fmt0_pixel_format, width, height, fps_min, fps_max],\n"
                   "   [fmt1_pixel_format, width, height, fps_min, fps_max], ...]\n\n"
                   "Indices:\n"
                   "  0  path          (string)\n"
                   "  1  device_name   (string)  V4L2 card name\n"
                   "  2  driver_name   (string)  kernel driver\n"
                   "  3  bus_info      (string)  USB/PCIe bus path\n"
                   "  4  backend       (string)  \"v4l2\" | \"libcamera\" | \"opencv\"\n"
                   "  5  is_capture    (bool)\n"
                   "  6  has_controls  (bool)\n"
                   "  7  format_count  (int)\n"
                   "  8+ formats       each a [pixel_format, w, h, fps_min, fps_max] array";
    _category = "video_io";
    _params = {
        ParamDef::required("device", BaseType::ANY,
            "Device path (\"/dev/video0\") or integer index (0)")
    };
    _example = "caps = capture_capabilities(\"/dev/video0\")\n"
               "caps = capture_capabilities(0)  # same thing\n"
               "# caps[7] = format count\n"
               "# caps[8] = [\"YUYV\", 640, 480, 5.0, 30.0]";
    _returnType = "array";
    _tags = {"camera", "capture", "capabilities", "formats", "v4l2"};
}

ExecutionResult CaptureCapabilitiesItem::execute(const std::vector<RuntimeValue>& args,
                                                  ExecutionContext& ctx) {
    if (args.empty()) {
        return ExecutionResult::fail("capture_capabilities: requires device argument");
    }

    std::string devicePathOrIndex;
    if (args[0].isNumeric()) {
        devicePathOrIndex = std::to_string(static_cast<int>(args[0].asNumber()));
    } else {
        devicePathOrIndex = args[0].asString();
    }

    CaptureDevice dev = captureCapabilities(devicePathOrIndex);

    std::cout << "\n=== Capture Capabilities: " << dev.path << " ===\n";
    printCaptureDevice(dev, /*withFormats=*/true);
    std::cout << "=========================================\n" << std::endl;

    return ExecutionResult::ok(captureDeviceToArray(dev));
}

// ============================================================================
// Registration
// ============================================================================

void registerDiscoveryItems(ItemRegistry& registry) {
    registry.add<DiscoverSinksItem>();
    registry.add<SinkPropertiesItem>();
    registry.add<DiscoverCaptureItem>();
    registry.add<CaptureCapabilitiesItem>();
}

} // namespace visionpipe

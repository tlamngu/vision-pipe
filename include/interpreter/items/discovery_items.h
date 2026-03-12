#ifndef VISIONPIPE_DISCOVERY_ITEMS_H
#define VISIONPIPE_DISCOVERY_ITEMS_H

/**
 * @file discovery_items.h
 * @brief VSP built-in functions for discovering sinks and capture devices
 *
 * Provides four VSP language functions:
 *
 *   discover_sinks()
 *       Returns an array of sink descriptor objects (session_id, sink_name,
 *       producer_pid, producer_alive) for every active frame_sink() IPC
 *       segment visible to the system.
 *
 *   sink_properties(session_id, sink_name)
 *       Returns a property object (rows, cols, channels, cv_type, step,
 *       frame_seq, producer_pid, producer_alive, is_valid) for the named sink.
 *
 *   discover_capture()
 *       Returns an array of capture device descriptor objects (path,
 *       device_name, driver_name, bus_info, backend, has_controls) for
 *       every detected video capture source.
 *
 *   capture_capabilities(device)
 *       Returns a device descriptor object populated with the full list of
 *       supported pixel formats, resolutions, and framerates for the given
 *       device path or index.
 *
 * Example (.vsp):
 * @code
 *   sinks = discover_sinks()
 *   log "Active sinks: {sinks}"
 *
 *   props = sink_properties("abc123_def456", "output")
 *   log "Frame size: {props.cols}x{props.rows}"
 *
 *   cameras = discover_capture()
 *   caps    = capture_capabilities("/dev/video0")
 * @endcode
 */

#include "interpreter/item_registry.h"

namespace visionpipe {

// ============================================================================
// discover_sinks()
// ============================================================================

/**
 * @brief Enumerate all active frame_sink IPC segments visible to the system.
 *
 * Parameters: none
 *
 * Returns: ARRAY of STRING-keyed property groups, each with:
 *   session_id    (string)  – session that owns the sink
 *   sink_name     (string)  – argument passed to frame_sink()
 *   shm_name      (string)  – full shared-memory segment name
 *   producer_pid  (int)     – PID of the producer process
 *   producer_alive(bool)    – whether the producer process is alive
 */
class DiscoverSinksItem : public InterpreterItem {
public:
    DiscoverSinksItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// sink_properties(session_id, sink_name)
// ============================================================================

/**
 * @brief Read frame properties from an active sink's shared-memory header.
 *
 * Parameters:
 *   session_id (string) – from discover_sinks() or known session
 *   sink_name  (string) – sink name passed to frame_sink()
 *
 * Returns: STRING-keyed property group with:
 *   is_valid      (bool)   – false when the segment cannot be opened
 *   rows          (int)    – frame height
 *   cols          (int)    – frame width
 *   channels      (int)    – number of colour channels
 *   cv_type       (int)    – OpenCV Mat type constant
 *   step          (int)    – bytes per row (stride)
 *   data_size     (int)    – frame payload bytes
 *   frame_seq     (int)    – current frame counter
 *   producer_pid  (int)    – producer PID
 *   producer_alive(bool)   – whether the producer is alive
 */
class SinkPropertiesItem : public InterpreterItem {
public:
    SinkPropertiesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// discover_capture()
// ============================================================================

/**
 * @brief Enumerate all available video capture devices.
 *
 * Parameters: none
 *
 * Returns: ARRAY of STRING-keyed property groups, each with:
 *   path          (string)  – device path, e.g. "/dev/video0"
 *   device_name   (string)  – human-readable name
 *   driver_name   (string)  – kernel driver / backend
 *   bus_info      (string)  – USB/PCIe bus identifier
 *   backend       (string)  – "v4l2", "libcamera", or "opencv"
 *   is_capture    (bool)    – true when the device produces video frames
 *   has_controls  (bool)    – true when V4L2 controls are available
 *   format_count  (int)     – number of formats in the formats sub-array
 */
class DiscoverCaptureItem : public InterpreterItem {
public:
    DiscoverCaptureItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// capture_capabilities(device)
// ============================================================================

/**
 * @brief Query detailed capabilities for a single capture device.
 *
 * Parameters:
 *   device (string|int) – device path ("/dev/video0") or integer index
 *
 * Returns: STRING-keyed property group with device + formats fields populated.
 *   path          (string)  – device path
 *   device_name   (string)  – human-readable name
 *   driver_name   (string)  – kernel driver / backend
 *   bus_info      (string)  – USB/PCIe bus identifier
 *   backend       (string)  – "v4l2", "libcamera", or "opencv"
 *   is_capture    (bool)
 *   has_controls  (bool)
 *   formats       (array)   – each element:
 *     pixel_format (string) – e.g. "YUYV", "MJPG"
 *     width        (int)
 *     height       (int)
 *     fps_min      (float)
 *     fps_max      (float)
 */
class CaptureCapabilitiesItem : public InterpreterItem {
public:
    CaptureCapabilitiesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Registration
// ============================================================================

void registerDiscoveryItems(ItemRegistry& registry);

} // namespace visionpipe

#endif // VISIONPIPE_DISCOVERY_ITEMS_H

#ifndef LIBVISIONPIPE_DISCOVERY_H
#define LIBVISIONPIPE_DISCOVERY_H

/**
 * @file discovery.h
 * @brief Runtime discovery APIs for VisionPipe sinks and capture devices
 *
 * Provides zero-dependency utilities for enumerating:
 *  - Active frame_sink() IPC segments (discover_sinks / sink_properties)
 *  - Available capture devices and their capabilities (discover_capture / capture_capabilities)
 *
 * All functions are synchronous and safe to call from any thread.
 */

#include <string>
#include <vector>
#include <cstdint>

// LIBVISIONPIPE_API visibility macro (defined in libvisionpipe.h)
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef LIBVISIONPIPE_BUILDING
    #define LIBVISIONPIPE_API __declspec(dllexport)
  #else
    #define LIBVISIONPIPE_API __declspec(dllimport)
  #endif
#else
  #define LIBVISIONPIPE_API __attribute__((visibility("default")))
#endif

namespace visionpipe {

// ============================================================================
// Sink discovery
// ============================================================================

/**
 * @brief Information about an active frame_sink IPC segment
 */
struct LIBVISIONPIPE_API SinkInfo {
    std::string sessionId;   ///< Session ID of the producer
    std::string sinkName;    ///< Sink name (argument to frame_sink("name"))
    std::string shmName;     ///< Full shared-memory segment name
    int32_t     producerPid; ///< PID of the visionpipe process that owns this sink
    bool        producerAlive; ///< true when kill(pid,0) succeeds (POSIX) or process handle valid (Win)
};

/**
 * @brief Detailed frame properties read from an active sink's shared-memory header
 */
struct LIBVISIONPIPE_API SinkProperties {
    bool     isValid    = false; ///< false when the SHM segment does not exist / magic mismatch
    int32_t  rows       = 0;     ///< Frame height in pixels
    int32_t  cols       = 0;     ///< Frame width in pixels
    int32_t  cvType     = 0;     ///< OpenCV Mat type (e.g. CV_8UC3 == 16)
    int32_t  channels   = 0;     ///< Number of colour channels
    int32_t  step       = 0;     ///< Bytes per row (stride)
    uint32_t dataSize   = 0;     ///< Actual frame payload bytes
    uint64_t frameSeq   = 0;     ///< Monotonic frame counter at query time
    int32_t  producerPid = 0;    ///< PID of the visionpipe process
    bool     producerAlive = false;
};

/**
 * @brief Enumerate all active frame_sink IPC segments visible to this process.
 *
 * On Linux/macOS this scans /dev/shm for entries whose names begin with the
 * SHM_PREFIX ("visionpipe_sink_").  On Windows the Global\\ object directory
 * is searched for matching Named Shared Memory objects.
 *
 * Each matching entry is opened read-only to read the FrameShmHeader and
 * verify the magic value; invalid segments are silently skipped.
 *
 * @return Vector of SinkInfo for every live segment found.
 */
LIBVISIONPIPE_API std::vector<SinkInfo> discoverSinks();

/**
 * @brief Read the frame properties from an existing sink without consuming frames.
 *
 * Opens the shared-memory segment for (sessionId, sinkName) read-only,
 * verifies the magic, and returns a snapshot of the FrameShmHeader.
 * The segment is closed immediately after reading.
 *
 * @param sessionId  Session ID of the producer (from SinkInfo::sessionId)
 * @param sinkName   Sink name  (from SinkInfo::sinkName)
 * @return SinkProperties; is_valid == false if not found.
 */
LIBVISIONPIPE_API SinkProperties sinkProperties(const std::string& sessionId,
                                                  const std::string& sinkName);

// ============================================================================
// Capture device discovery
// ============================================================================

/**
 * @brief A single supported format / resolution entry for a capture device
 */
struct LIBVISIONPIPE_API CaptureFormat {
    std::string pixelFormat; ///< FourCC string, e.g. "YUYV", "MJPG", "NV12", "SRGGB10"
    uint32_t    width;       ///< Width in pixels
    uint32_t    height;      ///< Height in pixels
    double      fpsMin;      ///< Minimum supported framerate
    double      fpsMax;      ///< Maximum supported framerate
};

/**
 * @brief Information about a single capture device
 */
struct LIBVISIONPIPE_API CaptureDevice {
    std::string path;         ///< e.g. "/dev/video0" or "0" on Windows
    std::string deviceName;   ///< Human-readable device card name
    std::string driverName;   ///< Kernel driver name (V4L2) or backend string
    std::string busInfo;      ///< USB bus path / PCIe slot string (V4L2)
    std::string backend;      ///< "v4l2", "libcamera", "opencv" – enumeration source
    bool        isCapture;    ///< true when the device can produce video frames
    bool        hasControls;  ///< true when V4L2 controls (exposure, focus…) are available
    std::vector<CaptureFormat> formats; ///< Supported format × resolution combinations
};

/**
 * @brief Enumerate all capture devices visible to this system.
 *
 * Probes available backends in order:
 *  1. V4L2 – scans /dev/video0 … /dev/video63 (Linux only)
 *  2. libcamera – enumerates camera manager if compiled in
 *
 * For each device found a compact CaptureDevice struct is returned.
 * The formats field is populated with the supported pixel formats and a
 * representative set of resolutions / framerates; it may be empty for
 * devices that cannot be opened.
 *
 * @return Vector of one CaptureDevice per detected capture source.
 */
LIBVISIONPIPE_API std::vector<CaptureDevice> discoverCapture();

/**
 * @brief Query detailed capabilities for a single capture device.
 *
 * @param devicePathOrIndex  Device path ("/dev/video0") or integer index as
 *                           string ("0").  The backend is auto-selected based
 *                           on the path prefix.
 * @return CaptureDevice with formats populated; isValid == false when the
 *         device cannot be opened.
 */
LIBVISIONPIPE_API CaptureDevice captureCapabilities(const std::string& devicePathOrIndex);

} // namespace visionpipe

#endif // LIBVISIONPIPE_DISCOVERY_H

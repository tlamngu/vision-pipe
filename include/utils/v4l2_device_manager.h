#ifndef VISIONPIPE_V4L2_DEVICE_MANAGER_H
#define VISIONPIPE_V4L2_DEVICE_MANAGER_H

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED

#include <opencv2/core/mat.hpp>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>

namespace visionpipe {

/**
 * @brief I/O method for V4L2 buffer exchange
 */
enum class V4L2IOMethod {
    MMAP
};

/**
 * @brief Configuration for the V4L2 native backend
 */
struct V4L2NativeConfig {
    int width = 640;
    int height = 480;
    int fps = 30;
    std::string pixelFormat = "YUYV"; // FourCC or named: "YUYV", "MJPG", "NV12", "SRGGB10", etc.
    int bufferCount = 4;
    V4L2IOMethod io_method = V4L2IOMethod::MMAP;
};

/**
 * @brief Singleton manager for native V4L2 device access.
 *
 * Provides full format negotiation (VIDIOC_S_FMT), mmap buffer management,
 * User-class and Camera-class control access, and frame acquisition using
 * kernel-level V4L2 APIs.
 */
class V4L2DeviceManager {
public:
    /**
     * @brief Get the singleton instance.
     */
    static V4L2DeviceManager& instance();

    // Non-copyable
    V4L2DeviceManager(const V4L2DeviceManager&) = delete;
    V4L2DeviceManager& operator=(const V4L2DeviceManager&) = delete;

    /**
     * @brief Acquire a frame from the V4L2 device.
     * @param devicePath e.g. "/dev/video0"
     * @param frame Output cv::Mat
     * @return true on success
     */
    bool acquireFrame(const std::string& devicePath, cv::Mat& frame);

    /**
     * @brief Open and configure a V4L2 device (VIDIOC_S_FMT + REQBUFS + mmap + STREAMON).
     * @param devicePath e.g. "/dev/video0"
     * @param config Desired configuration
     * @return true on success
     */
    bool prepareDevice(const std::string& devicePath, const V4L2NativeConfig& config);

    /**
     * @brief Release a specific V4L2 device (STREAMOFF + munmap + close).
     */
    void releaseDevice(const std::string& devicePath);

    /**
     * @brief Release all open V4L2 devices.
     */
    void releaseAll();

    /**
     * @brief Set a V4L2 control by name or numeric ID.
     * @param devicePath Device path
     * @param controlNameOrId Control name string (e.g. "Brightness") or numeric ID as string
     * @param value Control value
     * @return true on success
     */
    bool setControl(const std::string& devicePath, const std::string& controlNameOrId, int value);

    /**
     * @brief Get the current value of a V4L2 control.
     * @param devicePath Device path
     * @param controlNameOrId Control name string or numeric ID as string
     * @return Control value, or -1 on error
     */
    int getControl(const std::string& devicePath, const std::string& controlNameOrId);

    /**
     * @brief Print all User-class and Camera-class controls with ranges and current values.
     */
    void listControls(const std::string& devicePath);

    /**
     * @brief Print all supported pixel formats and frame sizes (VIDIOC_ENUM_FMT + ENUM_FRAMESIZES).
     */
    void listFormats(const std::string& devicePath);

    /**
     * @brief Derive the Bayer pattern string from the active FourCC.
     * @return "RGGB", "BGGR", "GBRG", "GRBG", or "" if not Bayer
     */
    std::string getBayerPattern(const std::string& devicePath);

    /**
     * @brief Check if a device session is active.
     */
    bool isOpen(const std::string& devicePath) const;

    /**
     * @brief Enumerate all readable V4L2 control names visible on the device
     *        and all its linked sub-devices (sensor, ISP, etc.).
     *
     * Scans the video node, the pinned preferred sub-dev (if any), and every
     * sub-device discovered via the media-controller BFS at prepareDevice()
     * time.  Names are returned in normalized form (lowercase, spaces replaced
     * by underscores) — the same form accepted by setControl / getControl —
     * with duplicates removed.
     *
     * This is used by video_sync for one-time control discovery so that even
     * sensor controls that live on /dev/v4l-subdev* (exposure, analogue_gain,
     * etc.) are included, not just the controls on the /dev/video* node.
     *
     * @param devicePath  e.g. "/dev/video0"
     * @return Deduplicated list of normalized control names.
     */
    std::vector<std::string> enumerateControlNames(const std::string& devicePath);

    /**
     * @brief Enable or disable verbose debug output (mirrors --verbose flag).
     */
    void setVerbose(bool verbose);

    /**
     * @brief Returns whether verbose debug output is enabled.
     */
    bool isVerbose() const;

    /**
     * @brief Pin a specific subdev path as the preferred control target for a
     *        video device.  When set, setControl/getControl will try this subdev
     *        first (before the media-controller BFS scan), saving discovery
     *        overhead and ensuring the correct node is targeted on complex ISP
     *        pipelines (e.g. Qualcomm MSM where the VFE is an intermediate node).
     *
     * @param videoDevPath  e.g. "/dev/video3"
     * @param subDevPath    e.g. "/dev/v4l-subdev28"
     */
    void setPreferredSubDev(const std::string& videoDevPath,
                            const std::string& subDevPath);

    /**
     * @brief Remove a previously pinned preferred subdev, reverting to full
     *        auto-discovery via the media controller.
     */
    void clearPreferredSubDev(const std::string& videoDevPath);

    // Public constructor/destructor to allow owned instances (e.g. per
    // CameraDeviceManager).  The singleton instance() is still available.
    V4L2DeviceManager() = default;
    ~V4L2DeviceManager();

private:

    struct MappedBuffer {
        struct Plane {
            void* start = nullptr;
            size_t length = 0;
        };
        // One entry per V4L2 data plane.
        // Single-plane and single-plane-MPLANE formats: planes.size() == 1
        // NV12/NV21 MPLANE: planes.size() == 2  (Y, UV)
        // YUV420 MPLANE:    planes.size() == 3  (Y, U, V)
        std::vector<Plane> planes;
    };

    struct V4L2Session {
        int fd = -1;
        V4L2NativeConfig config;
        uint32_t negotiatedPixFmt = 0; // V4L2 FourCC after S_FMT
        int negotiatedWidth = 0;
        int negotiatedHeight = 0;
        uint32_t negotiatedBytesPerLine = 0;
        uint32_t negotiatedSizeImage = 0;
        std::vector<MappedBuffer> buffers;
        bool streaming = false;
        bool isMplane = false; // true when device uses V4L2_CAP_VIDEO_CAPTURE_MPLANE
        // bytesperline for each data plane (from VIDIOC_S_FMT response)
        // index 0 = plane 0, index 1 = plane 1, etc.
        std::vector<uint32_t> planeBytesPerLine;
        // Sub-device paths discovered via media-controller BFS at prepareDevice()
        // time and cached here so every subsequent setControl / getControl call
        // can skip the expensive filesystem + ioctl enumeration.
        std::vector<std::string> cachedSubDevs;
    };

    bool openDevice(const std::string& devicePath, V4L2Session& session);
    uint32_t lookupPixelFormat(const std::string& name) const;
    uint32_t resolveControlId(int fd, const std::string& nameOrId) const;
    std::string fourccToString(uint32_t fourcc) const;

    /**
     * @brief Open an fd for the given device path using an existing session if available,
     *        otherwise open temporarily.  Caller must close the fd when done ONLY if
     *        *tempOpened is set to true.
     */
    int acquireFd(const std::string& devicePath, bool* tempOpened) const;

    /**
     * @brief Discover /dev/v4l-subdev* paths reachable upstream from a video
     *        device via BFS through the V4L2 media controller (/dev/media*).
     * @return Ordered list of sub-device paths, deepest upstream (sensor) first.
     */
    std::vector<std::string> findLinkedSubDevices(const std::string& videoDevPath) const;

    /**
     * @brief Resolve a control name or numeric ID to a V4L2 control ID,
     *        using a per-device cache so the VIDIOC_QUERYCTRL enumeration
     *        scan only happens once per (device, control-name) pair.
     */
    uint32_t resolveControlIdCached(const std::string& fdDevPath, int fd,
                                    const std::string& nameOrId) const;

    /**
     * @brief Return the cached sub-device list for a video device.
     *
     * If the device was opened via prepareDevice() the list was already built
     * by a one-time BFS at setup time and is returned directly from the session
     * (zero extra ioctls, zero filesystem stat calls).  For devices that were
     * never set up through prepareDevice() (e.g. ephemeral access) the live
     * BFS fallback is used transparently.
     */
    std::vector<std::string> cachedLinkedSubDevs(const std::string& videoDevPath) const;

    mutable std::recursive_mutex _mutex;
    std::map<std::string, V4L2Session> _sessions;
    // Optional pinned subdev per video device path (set by setPreferredSubDev)
    std::map<std::string, std::string> _preferredSubDevs;
    // Control-name → CID cache, keyed by device/subdev path.
    // Populated on first successful resolveControlIdCached() call; invalidated
    // when releaseDevice() removes the session for that path.
    mutable std::unordered_map<std::string,
                               std::unordered_map<std::string, uint32_t>> _controlIdCache;
    bool _verbose = false;
};

} // namespace visionpipe

#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

#endif // VISIONPIPE_V4L2_DEVICE_MANAGER_H

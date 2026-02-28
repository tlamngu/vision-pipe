#ifndef VISIONPIPE_V4L2_DEVICE_MANAGER_H
#define VISIONPIPE_V4L2_DEVICE_MANAGER_H

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED

#include <opencv2/core/mat.hpp>
#include <string>
#include <map>
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

private:
    V4L2DeviceManager() = default;
    ~V4L2DeviceManager();

    struct MappedBuffer {
        void* start = nullptr;
        size_t length = 0;
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
    };

    bool openDevice(const std::string& devicePath, V4L2Session& session);
    uint32_t lookupPixelFormat(const std::string& name) const;
    uint32_t resolveControlId(int fd, const std::string& nameOrId) const;
    std::string fourccToString(uint32_t fourcc) const;

    mutable std::recursive_mutex _mutex;
    std::map<std::string, V4L2Session> _sessions;
};

} // namespace visionpipe

#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

#endif // VISIONPIPE_V4L2_DEVICE_MANAGER_H

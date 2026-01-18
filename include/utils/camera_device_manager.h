#ifndef VISIONPIPE_CAMERA_DEVICE_MANAGER_H
#define VISIONPIPE_CAMERA_DEVICE_MANAGER_H

#include <opencv2/opencv.hpp>
#include <map>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>

#ifdef VISIONPIPE_LIBCAMERA_ENABLED
#include <libcamera/libcamera.h>
#endif

namespace visionpipe {

/**
 * @brief Enum for camera backend types
 */
enum class CameraBackend {
    OPENCV_AUTO,
    OPENCV_DSHOW,
    OPENCV_V4L2,
    OPENCV_FFMPEG,
    OPENCV_GSTREAMER,
    LIBCAMERA
};

/**
 * @brief Configuration for libcamera backend
 */
struct LibCameraConfig {
    int width = 640;
    int height = 480;
    std::string pixelFormat = "BGR888"; 
    int bufferCount = 4;
    std::string streamRole = "VideoRecording";
};

/**
 * @brief Centralized camera device manager for persistent session handling.
 * 
 * This singleton class manages camera lifecycles to avoid repeated
 * open/close cycles. It supports both OpenCV VideoCapture and native
 * libcamera backends.
 */
class CameraDeviceManager {
public:
    /**
     * @brief Get the singleton instance.
     */
    static CameraDeviceManager& instance();

    // Delete copy/move constructors
    CameraDeviceManager(const CameraDeviceManager&) = delete;
    CameraDeviceManager& operator=(const CameraDeviceManager&) = delete;

    /**
     * @brief Acquire a frame from the specified camera source.
     * Opens the camera if not already open.
     * 
     * @param sourceId Camera identifier (index as string, path, or URL)
     * @param backend Backend to use for capture
     * @param frame Output frame
     * @param requestedFormat Optional requested capture format (FourCC/pixel format)
     * @return true if frame was successfully acquired
     */
    bool acquireFrame(const std::string& sourceId, CameraBackend backend, cv::Mat& frame, const std::string& requestedFormat = "");

#ifdef VISIONPIPE_LIBCAMERA_ENABLED
    /**
     * @brief Set configuration for a libcamera source.
     * Must be called before the camera is opened (or will take effect on next open).
     */
    void setLibCameraConfig(const std::string& sourceId, const LibCameraConfig& config);

    /**
     * @brief Set a control value for a libcamera device.
     * 
     * @param sourceId Camera identifier
     * @param controlName Name of the control (e.g., "ExposureValue")
     * @param value Control value (float)
     * @return true if successful
     */
    bool setLibCameraControl(const std::string& sourceId, const std::string& controlName, float value);
#endif

    /**
     * @brief Get the OpenCV VideoCapture handle for a source.
     * Returns nullptr if not opened or using libcamera backend.
     */
    std::shared_ptr<cv::VideoCapture> getOpenCVCapture(const std::string& sourceId);

    /**
     * @brief Release a specific camera.
     */
    void releaseCamera(const std::string& sourceId);

    /**
     * @brief Release all cameras.
     */
    void releaseAll();

    /**
     * @brief Check if a camera is currently open.
     */
    bool isOpen(const std::string& sourceId) const;

    /**
     * @brief Get the backend type for an open camera.
     */
    CameraBackend getBackend(const std::string& sourceId) const;

    /**
     * @brief Parse backend string to enum.
     */
    static CameraBackend parseBackend(const std::string& backendStr);

    /**
     * @brief Convert OpenCV CAP_* constant to CameraBackend.
     */
    static CameraBackend fromOpenCVBackend(int cvBackend);

#ifdef VISIONPIPE_LIBCAMERA_ENABLED
    /**
     * @brief Get libcamera CameraManager instance.
     */
    libcamera::CameraManager* getLibCameraManager();

    /**
     * @brief Get libcamera Camera handle for a source.
     */
    std::shared_ptr<libcamera::Camera> getLibCamera(const std::string& sourceId);
#endif

private:
    CameraDeviceManager();
    ~CameraDeviceManager();

    struct CameraSession {
        CameraSession() : frameCond(std::make_unique<std::condition_variable>()) {}

        CameraBackend backend;
        std::shared_ptr<cv::VideoCapture> opencvCapture;
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
        std::shared_ptr<libcamera::Camera> libcameraDevice;
        std::unique_ptr<libcamera::CameraConfiguration> config;
        std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
        std::vector<std::unique_ptr<libcamera::Request>> requests;
        cv::Mat latestFrame;
        bool frameReady = false;
        std::unique_ptr<std::condition_variable> frameCond;
        LibCameraConfig targetConfig;
        std::map<std::string, float> activeControls;
#endif
    };

    bool openCamera(const std::string& sourceId, CameraBackend backend, const std::string& requestedFormat = "");
    bool readOpenCVFrame(CameraSession& session, cv::Mat& frame);

#ifdef VISIONPIPE_LIBCAMERA_ENABLED
    bool openLibCamera(const std::string& sourceId, CameraSession& session);
    // Updated to take unique_lock for condition_variable
    bool readLibCameraFrame(CameraSession& session, cv::Mat& frame, std::unique_lock<std::mutex>& lock);

    static cv::Mat unpackRAW10CSI2P(const uint8_t* packed, int width, int height, size_t stride);

    static cv::Mat unpackRAW12CSI2P(const uint8_t* packed, int width, int height, size_t stride);
    
    void libcameraRequestComplete(libcamera::Request* request);
    
    std::unique_ptr<libcamera::CameraManager> _libcameraManager;
    bool _libcameraManagerStarted = false;
#endif

    mutable std::mutex _mutex;
    std::map<std::string, CameraSession> _sessions;
};

} // namespace visionpipe

#endif // VISIONPIPE_CAMERA_DEVICE_MANAGER_H

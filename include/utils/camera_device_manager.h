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

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
#include "utils/v4l2_device_manager.h"
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
    LIBCAMERA,
    V4L2_NATIVE
};

/**
 * @brief Configuration for libcamera backend
 */
struct LibCameraConfig {
    int width = 640;
    int height = 480;
    std::string pixelFormat = "BGR888"; 
    int bufferCount = 4;
    std::string streamRole = "VideoRecording"; // Default to VideoRecording
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
     * @brief Get the default (unnamed) singleton instance.
     */
    static CameraDeviceManager& instance();

    /**
     * @brief Get or create a named device manager instance.
     * Each named manager has its own mutex, sessions, and (if enabled) its own
     * libcamera::CameraManager, so cameras bound to different managers can run
     * with zero lock contention between them.
     */
    static CameraDeviceManager& instance(const std::string& managerId);

    /**
     * @brief Get the device manager bound to a specific camera source.
     * Returns the default instance if no explicit binding exists.
     * Use bindSource() to associate a source with a named manager.
     */
    static CameraDeviceManager& forSource(const std::string& sourceId);

    /**
     * @brief Bind a camera source to a named manager.
     * Creates the named manager if it does not already exist.
     * All subsequent forSource() calls for this sourceId will return that manager.
     */
    static void bindSource(const std::string& sourceId, const std::string& managerId);

    /**
     * @brief Release all named managers and source bindings, plus the default
     * instance's cameras.  Call at program shutdown.
     */
    static void releaseAllManagers();

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

    /**
     * @brief Proactively prepare (open/configure) a camera without starting the stream.
     * Useful for multi-camera setups to avoid hardware link race conditions.
     */
    bool prepareCamera(const std::string& sourceId, CameraBackend backend, const std::string& requestedFormat = "");

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

    /**
     * @brief Get the Bayer pattern string for a libcamera source.
     * Returns empty string if not a Bayer format or source not found.
     */
    std::string getBayerPattern(const std::string& sourceId);

    /**
     * @brief List supported pixel formats and resolutions for a libcamera source.
     */
    void listLibCameraFormats(const std::string& sourceId);

    /**
     * @brief Print detailed debug information about a libcamera session configuration.
     */
    void debugLibCameraConfig(const std::string& sourceId);
#endif

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
    /**
     * @brief Get the V4L2DeviceManager owned by this CameraDeviceManager instance.
     * Each CameraDeviceManager has its own V4L2DeviceManager, so cameras bound
     * to separate managers have fully independent V4L2 sessions and locks.
     */
    V4L2DeviceManager& getV4L2DeviceManager();

    /**
     * @brief Set V4L2 native configuration for a source and open/reconfigure the device.
     * Returns true if the device was successfully opened with the given config.
     */
    bool setV4L2NativeConfig(const std::string& sourceId, const V4L2NativeConfig& config);

    /**
     * @brief Set a V4L2 native control by name.
     */
    bool setV4L2Control(const std::string& sourceId, const std::string& controlName, int value);

    /**
     * @brief Get the Bayer pattern for a V4L2 native source.
     */
    std::string getV4L2BayerPattern(const std::string& sourceId);

    /**
     * @brief List all V4L2 controls for a source.
     */
    void listV4L2Controls(const std::string& sourceId);

    /**
     * @brief List all V4L2 formats for a source.
     */
    void listV4L2Formats(const std::string& sourceId);
#endif



private:
    CameraDeviceManager();
    ~CameraDeviceManager();

    // Allow std::unique_ptr / std::default_delete to call the private destructor
    // (needed for the named-manager registry stored in unique_ptr).
    friend struct std::default_delete<CameraDeviceManager>;

    // --- Static multi-instance registry ---
    static std::mutex _registryMutex;
    static std::map<std::string, std::unique_ptr<CameraDeviceManager>> _namedManagers;
    static std::map<std::string, std::string> _sourceToManager; // sourceId -> managerId

    struct CameraSession {
        CameraSession() 
            : backend(CameraBackend::OPENCV_AUTO) {}

        CameraBackend backend;
        std::shared_ptr<cv::VideoCapture> opencvCapture;

        // Per-session read mutex: held during frame acquisition so that
        // different devices can read truly in parallel (the global _mutex is
        // released before entering the actual read call; see acquireFrame).
        std::unique_ptr<std::mutex> readMutex = std::make_unique<std::mutex>();
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
        std::shared_ptr<libcamera::Camera> libcameraDevice;
        std::unique_ptr<libcamera::CameraConfiguration> config;
        std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
        std::vector<std::unique_ptr<libcamera::Request>> requests;
        cv::Mat latestFrame;
        bool frameReady = false;
        bool libcameraStarted = false;
        std::unique_ptr<std::mutex> sessionMutex = std::make_unique<std::mutex>();
        std::unique_ptr<std::condition_variable> frameCond = std::make_unique<std::condition_variable>();
        LibCameraConfig targetConfig;
        std::map<std::string, float> activeControls;
        struct MappedBuffer {
            void* data;
            size_t length;
        };
        std::map<libcamera::FrameBuffer*, MappedBuffer> mappedBuffers;
        
        // Diagnostics
        int framesCaptured = 0;
        bool allZeros = false;
#endif
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
        V4L2NativeConfig v4l2Config;  // Config set by v4l2_setup; used by openCamera on first open
#endif
    };

    bool openCamera(const std::string& sourceId, CameraBackend backend, const std::string& requestedFormat = "", bool startImmediate = true);
    bool readOpenCVFrame(CameraSession& session, cv::Mat& frame);

#ifdef VISIONPIPE_LIBCAMERA_ENABLED
    bool openLibCamera(const std::string& sourceId, CameraSession& session, bool startImmediate = true);
    // Updated to take unique_lock for condition_variable
    bool readLibCameraFrame(CameraSession& session, cv::Mat& frame, std::unique_lock<std::mutex>& lock);
    void applyLibCameraControls(CameraSession& session, libcamera::Request* request);

    cv::Mat unpackRAW10CSI2P(const uint8_t* packed, int width, int height, size_t stride);
    
    cv::Mat unpackRAW12CSI2P(const uint8_t* packed, int width, int height, size_t stride);

    
    void libcameraRequestComplete(libcamera::Request* request);
    
    // Shared across all CameraDeviceManager instances (libcamera enforces a
    // single CameraManager per process).
    static std::shared_ptr<libcamera::CameraManager> _sharedLibcameraManager;
    static std::mutex _sharedLibcameraMutex;
    static bool _sharedLibcameraStarted;

    std::map<libcamera::Request*, std::string> _requestToSource;
#endif

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
    std::unique_ptr<V4L2DeviceManager> _v4l2Manager;
#endif

    mutable std::recursive_mutex _mutex;
    std::map<std::string, CameraSession> _sessions;
};

} // namespace visionpipe

#endif // VISIONPIPE_CAMERA_DEVICE_MANAGER_H

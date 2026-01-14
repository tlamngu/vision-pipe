#ifndef VISIONPIPE_VIDEO_IO_ITEMS_H
#define VISIONPIPE_VIDEO_IO_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <map>
#include <memory>

namespace visionpipe {

void registerVideoIOItems(ItemRegistry& registry);

// ============================================================================
// Video Capture
// ============================================================================

/**
 * @brief Captures video from a source (camera, file, or URL)
 * 
 * Parameters:
 * - source: Device index (0,1,2...), file path, or URL (RTSP/HTTP)
 * - api: (optional) Backend API (CAP_ANY, CAP_DSHOW, CAP_V4L2, CAP_FFMPEG)
 * - width: (optional) Frame width
 * - height: (optional) Frame height
 * - fps: (optional) Frame rate
 * - buffer_size: (optional) Buffer size for streaming
 */
class VideoCaptureItem : public InterpreterItem {
public:
    VideoCaptureItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    std::map<std::string, std::shared_ptr<cv::VideoCapture>> _captures;
};

/**
 * @brief Writes frames to a video file
 * 
 * Parameters:
 * - path: Output file path
 * - fourcc: Codec FourCC code (e.g., "MJPG", "XVID", "H264", "mp4v")
 * - fps: Frame rate (default: 30)
 * - width: Frame width (default: from input)
 * - height: Frame height (default: from input)
 * - is_color: Color flag (default: true)
 */
class VideoWriteItem : public InterpreterItem {
public:
    VideoWriteItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
    
private:
    std::map<std::string, std::shared_ptr<cv::VideoWriter>> _writers;
};

/**
 * @brief Closes a video writer and releases resources
 */
class VideoCloseItem : public InterpreterItem {
public:
    VideoCloseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Image I/O
// ============================================================================

/**
 * @brief Saves the current frame to an image file
 * 
 * Parameters:
 * - path: Output file path
 * - quality: JPEG quality 0-100 (default: 95)
 * - compression: PNG compression 0-9 (default: 3)
 * - optimize: Optimize for size (default: false)
 */
class SaveImageItem : public InterpreterItem {
public:
    SaveImageItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Loads an image from a file
 * 
 * Parameters:
 * - path: Image file path
 * - mode: Load mode ("color", "grayscale", "unchanged", "anydepth", "anycolor")
 */
class LoadImageItem : public InterpreterItem {
public:
    LoadImageItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Decodes image from memory buffer
 */
class ImDecodeItem : public InterpreterItem {
public:
    ImDecodeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Encodes image to memory buffer
 */
class ImEncodeItem : public InterpreterItem {
public:
    ImEncodeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Video Properties
// ============================================================================

/**
 * @brief Gets video capture properties
 * 
 * Parameters:
 * - source: Video source identifier
 * - property: Property name ("width", "height", "fps", "frame_count", "pos_frames", etc.)
 */
class GetVideoPropItem : public InterpreterItem {
public:
    GetVideoPropItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets video capture properties
 */
class SetVideoPropItem : public InterpreterItem {
public:
    SetVideoPropItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Seeks to a specific frame in video
 */
class VideoSeekItem : public InterpreterItem {
public:
    VideoSeekItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Advanced Camera Controls
// ============================================================================

/**
 * @brief Sets camera exposure settings
 * 
 * Parameters:
 * - source: Camera source identifier
 * - mode: Exposure mode ("manual", "auto", "aperture_priority", "shutter_priority")
 * - value: Exposure value (manual mode: -13 to 13, auto: 0.25 = 25%)
 */
class SetCameraExposureItem : public InterpreterItem {
public:
    SetCameraExposureItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets camera white balance
 * 
 * Parameters:
 * - source: Camera source identifier
 * - mode: WB mode ("auto", "manual", "daylight", "cloudy", "tungsten", "fluorescent")
 * - temperature: Color temperature in Kelvin (manual mode only, 2000-10000)
 */
class SetCameraWhiteBalanceItem : public InterpreterItem {
public:
    SetCameraWhiteBalanceItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets camera focus settings
 * 
 * Parameters:
 * - source: Camera source identifier
 * - mode: Focus mode ("auto", "manual", "continuous", "infinity", "macro")
 * - value: Focus value (0-255, manual mode only)
 */
class SetCameraFocusItem : public InterpreterItem {
public:
    SetCameraFocusItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets camera ISO/gain
 * 
 * Parameters:
 * - source: Camera source identifier
 * - mode: ISO mode ("auto", "manual")
 * - value: ISO value (100-6400, manual mode) or gain (0-100)
 */
class SetCameraISOItem : public InterpreterItem {
public:
    SetCameraISOItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets camera zoom and pan/tilt
 * 
 * Parameters:
 * - source: Camera source identifier
 * - zoom: Zoom level (1.0-10.0)
 * - pan: Pan angle (-180 to 180 degrees, optional)
 * - tilt: Tilt angle (-90 to 90 degrees, optional)
 */
class SetCameraZoomItem : public InterpreterItem {
public:
    SetCameraZoomItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets camera image adjustment properties
 * 
 * Parameters:
 * - source: Camera source identifier
 * - brightness: Brightness (-100 to 100, default: 0)
 * - contrast: Contrast (-100 to 100, default: 0)
 * - saturation: Saturation (-100 to 100, default: 0)
 * - hue: Hue (-180 to 180, default: 0)
 * - sharpness: Sharpness (0-100, default: 50)
 * - gamma: Gamma (50-300, default: 100)
 */
class SetCameraImageAdjustmentItem : public InterpreterItem {
public:
    SetCameraImageAdjustmentItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets camera capabilities and supported properties
 * 
 * Parameters:
 * - source: Camera source identifier
 * 
 * Returns: Stores capability info in cache as "camera_info"
 */
class GetCameraCapabilitiesItem : public InterpreterItem {
public:
    GetCameraCapabilitiesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Lists all available camera devices
 * 
 * Returns: List of camera indices with basic info
 */
class ListCamerasItem : public InterpreterItem {
public:
    ListCamerasItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets camera backend/API preferences
 * 
 * Parameters:
 * - source: Camera source identifier
 * - backend: Backend API ("dshow", "msmf", "v4l2", "avfoundation", "gstreamer", "ffmpeg")
 * - buffer_size: Internal buffer size (1-100 frames, default: 4)
 */
class SetCameraBackendItem : public InterpreterItem {
public:
    SetCameraBackendItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets camera trigger and strobe controls
 * 
 * Parameters:
 * - source: Camera source identifier
 * - trigger_mode: Trigger mode ("off", "software", "hardware")
 * - trigger_delay: Delay in microseconds (0-1000000)
 * - strobe_enabled: Enable strobe/flash (default: false)
 */
class SetCameraTriggerItem : public InterpreterItem {
public:
    SetCameraTriggerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Saves camera settings profile to file
 * 
 * Parameters:
 * - source: Camera source identifier
 * - path: Profile file path (.json or .xml)
 */
class SaveCameraProfileItem : public InterpreterItem {
public:
    SaveCameraProfileItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Loads camera settings profile from file
 * 
 * Parameters:
 * - source: Camera source identifier
 * - path: Profile file path (.json or .xml)
 */
class LoadCameraProfileItem : public InterpreterItem {
public:
    LoadCameraProfileItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_VIDEO_IO_ITEMS_H

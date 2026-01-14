#include "interpreter/items/video_io_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <map>

namespace visionpipe {

// Static storage for video captures and writers
static std::map<std::string, std::shared_ptr<cv::VideoCapture>> s_captures;
static std::map<std::string, std::shared_ptr<cv::VideoWriter>> s_writers;

void registerVideoIOItems(ItemRegistry& registry) {
    registry.add<VideoCaptureItem>();
    registry.add<VideoWriteItem>();
    registry.add<VideoCloseItem>();
    registry.add<SaveImageItem>();
    registry.add<LoadImageItem>();
    registry.add<ImDecodeItem>();
    registry.add<ImEncodeItem>();
    registry.add<GetVideoPropItem>();
    registry.add<SetVideoPropItem>();
    registry.add<VideoSeekItem>();
    
    // Advanced camera controls
    registry.add<SetCameraExposureItem>();
    registry.add<SetCameraWhiteBalanceItem>();
    registry.add<SetCameraFocusItem>();
    registry.add<SetCameraISOItem>();
    registry.add<SetCameraZoomItem>();
    registry.add<SetCameraImageAdjustmentItem>();
    registry.add<GetCameraCapabilitiesItem>();
    registry.add<ListCamerasItem>();
    registry.add<SetCameraBackendItem>();
    registry.add<SetCameraTriggerItem>();
    registry.add<SaveCameraProfileItem>();
    registry.add<LoadCameraProfileItem>();
}

// ============================================================================
// VideoCaptureItem
// ============================================================================

VideoCaptureItem::VideoCaptureItem() {
    _functionName = "video_cap";
    _description = "Captures video frames from a source (camera, file, or URL)";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::ANY, "Source: device index, file path, or URL"),
        ParamDef::optional("api", BaseType::STRING, "API backend: auto, dshow, v4l2, ffmpeg", "auto")
    };
    _example = "video_cap(0) | video_cap(\"video.mp4\")";
    _returnType = "mat";
    _tags = {"video", "capture", "input", "camera"};
}

ExecutionResult VideoCaptureItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId;
    int apiBackend = cv::CAP_ANY;
    
    if (args[0].isNumeric()) {
        sourceId = std::to_string(static_cast<int>(args[0].asNumber()));
    } else {
        sourceId = args[0].asString();
    }
    
    // Parse API backend
    if (args.size() > 1) {
        std::string api = args[1].asString();
        if (api == "dshow") apiBackend = cv::CAP_DSHOW;
        else if (api == "v4l2") apiBackend = cv::CAP_V4L2;
        else if (api == "ffmpeg") apiBackend = cv::CAP_FFMPEG;
        else if (api == "gstreamer") apiBackend = cv::CAP_GSTREAMER;
    }
    
    // Get or create capture
    if (s_captures.find(sourceId) == s_captures.end()) {
        auto cap = std::make_shared<cv::VideoCapture>();
        
        if (args[0].isNumeric()) {
            cap->open(static_cast<int>(args[0].asNumber()), apiBackend);
        } else {
            cap->open(args[0].asString(), apiBackend);
        }
        
        if (!cap->isOpened()) {
            return ExecutionResult::fail("Failed to open video source: " + sourceId);
        }
        
        s_captures[sourceId] = cap;
    }
    
    cv::Mat frame;
    if (!s_captures[sourceId]->read(frame)) {
        return ExecutionResult::fail("Failed to read frame from: " + sourceId);
    }
    
    return ExecutionResult::ok(frame);
}

// ============================================================================
// VideoWriteItem
// ============================================================================

VideoWriteItem::VideoWriteItem() {
    _functionName = "video_write";
    _description = "Writes frames to a video file";
    _category = "video_io";
    _params = {
        ParamDef::required("filename", BaseType::STRING, "Output video file path"),
        ParamDef::optional("fps", BaseType::FLOAT, "Frames per second", 30.0),
        ParamDef::optional("fourcc", BaseType::STRING, "FourCC codec code", "MJPG"),
        ParamDef::optional("is_color", BaseType::BOOL, "Write as color video", true)
    };
    _example = "video_write(\"output.avi\", 30, \"MJPG\")";
    _returnType = "mat";
    _tags = {"video", "output", "write", "record"};
}

ExecutionResult VideoWriteItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string filename = args[0].asString();
    double fps = args.size() > 1 ? args[1].asNumber() : 30.0;
    std::string fourcc = args.size() > 2 ? args[2].asString() : "MJPG";
    bool isColor = args.size() > 3 ? args[3].asBool() : true;
    
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("No frame to write");
    }
    
    // Ensure fourcc is at least 4 characters
    while (fourcc.length() < 4) fourcc += ' ';
    
    // Get or create writer
    if (s_writers.find(filename) == s_writers.end()) {
        auto writer = std::make_shared<cv::VideoWriter>();
        
        int codecCode = cv::VideoWriter::fourcc(
            fourcc[0], fourcc[1], fourcc[2], fourcc[3]
        );
        
        writer->open(filename, codecCode, fps, ctx.currentMat.size(), isColor);
        
        if (!writer->isOpened()) {
            return ExecutionResult::fail("Failed to open video writer: " + filename);
        }
        
        s_writers[filename] = writer;
    }
    
    s_writers[filename]->write(ctx.currentMat);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// VideoCloseItem
// ============================================================================

VideoCloseItem::VideoCloseItem() {
    _functionName = "video_close";
    _description = "Closes a video capture or writer";
    _category = "video_io";
    _params = {
        ParamDef::optional("source", BaseType::STRING, "Source identifier to close (empty = all)", "")
    };
    _example = "video_close(\"0\") | video_close()";
    _returnType = "void";
    _tags = {"video", "close", "release"};
}

ExecutionResult VideoCloseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (args.empty() || args[0].asString().empty()) {
        // Close all
        s_captures.clear();
        s_writers.clear();
    } else {
        std::string id = args[0].asString();
        s_captures.erase(id);
        s_writers.erase(id);
    }
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SaveImageItem
// ============================================================================

SaveImageItem::SaveImageItem() {
    _functionName = "save_image";
    _description = "Saves the current frame to an image file";
    _category = "video_io";
    _params = {
        ParamDef::required("filename", BaseType::STRING, "Output image file path"),
        ParamDef::optional("quality", BaseType::INT, "JPEG quality (0-100) or PNG compression (0-9)", 95)
    };
    _example = "save_image(\"frame.png\") | save_image(\"frame.jpg\", 90)";
    _returnType = "mat";
    _tags = {"image", "output", "save", "write"};
}

ExecutionResult SaveImageItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string filename = args[0].asString();
    
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("No frame to save");
    }
    
    std::vector<int> params;
    if (args.size() > 1) {
        int quality = static_cast<int>(args[1].asNumber());
        // Detect format from extension
        if (filename.find(".jpg") != std::string::npos || 
            filename.find(".jpeg") != std::string::npos) {
            params = {cv::IMWRITE_JPEG_QUALITY, quality};
        } else if (filename.find(".png") != std::string::npos) {
            params = {cv::IMWRITE_PNG_COMPRESSION, std::min(9, quality / 10)};
        } else if (filename.find(".webp") != std::string::npos) {
            params = {cv::IMWRITE_WEBP_QUALITY, quality};
        }
    }
    
    if (!cv::imwrite(filename, ctx.currentMat, params)) {
        return ExecutionResult::fail("Failed to save image: " + filename);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LoadImageItem
// ============================================================================

LoadImageItem::LoadImageItem() {
    _functionName = "load_image";
    _description = "Loads an image from a file";
    _category = "video_io";
    _params = {
        ParamDef::required("filename", BaseType::STRING, "Input image file path"),
        ParamDef::optional("mode", BaseType::STRING, "Load mode: color, grayscale, unchanged, anydepth, anycolor", "color")
    };
    _example = "load_image(\"input.png\") | load_image(\"depth.tiff\", \"unchanged\")";
    _returnType = "mat";
    _tags = {"image", "input", "load", "read"};
}

ExecutionResult LoadImageItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string filename = args[0].asString();
    
    int flags = cv::IMREAD_COLOR;
    if (args.size() > 1) {
        std::string mode = args[1].asString();
        if (mode == "grayscale" || mode == "gray") flags = cv::IMREAD_GRAYSCALE;
        else if (mode == "unchanged") flags = cv::IMREAD_UNCHANGED;
        else if (mode == "anydepth") flags = cv::IMREAD_ANYDEPTH;
        else if (mode == "anycolor") flags = cv::IMREAD_ANYCOLOR;
        else if (mode == "reduced_grayscale_2") flags = cv::IMREAD_REDUCED_GRAYSCALE_2;
        else if (mode == "reduced_color_2") flags = cv::IMREAD_REDUCED_COLOR_2;
        else if (mode == "reduced_grayscale_4") flags = cv::IMREAD_REDUCED_GRAYSCALE_4;
        else if (mode == "reduced_color_4") flags = cv::IMREAD_REDUCED_COLOR_4;
        else if (mode == "reduced_grayscale_8") flags = cv::IMREAD_REDUCED_GRAYSCALE_8;
        else if (mode == "reduced_color_8") flags = cv::IMREAD_REDUCED_COLOR_8;
    }
    
    cv::Mat image = cv::imread(filename, flags);
    if (image.empty()) {
        return ExecutionResult::fail("Failed to load image: " + filename);
    }
    
    return ExecutionResult::ok(image);
}

// ============================================================================
// ImDecodeItem
// ============================================================================

ImDecodeItem::ImDecodeItem() {
    _functionName = "imdecode";
    _description = "Decodes image from memory buffer";
    _category = "video_io";
    _params = {
        ParamDef::required("buffer", BaseType::ARRAY, "Image data buffer"),
        ParamDef::optional("mode", BaseType::STRING, "Decode mode: color, grayscale, unchanged", "color")
    };
    _example = "imdecode(buffer, \"color\")";
    _returnType = "mat";
    _tags = {"image", "decode", "memory"};
}

ExecutionResult ImDecodeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // This requires buffer handling - simplified version
    int flags = cv::IMREAD_COLOR;
    if (args.size() > 1) {
        std::string mode = args[1].asString();
        if (mode == "grayscale") flags = cv::IMREAD_GRAYSCALE;
        else if (mode == "unchanged") flags = cv::IMREAD_UNCHANGED;
    }
    
    // For now, this is a placeholder - actual buffer handling would need 
    // proper RuntimeValue array support
    return ExecutionResult::fail("imdecode requires buffer support - use load_image for files");
}

// ============================================================================
// ImEncodeItem
// ============================================================================

ImEncodeItem::ImEncodeItem() {
    _functionName = "imencode";
    _description = "Encodes image to memory buffer";
    _category = "video_io";
    _params = {
        ParamDef::required("ext", BaseType::STRING, "Image format extension (.jpg, .png, etc.)"),
        ParamDef::optional("quality", BaseType::INT, "Encoding quality", 95)
    };
    _example = "imencode(\".jpg\", 90)";
    _returnType = "array";
    _tags = {"image", "encode", "memory"};
}

ExecutionResult ImEncodeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string ext = args[0].asString();
    
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("No image to encode");
    }
    
    std::vector<int> params;
    if (args.size() > 1) {
        int quality = static_cast<int>(args[1].asNumber());
        if (ext == ".jpg" || ext == ".jpeg") {
            params = {cv::IMWRITE_JPEG_QUALITY, quality};
        } else if (ext == ".png") {
            params = {cv::IMWRITE_PNG_COMPRESSION, std::min(9, quality / 10)};
        }
    }
    
    std::vector<uchar> buffer;
    if (!cv::imencode(ext, ctx.currentMat, buffer, params)) {
        return ExecutionResult::fail("Failed to encode image");
    }
    
    // Return current mat unchanged (buffer size could be stored in variable)
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetVideoPropItem
// ============================================================================

GetVideoPropItem::GetVideoPropItem() {
    _functionName = "get_video_prop";
    _description = "Gets a video capture property";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Video source identifier"),
        ParamDef::required("prop", BaseType::STRING, 
            "Property: width, height, fps, frame_count, pos_frames, pos_msec, pos_avi_ratio, fourcc, format, brightness, contrast, saturation, hue, gain, exposure, auto_exposure, focus, autofocus, zoom")
    };
    _example = "get_video_prop(\"0\", \"width\")";
    _returnType = "float";
    _tags = {"video", "property", "get"};
}

ExecutionResult GetVideoPropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string prop = args[1].asString();
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Video source not found: " + sourceId);
    }
    
    int propId = -1;
    if (prop == "width") propId = cv::CAP_PROP_FRAME_WIDTH;
    else if (prop == "height") propId = cv::CAP_PROP_FRAME_HEIGHT;
    else if (prop == "fps") propId = cv::CAP_PROP_FPS;
    else if (prop == "frame_count") propId = cv::CAP_PROP_FRAME_COUNT;
    else if (prop == "pos_frames") propId = cv::CAP_PROP_POS_FRAMES;
    else if (prop == "pos_msec") propId = cv::CAP_PROP_POS_MSEC;
    else if (prop == "pos_avi_ratio") propId = cv::CAP_PROP_POS_AVI_RATIO;
    else if (prop == "fourcc") propId = cv::CAP_PROP_FOURCC;
    else if (prop == "format") propId = cv::CAP_PROP_FORMAT;
    else if (prop == "brightness") propId = cv::CAP_PROP_BRIGHTNESS;
    else if (prop == "contrast") propId = cv::CAP_PROP_CONTRAST;
    else if (prop == "saturation") propId = cv::CAP_PROP_SATURATION;
    else if (prop == "hue") propId = cv::CAP_PROP_HUE;
    else if (prop == "gain") propId = cv::CAP_PROP_GAIN;
    else if (prop == "exposure") propId = cv::CAP_PROP_EXPOSURE;
    else if (prop == "auto_exposure") propId = cv::CAP_PROP_AUTO_EXPOSURE;
    else if (prop == "focus") propId = cv::CAP_PROP_FOCUS;
    else if (prop == "autofocus") propId = cv::CAP_PROP_AUTOFOCUS;
    else if (prop == "zoom") propId = cv::CAP_PROP_ZOOM;
    else {
        return ExecutionResult::fail("Unknown video property: " + prop);
    }
    
    double value = it->second->get(propId);
    // Store value in context variable and return current mat
    ctx.variables["_video_prop"] = RuntimeValue(value);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetVideoPropItem
// ============================================================================

SetVideoPropItem::SetVideoPropItem() {
    _functionName = "set_video_prop";
    _description = "Sets a video capture property";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Video source identifier"),
        ParamDef::required("prop", BaseType::STRING, "Property name"),
        ParamDef::required("value", BaseType::FLOAT, "Property value")
    };
    _example = "set_video_prop(\"0\", \"exposure\", -5)";
    _returnType = "bool";
    _tags = {"video", "property", "set"};
}

ExecutionResult SetVideoPropItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string prop = args[1].asString();
    double value = args[2].asNumber();
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Video source not found: " + sourceId);
    }
    
    int propId = -1;
    if (prop == "width") propId = cv::CAP_PROP_FRAME_WIDTH;
    else if (prop == "height") propId = cv::CAP_PROP_FRAME_HEIGHT;
    else if (prop == "fps") propId = cv::CAP_PROP_FPS;
    else if (prop == "pos_frames") propId = cv::CAP_PROP_POS_FRAMES;
    else if (prop == "pos_msec") propId = cv::CAP_PROP_POS_MSEC;
    else if (prop == "brightness") propId = cv::CAP_PROP_BRIGHTNESS;
    else if (prop == "contrast") propId = cv::CAP_PROP_CONTRAST;
    else if (prop == "saturation") propId = cv::CAP_PROP_SATURATION;
    else if (prop == "hue") propId = cv::CAP_PROP_HUE;
    else if (prop == "gain") propId = cv::CAP_PROP_GAIN;
    else if (prop == "exposure") propId = cv::CAP_PROP_EXPOSURE;
    else if (prop == "auto_exposure") propId = cv::CAP_PROP_AUTO_EXPOSURE;
    else if (prop == "focus") propId = cv::CAP_PROP_FOCUS;
    else if (prop == "autofocus") propId = cv::CAP_PROP_AUTOFOCUS;
    else if (prop == "zoom") propId = cv::CAP_PROP_ZOOM;
    else {
        return ExecutionResult::fail("Unknown video property: " + prop);
    }
    
    bool success = it->second->set(propId, value);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// VideoSeekItem
// ============================================================================

VideoSeekItem::VideoSeekItem() {
    _functionName = "video_seek";
    _description = "Seeks to a specific position in a video file";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Video source identifier"),
        ParamDef::required("position", BaseType::FLOAT, "Position (frame number or msec)"),
        ParamDef::optional("unit", BaseType::STRING, "Position unit: frames, msec, ratio", "frames")
    };
    _example = "video_seek(\"video.mp4\", 100, \"frames\")";
    _returnType = "bool";
    _tags = {"video", "seek", "position"};
}

ExecutionResult VideoSeekItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    double position = args[1].asNumber();
    std::string unit = args.size() > 2 ? args[2].asString() : "frames";
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Video source not found: " + sourceId);
    }
    
    int propId = cv::CAP_PROP_POS_FRAMES;
    if (unit == "msec") propId = cv::CAP_PROP_POS_MSEC;
    else if (unit == "ratio") propId = cv::CAP_PROP_POS_AVI_RATIO;
    
    bool success = it->second->set(propId, position);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraExposureItem
// ============================================================================

SetCameraExposureItem::SetCameraExposureItem() {
    _functionName = "set_camera_exposure";
    _description = "Sets camera exposure mode and value";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("mode", BaseType::STRING, "Exposure mode: manual, auto, aperture_priority, shutter_priority", "auto"),
        ParamDef::optional("value", BaseType::FLOAT, "Exposure value (-13 to 13 for manual, 0-1 for auto)", 0.0)
    };
    _example = "set_camera_exposure(\"0\", \"manual\", -5)";
    _returnType = "mat";
    _tags = {"camera", "exposure", "control"};
}

ExecutionResult SetCameraExposureItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string mode = args.size() > 1 ? args[1].asString() : "auto";
    double value = args.size() > 2 ? args[2].asNumber() : 0.0;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    // Set auto exposure mode
    double autoExpMode = (mode == "manual") ? 0.25 : 0.75; // 0.25 = manual, 0.75 = auto
    if (mode == "aperture_priority") autoExpMode = 0.5;
    else if (mode == "shutter_priority") autoExpMode = 0.5;
    
    it->second->set(cv::CAP_PROP_AUTO_EXPOSURE, autoExpMode);
    
    // Set exposure value if in manual mode
    if (mode == "manual") {
        it->second->set(cv::CAP_PROP_EXPOSURE, value);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraWhiteBalanceItem
// ============================================================================

SetCameraWhiteBalanceItem::SetCameraWhiteBalanceItem() {
    _functionName = "set_camera_white_balance";
    _description = "Sets camera white balance mode and temperature";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("mode", BaseType::STRING, "WB mode: auto, manual, daylight, cloudy, tungsten, fluorescent", "auto"),
        ParamDef::optional("temperature", BaseType::INT, "Color temperature (2000-10000K)", 5500)
    };
    _example = "set_camera_white_balance(\"0\", \"manual\", 6500)";
    _returnType = "mat";
    _tags = {"camera", "white_balance", "control"};
}

ExecutionResult SetCameraWhiteBalanceItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string mode = args.size() > 1 ? args[1].asString() : "auto";
    int temperature = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 5500;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    // Set auto white balance
    double autoWB = (mode == "auto") ? 1.0 : 0.0;
    it->second->set(cv::CAP_PROP_AUTO_WB, autoWB);
    
    // Set manual temperature
    if (mode == "manual") {
        it->second->set(cv::CAP_PROP_WB_TEMPERATURE, temperature);
    } else if (mode == "daylight") {
        it->second->set(cv::CAP_PROP_WB_TEMPERATURE, 5500);
    } else if (mode == "cloudy") {
        it->second->set(cv::CAP_PROP_WB_TEMPERATURE, 6500);
    } else if (mode == "tungsten") {
        it->second->set(cv::CAP_PROP_WB_TEMPERATURE, 3200);
    } else if (mode == "fluorescent") {
        it->second->set(cv::CAP_PROP_WB_TEMPERATURE, 4000);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraFocusItem
// ============================================================================

SetCameraFocusItem::SetCameraFocusItem() {
    _functionName = "set_camera_focus";
    _description = "Sets camera focus mode and value";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("mode", BaseType::STRING, "Focus mode: auto, manual, continuous, infinity, macro", "auto"),
        ParamDef::optional("value", BaseType::INT, "Focus value (0-255, manual mode only)", 128)
    };
    _example = "set_camera_focus(\"0\", \"manual\", 200)";
    _returnType = "mat";
    _tags = {"camera", "focus", "control"};
}

ExecutionResult SetCameraFocusItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string mode = args.size() > 1 ? args[1].asString() : "auto";
    int value = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 128;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    // Set autofocus
    double autoFocus = (mode == "auto" || mode == "continuous") ? 1.0 : 0.0;
    it->second->set(cv::CAP_PROP_AUTOFOCUS, autoFocus);
    
    // Set manual focus value
    if (mode == "manual") {
        it->second->set(cv::CAP_PROP_FOCUS, value);
    } else if (mode == "infinity") {
        it->second->set(cv::CAP_PROP_FOCUS, 255);
    } else if (mode == "macro") {
        it->second->set(cv::CAP_PROP_FOCUS, 0);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraISOItem
// ============================================================================

SetCameraISOItem::SetCameraISOItem() {
    _functionName = "set_camera_iso";
    _description = "Sets camera ISO/gain value";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("mode", BaseType::STRING, "ISO mode: auto, manual", "auto"),
        ParamDef::optional("value", BaseType::INT, "ISO value (100-6400) or gain (0-100)", 400)
    };
    _example = "set_camera_iso(\"0\", \"manual\", 800)";
    _returnType = "mat";
    _tags = {"camera", "iso", "gain", "control"};
}

ExecutionResult SetCameraISOItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string mode = args.size() > 1 ? args[1].asString() : "auto";
    int value = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 400;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    // Try setting ISO if supported
    if (mode == "manual") {
        it->second->set(cv::CAP_PROP_ISO_SPEED, value);
        // Also set gain as normalized value (ISO/100)
        double gain = value / 100.0;
        it->second->set(cv::CAP_PROP_GAIN, gain);
    } else {
        // Enable auto gain
        it->second->set(cv::CAP_PROP_GAIN, 0);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraZoomItem
// ============================================================================

SetCameraZoomItem::SetCameraZoomItem() {
    _functionName = "set_camera_zoom";
    _description = "Sets camera zoom, pan, and tilt";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("zoom", BaseType::FLOAT, "Zoom level (1.0-10.0)", 1.0),
        ParamDef::optional("pan", BaseType::FLOAT, "Pan angle (-180 to 180)", 0.0),
        ParamDef::optional("tilt", BaseType::FLOAT, "Tilt angle (-90 to 90)", 0.0)
    };
    _example = "set_camera_zoom(\"0\", 2.5, 30, 0)";
    _returnType = "mat";
    _tags = {"camera", "zoom", "pan", "tilt", "control"};
}

ExecutionResult SetCameraZoomItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    double zoom = args.size() > 1 ? args[1].asNumber() : 1.0;
    double pan = args.size() > 2 ? args[2].asNumber() : 0.0;
    double tilt = args.size() > 3 ? args[3].asNumber() : 0.0;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    it->second->set(cv::CAP_PROP_ZOOM, zoom);
    it->second->set(cv::CAP_PROP_PAN, pan);
    it->second->set(cv::CAP_PROP_TILT, tilt);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraImageAdjustmentItem
// ============================================================================

SetCameraImageAdjustmentItem::SetCameraImageAdjustmentItem() {
    _functionName = "set_camera_image_adjustment";
    _description = "Sets camera image quality parameters";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("brightness", BaseType::FLOAT, "Brightness (-100 to 100)", 0.0),
        ParamDef::optional("contrast", BaseType::FLOAT, "Contrast (-100 to 100)", 0.0),
        ParamDef::optional("saturation", BaseType::FLOAT, "Saturation (-100 to 100)", 0.0),
        ParamDef::optional("hue", BaseType::FLOAT, "Hue (-180 to 180)", 0.0),
        ParamDef::optional("sharpness", BaseType::FLOAT, "Sharpness (0-100)", 50.0),
        ParamDef::optional("gamma", BaseType::FLOAT, "Gamma (50-300)", 100.0)
    };
    _example = "set_camera_image_adjustment(\"0\", 10, 20, 0, 0, 60, 100)";
    _returnType = "mat";
    _tags = {"camera", "brightness", "contrast", "saturation", "control"};
}

ExecutionResult SetCameraImageAdjustmentItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    double brightness = args.size() > 1 ? args[1].asNumber() : 0.0;
    double contrast = args.size() > 2 ? args[2].asNumber() : 0.0;
    double saturation = args.size() > 3 ? args[3].asNumber() : 0.0;
    double hue = args.size() > 4 ? args[4].asNumber() : 0.0;
    double sharpness = args.size() > 5 ? args[5].asNumber() : 50.0;
    double gamma = args.size() > 6 ? args[6].asNumber() : 100.0;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    it->second->set(cv::CAP_PROP_BRIGHTNESS, brightness);
    it->second->set(cv::CAP_PROP_CONTRAST, contrast);
    it->second->set(cv::CAP_PROP_SATURATION, saturation);
    it->second->set(cv::CAP_PROP_HUE, hue);
    it->second->set(cv::CAP_PROP_SHARPNESS, sharpness);
    it->second->set(cv::CAP_PROP_GAMMA, gamma);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetCameraCapabilitiesItem
// ============================================================================

GetCameraCapabilitiesItem::GetCameraCapabilitiesItem() {
    _functionName = "get_camera_capabilities";
    _description = "Gets camera capabilities and supported properties";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier")
    };
    _example = "get_camera_capabilities(\"0\")";
    _returnType = "mat";
    _tags = {"camera", "info", "capabilities"};
}

ExecutionResult GetCameraCapabilitiesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    auto& cap = *it->second;
    
    std::cout << "\n=== Camera Capabilities: " << sourceId << " ===" << std::endl;
    std::cout << "Resolution: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x" 
              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
    std::cout << "FPS: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
    std::cout << "Format/FourCC: " << (int)cap.get(cv::CAP_PROP_FOURCC) << std::endl;
    std::cout << "Backend: " << cap.getBackendName() << std::endl;
    
    std::cout << "\nSupported Properties:" << std::endl;
    std::cout << "  Brightness: " << cap.get(cv::CAP_PROP_BRIGHTNESS) << std::endl;
    std::cout << "  Contrast: " << cap.get(cv::CAP_PROP_CONTRAST) << std::endl;
    std::cout << "  Saturation: " << cap.get(cv::CAP_PROP_SATURATION) << std::endl;
    std::cout << "  Hue: " << cap.get(cv::CAP_PROP_HUE) << std::endl;
    std::cout << "  Gain: " << cap.get(cv::CAP_PROP_GAIN) << std::endl;
    std::cout << "  Exposure: " << cap.get(cv::CAP_PROP_EXPOSURE) << std::endl;
    std::cout << "  Focus: " << cap.get(cv::CAP_PROP_FOCUS) << std::endl;
    std::cout << "  Zoom: " << cap.get(cv::CAP_PROP_ZOOM) << std::endl;
    std::cout << "  Auto Exposure: " << cap.get(cv::CAP_PROP_AUTO_EXPOSURE) << std::endl;
    std::cout << "  Auto Focus: " << cap.get(cv::CAP_PROP_AUTOFOCUS) << std::endl;
    std::cout << "  Auto WB: " << cap.get(cv::CAP_PROP_AUTO_WB) << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ListCamerasItem
// ============================================================================

ListCamerasItem::ListCamerasItem() {
    _functionName = "list_cameras";
    _description = "Lists all available camera devices";
    _category = "video_io";
    _params = {};
    _example = "list_cameras()";
    _returnType = "mat";
    _tags = {"camera", "list", "enumerate"};
}

ExecutionResult ListCamerasItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::cout << "\n=== Available Cameras ===" << std::endl;
    
    // Try to open first 10 camera indices
    for (int i = 0; i < 10; i++) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
            double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
            double fps = cap.get(cv::CAP_PROP_FPS);
            std::string backend = cap.getBackendName();
            
            std::cout << "Camera " << i << ": " 
                      << width << "x" << height << " @ " << fps << " FPS"
                      << " [" << backend << "]" << std::endl;
            cap.release();
        }
    }
    
    std::cout << "========================\n" << std::endl;
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraBackendItem
// ============================================================================

SetCameraBackendItem::SetCameraBackendItem() {
    _functionName = "set_camera_backend";
    _description = "Sets camera backend API and buffer preferences";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("backend", BaseType::STRING, "Backend: dshow, msmf, v4l2, avfoundation, gstreamer, ffmpeg", "auto"),
        ParamDef::optional("buffer_size", BaseType::INT, "Buffer size (1-100 frames)", 4)
    };
    _example = "set_camera_backend(\"0\", \"dshow\", 1)";
    _returnType = "mat";
    _tags = {"camera", "backend", "buffer", "control"};
}

ExecutionResult SetCameraBackendItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string backend = args.size() > 1 ? args[1].asString() : "auto";
    int bufferSize = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 4;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    // Set buffer size
    it->second->set(cv::CAP_PROP_BUFFERSIZE, bufferSize);
    
    std::cout << "Camera backend and buffer settings updated" << std::endl;
    std::cout << "Note: Backend must be set during camera initialization" << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetCameraTriggerItem
// ============================================================================

SetCameraTriggerItem::SetCameraTriggerItem() {
    _functionName = "set_camera_trigger";
    _description = "Sets camera trigger and strobe controls";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::optional("trigger_mode", BaseType::STRING, "Trigger mode: off, software, hardware", "off"),
        ParamDef::optional("trigger_delay", BaseType::INT, "Delay in microseconds (0-1000000)", 0),
        ParamDef::optional("strobe_enabled", BaseType::BOOL, "Enable strobe/flash", false)
    };
    _example = "set_camera_trigger(\"0\", \"software\", 1000, false)";
    _returnType = "mat";
    _tags = {"camera", "trigger", "strobe", "control"};
}

ExecutionResult SetCameraTriggerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string triggerMode = args.size() > 1 ? args[1].asString() : "off";
    int triggerDelay = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;
    bool strobeEnabled = args.size() > 3 ? args[3].asBool() : false;
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    // Set trigger mode
    double triggerModeValue = 0.0; // off
    if (triggerMode == "software") triggerModeValue = 1.0;
    else if (triggerMode == "hardware") triggerModeValue = 2.0;
    
    it->second->set(cv::CAP_PROP_TRIGGER, triggerModeValue);
    it->second->set(cv::CAP_PROP_TRIGGER_DELAY, triggerDelay);
    
    std::cout << "Camera trigger configured: " << triggerMode << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SaveCameraProfileItem
// ============================================================================

SaveCameraProfileItem::SaveCameraProfileItem() {
    _functionName = "save_camera_profile";
    _description = "Saves camera settings to a profile file";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::required("path", BaseType::STRING, "Profile file path (.xml or .json)")
    };
    _example = "save_camera_profile(\"0\", \"camera_profile.xml\")";
    _returnType = "mat";
    _tags = {"camera", "profile", "save", "settings"};
}

ExecutionResult SaveCameraProfileItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string path = args[1].asString();
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    auto& cap = *it->second;
    
    // Save camera settings to file
    cv::FileStorage fs(path, cv::FileStorage::WRITE);
    if (!fs.isOpened()) {
        return ExecutionResult::fail("Failed to open file for writing: " + path);
    }
    
    fs << "camera_profile" << "{";
    fs << "source" << sourceId;
    fs << "width" << cap.get(cv::CAP_PROP_FRAME_WIDTH);
    fs << "height" << cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    fs << "fps" << cap.get(cv::CAP_PROP_FPS);
    fs << "brightness" << cap.get(cv::CAP_PROP_BRIGHTNESS);
    fs << "contrast" << cap.get(cv::CAP_PROP_CONTRAST);
    fs << "saturation" << cap.get(cv::CAP_PROP_SATURATION);
    fs << "hue" << cap.get(cv::CAP_PROP_HUE);
    fs << "gain" << cap.get(cv::CAP_PROP_GAIN);
    fs << "exposure" << cap.get(cv::CAP_PROP_EXPOSURE);
    fs << "auto_exposure" << cap.get(cv::CAP_PROP_AUTO_EXPOSURE);
    fs << "focus" << cap.get(cv::CAP_PROP_FOCUS);
    fs << "autofocus" << cap.get(cv::CAP_PROP_AUTOFOCUS);
    fs << "zoom" << cap.get(cv::CAP_PROP_ZOOM);
    fs << "auto_wb" << cap.get(cv::CAP_PROP_AUTO_WB);
    fs << "wb_temperature" << cap.get(cv::CAP_PROP_WB_TEMPERATURE);
    fs << "sharpness" << cap.get(cv::CAP_PROP_SHARPNESS);
    fs << "gamma" << cap.get(cv::CAP_PROP_GAMMA);
    fs << "}";
    
    fs.release();
    
    std::cout << "Camera profile saved to: " << path << std::endl;
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LoadCameraProfileItem
// ============================================================================

LoadCameraProfileItem::LoadCameraProfileItem() {
    _functionName = "load_camera_profile";
    _description = "Loads camera settings from a profile file";
    _category = "video_io";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Camera source identifier"),
        ParamDef::required("path", BaseType::STRING, "Profile file path (.xml or .json)")
    };
    _example = "load_camera_profile(\"0\", \"camera_profile.xml\")";
    _returnType = "mat";
    _tags = {"camera", "profile", "load", "settings"};
}

ExecutionResult LoadCameraProfileItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sourceId = args[0].asString();
    std::string path = args[1].asString();
    
    auto it = s_captures.find(sourceId);
    if (it == s_captures.end()) {
        return ExecutionResult::fail("Camera source not found: " + sourceId);
    }
    
    // Load camera settings from file
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        return ExecutionResult::fail("Failed to open file for reading: " + path);
    }
    
    auto& cap = *it->second;
    cv::FileNode profile = fs["camera_profile"];
    
    if (!profile.empty()) {
        if (!profile["width"].empty()) cap.set(cv::CAP_PROP_FRAME_WIDTH, (double)profile["width"]);
        if (!profile["height"].empty()) cap.set(cv::CAP_PROP_FRAME_HEIGHT, (double)profile["height"]);
        if (!profile["fps"].empty()) cap.set(cv::CAP_PROP_FPS, (double)profile["fps"]);
        if (!profile["brightness"].empty()) cap.set(cv::CAP_PROP_BRIGHTNESS, (double)profile["brightness"]);
        if (!profile["contrast"].empty()) cap.set(cv::CAP_PROP_CONTRAST, (double)profile["contrast"]);
        if (!profile["saturation"].empty()) cap.set(cv::CAP_PROP_SATURATION, (double)profile["saturation"]);
        if (!profile["hue"].empty()) cap.set(cv::CAP_PROP_HUE, (double)profile["hue"]);
        if (!profile["gain"].empty()) cap.set(cv::CAP_PROP_GAIN, (double)profile["gain"]);
        if (!profile["exposure"].empty()) cap.set(cv::CAP_PROP_EXPOSURE, (double)profile["exposure"]);
        if (!profile["auto_exposure"].empty()) cap.set(cv::CAP_PROP_AUTO_EXPOSURE, (double)profile["auto_exposure"]);
        if (!profile["focus"].empty()) cap.set(cv::CAP_PROP_FOCUS, (double)profile["focus"]);
        if (!profile["autofocus"].empty()) cap.set(cv::CAP_PROP_AUTOFOCUS, (double)profile["autofocus"]);
        if (!profile["zoom"].empty()) cap.set(cv::CAP_PROP_ZOOM, (double)profile["zoom"]);
        if (!profile["auto_wb"].empty()) cap.set(cv::CAP_PROP_AUTO_WB, (double)profile["auto_wb"]);
        if (!profile["wb_temperature"].empty()) cap.set(cv::CAP_PROP_WB_TEMPERATURE, (double)profile["wb_temperature"]);
        if (!profile["sharpness"].empty()) cap.set(cv::CAP_PROP_SHARPNESS, (double)profile["sharpness"]);
        if (!profile["gamma"].empty()) cap.set(cv::CAP_PROP_GAMMA, (double)profile["gamma"]);
        
        std::cout << "Camera profile loaded from: " << path << std::endl;
    } else {
        fs.release();
        return ExecutionResult::fail("Invalid profile file format");
    }
    
    fs.release();
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

#include "utils/camera_device_manager.h"
#include "utils/Logger.h"
#include <algorithm>
#include <iostream>
#include <sys/mman.h>

namespace visionpipe {

// Component ID for logging
static const std::string LOG_COMPONENT = "CameraDeviceManager";

CameraDeviceManager& CameraDeviceManager::instance() {
    static CameraDeviceManager instance;
    return instance;
}

CameraDeviceManager::CameraDeviceManager() {
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
    _libcameraManager = std::make_unique<libcamera::CameraManager>();
#endif
}

CameraDeviceManager::~CameraDeviceManager() {
    releaseAll();
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
    if (_libcameraManagerStarted && _libcameraManager) {
        _libcameraManager->stop();
    }
#endif
}

CameraBackend CameraDeviceManager::parseBackend(const std::string& backendStr) {
    std::string lower = backendStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "dshow") {  return CameraBackend::OPENCV_DSHOW; }
    if (lower == "v4l2") {  return CameraBackend::OPENCV_V4L2; }
    if (lower == "ffmpeg") {  return CameraBackend::OPENCV_FFMPEG; }
    if (lower == "gstreamer") {  return CameraBackend::OPENCV_GSTREAMER; }
    if (lower == "libcamera") {  return CameraBackend::LIBCAMERA; }
    
    return CameraBackend::OPENCV_AUTO;
}

CameraBackend CameraDeviceManager::fromOpenCVBackend(int cvBackend) {
    switch (cvBackend) {
        case cv::CAP_DSHOW: return CameraBackend::OPENCV_DSHOW;
        case cv::CAP_V4L2: return CameraBackend::OPENCV_V4L2;
        case cv::CAP_FFMPEG: return CameraBackend::OPENCV_FFMPEG;
        case cv::CAP_GSTREAMER: return CameraBackend::OPENCV_GSTREAMER;
        default: return CameraBackend::OPENCV_AUTO;
    }
}

bool CameraDeviceManager::acquireFrame(const std::string& sourceId, CameraBackend backend, cv::Mat& frame, const std::string& requestedFormat) {
    std::unique_lock<std::mutex> lock(_mutex);
    
    auto it = _sessions.find(sourceId);
    if (it == _sessions.end()) {
        // Need to open camera
        if (!openCamera(sourceId, backend, requestedFormat)) {
            return false;
        }
        it = _sessions.find(sourceId);
    }
    
    CameraSession& session = it->second;
    
    // Check if backend matches
    if (session.backend != backend && backend != CameraBackend::OPENCV_AUTO) {
        SystemLogger::warning(LOG_COMPONENT, "Requested backend differs from opened camera, using existing: " + sourceId);
    }
    
    // Read frame based on backend
    if (session.backend == CameraBackend::LIBCAMERA) {
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
        return readLibCameraFrame(session, frame, lock);
#else
        SystemLogger::error(LOG_COMPONENT, "libcamera backend requested but not compiled in");
        return false;
#endif
    } else {
        return readOpenCVFrame(session, frame);
    }
}

std::shared_ptr<cv::VideoCapture> CameraDeviceManager::getOpenCVCapture(const std::string& sourceId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sourceId);
    if (it != _sessions.end() && it->second.backend != CameraBackend::LIBCAMERA) {
        return it->second.opencvCapture;
    }
    return nullptr;
}

void CameraDeviceManager::releaseCamera(const std::string& sourceId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sourceId);
    if (it != _sessions.end()) {
        if (it->second.opencvCapture) {
            it->second.opencvCapture->release();
        }
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
        if (it->second.libcameraDevice) {
            it->second.requests.clear();
            it->second.allocator.reset();
            it->second.config.reset();
            it->second.libcameraDevice->stop();
            it->second.libcameraDevice->release();
        }
#endif
        _sessions.erase(it);
        SystemLogger::info(LOG_COMPONENT, "Released camera: " + sourceId);
    }
}

void CameraDeviceManager::releaseAll() {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& pair : _sessions) {
        if (pair.second.opencvCapture) {
            pair.second.opencvCapture->release();
        }
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
        if (pair.second.libcameraDevice) {
            pair.second.requests.clear();
            pair.second.allocator.reset();
            pair.second.config.reset();
            pair.second.libcameraDevice->stop();
            pair.second.libcameraDevice->release();
        }
#endif
    }
    _sessions.clear();
    SystemLogger::info(LOG_COMPONENT, "Released all cameras");
}

bool CameraDeviceManager::isOpen(const std::string& sourceId) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _sessions.find(sourceId) != _sessions.end();
}

CameraBackend CameraDeviceManager::getBackend(const std::string& sourceId) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sourceId);
    if (it != _sessions.end()) {
        return it->second.backend;
    }
    return CameraBackend::OPENCV_AUTO;
}

#ifdef VISIONPIPE_LIBCAMERA_ENABLED
void CameraDeviceManager::setLibCameraConfig(const std::string& sourceId, const LibCameraConfig& config) {
    std::lock_guard<std::mutex> lock(_mutex);
    // Create session if not exists, or update existing
    _sessions[sourceId].targetConfig = config;
}

bool CameraDeviceManager::setLibCameraControl(const std::string& sourceId, const std::string& controlName, float value) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sourceId);
    if (it == _sessions.end()) return false;
    
    CameraSession& session = it->second;
    if (session.backend != CameraBackend::LIBCAMERA) return false;

    if (!session.libcameraDevice) return false;
    
    // Validate control exists
    const auto& controls = session.libcameraDevice->controls();
    for (const auto& [id, info] : controls) {
        if (id->name() == controlName) {
            session.activeControls[controlName] = value;
            return true;
        }
    }
    SystemLogger::warning(LOG_COMPONENT, "Control not supported: " + controlName);
    return false;
}
#endif

bool CameraDeviceManager::openCamera(const std::string& sourceId, CameraBackend backend, const std::string& requestedFormat) {
    CameraSession session;
    session.backend = backend;
    
    if (backend == CameraBackend::LIBCAMERA) {
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
        // Set format in config if requested
        if (!requestedFormat.empty()) {
            session.targetConfig.pixelFormat = requestedFormat;
        } 
        
        if (!openLibCamera(sourceId, session)) {
            return false;
        }
#else
        SystemLogger::error(LOG_COMPONENT, "libcamera backend not available");
        return false;
#endif
    } else {
        // OpenCV backend
        int cvBackend = cv::CAP_ANY;
        switch (backend) {
            case CameraBackend::OPENCV_DSHOW: cvBackend = cv::CAP_DSHOW; break;
            case CameraBackend::OPENCV_V4L2: cvBackend = cv::CAP_V4L2; break;
            case CameraBackend::OPENCV_FFMPEG: cvBackend = cv::CAP_FFMPEG; break;
            case CameraBackend::OPENCV_GSTREAMER: cvBackend = cv::CAP_GSTREAMER; break;
            default: cvBackend = cv::CAP_ANY; break;
        }
        
        session.opencvCapture = std::make_shared<cv::VideoCapture>();
        
        // Try to parse sourceId as integer for camera index
        int index = -1;
        bool isIndex = false;

        try {
            index = std::stoi(sourceId);
            isIndex = true;
        } catch (...) {
            // Check for /dev/videoN pattern
            if (sourceId.find("/dev/video") == 0 && sourceId.length() > 10) {
                try {
                    std::string num = sourceId.substr(10); // length of "/dev/video"
                    index = std::stoi(num);
                    isIndex = true;
                } catch (...) {}
            }
        }

        if (isIndex) {
            std::cerr << "[DEBUG] Opening camera with index: " << index << " backend: " << cvBackend << std::endl;
            session.opencvCapture->open(index, cvBackend);
        } else {
            std::cerr << "[DEBUG] Opening camera with path: " << sourceId << " backend: " << cvBackend << std::endl;
            session.opencvCapture->open(sourceId, cvBackend);
        }
        
        if (!requestedFormat.empty() && requestedFormat.length() >= 4) {
            std::string fmtUpper = requestedFormat;
            std::transform(fmtUpper.begin(), fmtUpper.end(), fmtUpper.begin(), ::toupper);
            
            int fourcc = cv::VideoWriter::fourcc(
                fmtUpper[0], fmtUpper[1], fmtUpper[2], fmtUpper[3]
            );
            session.opencvCapture->set(cv::CAP_PROP_FOURCC, fourcc);
            std::cerr << "[DEBUG] Requested FourCC: " << fmtUpper << " (" << fourcc << ")" << std::endl;
        }
        
        if (!session.opencvCapture->isOpened()) {
            std::cerr << "[ERROR] Failed to open camera with OpenCV: " << sourceId << std::endl;
            return false;
        }
        
        std::cerr << "[DEBUG] Camera opened successfully: " << sourceId << std::endl;
    }
    
    _sessions[sourceId] = std::move(session);
    return true;
}

bool CameraDeviceManager::readOpenCVFrame(CameraSession& session, cv::Mat& frame) {
    if (!session.opencvCapture || !session.opencvCapture->isOpened()) {
        std::cerr << "[ERROR] readOpenCVFrame: Camera not open" << std::endl;
        return false;
    }
    bool success = session.opencvCapture->read(frame);
    if (!success) {
        std::cerr << "[ERROR] readOpenCVFrame: Failed to read frame (returned false)" << std::endl;
    } else if (frame.empty()) {
        std::cerr << "[ERROR] readOpenCVFrame: Success but frame empty" << std::endl;
    }
    return success;
}

#ifdef VISIONPIPE_LIBCAMERA_ENABLED

libcamera::CameraManager* CameraDeviceManager::getLibCameraManager() {
    if (!_libcameraManagerStarted && _libcameraManager) {
        int ret = _libcameraManager->start();
        if (ret < 0) {
            SystemLogger::error(LOG_COMPONENT, "Failed to start libcamera manager");
            return nullptr;
        }
        _libcameraManagerStarted = true;
    }
    return _libcameraManager.get();
}

std::shared_ptr<libcamera::Camera> CameraDeviceManager::getLibCamera(const std::string& sourceId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sourceId);
    if (it != _sessions.end() && it->second.backend == CameraBackend::LIBCAMERA) {
        return it->second.libcameraDevice;
    }
    return nullptr;
}

bool CameraDeviceManager::openLibCamera(const std::string& sourceId, CameraSession& session) {
    auto* manager = getLibCameraManager();
    if (!manager) {
        return false;
    }
    
    auto cameras = manager->cameras();
    if (cameras.empty()) {
        SystemLogger::error(LOG_COMPONENT, "No libcamera devices found");
        return false;
    }
    
    // Find camera by index or ID
    std::shared_ptr<libcamera::Camera> camera;
    try {
        int index = std::stoi(sourceId);
        if (index >= 0 && index < static_cast<int>(cameras.size())) {
            camera = cameras[index];
        }
    } catch (...) {
        // Try by ID string
        camera = manager->get(sourceId);
    }
    
    if (!camera) {
        SystemLogger::error(LOG_COMPONENT, "libcamera device not found: " + sourceId);
        return false;
    }
    
    if (camera->acquire() < 0) {
        SystemLogger::error(LOG_COMPONENT, "Failed to acquire libcamera device: " + sourceId);
        return false;
    }
    
    session.libcameraDevice = camera;
    
    // Map string role to StreamRole
    libcamera::StreamRole role = libcamera::StreamRole::VideoRecording;
    if (session.targetConfig.streamRole == "StillCapture") 
        role = libcamera::StreamRole::StillCapture;
    else if (session.targetConfig.streamRole == "Viewfinder") 
        role = libcamera::StreamRole::Viewfinder;

    session.config = camera->generateConfiguration({role});
    if (!session.config) {
        SystemLogger::error(LOG_COMPONENT, "Failed to generate libcamera configuration");
        camera->release();
        return false;
    }

    // Extended pixel format mapping covering YUV, RGB, Bayer, MJPEG, Greyscale
    // Extended pixel format mapping covering YUV, RGB, Bayer, MJPEG
    static const std::map<std::string, libcamera::PixelFormat> lcFormats = {
        // Packed YUV 4:2:2
        { "YUYV",  libcamera::formats::YUYV },
        { "YUY2",  libcamera::formats::YUYV },
        { "UYVY",  libcamera::formats::UYVY },
        { "YVYU",  libcamera::formats::YVYU },
        { "VYUY",  libcamera::formats::VYUY },

        // Planar / semi-planar YUV 4:2:0
        { "NV12",  libcamera::formats::NV12 },
        { "NV21",  libcamera::formats::NV21 },
        { "YUV420", libcamera::formats::YUV420 },

        // RGB / BGR (8-bit)
        { "RGB888", libcamera::formats::RGB888 },
        { "BGR888", libcamera::formats::BGR888 },
        { "RGB24",  libcamera::formats::RGB888 },
        { "BGR24",  libcamera::formats::BGR888 },

        // Compressed
        { "MJPEG", libcamera::formats::MJPEG },
        { "MJPG",  libcamera::formats::MJPEG },
        { "JPEG",  libcamera::formats::MJPEG },

        // 8-bit Bayer
        { "SBGGR8", libcamera::formats::SBGGR8 },
        { "SGBRG8", libcamera::formats::SGBRG8 },
        { "SGRBG8", libcamera::formats::SGRBG8 },
        { "SRGGB8", libcamera::formats::SRGGB8 },

        // 10-bit Bayer packed (CSI2P; IMX219, IMX477, etc.)
        { "SBGGR10", libcamera::formats::SBGGR10_CSI2P },
        { "SGBRG10", libcamera::formats::SGBRG10_CSI2P },
        { "SGRBG10", libcamera::formats::SGRBG10_CSI2P },
        { "SRGGB10", libcamera::formats::SRGGB10_CSI2P },

        // 12-bit Bayer packed
        { "SBGGR12", libcamera::formats::SBGGR12_CSI2P },
        { "SGBRG12", libcamera::formats::SGBRG12_CSI2P },
        { "SGRBG12", libcamera::formats::SGRBG12_CSI2P },
        { "SRGGB12", libcamera::formats::SRGGB12_CSI2P },

        // 16-bit unpacked Bayer
        { "SBGGR16", libcamera::formats::SBGGR16 },
        { "SGBRG16", libcamera::formats::SGBRG16 },
        { "SGRBG16", libcamera::formats::SGRBG16 },
        { "SRGGB16", libcamera::formats::SRGGB16 },
    };
    
    // Apply configuration
    libcamera::StreamConfiguration& streamCfg = session.config->at(0);
    
    // Look up pixel format (case-insensitive)
    std::string fmtKey = session.targetConfig.pixelFormat;
    std::transform(fmtKey.begin(), fmtKey.end(), fmtKey.begin(), ::toupper);
    
    auto it = lcFormats.find(fmtKey);
    if (it != lcFormats.end()) {
        streamCfg.pixelFormat = it->second;
        SystemLogger::info(LOG_COMPONENT, "Requested pixel format: " + fmtKey);
    } else {
        streamCfg.pixelFormat = libcamera::formats::BGR888; // Safe default
        SystemLogger::warning(LOG_COMPONENT, "Unknown format '" + fmtKey + "', defaulting to BGR888");
    }
    
    streamCfg.size = {
        static_cast<unsigned int>(session.targetConfig.width), 
        static_cast<unsigned int>(session.targetConfig.height)
    };
    streamCfg.bufferCount = static_cast<unsigned int>(session.targetConfig.bufferCount);
    
    SystemLogger::info(LOG_COMPONENT, 
        "Requested config: " + std::to_string(streamCfg.size.width) + "x" + 
        std::to_string(streamCfg.size.height) + " " + 
        streamCfg.pixelFormat.toString());
    
    // Validate configuration
    libcamera::CameraConfiguration::Status validation = session.config->validate();
    if (validation == libcamera::CameraConfiguration::Invalid) {
        SystemLogger::error(LOG_COMPONENT, "Invalid libcamera configuration");
        camera->release();
        return false;
    } else if (validation == libcamera::CameraConfiguration::Adjusted) {
        auto& finalCfg = session.config->at(0);
        SystemLogger::warning(LOG_COMPONENT, 
            "libcamera configuration adjusted to: " + 
            std::to_string(finalCfg.size.width) + "x" + 
            std::to_string(finalCfg.size.height) + " " + 
            finalCfg.pixelFormat.toString());
    } else {
        SystemLogger::info(LOG_COMPONENT, "libcamera configuration validated successfully");
    }
    
    if (camera->configure(session.config.get()) < 0) {
        SystemLogger::error(LOG_COMPONENT, "Failed to configure libcamera");
        camera->release();
        return false;
    }
    
    // Allocate buffers
    session.allocator = std::make_unique<libcamera::FrameBufferAllocator>(camera);
    libcamera::Stream* stream = session.config->at(0).stream();
    
    if (session.allocator->allocate(stream) < 0) {
        SystemLogger::error(LOG_COMPONENT, "Failed to allocate libcamera buffers");
        camera->release();
        return false;
    }
    
    // Create requests
    const std::vector<std::unique_ptr<libcamera::FrameBuffer>>& buffers = session.allocator->buffers(stream);
    for (const auto& buffer : buffers) {
        auto request = camera->createRequest();
        if (!request) {
            SystemLogger::error(LOG_COMPONENT, "Failed to create libcamera request");
            camera->release();
            return false;
        }
        request->addBuffer(stream, buffer.get());
        session.requests.push_back(std::move(request));
    }
    
    // Connect signal for completed requests
    camera->requestCompleted.connect(this, &CameraDeviceManager::libcameraRequestComplete);
    
    // Start camera
    if (camera->start() < 0) {
        SystemLogger::error(LOG_COMPONENT, "Failed to start libcamera");
        camera->release();
        return false;
    }
    
    // Queue initial requests
    for (auto& request : session.requests) {
        camera->queueRequest(request.get());
    }
    
    SystemLogger::info(LOG_COMPONENT, "Successfully opened camera with libcamera backend: " + sourceId);
    return true;
}


bool CameraDeviceManager::readLibCameraFrame(CameraSession& session, cv::Mat& frame, std::unique_lock<std::mutex>& lock) {
    // Wait for frame with timeout
    if (session.frameCond->wait_for(lock, std::chrono::milliseconds(500), [&]{ return session.frameReady; })) {
        frame = session.latestFrame.clone();
        session.frameReady = false;
        return true;
    }
    return false;
}

void CameraDeviceManager::libcameraRequestComplete(libcamera::Request* request) {
    if (request->status() == libcamera::Request::RequestCancelled) {
        return;
    }
    
    // Protect access to _sessions map
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Find the session this request belongs to
    for (auto& pair : _sessions) {
        CameraSession& session = pair.second;
        if (session.backend != CameraBackend::LIBCAMERA) continue;
        
        for (auto& req : session.requests) {
            if (req.get() == request) {
                // Get the buffer and convert to cv::Mat
                const libcamera::Request::BufferMap& buffers = request->buffers();
                for (const auto& bufferPair : buffers) {
                    libcamera::FrameBuffer* buffer = bufferPair.second;
                    const auto& planes = buffer->planes();
                    if (planes.empty()) continue;
                    const libcamera::FrameBuffer::Plane& plane = planes[0];
                    
                    // Memory map the buffer
                    void* data = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
                    if (data != MAP_FAILED) {
                        libcamera::StreamConfiguration& streamCfg = session.config->at(0);
                        
                        // Determine type based on format
                        int type = CV_8UC3;
                        int width = streamCfg.size.width;
                        int height = streamCfg.size.height;
                        
                        libcamera::PixelFormat fmt = streamCfg.pixelFormat;
                        if (fmt == libcamera::formats::YUYV || fmt == libcamera::formats::UYVY || fmt == libcamera::formats::YVYU) {
                            type = CV_8UC2; // 2 bytes per pixel
                        } else if (fmt == libcamera::formats::NV12 || fmt == libcamera::formats::NV21) {
                            type = CV_8UC1; 
                            height = height * 3 / 2; // 1.5 height for Y + UV planes
                        } else if (fmt == libcamera::formats::MJPEG) {
                            // Treat as 1D array
                            type = CV_8UC1;
                            width = plane.length; // Max buffer size
                            height = 1;
                            // Note: real size is in metadata, but for now we map full buffer
                        }
                        
                        session.latestFrame = cv::Mat(height, width, type, data).clone();
                        session.frameReady = true;
                        if (session.activeControls.size() > 0) {
                             // Apply active controls to the next request
                             libcamera::ControlList& ctrls = request->controls();
                             // Iterate over supported controls to match our active map
                             for (const auto& [id, info] : session.libcameraDevice->controls()) {
                                 auto it = session.activeControls.find(id->name());
                                 if (it != session.activeControls.end()) {
                                     float val = it->second;
                                     switch(id->type()) {
                                         case libcamera::ControlTypeBool: ctrls.set(id->id(), static_cast<bool>(val)); break;
                                         case libcamera::ControlTypeByte: ctrls.set(id->id(), static_cast<uint8_t>(val)); break;
                                         case libcamera::ControlTypeInteger32: ctrls.set(id->id(), static_cast<int32_t>(val)); break;
                                         case libcamera::ControlTypeInteger64: ctrls.set(id->id(), static_cast<int64_t>(val)); break;
                                         case libcamera::ControlTypeFloat: ctrls.set(id->id(), val); break;
                                         default: break;
                                     }
                                 }
                             }
                        }
                        munmap(data, plane.length);
                    }
                }
                
                // Signal waiting threads
                session.frameCond->notify_all();

                // Requeue the request
                request->reuse(libcamera::Request::ReuseBuffers);
                session.libcameraDevice->queueRequest(request);
                return;
            }
        }
    }
}

#endif // VISIONPIPE_LIBCAMERA_ENABLED

} // namespace visionpipe

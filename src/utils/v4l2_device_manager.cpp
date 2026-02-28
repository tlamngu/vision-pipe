#include "utils/v4l2_device_manager.h"

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED

#include "utils/Logger.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace visionpipe {

static const std::string LOG_COMPONENT = "V4L2DeviceManager";

// ============================================================================
// Format lookup table: string name → V4L2 FourCC
// ============================================================================
static const std::map<std::string, uint32_t> kFormatTable = {
    // YUV
    {"YUYV",     V4L2_PIX_FMT_YUYV},
    {"UYVY",     V4L2_PIX_FMT_UYVY},
    {"NV12",     V4L2_PIX_FMT_NV12},
    {"NV21",     V4L2_PIX_FMT_NV21},
    {"YUV420",   V4L2_PIX_FMT_YUV420},
    // RGB
    {"BGR24",    V4L2_PIX_FMT_BGR24},
    {"RGB24",    V4L2_PIX_FMT_RGB24},
    // Compressed
    {"MJPG",     V4L2_PIX_FMT_MJPEG},
    {"MJPEG",    V4L2_PIX_FMT_MJPEG},
    // 8-bit Bayer
    {"SBGGR8",   V4L2_PIX_FMT_SBGGR8},
    {"SGBRG8",   V4L2_PIX_FMT_SGBRG8},
    {"SGRBG8",   V4L2_PIX_FMT_SGRBG8},
    {"SRGGB8",   V4L2_PIX_FMT_SRGGB8},
    // 10-bit Bayer (packed)
    {"SBGGR10",  V4L2_PIX_FMT_SBGGR10P},
    {"SGBRG10",  V4L2_PIX_FMT_SGBRG10P},
    {"SGRBG10",  V4L2_PIX_FMT_SGRBG10P},
    {"SRGGB10",  V4L2_PIX_FMT_SRGGB10P},
    // 12-bit Bayer (packed)
    {"SBGGR12",  V4L2_PIX_FMT_SBGGR12P},
    {"SGBRG12",  V4L2_PIX_FMT_SGBRG12P},
    {"SGRBG12",  V4L2_PIX_FMT_SGRBG12P},
    {"SRGGB12",  V4L2_PIX_FMT_SRGGB12P},
    // 16-bit Bayer
    {"SBGGR16",  V4L2_PIX_FMT_SBGGR16},
    {"SGBRG16",  V4L2_PIX_FMT_SGBRG16},
    {"SGRBG16",  V4L2_PIX_FMT_SGRBG16},
    {"SRGGB16",  V4L2_PIX_FMT_SRGGB16},
};

// Helper: xioctl with EINTR retry
static int xioctl(int fd, unsigned long request, void* arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

// ============================================================================
// Singleton
// ============================================================================

V4L2DeviceManager& V4L2DeviceManager::instance() {
    static V4L2DeviceManager inst;
    return inst;
}

V4L2DeviceManager::~V4L2DeviceManager() {
    releaseAll();
}

// ============================================================================
// lookupPixelFormat
// ============================================================================
uint32_t V4L2DeviceManager::lookupPixelFormat(const std::string& name) const {
    // Try exact match first
    auto it = kFormatTable.find(name);
    if (it != kFormatTable.end()) return it->second;

    // Case-insensitive
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    it = kFormatTable.find(upper);
    if (it != kFormatTable.end()) return it->second;

    // Try interpreting as raw FourCC
    if (name.size() == 4) {
        return v4l2_fourcc(name[0], name[1], name[2], name[3]);
    }
    return 0;
}

// ============================================================================
// fourccToString
// ============================================================================
std::string V4L2DeviceManager::fourccToString(uint32_t fourcc) const {
    char buf[5];
    buf[0] = static_cast<char>(fourcc & 0xFF);
    buf[1] = static_cast<char>((fourcc >> 8) & 0xFF);
    buf[2] = static_cast<char>((fourcc >> 16) & 0xFF);
    buf[3] = static_cast<char>((fourcc >> 24) & 0xFF);
    buf[4] = '\0';
    return std::string(buf);
}

// ============================================================================
// prepareDevice
// ============================================================================
bool V4L2DeviceManager::prepareDevice(const std::string& devicePath, const V4L2NativeConfig& config) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (_verbose) {
        std::cout << "[DEBUG] V4L2: prepareDevice(" << devicePath << ")"
                  << " w=" << config.width << " h=" << config.height
                  << " fmt=" << config.pixelFormat << " fps=" << config.fps
                  << " buffers=" << config.bufferCount << std::endl;
    }

    // Release existing session if any
    auto it = _sessions.find(devicePath);
    if (it != _sessions.end()) {
        if (_verbose) std::cout << "[DEBUG] V4L2: releasing existing session for " << devicePath << std::endl;
        releaseDevice(devicePath);
    }

    V4L2Session session;
    session.config = config;

    if (!openDevice(devicePath, session)) {
        return false;
    }

    _sessions[devicePath] = std::move(session);
    if (_verbose) std::cout << "[DEBUG] V4L2: prepareDevice succeeded for " << devicePath << std::endl;
    SystemLogger::info(LOG_COMPONENT, "Prepared device: " + devicePath);
    return true;
}

// ============================================================================
// openDevice — full open + negotiate + mmap + STREAMON
// ============================================================================
bool V4L2DeviceManager::openDevice(const std::string& devicePath, V4L2Session& session) {
    // 1. Open device
    if (_verbose) std::cout << "[DEBUG] V4L2: openDevice(" << devicePath << ")" << std::endl;
    session.fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if (session.fd < 0) {
        SystemLogger::error(LOG_COMPONENT, "Cannot open device " + devicePath + ": " + std::string(strerror(errno)));
        return false;
    }
    if (_verbose) std::cout << "[DEBUG] V4L2: opened fd=" << session.fd << " for " << devicePath << std::endl;

    // 2. VIDIOC_QUERYCAP
    struct v4l2_capability cap{};
    if (xioctl(session.fd, VIDIOC_QUERYCAP, &cap) < 0) {
        std::string err = "VIDIOC_QUERYCAP failed: " + std::string(strerror(errno));
        SystemLogger::error(LOG_COMPONENT, err);
        if (_verbose) std::cout << "[DEBUG] V4L2: ERROR " << err << std::endl;
        ::close(session.fd);
        session.fd = -1;
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::string err = devicePath + " is not a video capture device (caps=0x" +
                          [&]{ std::ostringstream s; s << std::hex << cap.capabilities; return s.str(); }() + ")";
        SystemLogger::error(LOG_COMPONENT, err);
        if (_verbose) std::cout << "[DEBUG] V4L2: ERROR " << err << std::endl;
        ::close(session.fd);
        session.fd = -1;
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        std::string err = devicePath + " does not support streaming I/O (caps=0x" +
                          [&]{ std::ostringstream s; s << std::hex << cap.capabilities; return s.str(); }() + ")";
        SystemLogger::error(LOG_COMPONENT, err);
        if (_verbose) std::cout << "[DEBUG] V4L2: ERROR " << err << std::endl;
        ::close(session.fd);
        session.fd = -1;
        return false;
    }

    SystemLogger::info(LOG_COMPONENT, "Device: " + std::string(reinterpret_cast<char*>(cap.card)) +
                       " driver: " + std::string(reinterpret_cast<char*>(cap.driver)));
    if (_verbose) {
        std::cout << "[DEBUG] V4L2: QUERYCAP driver=" << cap.driver
                  << " card=" << cap.card
                  << " bus=" << cap.bus_info
                  << " caps=0x" << std::hex << cap.capabilities << std::dec << std::endl;
    }

    // 3. VIDIOC_S_FMT
    uint32_t pixfmt = lookupPixelFormat(session.config.pixelFormat);
    if (pixfmt == 0) {
        std::string err = "Unknown pixel format: " + session.config.pixelFormat;
        SystemLogger::error(LOG_COMPONENT, err);
        if (_verbose) std::cout << "[DEBUG] V4L2: ERROR " << err << std::endl;
        ::close(session.fd);
        session.fd = -1;
        return false;
    }

    struct v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = static_cast<uint32_t>(session.config.width);
    fmt.fmt.pix.height = static_cast<uint32_t>(session.config.height);
    fmt.fmt.pix.pixelformat = pixfmt;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(session.fd, VIDIOC_S_FMT, &fmt) < 0) {
        SystemLogger::error(LOG_COMPONENT, "VIDIOC_S_FMT failed: " + std::string(strerror(errno)));
        ::close(session.fd);
        session.fd = -1;
        return false;
    }

    // Read back negotiated values
    session.negotiatedPixFmt = fmt.fmt.pix.pixelformat;
    session.negotiatedWidth = static_cast<int>(fmt.fmt.pix.width);
    session.negotiatedHeight = static_cast<int>(fmt.fmt.pix.height);
    session.negotiatedBytesPerLine = fmt.fmt.pix.bytesperline;
    session.negotiatedSizeImage = fmt.fmt.pix.sizeimage;

    SystemLogger::info(LOG_COMPONENT, "Negotiated format: " + fourccToString(session.negotiatedPixFmt) +
                       " " + std::to_string(session.negotiatedWidth) + "x" + std::to_string(session.negotiatedHeight) +
                       " bytesperline=" + std::to_string(session.negotiatedBytesPerLine) +
                       " sizeimage=" + std::to_string(session.negotiatedSizeImage));
    if (_verbose) {
        std::cout << "[DEBUG] V4L2: S_FMT requested=" << session.config.pixelFormat
                  << " " << session.config.width << "x" << session.config.height
                  << " -> negotiated=" << fourccToString(session.negotiatedPixFmt)
                  << " " << session.negotiatedWidth << "x" << session.negotiatedHeight
                  << " bytesperline=" << session.negotiatedBytesPerLine
                  << " sizeimage=" << session.negotiatedSizeImage << std::endl;
    }

    // 4. Set framerate via VIDIOC_S_PARM
    struct v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = static_cast<uint32_t>(session.config.fps);
    if (xioctl(session.fd, VIDIOC_S_PARM, &parm) < 0) {
        SystemLogger::warning(LOG_COMPONENT, "VIDIOC_S_PARM (set fps) failed, continuing: " + std::string(strerror(errno)));
    }
    if (_verbose) {
        auto& tpf = parm.parm.capture.timeperframe;
        std::cout << "[DEBUG] V4L2: S_PARM requested fps=" << session.config.fps
                  << " actual=" << tpf.denominator << "/" << tpf.numerator << std::endl;
    }

    // 5. VIDIOC_REQBUFS
    struct v4l2_requestbuffers reqbufs{};
    reqbufs.count = static_cast<uint32_t>(session.config.bufferCount);
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;

    if (xioctl(session.fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        SystemLogger::error(LOG_COMPONENT, "VIDIOC_REQBUFS failed: " + std::string(strerror(errno)));
        ::close(session.fd);
        session.fd = -1;
        return false;
    }

    if (reqbufs.count < 2) {
        SystemLogger::error(LOG_COMPONENT, "Insufficient buffer count: " + std::to_string(reqbufs.count));
        ::close(session.fd);
        session.fd = -1;
        return false;
    }
    if (_verbose) std::cout << "[DEBUG] V4L2: REQBUFS requested=" << session.config.bufferCount << " granted=" << reqbufs.count << std::endl;

    // 6. VIDIOC_QUERYBUF + mmap each buffer, then VIDIOC_QBUF
    session.buffers.resize(reqbufs.count);
    for (uint32_t i = 0; i < reqbufs.count; ++i) {
        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(session.fd, VIDIOC_QUERYBUF, &buf) < 0) {
            SystemLogger::error(LOG_COMPONENT, "VIDIOC_QUERYBUF failed for index " + std::to_string(i));
            ::close(session.fd);
            session.fd = -1;
            return false;
        }

        session.buffers[i].length = buf.length;
        session.buffers[i].start = mmap(nullptr, buf.length,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED,
                                         session.fd, buf.m.offset);
        if (session.buffers[i].start == MAP_FAILED) {
            SystemLogger::error(LOG_COMPONENT, "mmap failed for buffer " + std::to_string(i));
            ::close(session.fd);
            session.fd = -1;
            return false;
        }
        if (_verbose) {
            std::cout << "[DEBUG] V4L2: buffer[" << i << "] mmap offset=" << buf.m.offset
                      << " length=" << buf.length
                      << " addr=" << session.buffers[i].start << std::endl;
        }

        // Enqueue buffer
        if (xioctl(session.fd, VIDIOC_QBUF, &buf) < 0) {
            SystemLogger::error(LOG_COMPONENT, "VIDIOC_QBUF failed for buffer " + std::to_string(i));
            ::close(session.fd);
            session.fd = -1;
            return false;
        }
        if (_verbose) std::cout << "[DEBUG] V4L2: buffer[" << i << "] QBUF enqueued" << std::endl;
    }

    // 7. VIDIOC_STREAMON
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(session.fd, VIDIOC_STREAMON, &type) < 0) {
        SystemLogger::error(LOG_COMPONENT, "VIDIOC_STREAMON failed: " + std::string(strerror(errno)));
        ::close(session.fd);
        session.fd = -1;
        return false;
    }

    if (_verbose) std::cout << "[DEBUG] V4L2: STREAMON ok, streaming started on " << devicePath << std::endl;
    session.streaming = true;
    return true;
}

// ============================================================================
// Unpack helpers for 10-bit and 12-bit packed Bayer
// ============================================================================

static cv::Mat unpackRAW10(const uint8_t* packed, int width, int height, uint32_t bytesperline) {
    // MIPI RAW10 packing: 4 pixels in 5 bytes
    cv::Mat out(height, width, CV_16UC1);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = packed + y * bytesperline;
        uint16_t* dst = out.ptr<uint16_t>(y);
        for (int x = 0; x < width; x += 4) {
            int byteIdx = (x / 4) * 5;
            uint8_t lsb = row[byteIdx + 4];
            dst[x + 0] = (static_cast<uint16_t>(row[byteIdx + 0]) << 2) | ((lsb >> 0) & 0x03);
            dst[x + 1] = (static_cast<uint16_t>(row[byteIdx + 1]) << 2) | ((lsb >> 2) & 0x03);
            dst[x + 2] = (static_cast<uint16_t>(row[byteIdx + 2]) << 2) | ((lsb >> 4) & 0x03);
            dst[x + 3] = (static_cast<uint16_t>(row[byteIdx + 3]) << 2) | ((lsb >> 6) & 0x03);
        }
    }
    return out;
}

static cv::Mat unpackRAW12(const uint8_t* packed, int width, int height, uint32_t bytesperline) {
    // MIPI RAW12 packing: 2 pixels in 3 bytes
    cv::Mat out(height, width, CV_16UC1);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = packed + y * bytesperline;
        uint16_t* dst = out.ptr<uint16_t>(y);
        for (int x = 0; x < width; x += 2) {
            int byteIdx = (x / 2) * 3;
            uint8_t lsb = row[byteIdx + 2];
            dst[x + 0] = (static_cast<uint16_t>(row[byteIdx + 0]) << 4) | ((lsb >> 0) & 0x0F);
            dst[x + 1] = (static_cast<uint16_t>(row[byteIdx + 1]) << 4) | ((lsb >> 4) & 0x0F);
        }
    }
    return out;
}

// ============================================================================
// acquireFrame
// ============================================================================
bool V4L2DeviceManager::acquireFrame(const std::string& devicePath, cv::Mat& frame) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    auto it = _sessions.find(devicePath);
    if (it == _sessions.end()) {
        SystemLogger::error(LOG_COMPONENT, "Device not prepared: " + devicePath);
        return false;
    }
    V4L2Session& session = it->second;
    if (!session.streaming) {
        SystemLogger::error(LOG_COMPONENT, "Device not streaming: " + devicePath);
        return false;
    }

    // 1. poll
    struct pollfd pfd{};
    pfd.fd = session.fd;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, 2000);
    if (ret <= 0) {
        SystemLogger::error(LOG_COMPONENT, "poll timeout or error on " + devicePath);
        return false;
    }
    if (_verbose) std::cout << "[DEBUG] V4L2: poll ready on " << devicePath << std::endl;

    // 2. VIDIOC_DQBUF
    struct v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(session.fd, VIDIOC_DQBUF, &buf) < 0) {
        SystemLogger::error(LOG_COMPONENT, "VIDIOC_DQBUF failed: " + std::string(strerror(errno)));
        return false;
    }

    const uint8_t* data = static_cast<const uint8_t*>(session.buffers[buf.index].start);
    int w = session.negotiatedWidth;
    int h = session.negotiatedHeight;
    uint32_t pf = session.negotiatedPixFmt;
    if (_verbose) {
        std::cout << "[DEBUG] V4L2: DQBUF index=" << buf.index
                  << " bytesused=" << buf.bytesused
                  << " fmt=" << fourccToString(pf)
                  << " size=" << w << "x" << h << std::endl;
    }

    // 3. Convert raw buffer to cv::Mat
    bool ok = true;
    if (pf == V4L2_PIX_FMT_YUYV) {
        cv::Mat yuyv(h, w, CV_8UC2, const_cast<uint8_t*>(data));
        cv::cvtColor(yuyv, frame, cv::COLOR_YUV2BGR_YUY2);
    } else if (pf == V4L2_PIX_FMT_UYVY) {
        cv::Mat uyvy(h, w, CV_8UC2, const_cast<uint8_t*>(data));
        cv::cvtColor(uyvy, frame, cv::COLOR_YUV2BGR_UYVY);
    } else if (pf == V4L2_PIX_FMT_MJPEG) {
        std::vector<uint8_t> jpgBuf(data, data + buf.bytesused);
        frame = cv::imdecode(jpgBuf, cv::IMREAD_COLOR);
        if (frame.empty()) ok = false;
    } else if (pf == V4L2_PIX_FMT_NV12) {
        cv::Mat nv12(h * 3 / 2, w, CV_8UC1, const_cast<uint8_t*>(data));
        cv::cvtColor(nv12, frame, cv::COLOR_YUV2BGR_NV12);
    } else if (pf == V4L2_PIX_FMT_NV21) {
        cv::Mat nv21(h * 3 / 2, w, CV_8UC1, const_cast<uint8_t*>(data));
        cv::cvtColor(nv21, frame, cv::COLOR_YUV2BGR_NV21);
    } else if (pf == V4L2_PIX_FMT_BGR24) {
        frame = cv::Mat(h, w, CV_8UC3, const_cast<uint8_t*>(data)).clone();
    } else if (pf == V4L2_PIX_FMT_RGB24) {
        cv::Mat rgb(h, w, CV_8UC3, const_cast<uint8_t*>(data));
        cv::cvtColor(rgb, frame, cv::COLOR_RGB2BGR);
    }
    // 8-bit Bayer — return as CV_8UC1 (user applies debayer)
    else if (pf == V4L2_PIX_FMT_SBGGR8 || pf == V4L2_PIX_FMT_SGBRG8 ||
             pf == V4L2_PIX_FMT_SGRBG8 || pf == V4L2_PIX_FMT_SRGGB8) {
        frame = cv::Mat(h, w, CV_8UC1, const_cast<uint8_t*>(data)).clone();
    }
    // 10-bit packed Bayer → unpack to CV_16UC1
    else if (pf == V4L2_PIX_FMT_SBGGR10P || pf == V4L2_PIX_FMT_SGBRG10P ||
             pf == V4L2_PIX_FMT_SGRBG10P || pf == V4L2_PIX_FMT_SRGGB10P) {
        frame = unpackRAW10(data, w, h, session.negotiatedBytesPerLine);
    }
    // 12-bit packed Bayer → unpack to CV_16UC1
    else if (pf == V4L2_PIX_FMT_SBGGR12P || pf == V4L2_PIX_FMT_SGBRG12P ||
             pf == V4L2_PIX_FMT_SGRBG12P || pf == V4L2_PIX_FMT_SRGGB12P) {
        frame = unpackRAW12(data, w, h, session.negotiatedBytesPerLine);
    }
    // 16-bit Bayer — return as CV_16UC1
    else if (pf == V4L2_PIX_FMT_SBGGR16 || pf == V4L2_PIX_FMT_SGBRG16 ||
             pf == V4L2_PIX_FMT_SGRBG16 || pf == V4L2_PIX_FMT_SRGGB16) {
        frame = cv::Mat(h, w, CV_16UC1, const_cast<uint8_t*>(data)).clone();
    } else {
        SystemLogger::error(LOG_COMPONENT, "Unsupported pixel format for conversion: " + fourccToString(pf));
        ok = false;
    }

    // 4. Re-enqueue buffer
    if (xioctl(session.fd, VIDIOC_QBUF, &buf) < 0) {
        SystemLogger::warning(LOG_COMPONENT, "VIDIOC_QBUF re-enqueue failed: " + std::string(strerror(errno)));
    }
    if (_verbose) std::cout << "[DEBUG] V4L2: QBUF re-enqueued index=" << buf.index << " ok=" << ok << std::endl;

    return ok;
}

// ============================================================================
// releaseDevice
// ============================================================================
void V4L2DeviceManager::releaseDevice(const std::string& devicePath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _sessions.find(devicePath);
    if (it == _sessions.end()) return;

    V4L2Session& session = it->second;

    if (session.streaming) {
        if (_verbose) std::cout << "[DEBUG] V4L2: STREAMOFF " << devicePath << std::endl;
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(session.fd, VIDIOC_STREAMOFF, &type);
        session.streaming = false;
    }

    int bufIdx = 0;
    for (auto& mb : session.buffers) {
        if (mb.start && mb.start != MAP_FAILED) {
            if (_verbose) std::cout << "[DEBUG] V4L2: munmap buffer[" << bufIdx << "] addr=" << mb.start << " len=" << mb.length << std::endl;
            munmap(mb.start, mb.length);
        }
        ++bufIdx;
    }
    session.buffers.clear();

    if (session.fd >= 0) {
        if (_verbose) std::cout << "[DEBUG] V4L2: closing fd=" << session.fd << " for " << devicePath << std::endl;
        ::close(session.fd);
        session.fd = -1;
    }

    _sessions.erase(it);
    SystemLogger::info(LOG_COMPONENT, "Released device: " + devicePath);
}

// ============================================================================
// releaseAll
// ============================================================================
void V4L2DeviceManager::releaseAll() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    // Collect keys first to avoid iterator invalidation
    std::vector<std::string> keys;
    keys.reserve(_sessions.size());
    for (auto& kv : _sessions) {
        keys.push_back(kv.first);
    }
    for (auto& k : keys) {
        releaseDevice(k);
    }
}

// ============================================================================
// isOpen
// ============================================================================
bool V4L2DeviceManager::isOpen(const std::string& devicePath) const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _sessions.find(devicePath) != _sessions.end();
}

// ============================================================================
// setVerbose / isVerbose
// ============================================================================
void V4L2DeviceManager::setVerbose(bool verbose) { _verbose = verbose; }
bool V4L2DeviceManager::isVerbose() const { return _verbose; }

// ============================================================================
// resolveControlId — map a control name or numeric string to V4L2 CID
// ============================================================================
uint32_t V4L2DeviceManager::resolveControlId(int fd, const std::string& nameOrId) const {
    // Try numeric parse first
    try {
        unsigned long val = std::stoul(nameOrId);
        if (_verbose) std::cout << "[DEBUG] V4L2: resolveControlId numeric '" << nameOrId << "' -> 0x" << std::hex << val << std::dec << std::endl;
        return static_cast<uint32_t>(val);
    } catch (...) {}

    if (_verbose) std::cout << "[DEBUG] V4L2: resolveControlId scanning controls for '" << nameOrId << "'" << std::endl;

    // Iterate User-class controls
    std::string lowerName = nameOrId;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    struct v4l2_queryctrl qctrl{};
    qctrl.id = V4L2_CID_BASE;
    while (qctrl.id < V4L2_CID_LASTP1) {
        if (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
            std::string ctrlName(reinterpret_cast<char*>(qctrl.name));
            std::string ctrlLower = ctrlName;
            std::transform(ctrlLower.begin(), ctrlLower.end(), ctrlLower.begin(), ::tolower);
            if (ctrlLower == lowerName) {
                if (_verbose) std::cout << "[DEBUG] V4L2: resolveControlId found '" << ctrlName << "' -> 0x" << std::hex << qctrl.id << std::dec << " (User class)" << std::endl;
                return qctrl.id;
            }
        }
        qctrl.id++;
    }

    // Iterate Camera-class controls
    qctrl.id = V4L2_CID_CAMERA_CLASS_BASE;
    while (qctrl.id < V4L2_CID_CAMERA_CLASS_BASE + 64) {
        if (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
            std::string ctrlName(reinterpret_cast<char*>(qctrl.name));
            std::string ctrlLower = ctrlName;
            std::transform(ctrlLower.begin(), ctrlLower.end(), ctrlLower.begin(), ::tolower);
            if (ctrlLower == lowerName) {
                if (_verbose) std::cout << "[DEBUG] V4L2: resolveControlId found '" << ctrlName << "' -> 0x" << std::hex << qctrl.id << std::dec << " (Camera class)" << std::endl;
                return qctrl.id;
            }
        }
        qctrl.id++;
    }

    if (_verbose) std::cout << "[DEBUG] V4L2: resolveControlId '" << nameOrId << "' not found" << std::endl;
    return 0; // Not found
}

// ============================================================================
// setControl
// ============================================================================
bool V4L2DeviceManager::setControl(const std::string& devicePath, const std::string& controlNameOrId, int value) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _sessions.find(devicePath);
    if (it == _sessions.end()) {
        SystemLogger::error(LOG_COMPONENT, "Device not open: " + devicePath);
        return false;
    }

    uint32_t cid = resolveControlId(it->second.fd, controlNameOrId);
    if (cid == 0) {
        SystemLogger::error(LOG_COMPONENT, "Control not found: " + controlNameOrId);
        return false;
    }
    if (_verbose) {
        std::cout << "[DEBUG] V4L2: setControl '" << controlNameOrId
                  << "' -> CID=0x" << std::hex << cid << std::dec
                  << " value=" << value << std::endl;
    }

    struct v4l2_control ctrl{};
    ctrl.id = cid;
    ctrl.value = value;
    if (xioctl(it->second.fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        if (_verbose) std::cout << "[DEBUG] V4L2: VIDIOC_S_CTRL failed, trying ext controls" << std::endl;
        // Fallback to extended controls for camera-class
        struct v4l2_ext_control extCtrl{};
        extCtrl.id = cid;
        extCtrl.value = value;

        struct v4l2_ext_controls extCtrls{};
        extCtrls.count = 1;
        extCtrls.controls = &extCtrl;
        extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

        if (xioctl(it->second.fd, VIDIOC_S_EXT_CTRLS, &extCtrls) < 0) {
            SystemLogger::error(LOG_COMPONENT, "Failed to set control " + controlNameOrId + ": " + std::string(strerror(errno)));
            return false;
        }
        if (_verbose) std::cout << "[DEBUG] V4L2: setControl via VIDIOC_S_EXT_CTRLS ok" << std::endl;
    } else if (_verbose) {
        std::cout << "[DEBUG] V4L2: setControl via VIDIOC_S_CTRL ok" << std::endl;
    }
    return true;
}

// ============================================================================
// getControl
// ============================================================================
int V4L2DeviceManager::getControl(const std::string& devicePath, const std::string& controlNameOrId) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _sessions.find(devicePath);
    if (it == _sessions.end()) {
        SystemLogger::error(LOG_COMPONENT, "Device not open: " + devicePath);
        return -1;
    }

    uint32_t cid = resolveControlId(it->second.fd, controlNameOrId);
    if (cid == 0) {
        SystemLogger::error(LOG_COMPONENT, "Control not found: " + controlNameOrId);
        return -1;
    }

    struct v4l2_control ctrl{};
    ctrl.id = cid;
    if (xioctl(it->second.fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        SystemLogger::error(LOG_COMPONENT, "Failed to get control " + controlNameOrId + ": " + std::string(strerror(errno)));
        return -1;
    }
    if (_verbose) {
        std::cout << "[DEBUG] V4L2: getControl '" << controlNameOrId
                  << "' -> CID=0x" << std::hex << cid << std::dec
                  << " value=" << ctrl.value << std::endl;
    }
    return ctrl.value;
}

// ============================================================================
// listControls
// ============================================================================
void V4L2DeviceManager::listControls(const std::string& devicePath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _sessions.find(devicePath);
    if (it == _sessions.end()) {
        SystemLogger::error(LOG_COMPONENT, "Device not open: " + devicePath);
        return;
    }
    int fd = it->second.fd;

    std::cout << "\n=== V4L2 Controls for " << devicePath << " ===" << std::endl;

    auto enumRange = [&](uint32_t startId, uint32_t endId, const char* label) {
        std::cout << "--- " << label << " ---" << std::endl;
        struct v4l2_queryctrl qctrl{};
        for (uint32_t id = startId; id < endId; ++id) {
            qctrl.id = id;
            if (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
                if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;

                struct v4l2_control ctrl{};
                ctrl.id = qctrl.id;
                int curVal = 0;
                if (xioctl(fd, VIDIOC_G_CTRL, &ctrl) == 0) {
                    curVal = ctrl.value;
                }

                std::cout << "  " << qctrl.name
                          << "  min=" << qctrl.minimum
                          << "  max=" << qctrl.maximum
                          << "  step=" << qctrl.step
                          << "  default=" << qctrl.default_value
                          << "  current=" << curVal
                          << std::endl;
            }
        }
    };

    enumRange(V4L2_CID_BASE, V4L2_CID_LASTP1, "User Controls");
    enumRange(V4L2_CID_CAMERA_CLASS_BASE, V4L2_CID_CAMERA_CLASS_BASE + 64, "Camera Controls");

    std::cout << "=========================================\n" << std::endl;
}

// ============================================================================
// listFormats
// ============================================================================
void V4L2DeviceManager::listFormats(const std::string& devicePath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _sessions.find(devicePath);
    int fd = -1;
    bool tempOpen = false;

    if (it != _sessions.end()) {
        fd = it->second.fd;
    } else {
        fd = ::open(devicePath.c_str(), O_RDWR);
        if (fd < 0) {
            SystemLogger::error(LOG_COMPONENT, "Cannot open " + devicePath + " for format enumeration");
            return;
        }
        tempOpen = true;
    }

    std::cout << "\n=== V4L2 Formats for " << devicePath << " ===" << std::endl;

    struct v4l2_fmtdesc fmtdesc{};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    while (xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        std::cout << "  [" << fmtdesc.index << "] " << fmtdesc.description
                  << " (" << fourccToString(fmtdesc.pixelformat) << ")" << std::endl;

        // Enumerate frame sizes
        struct v4l2_frmsizeenum frmsize{};
        frmsize.pixel_format = fmtdesc.pixelformat;
        frmsize.index = 0;
        while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                std::cout << "    " << frmsize.discrete.width << "x" << frmsize.discrete.height << std::endl;
            } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                       frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                std::cout << "    " << frmsize.stepwise.min_width << "x" << frmsize.stepwise.min_height
                          << " to " << frmsize.stepwise.max_width << "x" << frmsize.stepwise.max_height
                          << " step " << frmsize.stepwise.step_width << "x" << frmsize.stepwise.step_height
                          << std::endl;
            }
            frmsize.index++;
        }
        fmtdesc.index++;
    }

    std::cout << "=========================================\n" << std::endl;

    if (tempOpen) ::close(fd);
}

// ============================================================================
// getBayerPattern
// ============================================================================
std::string V4L2DeviceManager::getBayerPattern(const std::string& devicePath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _sessions.find(devicePath);
    if (it == _sessions.end()) return "";

    uint32_t pf = it->second.negotiatedPixFmt;
    if (_verbose) std::cout << "[DEBUG] V4L2: getBayerPattern pixfmt=" << fourccToString(pf) << std::endl;

    // BGGR variants
    if (pf == V4L2_PIX_FMT_SBGGR8 || pf == V4L2_PIX_FMT_SBGGR10P ||
        pf == V4L2_PIX_FMT_SBGGR12P || pf == V4L2_PIX_FMT_SBGGR16) {
        return "BGGR";
    }
    // GBRG variants
    if (pf == V4L2_PIX_FMT_SGBRG8 || pf == V4L2_PIX_FMT_SGBRG10P ||
        pf == V4L2_PIX_FMT_SGBRG12P || pf == V4L2_PIX_FMT_SGBRG16) {
        return "GBRG";
    }
    // GRBG variants
    if (pf == V4L2_PIX_FMT_SGRBG8 || pf == V4L2_PIX_FMT_SGRBG10P ||
        pf == V4L2_PIX_FMT_SGRBG12P || pf == V4L2_PIX_FMT_SGRBG16) {
        return "GRBG";
    }
    // RGGB variants
    if (pf == V4L2_PIX_FMT_SRGGB8 || pf == V4L2_PIX_FMT_SRGGB10P ||
        pf == V4L2_PIX_FMT_SRGGB12P || pf == V4L2_PIX_FMT_SRGGB16) {
        return "RGGB";
    }

    if (_verbose) std::cout << "[DEBUG] V4L2: getBayerPattern: no Bayer pattern for fmt=" << fourccToString(pf) << std::endl;
    return "";
}

} // namespace visionpipe

#endif // VISIONPIPE_V4L2_NATIVE_ENABLED

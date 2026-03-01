#include "utils/v4l2_device_manager.h"

#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED

#include "utils/Logger.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <linux/videodev2.h>
#include <linux/media.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <poll.h>
#include <unistd.h>
#include <cstring>
#include <climits>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <queue>
#include <set>

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
    // Accept both single-plane and multi-plane capture devices
    bool isMplane = false;
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        isMplane = true;
    } else if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::string err = devicePath + " is not a video capture device (caps=0x" +
                          [&]{ std::ostringstream s; s << std::hex << cap.capabilities; return s.str(); }() + ")";
        SystemLogger::error(LOG_COMPONENT, err);
        if (_verbose) std::cout << "[DEBUG] V4L2: ERROR " << err << std::endl;
        ::close(session.fd);
        session.fd = -1;
        return false;
    }
    session.isMplane = isMplane;
    if (_verbose) std::cout << "[DEBUG] V4L2: device is " << (isMplane ? "MPLANE" : "single-plane") << std::endl;
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
    if (isMplane) {
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = static_cast<uint32_t>(session.config.width);
        fmt.fmt.pix_mp.height = static_cast<uint32_t>(session.config.height);
        fmt.fmt.pix_mp.pixelformat = pixfmt;
        fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
        fmt.fmt.pix_mp.num_planes = 1;
    } else {
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = static_cast<uint32_t>(session.config.width);
        fmt.fmt.pix.height = static_cast<uint32_t>(session.config.height);
        fmt.fmt.pix.pixelformat = pixfmt;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
    }

    if (xioctl(session.fd, VIDIOC_S_FMT, &fmt) < 0) {
        SystemLogger::error(LOG_COMPONENT, "VIDIOC_S_FMT failed: " + std::string(strerror(errno)));
        ::close(session.fd);
        session.fd = -1;
        return false;
    }

    // Read back negotiated values
    if (isMplane) {
        session.negotiatedPixFmt = fmt.fmt.pix_mp.pixelformat;
        session.negotiatedWidth = static_cast<int>(fmt.fmt.pix_mp.width);
        session.negotiatedHeight = static_cast<int>(fmt.fmt.pix_mp.height);
        session.negotiatedBytesPerLine = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        session.negotiatedSizeImage = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
        // Store per-plane bytesperline for correct strided access during frame conversion
        uint32_t numFmtPlanes = std::max(1u, static_cast<uint32_t>(fmt.fmt.pix_mp.num_planes));
        session.planeBytesPerLine.resize(numFmtPlanes);
        for (uint32_t p = 0; p < numFmtPlanes; ++p)
            session.planeBytesPerLine[p] = fmt.fmt.pix_mp.plane_fmt[p].bytesperline;
    } else {
        session.negotiatedPixFmt = fmt.fmt.pix.pixelformat;
        session.negotiatedWidth = static_cast<int>(fmt.fmt.pix.width);
        session.negotiatedHeight = static_cast<int>(fmt.fmt.pix.height);
        session.negotiatedBytesPerLine = fmt.fmt.pix.bytesperline;
        session.negotiatedSizeImage = fmt.fmt.pix.sizeimage;
        session.planeBytesPerLine = {fmt.fmt.pix.bytesperline};
    }

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
    parm.type = isMplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
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
    reqbufs.type = isMplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
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

    // 6. VIDIOC_QUERYBUF + mmap each buffer (all planes), then VIDIOC_QBUF
    static constexpr uint32_t MAX_PLANES = 4;
    session.buffers.resize(reqbufs.count);
    for (uint32_t i = 0; i < reqbufs.count; ++i) {
        struct v4l2_plane queryPlanes[MAX_PLANES]{};
        struct v4l2_buffer buf{};
        buf.type = isMplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (isMplane) {
            buf.m.planes = queryPlanes;
            buf.length = MAX_PLANES; // kernel fills with actual num_planes on return
        }

        if (xioctl(session.fd, VIDIOC_QUERYBUF, &buf) < 0) {
            SystemLogger::error(LOG_COMPONENT, "VIDIOC_QUERYBUF failed for index " + std::to_string(i));
            ::close(session.fd);
            session.fd = -1;
            return false;
        }

        // For MPLANE, buf.length contains the actual number of data planes after QUERYBUF.
        uint32_t numPlanes = isMplane ? buf.length : 1;
        session.buffers[i].planes.resize(numPlanes);

        for (uint32_t p = 0; p < numPlanes; ++p) {
            size_t planeLength = isMplane ? queryPlanes[p].length : buf.length;
            size_t planeOffset = isMplane ? queryPlanes[p].m.mem_offset : buf.m.offset;
            void* addr = mmap(nullptr, planeLength, PROT_READ | PROT_WRITE,
                              MAP_SHARED, session.fd, static_cast<off_t>(planeOffset));
            if (addr == MAP_FAILED) {
                SystemLogger::error(LOG_COMPONENT, "mmap failed for buffer " + std::to_string(i)
                                    + " plane " + std::to_string(p));
                ::close(session.fd);
                session.fd = -1;
                return false;
            }
            session.buffers[i].planes[p] = {addr, planeLength};
            if (_verbose) {
                std::cout << "[DEBUG] V4L2: buffer[" << i << "] plane[" << p
                          << "] mmap offset=" << planeOffset
                          << " length=" << planeLength
                          << " addr=" << addr << std::endl;
            }
        }

        // Enqueue buffer
        if (isMplane) {
            buf.m.planes = queryPlanes;
            buf.length = numPlanes;
        }
        if (xioctl(session.fd, VIDIOC_QBUF, &buf) < 0) {
            SystemLogger::error(LOG_COMPONENT, "VIDIOC_QBUF failed for buffer " + std::to_string(i));
            ::close(session.fd);
            session.fd = -1;
            return false;
        }
        if (_verbose) std::cout << "[DEBUG] V4L2: buffer[" << i << "] QBUF enqueued (" << numPlanes << " plane(s))" << std::endl;
    }

    // 7. VIDIOC_STREAMON
    enum v4l2_buf_type type = isMplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
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
    static constexpr uint32_t MAX_PLANES = 4;
    struct v4l2_plane dqPlanes[MAX_PLANES]{};
    struct v4l2_buffer buf{};
    buf.type = session.isMplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (session.isMplane) {
        buf.m.planes = dqPlanes;
        buf.length = MAX_PLANES;
    }
    if (xioctl(session.fd, VIDIOC_DQBUF, &buf) < 0) {
        SystemLogger::error(LOG_COMPONENT, "VIDIOC_DQBUF failed: " + std::string(strerror(errno)));
        return false;
    }

    uint32_t bytesUsed = session.isMplane ? dqPlanes[0].bytesused : buf.bytesused;
    const auto& mappedBuf = session.buffers[buf.index];
    const uint8_t* data = static_cast<const uint8_t*>(mappedBuf.planes[0].start);
    uint32_t numDataPlanes = static_cast<uint32_t>(mappedBuf.planes.size());
    int w = session.negotiatedWidth;
    int h = session.negotiatedHeight;
    uint32_t pf = session.negotiatedPixFmt;
    if (_verbose) {
        std::cout << "[DEBUG] V4L2: DQBUF index=" << buf.index
                  << " planes=" << numDataPlanes
                  << " bytesused=" << bytesUsed
                  << " fmt=" << fourccToString(pf)
                  << " size=" << w << "x" << h << std::endl;
    }

    // 3. Convert raw buffer to cv::Mat
    // stride0/stride1 are the actual bytes-per-row for plane 0 and plane 1 respectively.
    // Passing them to cv::Mat prevents row-shear artefacts when bytesperline != width*bpp.
    size_t stride0 = session.planeBytesPerLine.size() > 0 ? session.planeBytesPerLine[0] : static_cast<size_t>(w);
    size_t stride1 = session.planeBytesPerLine.size() > 1 ? session.planeBytesPerLine[1] : stride0;
    bool ok = true;
    if (pf == V4L2_PIX_FMT_YUYV) {
        cv::Mat yuyv(h, w, CV_8UC2, const_cast<uint8_t*>(data), stride0);
        cv::cvtColor(yuyv, frame, cv::COLOR_YUV2BGR_YUY2);
    } else if (pf == V4L2_PIX_FMT_UYVY) {
        cv::Mat uyvy(h, w, CV_8UC2, const_cast<uint8_t*>(data), stride0);
        cv::cvtColor(uyvy, frame, cv::COLOR_YUV2BGR_UYVY);
    } else if (pf == V4L2_PIX_FMT_MJPEG) {
        std::vector<uint8_t> jpgBuf(data, data + bytesUsed);
        frame = cv::imdecode(jpgBuf, cv::IMREAD_COLOR);
        if (frame.empty()) ok = false;
    } else if (pf == V4L2_PIX_FMT_NV12 || pf == V4L2_PIX_FMT_NV21) {
        int cvCode = (pf == V4L2_PIX_FMT_NV12) ? cv::COLOR_YUV2BGR_NV12 : cv::COLOR_YUV2BGR_NV21;
        if (numDataPlanes >= 2) {
            // Y and UV/VU in separate mmap regions — copy row-by-row into compact temp
            const uint8_t* yPlane  = static_cast<const uint8_t*>(mappedBuf.planes[0].start);
            const uint8_t* uvPlane = static_cast<const uint8_t*>(mappedBuf.planes[1].start);
            cv::Mat tmp(h * 3 / 2, w, CV_8UC1);
            for (int row = 0; row < h; ++row)
                memcpy(tmp.ptr(row), yPlane + row * stride0, static_cast<size_t>(w));
            for (int row = 0; row < h / 2; ++row)
                memcpy(tmp.ptr(h + row), uvPlane + row * stride1, static_cast<size_t>(w));
            cv::cvtColor(tmp, frame, cvCode);
        } else {
            // Single contiguous buffer — pass stride so OpenCV locates rows correctly
            cv::Mat nv(h * 3 / 2, w, CV_8UC1, const_cast<uint8_t*>(data), stride0);
            cv::cvtColor(nv, frame, cvCode);
        }
    } else if (pf == V4L2_PIX_FMT_YUV420) {
        if (numDataPlanes >= 3) {
            // Y, U, V in separate mmap regions (I420); U/V planes are half-width
            const uint8_t* yPlane = static_cast<const uint8_t*>(mappedBuf.planes[0].start);
            const uint8_t* uPlane = static_cast<const uint8_t*>(mappedBuf.planes[1].start);
            const uint8_t* vPlane = static_cast<const uint8_t*>(mappedBuf.planes[2].start);
            size_t strideU = session.planeBytesPerLine.size() > 1 ? session.planeBytesPerLine[1] : stride0 / 2;
            size_t strideV = session.planeBytesPerLine.size() > 2 ? session.planeBytesPerLine[2] : stride0 / 2;
            cv::Mat tmp(h * 3 / 2, w, CV_8UC1);
            for (int row = 0; row < h; ++row)
                memcpy(tmp.ptr(row), yPlane + row * stride0, static_cast<size_t>(w));
            for (int row = 0; row < h / 2; ++row)
                memcpy(tmp.ptr(h + row), uPlane + row * strideU, static_cast<size_t>(w / 2));
            for (int row = 0; row < h / 2; ++row)
                memcpy(tmp.ptr(h + h / 4 + row), vPlane + row * strideV, static_cast<size_t>(w / 2));
            cv::cvtColor(tmp, frame, cv::COLOR_YUV2BGR_I420);
        } else {
            cv::Mat yuv(h * 3 / 2, w, CV_8UC1, const_cast<uint8_t*>(data), stride0);
            cv::cvtColor(yuv, frame, cv::COLOR_YUV2BGR_I420);
        }
    } else if (pf == V4L2_PIX_FMT_BGR24) {
        frame = cv::Mat(h, w, CV_8UC3, const_cast<uint8_t*>(data), stride0).clone();
    } else if (pf == V4L2_PIX_FMT_RGB24) {
        cv::Mat rgb(h, w, CV_8UC3, const_cast<uint8_t*>(data), stride0);
        cv::cvtColor(rgb, frame, cv::COLOR_RGB2BGR);
    }
    // 8-bit Bayer — return compact CV_8UC1 (user applies debayer)
    else if (pf == V4L2_PIX_FMT_SBGGR8 || pf == V4L2_PIX_FMT_SGBRG8 ||
             pf == V4L2_PIX_FMT_SGRBG8 || pf == V4L2_PIX_FMT_SRGGB8) {
        frame = cv::Mat(h, w, CV_8UC1, const_cast<uint8_t*>(data), stride0).clone();
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
    // 16-bit Bayer — return compact CV_16UC1
    else if (pf == V4L2_PIX_FMT_SBGGR16 || pf == V4L2_PIX_FMT_SGBRG16 ||
             pf == V4L2_PIX_FMT_SGRBG16 || pf == V4L2_PIX_FMT_SRGGB16) {
        frame = cv::Mat(h, w, CV_16UC1, const_cast<uint8_t*>(data), stride0).clone();
    } else {
        SystemLogger::error(LOG_COMPONENT, "Unsupported pixel format for conversion: " + fourccToString(pf));
        ok = false;
    }

    // 4. Re-enqueue buffer
    if (session.isMplane) {
        buf.m.planes = dqPlanes;
        buf.length = numDataPlanes;
    }
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
        enum v4l2_buf_type type = session.isMplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(session.fd, VIDIOC_STREAMOFF, &type);
        session.streaming = false;
    }

    int bufIdx = 0;
    for (auto& mb : session.buffers) {
        int planeIdx = 0;
        for (auto& plane : mb.planes) {
            if (plane.start && plane.start != MAP_FAILED) {
                if (_verbose) std::cout << "[DEBUG] V4L2: munmap buffer[" << bufIdx << "] plane[" << planeIdx << "] addr=" << plane.start << " len=" << plane.length << std::endl;
                munmap(plane.start, plane.length);
            }
            ++planeIdx;
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
// setPreferredSubDev / clearPreferredSubDev
// ============================================================================
void V4L2DeviceManager::setPreferredSubDev(const std::string& videoDevPath,
                                            const std::string& subDevPath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _preferredSubDevs[videoDevPath] = subDevPath;
    if (_verbose)
        std::cout << "[DEBUG] V4L2: preferred subdev for " << videoDevPath
                  << " set to " << subDevPath << std::endl;
}

void V4L2DeviceManager::clearPreferredSubDev(const std::string& videoDevPath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _preferredSubDevs.erase(videoDevPath);
}

// ============================================================================
// acquireFd — return an open file descriptor for devicePath.
// Uses an existing session fd if available; otherwise opens ephemerally.
// Sets *tempOpened = true when the caller must ::close() the fd afterwards.
// ============================================================================
int V4L2DeviceManager::acquireFd(const std::string& devicePath, bool* tempOpened) const {
    *tempOpened = false;
    auto it = _sessions.find(devicePath);
    if (it != _sessions.end() && it->second.fd >= 0) {
        return it->second.fd;
    }
    // Ephemeral open — works for /dev/v4l-subdev* and un-setup video nodes
    int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
        *tempOpened = true;
        if (_verbose)
            std::cout << "[DEBUG] V4L2: acquireFd ephemeral open " << devicePath
                      << " fd=" << fd << std::endl;
    }
    return fd;
}

// ============================================================================
// findLinkedSubDevices — discover /dev/v4l-subdev* nodes reachable upstream
// from a video node through the V4L2 media controller (/dev/media*).
//
// On simple platforms (USB UVC, etc.) the sensor subdev is directly linked
// to the video node and is returned immediately (backward-compatible).
//
// On complex ISP pipelines (Qualcomm MSM, Rockchip ISP, etc.) the video node
// is connected to an ISP/VFE intermediate subdev which is itself fed by the
// actual sensor through CSIPHY and other blocks.  A simple 1-hop search
// would return the ISP subdev (which has no sensor controls), so instead we
// perform a BFS upstream traversal of the entire media-graph and collect
// every subdev reachable via enabled links in the source→sink direction.
// Results are returned deepest-first so the actual sensor (pipeline source,
// no further upstream neighbours) appears at the front of the list, giving
// callers the best chance of finding sensor controls on the first attempt.
// ============================================================================
std::vector<std::string> V4L2DeviceManager::findLinkedSubDevices(
    const std::string& videoDevPath) const
{
    std::vector<std::string> result;

    // 1. Get major:minor of the target video device
    struct stat vst{};
    if (::stat(videoDevPath.c_str(), &vst) != 0) return result;
    unsigned int vmaj = major(vst.st_rdev);
    unsigned int vmin = minor(vst.st_rdev);

    // 2. Build a map: (major, minor) -> /dev/v4l-subdev* path
    std::map<std::pair<uint32_t, uint32_t>, std::string> subdevByDevno;
    for (int i = 0; i < 64; i++) {
        std::string p = "/dev/v4l-subdev" + std::to_string(i);
        struct stat sst{};
        if (::stat(p.c_str(), &sst) == 0) {
            subdevByDevno[{major(sst.st_rdev), minor(sst.st_rdev)}] = p;
        }
    }

    // 3. Scan /dev/media* devices
    for (int mi = 0; mi < 16; mi++) {
        std::string mediaPath = "/dev/media" + std::to_string(mi);
        int mfd = ::open(mediaPath.c_str(), O_RDWR | O_NONBLOCK);
        if (mfd < 0) continue;

        // 3a. Enumerate ALL entities; identify our target and build entity table.
        struct EntityInfo {
            uint32_t id = 0;
            char     name[32]{};
            uint32_t devMajor = 0;
            uint32_t devMinor = 0;
            uint32_t numPads  = 0;
            uint32_t numLinks = 0;
        };
        std::map<uint32_t, EntityInfo> entities;
        uint32_t targetEntityId = 0;

        {
            struct media_entity_desc ent{};
            ent.id = 0 | MEDIA_ENT_ID_FLAG_NEXT;
            while (ioctl(mfd, MEDIA_IOC_ENUM_ENTITIES, &ent) == 0) {
                EntityInfo info;
                info.id       = ent.id;
                info.devMajor = ent.dev.major;
                info.devMinor = ent.dev.minor;
                info.numPads  = ent.pads;
                info.numLinks = ent.links;
                std::memcpy(info.name, ent.name,
                            std::min(sizeof(info.name), sizeof(ent.name)));
                info.name[sizeof(info.name) - 1] = '\0';
                entities[ent.id] = info;

                if (ent.dev.major == vmaj && ent.dev.minor == vmin) {
                    targetEntityId = ent.id;
                    if (_verbose)
                        std::cout << "[DEBUG] V4L2: media entity for " << videoDevPath
                                  << " id=" << targetEntityId
                                  << " name='" << ent.name << "'" << std::endl;
                }
                ent.id |= MEDIA_ENT_ID_FLAG_NEXT;
            }
        }

        if (targetEntityId == 0) {
            ::close(mfd);
            continue;
        }

        // 3b. Build an upstream adjacency map from enabled links.
        //     upstream[sink_entity_id] contains all source_entity_ids.
        //     MEDIA_IOC_ENUM_LINKS on entity X returns all links touching X
        //     (both as source and as sink), so processing every entity once
        //     is sufficient to discover all edges; duplicates are harmless
        //     because BFS tracks visited nodes.
        std::map<uint32_t, std::vector<uint32_t>> upstream;

        for (auto& [entId, info] : entities) {
            if (info.numLinks == 0) continue;
            std::vector<struct media_pad_desc>  pads(info.numPads ? info.numPads : 1);
            std::vector<struct media_link_desc> links(info.numLinks);
            struct media_links_enum le{};
            le.entity = entId;
            le.pads   = info.numPads ? pads.data()  : nullptr;
            le.links  = links.data();
            if (ioctl(mfd, MEDIA_IOC_ENUM_LINKS, &le) != 0) continue;

            for (auto& lnk : links) {
                if (!(lnk.flags & MEDIA_LNK_FL_ENABLED)) continue;
                // lnk.source sends data to lnk.sink
                upstream[lnk.sink.entity].push_back(lnk.source.entity);
            }
        }

        // 3c. BFS upstream from the video entity.
        //     Collect every subdev entity encountered (in BFS / breadth-first order,
        //     closest to video node first).
        std::vector<std::string> bfsSubdevs; // closest-first
        std::set<uint32_t>       visited;
        std::queue<uint32_t>     queue;

        queue.push(targetEntityId);
        visited.insert(targetEntityId);

        while (!queue.empty()) {
            uint32_t cur = queue.front();
            queue.pop();

            auto upIt = upstream.find(cur);
            if (upIt == upstream.end()) continue;

            for (uint32_t srcId : upIt->second) {
                if (visited.count(srcId)) continue;
                visited.insert(srcId);
                queue.push(srcId);

                auto entIt = entities.find(srcId);
                if (entIt == entities.end()) continue;

                auto key = std::make_pair(entIt->second.devMajor,
                                          entIt->second.devMinor);
                auto sdIt = subdevByDevno.find(key);
                if (sdIt == subdevByDevno.end()) continue;

                bfsSubdevs.push_back(sdIt->second);
                if (_verbose)
                    std::cout << "[DEBUG] V4L2: upstream subdev "
                              << sdIt->second
                              << " (entity '" << entIt->second.name << "')"
                              << " -> " << videoDevPath << std::endl;
            }
        }

        ::close(mfd);

        if (!bfsSubdevs.empty()) {
            // Reverse so the deepest upstream entity (sensor source) is first.
            // On a simple platform (1-hop subdev) the order is unchanged.
            result.assign(bfsSubdevs.rbegin(), bfsSubdevs.rend());
            break; // Stop at the first media device that resolved our node
        }
    }

    return result;
}

// ============================================================================
// Uses V4L2_CTRL_FLAG_NEXT_CTRL to enumerate ALL control classes:
// User, Camera, Image Source, Image Processing, etc.
// ============================================================================
uint32_t V4L2DeviceManager::resolveControlId(int fd, const std::string& nameOrId) const {
    // Numeric parse first (decimal or "0x..." hex) — stoul with base 0 handles both
    try {
        size_t pos = 0;
        unsigned long val = std::stoul(nameOrId, &pos, 0);
        if (pos == nameOrId.size())  // fully consumed = pure number
            return static_cast<uint32_t>(val);
    } catch (...) {}

    // Prepare lower-case and underscore-normalised variants for flexible matching
    auto normalise = [](const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        std::replace(r.begin(), r.end(), ' ', '_');
        return r;
    };
    std::string wantLower = normalise(nameOrId);

    if (_verbose)
        std::cout << "[DEBUG] V4L2: resolveControlId scanning all controls for '"
                  << nameOrId << "'" << std::endl;

    // Walk every control on this fd using NEXT_CTRL (covers ALL classes)
    struct v4l2_queryctrl qctrl{};
    qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
        if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
            std::string ctrlNorm = normalise(
                std::string(reinterpret_cast<const char*>(qctrl.name)));
            if (ctrlNorm == wantLower) {
                if (_verbose)
                    std::cout << "[DEBUG] V4L2: resolveControlId found '"
                              << qctrl.name << "' -> 0x"
                              << std::hex << qctrl.id << std::dec << std::endl;
                return qctrl.id;
            }
        }
        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    if (_verbose)
        std::cout << "[DEBUG] V4L2: resolveControlId '" << nameOrId << "' not found" << std::endl;
    return 0;
}

// ============================================================================
// setControl
// ============================================================================
bool V4L2DeviceManager::setControl(const std::string& devicePath,
                                    const std::string& controlNameOrId, int value)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (_verbose)
        std::cout << "[DEBUG] V4L2: setControl '" << controlNameOrId
                  << "' = " << value << " on " << devicePath << std::endl;

    // Helper: try to set a control on a specific open fd.
    // Logs errno on each failed ioctl attempt.
    auto trySet = [&](int fd) -> bool {
        uint32_t cid = resolveControlId(fd, controlNameOrId);
        if (cid == 0) return false;

        if (_verbose)
            std::cout << "[DEBUG] V4L2: setControl CID=0x" << std::hex << cid
                      << std::dec << " value=" << value << std::endl;

        struct v4l2_control ctrl{};
        ctrl.id    = cid;
        ctrl.value = value;
        if (xioctl(fd, VIDIOC_S_CTRL, &ctrl) == 0) {
            if (_verbose) std::cout << "[DEBUG] V4L2: setControl VIDIOC_S_CTRL ok" << std::endl;
            return true;
        }
        if (_verbose)
            std::cout << "[DEBUG] V4L2: setControl VIDIOC_S_CTRL failed: "
                      << strerror(errno) << " (" << errno << ")" << std::endl;

        // Fallback: extended controls.
        // First attempt with ctrl_class=0 (which=V4L2_CTRL_WHICH_CUR_VAL=0) — the most
        // permissive form and the only one accepted by many subdev drivers.
        // Then retry with explicit class values for older kernels/drivers.
        const uint32_t classes[] = {
            0,  // ctrl_class=0  → kernel uses 'which' field (= V4L2_CTRL_WHICH_CUR_VAL)
            V4L2_CTRL_CLASS_USER,
            V4L2_CTRL_CLASS_CAMERA,
            V4L2_CTRL_CLASS_IMAGE_SOURCE,
            V4L2_CTRL_CLASS_IMAGE_PROC,
        };
        for (uint32_t cls : classes) {
            struct v4l2_ext_control ec{};
            ec.id    = cid;
            ec.value = value;
            struct v4l2_ext_controls ecs{};
            ecs.count      = 1;
            ecs.controls   = &ec;
            ecs.ctrl_class = cls;  // 0 → use .which (V4L2_CTRL_WHICH_CUR_VAL)
            if (xioctl(fd, VIDIOC_S_EXT_CTRLS, &ecs) == 0) {
                if (_verbose)
                    std::cout << "[DEBUG] V4L2: setControl VIDIOC_S_EXT_CTRLS ok"
                              << " (ctrl_class=0x" << std::hex << cls << std::dec << ")" << std::endl;
                return true;
            }
            if (_verbose)
                std::cout << "[DEBUG] V4L2: setControl VIDIOC_S_EXT_CTRLS ctrl_class=0x"
                          << std::hex << cls << std::dec
                          << " failed: " << strerror(errno) << " (" << errno << ")" << std::endl;
        }
        return false;
    };

    // Helper: STREAMOFF the video device, run fn(), re-enqueue all buffers, STREAMON.
    // On complex ISP pipelines many sensor controls (flip, gain, exposure) can only be
    // applied when the sensor pipeline is NOT streaming.  This helper transparently
    // pauses streaming, applies the control, then resumes — invisible to the caller.
    // If the device is not streaming (or STREAMOFF fails) fn() is called directly.
    auto withStreamPaused = [&](const std::string& vidPath,
                                std::function<bool()> fn) -> bool {
        auto sit = _sessions.find(vidPath);
        if (sit == _sessions.end() || !sit->second.streaming)
            return fn();

        V4L2Session& sess = sit->second;
        enum v4l2_buf_type bufType = sess.isMplane
            ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
            : V4L2_BUF_TYPE_VIDEO_CAPTURE;

        // 1. STREAMOFF
        if (xioctl(sess.fd, VIDIOC_STREAMOFF, &bufType) < 0) {
            if (_verbose)
                std::cout << "[DEBUG] V4L2: setControl STREAMOFF failed ("
                          << strerror(errno) << "), trying control anyway" << std::endl;
            return fn();
        }
        sess.streaming = false;
        if (_verbose)
            std::cout << "[DEBUG] V4L2: setControl: stream paused on " << vidPath << std::endl;

        // 2. Apply the control while sensor pipeline is idle
        bool ok = fn();

        // 3. Re-enqueue all mapped buffers
        static constexpr uint32_t kMaxPlanes = 4;
        for (uint32_t i = 0; i < static_cast<uint32_t>(sess.buffers.size()); ++i) {
            struct v4l2_plane planes[kMaxPlanes]{};
            struct v4l2_buffer buf{};
            buf.type   = bufType;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index  = i;
            uint32_t np = static_cast<uint32_t>(sess.buffers[i].planes.size());
            if (sess.isMplane) {
                buf.m.planes = planes;
                buf.length   = np;
            }
            xioctl(sess.fd, VIDIOC_QBUF, &buf); // best-effort
        }

        // 4. STREAMON
        if (xioctl(sess.fd, VIDIOC_STREAMON, &bufType) == 0) {
            sess.streaming = true;
            if (_verbose)
                std::cout << "[DEBUG] V4L2: setControl: stream resumed on " << vidPath << std::endl;
        } else {
            SystemLogger::error(LOG_COMPONENT,
                "STREAMON failed after subdev control on " + vidPath + ": " +
                std::string(strerror(errno)));
        }
        return ok;
    };

    // 1. Try primary device (session fd or ephemeral open)
    bool temp = false;
    int fd = acquireFd(devicePath, &temp);
    bool ok = false;
    if (fd >= 0) {
        ok = trySet(fd);
        if (temp) ::close(fd);
    }
    if (ok) return true;

    // 2. Try preferred subdev if pinned (set by v4l2_setup or explicit override).
    //    Skip full BFS when user has already identified the correct sensor subdev.
    auto prefIt = _preferredSubDevs.find(devicePath);
    if (prefIt != _preferredSubDevs.end()) {
        const std::string& prefPath = prefIt->second;
        if (_verbose)
            std::cout << "[DEBUG] V4L2: setControl using preferred subdev " << prefPath << std::endl;
        bool ptmp = false;
        int pfd = acquireFd(prefPath, &ptmp);
        if (pfd >= 0) {
            // Use stream-pause wrapper: many sensor controls (flip, gain, exposure)
            // can only be applied while the pipeline is NOT streaming.
            ok = withStreamPaused(devicePath, [&] { return trySet(pfd); });
            if (ptmp) ::close(pfd);
            if (ok) {
                SystemLogger::info(LOG_COMPONENT,
                    "Set '" + controlNameOrId + "' on preferred sub-device " + prefPath);
                return true;
            }
        }
    }

    // 3. Auto-discover linked sub-devices via media-controller BFS and retry.
    auto subdevs = findLinkedSubDevices(devicePath);
    if (subdevs.empty() && _verbose)
        std::cout << "[DEBUG] V4L2: no linked sub-devices found for " << devicePath << std::endl;

    for (const auto& subPath : subdevs) {
        // Skip if already tried as preferred
        if (prefIt != _preferredSubDevs.end() && subPath == prefIt->second) continue;
        if (_verbose)
            std::cout << "[DEBUG] V4L2: retrying control on subdev " << subPath << std::endl;
        bool stmp = false;
        int sfd = acquireFd(subPath, &stmp);
        if (sfd < 0) continue;
        // Use stream-pause wrapper for the same reason as above
        ok = withStreamPaused(devicePath, [&] { return trySet(sfd); });
        if (stmp) ::close(sfd);
        if (ok) {
            SystemLogger::info(LOG_COMPONENT,
                "Set '" + controlNameOrId + "' on sub-device " + subPath);
            return true;
        }
    }

    SystemLogger::error(LOG_COMPONENT,
        "Failed to set control '" + controlNameOrId + "' on "
        + devicePath + " or any linked sub-device");
    return false;
}

// ============================================================================
// getControl
// ============================================================================
int V4L2DeviceManager::getControl(const std::string& devicePath,
                                   const std::string& controlNameOrId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    auto tryGet = [&](int fd) -> int {
        uint32_t cid = resolveControlId(fd, controlNameOrId);
        if (cid == 0) return INT_MIN;
        struct v4l2_control ctrl{};
        ctrl.id = cid;
        if (xioctl(fd, VIDIOC_G_CTRL, &ctrl) == 0) return ctrl.value;
        // Try extended
        struct v4l2_ext_control ec{};
        ec.id = cid;
        struct v4l2_ext_controls ecs{};
        ecs.count    = 1;
        ecs.controls = &ec;
        ecs.which    = V4L2_CTRL_WHICH_CUR_VAL;
        if (xioctl(fd, VIDIOC_G_EXT_CTRLS, &ecs) == 0) return ec.value;
        return INT_MIN;
    };

    // 1. Primary device
    bool temp = false;
    int fd = acquireFd(devicePath, &temp);
    int val = INT_MIN;
    if (fd >= 0) {
        val = tryGet(fd);
        if (temp) ::close(fd);
    }
    if (val != INT_MIN) return val;

    // 2. Try preferred subdev first
    auto prefIt = _preferredSubDevs.find(devicePath);
    if (prefIt != _preferredSubDevs.end()) {
        const std::string& prefPath = prefIt->second;
        bool ptmp = false;
        int pfd = acquireFd(prefPath, &ptmp);
        if (pfd >= 0) {
            val = tryGet(pfd);
            if (ptmp) ::close(pfd);
            if (val != INT_MIN) return val;
        }
    }

    // 3. Linked sub-devices via BFS
    for (const auto& subPath : findLinkedSubDevices(devicePath)) {
        if (prefIt != _preferredSubDevs.end() && subPath == prefIt->second) continue;
        bool stmp = false;
        int sfd = acquireFd(subPath, &stmp);
        if (sfd < 0) continue;
        val = tryGet(sfd);
        if (stmp) ::close(sfd);
        if (val != INT_MIN) return val;
    }

    SystemLogger::error(LOG_COMPONENT,
        "Control not found: '" + controlNameOrId + "' on " + devicePath);
    return -1;
}

// ============================================================================
// listControls
// ============================================================================
void V4L2DeviceManager::listControls(const std::string& devicePath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    std::cout << "\n=== V4L2 Controls for " << devicePath << " ===" << std::endl;

    // Helper: print all controls on a given fd
    auto printControls = [&](int fd, const std::string& label) {
        bool any = false;
        struct v4l2_queryctrl qctrl{};
        qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        while (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
            if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
                if (!any) {
                    std::cout << "--- " << label << " ---" << std::endl;
                    any = true;
                }

                // Read current value
                int curVal = 0;
                struct v4l2_control ctrl{};
                ctrl.id = qctrl.id;
                if (xioctl(fd, VIDIOC_G_CTRL, &ctrl) == 0) {
                    curVal = ctrl.value;
                } else {
                    // Try extended get
                    struct v4l2_ext_control ec{};
                    ec.id = qctrl.id;
                    struct v4l2_ext_controls ecs{};
                    ecs.count    = 1;
                    ecs.controls = &ec;
                    ecs.which    = V4L2_CTRL_WHICH_CUR_VAL;
                    if (xioctl(fd, VIDIOC_G_EXT_CTRLS, &ecs) == 0) curVal = ec.value;
                }

                printf("  %-32s  id=0x%08x  min=%-6d  max=%-6d  step=%-5d  def=%-6d  cur=%d\n",
                    qctrl.name,
                    qctrl.id,
                    qctrl.minimum,
                    qctrl.maximum,
                    qctrl.step,
                    qctrl.default_value,
                    curVal);
            }
            qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
        return any;
    };

    // 1. Primary device (ephemeral or session fd)
    bool temp = false;
    int fd = acquireFd(devicePath, &temp);
    bool foundAny = false;
    if (fd >= 0) {
        foundAny = printControls(fd, devicePath);
        if (temp) ::close(fd);
    } else {
        std::cout << "(could not open " << devicePath << ")" << std::endl;
    }

    // 2. Linked sub-devices
    auto subdevs = findLinkedSubDevices(devicePath);
    for (const auto& subPath : subdevs) {
        bool stmp = false;
        int sfd = acquireFd(subPath, &stmp);
        if (sfd < 0) continue;
        bool had = printControls(sfd, subPath);
        if (stmp) ::close(sfd);
        foundAny = foundAny || had;
    }

    if (!foundAny)
        std::cout << "  (no controls found on " << devicePath
                  << " or its sub-devices)" << std::endl;

    std::cout << "=========================================\n" << std::endl;
}

// ============================================================================
// listFormats
// ============================================================================
void V4L2DeviceManager::listFormats(const std::string& devicePath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    bool tempOpen = false;
    int fd = acquireFd(devicePath, &tempOpen);
    if (fd < 0) {
        SystemLogger::error(LOG_COMPONENT,
            "Cannot open " + devicePath + " for format enumeration");
        return;
    }

    std::cout << "\n=== V4L2 Formats for " << devicePath << " ===" << std::endl;

    // Try both single-plane and multi-plane capture types
    const uint32_t bufTypes[] = {
        V4L2_BUF_TYPE_VIDEO_CAPTURE,
        V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
    };

    bool foundAny = false;
    for (uint32_t bufType : bufTypes) {
        struct v4l2_fmtdesc fmtdesc{};
        fmtdesc.type  = bufType;
        fmtdesc.index = 0;
        bool firstOfType = true;
        while (xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
            if (firstOfType) {
                std::cout << "  [" << (bufType == V4L2_BUF_TYPE_VIDEO_CAPTURE
                               ? "single-plane" : "multi-plane") << "]" << std::endl;
                firstOfType = false;
                foundAny = true;
            }
            std::cout << "  [" << fmtdesc.index << "] "
                      << fmtdesc.description
                      << " (" << fourccToString(fmtdesc.pixelformat) << ")"
                      << std::endl;

            // Enumerate discrete / stepwise frame sizes
            struct v4l2_frmsizeenum frmsize{};
            frmsize.pixel_format = fmtdesc.pixelformat;
            frmsize.index = 0;
            while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
                if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    // Also enumerate frame rates for this size
                    std::cout << "    "
                              << frmsize.discrete.width << "x"
                              << frmsize.discrete.height;

                    struct v4l2_frmivalenum frmival{};
                    frmival.pixel_format = fmtdesc.pixelformat;
                    frmival.width        = frmsize.discrete.width;
                    frmival.height       = frmsize.discrete.height;
                    frmival.index        = 0;
                    bool firstFps = true;
                    while (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                        if (firstFps) { std::cout << "  @"; firstFps = false; }
                        else std::cout << ",";
                        if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                            if (frmival.discrete.numerator > 0)
                                std::cout << (frmival.discrete.denominator /
                                              frmival.discrete.numerator) << "fps";
                        }
                        frmival.index++;
                    }
                    std::cout << std::endl;
                } else {
                    std::cout << "    "
                              << frmsize.stepwise.min_width << "x"
                              << frmsize.stepwise.min_height
                              << " to "
                              << frmsize.stepwise.max_width << "x"
                              << frmsize.stepwise.max_height
                              << " step "
                              << frmsize.stepwise.step_width << "x"
                              << frmsize.stepwise.step_height
                              << std::endl;
                }
                frmsize.index++;
            }
            fmtdesc.index++;
        }
    }

    if (!foundAny)
        std::cout << "  (no formats reported — device may use media-controller pipeline)" << std::endl;

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

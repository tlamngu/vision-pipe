#include "libvisionpipe/discovery.h"
#include "libvisionpipe/frame_transport.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <tlhelp32.h>
#else
  #include <dirent.h>
  #include <fcntl.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #include <signal.h>
  // V4L2 (Linux only)
  #if defined(__linux__)
    #include <linux/videodev2.h>
    #include <sys/ioctl.h>
  #endif
#endif

namespace visionpipe {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// Parse "visionpipe_sink_<sessionId>_<sinkName>" from a bare shm name
// The SHM_PREFIX is "/visionpipe_sink_" on POSIX, "Local\\visionpipe_sink_" on Win.
// 'bare' here means the name stripped of the leading "/" or namespace prefix.
bool parseShmName(const std::string& bare, std::string& sessionId, std::string& sinkName) {
    // Expected pattern: "visionpipe_sink_<sessionId>_<sinkName>"
    const std::string prefix = "visionpipe_sink_";
    if (bare.compare(0, prefix.size(), prefix) != 0) return false;
    std::string rest = bare.substr(prefix.size());
    // Session IDs are "<hex_pid>_<hex_random>" – find the second '_'
    // Strategy: the sink name itself may contain underscores, but session IDs
    // are guaranteed to have exactly one underscore between the two hex parts.
    // So the session ID ends at the third underscore in 'rest'.
    size_t first = rest.find('_');
    if (first == std::string::npos) return false;
    size_t second = rest.find('_', first + 1);
    if (second == std::string::npos) return false;
    sessionId = rest.substr(0, second);
    sinkName  = rest.substr(second + 1);
    return !sinkName.empty();
}

// Open a POSIX shm segment read-only and map just the header.
// Returns a pointer to the mapped memory (must be munmap'd with shmTotalSize()),
// or nullptr on failure.
#if !defined(_WIN32)
const FrameShmHeader* openShmReadOnly(const std::string& shmName) {
    std::string fullName = "/" + shmName;
    int fd = ::shm_open(fullName.c_str(), O_RDONLY, 0444);
    if (fd < 0) return nullptr;
    void* mapped = ::mmap(nullptr, shmTotalSize(), PROT_READ, MAP_SHARED, fd, 0);
    ::close(fd);
    if (mapped == MAP_FAILED) return nullptr;
    const auto* hdr = static_cast<const FrameShmHeader*>(mapped);
    if (hdr->magic != SHM_MAGIC) {
        ::munmap(mapped, shmTotalSize());
        return nullptr;
    }
    return hdr;
}

void closeShmReadOnly(const FrameShmHeader* hdr) {
    if (hdr) ::munmap(const_cast<FrameShmHeader*>(hdr), shmTotalSize());
}

bool isPidAlive(int32_t pid) {
    if (pid <= 0) return true; // unknown – assume alive
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}
#endif // !_WIN32

} // anonymous namespace

// ============================================================================
// discoverSinks
// ============================================================================

std::vector<SinkInfo> discoverSinks() {
    std::vector<SinkInfo> results;

#if defined(_WIN32)
    // On Windows, Named Shared Memory objects live under the "Local\" namespace.
    // We need to enumerate the Local\ object directory.
    // Use NtQueryDirectoryObject (undocumented) or enumerate via toolhelp – neither
    // is straightforward.  Instead, we enumerate running processes, probe their
    // port files, and construct names.  For simplicity on Windows we return an
    // empty list with a note – full Windows support can be added later.
    (void)results;
    return results;

#else
    // Linux/macOS: shm objects live as files in /dev/shm
    const char* shmDir = "/dev/shm";
    DIR* dir = ::opendir(shmDir);
    if (!dir) return results;

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        std::string sessionId, sinkName;
        if (!parseShmName(name, sessionId, sinkName)) continue;

        const FrameShmHeader* hdr = openShmReadOnly(name);
        if (!hdr) continue;

        SinkInfo info;
        info.sessionId    = sessionId;
        info.sinkName     = sinkName;
        info.shmName      = name;
        info.producerPid  = hdr->producerPid;
        info.producerAlive = isPidAlive(hdr->producerPid);

        closeShmReadOnly(hdr);
        results.push_back(std::move(info));
    }
    ::closedir(dir);

    // Stable ordering: session_id then sink_name
    std::sort(results.begin(), results.end(), [](const SinkInfo& a, const SinkInfo& b) {
        if (a.sessionId != b.sessionId) return a.sessionId < b.sessionId;
        return a.sinkName < b.sinkName;
    });

    return results;
#endif
}

// ============================================================================
// sinkProperties
// ============================================================================

SinkProperties sinkProperties(const std::string& sessionId, const std::string& sinkName) {
    SinkProperties props;

#if defined(_WIN32)
    (void)sessionId; (void)sinkName;
    return props; // Windows: not yet implemented
#else
    std::string fullShmName = buildShmName(sessionId, sinkName);
    // buildShmName prepends "/" on POSIX; strip it to get the bare name for /dev/shm
    std::string bareName = fullShmName;
    if (!bareName.empty() && bareName[0] == '/') bareName = bareName.substr(1);

    const FrameShmHeader* hdr = openShmReadOnly(bareName);
    if (!hdr) return props;

    props.isValid      = true;
    props.rows         = hdr->rows;
    props.cols         = hdr->cols;
    props.cvType       = hdr->type;
    props.step         = hdr->step;
    props.dataSize     = hdr->dataSize;
    props.frameSeq     = hdr->frameSeq.load(std::memory_order_relaxed);
    props.producerPid  = hdr->producerPid;
    props.producerAlive = isPidAlive(hdr->producerPid);

    // Derive channel count from OpenCV Mat type encoding: channels = (type >> 3) + 1
    props.channels = (hdr->type >> 3) + 1;

    closeShmReadOnly(hdr);
    return props;
#endif
}

// ============================================================================
// captureCapabilities (V4L2 backend, Linux)
// ============================================================================

#if defined(__linux__)

namespace {

std::string fourccToString(uint32_t fourcc) {
    char buf[5];
    buf[0] = static_cast<char>(fourcc & 0xFF);
    buf[1] = static_cast<char>((fourcc >> 8) & 0xFF);
    buf[2] = static_cast<char>((fourcc >> 16) & 0xFF);
    buf[3] = static_cast<char>((fourcc >> 24) & 0xFF);
    buf[4] = '\0';
    // Strip trailing spaces
    for (int i = 3; i >= 0 && buf[i] == ' '; --i) buf[i] = '\0';
    return buf;
}

// Enumerate V4L2 frame sizes and framerates for a given format
void enumerateV4L2Formats(int fd, CaptureDevice& dev) {
    struct v4l2_fmtdesc fmtd{};
    fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (fmtd.index = 0; ::ioctl(fd, VIDIOC_ENUM_FMT, &fmtd) == 0; ++fmtd.index) {
        std::string pixFmt = fourccToString(fmtd.pixelformat);

        // Enumerate frame sizes for this format
        struct v4l2_frmsizeenum fsize{};
        fsize.pixel_format = fmtd.pixelformat;
        fsize.index = 0;

        bool gotSizes = false;
        while (::ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize) == 0) {
            gotSizes = true;
            uint32_t w = 0, h = 0;

            if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                w = fsize.discrete.width;
                h = fsize.discrete.height;
            } else {
                // Stepwise / continuous – advertise max size
                w = fsize.stepwise.max_width;
                h = fsize.stepwise.max_height;
            }

            // Enumerate framerates for this format + size pair
            struct v4l2_frmivalenum fival{};
            fival.pixel_format = fmtd.pixelformat;
            fival.width  = w;
            fival.height = h;
            fival.index  = 0;

            double fpsMin = 1.0, fpsMax = 60.0;
            bool gotFps = false;
            while (::ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival) == 0) {
                double fps = 0.0;
                if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                    if (fival.discrete.numerator > 0)
                        fps = static_cast<double>(fival.discrete.denominator)
                              / fival.discrete.numerator;
                } else {
                    // Continuous / stepwise
                    if (fival.stepwise.min.numerator > 0)
                        fpsMax = static_cast<double>(fival.stepwise.min.denominator)
                                 / fival.stepwise.min.numerator;
                    if (fival.stepwise.max.numerator > 0)
                        fpsMin = static_cast<double>(fival.stepwise.max.denominator)
                                 / fival.stepwise.max.numerator;
                    fps = fpsMax;
                }
                if (fps > 0.0) {
                    if (!gotFps) { fpsMin = fpsMax = fps; gotFps = true; }
                    else { fpsMin = std::min(fpsMin, fps); fpsMax = std::max(fpsMax, fps); }
                }
                ++fival.index;
                if (fival.type != V4L2_FRMIVAL_TYPE_DISCRETE) break;
            }

            CaptureFormat fmt;
            fmt.pixelFormat = pixFmt;
            fmt.width  = w;
            fmt.height = h;
            fmt.fpsMin = gotFps ? fpsMin : 1.0;
            fmt.fpsMax = gotFps ? fpsMax : 30.0;
            dev.formats.push_back(fmt);

            ++fsize.index;
            if (fsize.type != V4L2_FRMSIZE_TYPE_DISCRETE) break;
        }

        // If ENUM_FRAMESIZES is not supported, add a format entry without sizes
        if (!gotSizes) {
            CaptureFormat fmt;
            fmt.pixelFormat = pixFmt;
            fmt.width = 0; fmt.height = 0;
            fmt.fpsMin = 1.0; fmt.fpsMax = 60.0;
            dev.formats.push_back(fmt);
        }
    }
}

CaptureDevice queryV4L2Device(const std::string& path) {
    CaptureDevice dev;
    dev.path    = path;
    dev.backend = "v4l2";

    int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return dev;

    struct v4l2_capability cap{};
    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        dev.deviceName = reinterpret_cast<const char*>(cap.card);
        dev.driverName = reinterpret_cast<const char*>(cap.driver);
        dev.busInfo    = reinterpret_cast<const char*>(cap.bus_info);
        uint32_t caps  = (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
                         ? cap.device_caps : cap.capabilities;
        dev.isCapture  = (caps & V4L2_CAP_VIDEO_CAPTURE) != 0;
        // Check whether any V4L2 controls exist
        struct v4l2_queryctrl qctrl{};
        qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        dev.hasControls = (::ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0);

        if (dev.isCapture) {
            enumerateV4L2Formats(fd, dev);
        }
    }

    ::close(fd);
    return dev;
}

} // anonymous namespace

#endif // __linux__

// ============================================================================
// discoverCapture
// ============================================================================

std::vector<CaptureDevice> discoverCapture() {
    std::vector<CaptureDevice> results;

#if defined(__linux__)
    for (int i = 0; i < 64; ++i) {
        std::string path = "/dev/video" + std::to_string(i);
        struct stat st{};
        if (::stat(path.c_str(), &st) != 0) continue; // does not exist

        CaptureDevice dev = queryV4L2Device(path);
        if (!dev.deviceName.empty() && dev.isCapture) {
            results.push_back(std::move(dev));
        }
    }
#endif

    return results;
}

// ============================================================================
// captureCapabilities
// ============================================================================

CaptureDevice captureCapabilities(const std::string& devicePathOrIndex) {
#if defined(__linux__)
    std::string path = devicePathOrIndex;

    // Accept plain integer indices like "0", "1"
    bool isIndex = !path.empty() && path.find_first_not_of("0123456789") == std::string::npos;
    if (isIndex) {
        path = "/dev/video" + path;
    }

    return queryV4L2Device(path);
#else
    CaptureDevice dev;
    dev.path    = devicePathOrIndex;
    dev.backend = "opencv";
    // On non-Linux platforms use OpenCV to query basic device info
    // Full format enumeration is not available without V4L2
    dev.isCapture   = true;
    dev.hasControls = false;
    return dev;
#endif
}

} // namespace visionpipe

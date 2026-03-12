// When the Iceoryx2 IPC backend is selected, the shm_ symbols are provided as
// inlines in shm_frame_transport.h.  We skip this entire compilation unit so
// that the POSIX shm_open/mmap symbols are never defined.
#ifndef VISIONPIPE_IPC_USE_ICEORYX2

#include "utils/shm_frame_transport.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace visionpipe {

// ============================================================================
// Internal bookkeeping
// ============================================================================

struct ShmRegion {
    int         fd      = -1;
    void*       addr    = MAP_FAILED;
    size_t      mapSize = 0;
    std::string posixName;           // e.g. "/vp_left_cam"
    uint64_t    lastReadSeq = 0;     // per-process read cursor
};

static std::mutex                                    g_shmMutex;
static std::unordered_map<std::string, ShmRegion>    g_shmRegions;

static std::string shmPosixName(const std::string& name) {
    return "/vp_" + name;
}

static ShmFrameHeader* getHeader(ShmRegion& r) {
    return reinterpret_cast<ShmFrameHeader*>(r.addr);
}

static ShmRegion* findOrOpen(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_shmMutex);
    auto it = g_shmRegions.find(name);
    if (it != g_shmRegions.end()) return &it->second;

    // Try to open an existing region (reader side).
    std::string pname = shmPosixName(name);
    int fd = shm_open(pname.c_str(), O_RDWR, 0666);
    if (fd < 0) return nullptr;

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size == 0) {
        close(fd);
        return nullptr;
    }

    void* addr = mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        return nullptr;
    }

    ShmRegion reg;
    reg.fd        = fd;
    reg.addr      = addr;
    reg.mapSize   = static_cast<size_t>(st.st_size);
    reg.posixName = pname;

    auto [ins, _] = g_shmRegions.emplace(name, std::move(reg));
    return &ins->second;
}

// ============================================================================
// Public API
// ============================================================================

bool shmFrameCreate(const std::string& name, int width, int height, int type) {
    size_t frameBytes = static_cast<size_t>(width) * height * CV_ELEM_SIZE(type);
    // header + 2× frame buffer (double-buffered)
    size_t totalSize  = sizeof(ShmFrameHeader) + 2 * frameBytes;

    // Align total to page boundary.
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0)
        totalSize = (totalSize + pageSize - 1) & ~(pageSize - 1);

    std::string pname = shmPosixName(name);

    // Create (or truncate) the shm object.
    int fd = shm_open(pname.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        std::cerr << "[shm_create] shm_open failed for '" << pname
                  << "': " << strerror(errno) << "\n";
        return false;
    }

    if (ftruncate(fd, static_cast<off_t>(totalSize)) < 0) {
        std::cerr << "[shm_create] ftruncate failed: " << strerror(errno) << "\n";
        close(fd);
        shm_unlink(pname.c_str());
        return false;
    }

    void* addr = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        std::cerr << "[shm_create] mmap failed: " << strerror(errno) << "\n";
        close(fd);
        shm_unlink(pname.c_str());
        return false;
    }

    // Initialise header (use placement new to value-initialise).
    auto* hdr = new (addr) ShmFrameHeader{};
    hdr->writeSeq.store(0, std::memory_order_relaxed);
    hdr->shutdown.store(0, std::memory_order_relaxed);
    hdr->width    = width;
    hdr->height   = height;
    hdr->type     = type;
    hdr->channels = CV_MAT_CN(type);
    hdr->dataSize = frameBytes;
    hdr->bufferOffset[0] = sizeof(ShmFrameHeader);
    hdr->bufferOffset[1] = sizeof(ShmFrameHeader) + frameBytes;

    std::lock_guard<std::mutex> lk(g_shmMutex);
    ShmRegion reg;
    reg.fd        = fd;
    reg.addr      = addr;
    reg.mapSize   = totalSize;
    reg.posixName = pname;
    g_shmRegions[name] = std::move(reg);

    return true;
}

bool shmFrameWrite(const std::string& name, const cv::Mat& frame) {
    ShmRegion* reg = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_shmMutex);
        auto it = g_shmRegions.find(name);
        if (it != g_shmRegions.end()) reg = &it->second;
    }
    if (!reg) {
        reg = findOrOpen(name);
        if (!reg) return false;
    }

    auto* hdr = getHeader(*reg);

    // Validate dimensions.
    if (frame.cols != hdr->width || frame.rows != hdr->height ||
        frame.type() != hdr->type) {
        std::cerr << "[shm_write] Frame dimensions/type mismatch for '" << name
                  << "': expected " << hdr->width << "x" << hdr->height
                  << " type=" << hdr->type << ", got " << frame.cols << "x"
                  << frame.rows << " type=" << frame.type() << "\n";
        return false;
    }

    // Write into the NEXT buffer (double-buffered).
    uint64_t seq       = hdr->writeSeq.load(std::memory_order_relaxed);
    int      nextBuf   = static_cast<int>((seq + 1) & 1);
    uint8_t* dst       = reinterpret_cast<uint8_t*>(reg->addr) + hdr->bufferOffset[nextBuf];

    // Continuous frames can be memcpy'd in one shot.
    if (frame.isContinuous()) {
        std::memcpy(dst, frame.data, hdr->dataSize);
    } else {
        size_t rowBytes = static_cast<size_t>(frame.cols) * frame.elemSize();
        for (int r = 0; r < frame.rows; ++r) {
            std::memcpy(dst + r * rowBytes, frame.ptr(r), rowBytes);
        }
    }

    // Publish: bump sequence with release semantics so readers see the data.
    hdr->writeSeq.store(seq + 1, std::memory_order_release);
    return true;
}

bool shmFrameRead(const std::string& name, cv::Mat& out) {
    ShmRegion* reg = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_shmMutex);
        auto it = g_shmRegions.find(name);
        if (it != g_shmRegions.end()) reg = &it->second;
    }
    if (!reg) {
        reg = findOrOpen(name);
        if (!reg) return false;
    }

    auto* hdr = getHeader(*reg);

    uint64_t seq = hdr->writeSeq.load(std::memory_order_acquire);
    if (seq == 0) {
        // Nothing written yet.
        return false;
    }

    // Determine which buffer holds the latest completed frame.
    int curBuf = static_cast<int>(seq & 1);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(reg->addr) + hdr->bufferOffset[curBuf];

    // Allocate output if needed.
    if (out.cols != hdr->width || out.rows != hdr->height || out.type() != hdr->type) {
        out.create(hdr->height, hdr->width, hdr->type);
    }

    std::memcpy(out.data, src, hdr->dataSize);
    reg->lastReadSeq = seq;
    return true;
}

void shmFrameSetShutdown(const std::string& name) {
    ShmRegion* reg = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_shmMutex);
        auto it = g_shmRegions.find(name);
        if (it != g_shmRegions.end()) reg = &it->second;
    }
    if (!reg) return;
    auto* hdr = getHeader(*reg);
    hdr->shutdown.store(1, std::memory_order_release);
}

bool shmFrameIsShutdown(const std::string& name) {
    ShmRegion* reg = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_shmMutex);
        auto it = g_shmRegions.find(name);
        if (it != g_shmRegions.end()) reg = &it->second;
    }
    if (!reg) {
        reg = findOrOpen(name);
        if (!reg) return true;  // can't reach shm → treat as shutdown
    }
    auto* hdr = getHeader(*reg);
    return hdr->shutdown.load(std::memory_order_acquire) != 0;
}

void shmFrameDestroy(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_shmMutex);
    auto it = g_shmRegions.find(name);
    if (it == g_shmRegions.end()) return;

    auto& r = it->second;
    if (r.addr != MAP_FAILED) munmap(r.addr, r.mapSize);
    if (r.fd >= 0) close(r.fd);
    shm_unlink(r.posixName.c_str());
    g_shmRegions.erase(it);
}

void shmFrameDestroyAll() {
    std::lock_guard<std::mutex> lk(g_shmMutex);
    for (auto& [name, r] : g_shmRegions) {
        if (r.addr != MAP_FAILED) munmap(r.addr, r.mapSize);
        if (r.fd >= 0) close(r.fd);
        shm_unlink(r.posixName.c_str());
    }
    g_shmRegions.clear();
}

std::vector<std::string> shmFrameListRegions() {
    std::lock_guard<std::mutex> lk(g_shmMutex);
    std::vector<std::string> out;
    out.reserve(g_shmRegions.size());
    for (auto& [n, _] : g_shmRegions) out.push_back(n);
    return out;
}

} // namespace visionpipe

#endif // VISIONPIPE_IPC_USE_ICEORYX2

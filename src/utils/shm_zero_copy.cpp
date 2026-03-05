#include "utils/shm_zero_copy.h"

#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <atomic>
#include <climits>

namespace visionpipe {

// ============================================================================
// Shared-memory layout structures (all live inside the anonymous mmap)
// ============================================================================

/// Top-level arena header — one per arena.
struct alignas(128) ShmArenaHeader {
    std::atomic<int32_t> shutdown{0};
    int32_t  maxSlots{0};
    size_t   maxFrameBytes{0};
    size_t   bufferStride{0};        // cache-line-aligned per-buffer size
    char     _pad[128 - 4 - 4 - 8 - 8];
};

/// Per-slot header — one per named frame channel.
struct alignas(128) ShmSlotHeader {
    // Writer-hot fields (first cache line)
    std::atomic<uint64_t> writeSeq{0};     // monotonic frame counter
    std::atomic<uint32_t> writeIdx{0};     // latest completed buffer (0, 1, or 2)
    std::atomic<uint32_t> active{0};       // 0=free, 1=initialising, 2=ready
    int32_t  width{0};
    int32_t  height{0};
    int32_t  type{0};                      // cv::Mat type (e.g. CV_8UC1)
    uint32_t frameBytes{0};                // actual byte size of one frame
    char     name[48]{};                   // null-terminated cache key
    // 8 + 4 + 4 + 4*4 + 48 = 80 → pad to 128
    char     _pad[128 - 80];
};

/// Per-fork-child throughput counters (written by child, read by parent).
struct alignas(128) ShmThroughputSlot {
    std::atomic<uint64_t> callCount{0};
    std::atomic<uint64_t> totalNs{0};
    std::atomic<uint64_t> minNs{UINT64_MAX};
    std::atomic<uint64_t> maxNs{0};
    std::atomic<uint32_t> active{0};       // 0=free, 1=initialising, 2=ready
    char     name[48]{};                   // pipeline name
    // 8*4 + 4 + 48 = 84 → pad to 128
    char     _pad[128 - 84];
};

// ============================================================================
// Arena handle (per-process bookkeeping, NOT in the shared mmap)
//
// Layout of the mmap region:
//
//   [ShmArenaHeader : 128 B]
//   [ShmSlotHeader[0..maxSlots-1] : maxSlots × 128 B]
//   [ShmThroughputSlot[0..maxSlots-1] : maxSlots × 128 B]
//   [Buffer pool : maxSlots × 3 × bufferStride B]
//
// Buffer(slotIdx, bufIdx) = poolBase + (slotIdx * 3 + bufIdx) * bufferStride
// ============================================================================

struct ShmArena {
    void*  baseAddr{nullptr};
    size_t totalSize{0};
    int    maxSlots{0};
    size_t maxFrameBytes{0};
    size_t bufferStride{0};

    ShmArenaHeader* header() const {
        return reinterpret_cast<ShmArenaHeader*>(baseAddr);
    }
    ShmSlotHeader* slot(int i) const {
        auto* p = static_cast<uint8_t*>(baseAddr) + sizeof(ShmArenaHeader);
        return reinterpret_cast<ShmSlotHeader*>(p + i * sizeof(ShmSlotHeader));
    }
    ShmThroughputSlot* tput(int i) const {
        auto* p = static_cast<uint8_t*>(baseAddr)
                + sizeof(ShmArenaHeader)
                + maxSlots * sizeof(ShmSlotHeader);
        return reinterpret_cast<ShmThroughputSlot*>(p + i * sizeof(ShmThroughputSlot));
    }
    uint8_t* buf(int slotIdx, int bufIdx) const {
        auto* poolBase = static_cast<uint8_t*>(baseAddr)
                       + sizeof(ShmArenaHeader)
                       + maxSlots * sizeof(ShmSlotHeader)
                       + maxSlots * sizeof(ShmThroughputSlot);
        return poolBase + static_cast<size_t>(slotIdx * 3 + bufIdx) * bufferStride;
    }
};

// ============================================================================
// Internal helpers
// ============================================================================

static ShmSlotHeader* findSlot(ShmArena* arena, const std::string& name,
                                bool create) {
    // Pass 1: find an existing ready slot with the matching name.
    for (int i = 0; i < arena->maxSlots; i++) {
        auto* s = arena->slot(i);
        if (s->active.load(std::memory_order_acquire) == 2 &&
            std::strncmp(s->name, name.c_str(), sizeof(s->name) - 1) == 0) {
            return s;
        }
    }
    if (!create) return nullptr;

    // Pass 2: claim a free slot (CAS 0 → 1 = initialising).
    for (int i = 0; i < arena->maxSlots; i++) {
        auto* s = arena->slot(i);
        uint32_t expected = 0;
        if (s->active.compare_exchange_strong(expected, 1,
                                              std::memory_order_acq_rel)) {
            std::strncpy(s->name, name.c_str(), sizeof(s->name) - 1);
            s->name[sizeof(s->name) - 1] = '\0';
            s->active.store(2, std::memory_order_release);  // 1 → 2 = ready
            return s;
        }
    }
    return nullptr;  // arena full
}

static int slotIndex(ShmArena* arena, ShmSlotHeader* slot) {
    auto* base = reinterpret_cast<uint8_t*>(arena->slot(0));
    return static_cast<int>(
        (reinterpret_cast<uint8_t*>(slot) - base) / sizeof(ShmSlotHeader));
}

static ShmThroughputSlot* findTputSlot(ShmArena* arena,
                                        const std::string& name,
                                        bool create) {
    for (int i = 0; i < arena->maxSlots; i++) {
        auto* s = arena->tput(i);
        if (s->active.load(std::memory_order_acquire) == 2 &&
            std::strncmp(s->name, name.c_str(), sizeof(s->name) - 1) == 0) {
            return s;
        }
    }
    if (!create) return nullptr;

    for (int i = 0; i < arena->maxSlots; i++) {
        auto* s = arena->tput(i);
        uint32_t expected = 0;
        if (s->active.compare_exchange_strong(expected, 1,
                                              std::memory_order_acq_rel)) {
            std::strncpy(s->name, name.c_str(), sizeof(s->name) - 1);
            s->name[sizeof(s->name) - 1] = '\0';
            s->active.store(2, std::memory_order_release);
            return s;
        }
    }
    return nullptr;
}

// ============================================================================
// Public API
// ============================================================================

ShmArena* shmArenaCreate(int maxSlots, size_t maxFrameBytes) {
    // Align buffer stride to 64 bytes (cache line).
    size_t stride    = (maxFrameBytes + 63) & ~size_t(63);
    size_t hdrBytes  = sizeof(ShmArenaHeader);
    size_t slotBytes = static_cast<size_t>(maxSlots) * sizeof(ShmSlotHeader);
    size_t tputBytes = static_cast<size_t>(maxSlots) * sizeof(ShmThroughputSlot);
    size_t poolBytes = static_cast<size_t>(maxSlots) * 3 * stride;
    size_t totalSize = hdrBytes + slotBytes + tputBytes + poolBytes;

    // Page-align.
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0)
        totalSize = (totalSize + size_t(pageSize) - 1) & ~(size_t(pageSize) - 1);

    void* addr = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        std::cerr << "[shm_arena] mmap(" << totalSize << " B) failed: "
                  << strerror(errno) << "\n";
        return nullptr;
    }

    // Initialize header.
    auto* hdr = new (addr) ShmArenaHeader{};
    hdr->maxSlots      = maxSlots;
    hdr->maxFrameBytes = maxFrameBytes;
    hdr->bufferStride  = stride;

    auto* arena = new ShmArena();
    arena->baseAddr      = addr;
    arena->totalSize     = totalSize;
    arena->maxSlots      = maxSlots;
    arena->maxFrameBytes = maxFrameBytes;
    arena->bufferStride  = stride;

    // Placement-new slot and throughput headers (MAP_ANONYMOUS guarantees
    // zero-fill, but we use placement new for correct atomic initialisation).
    for (int i = 0; i < maxSlots; i++) {
        new (arena->slot(i)) ShmSlotHeader{};
        new (arena->tput(i)) ShmThroughputSlot{};
    }

    return arena;
}

void shmArenaDestroy(ShmArena* arena) {
    if (!arena) return;
    if (arena->baseAddr && arena->baseAddr != MAP_FAILED)
        munmap(arena->baseAddr, arena->totalSize);
    delete arena;
}

bool shmArenaWrite(ShmArena* arena, const std::string& name,
                    const cv::Mat& frame) {
    if (!arena || frame.empty()) return false;

    auto* slot = findSlot(arena, name, /*create=*/true);
    if (!slot) {
        std::cerr << "[shm_arena] No free slots for '" << name << "'\n";
        return false;
    }

    size_t needed = frame.total() * frame.elemSize();
    if (needed > arena->maxFrameBytes) {
        std::cerr << "[shm_arena] Frame '" << name << "' too large: "
                  << needed << " > " << arena->maxFrameBytes << "\n";
        return false;
    }

    // Update metadata.
    slot->width      = frame.cols;
    slot->height     = frame.rows;
    slot->type       = frame.type();
    slot->frameBytes = static_cast<uint32_t>(needed);

    // Triple-buffered: write into the NEXT buffer after the latest.
    uint32_t curIdx  = slot->writeIdx.load(std::memory_order_relaxed);
    uint32_t nextBuf = (curIdx + 1) % 3;
    int      si      = slotIndex(arena, slot);
    uint8_t* dst     = arena->buf(si, nextBuf);

    if (frame.isContinuous()) {
        std::memcpy(dst, frame.data, needed);
    } else {
        size_t rowBytes = static_cast<size_t>(frame.cols) * frame.elemSize();
        for (int r = 0; r < frame.rows; r++)
            std::memcpy(dst + r * rowBytes, frame.ptr(r), rowBytes);
    }

    // Publish: bump sequence, then update the latest buffer index.
    slot->writeSeq.fetch_add(1, std::memory_order_release);
    slot->writeIdx.store(nextBuf, std::memory_order_release);
    return true;
}

bool shmArenaRead(ShmArena* arena, const std::string& name, cv::Mat& out) {
    if (!arena) return false;

    auto* slot = findSlot(arena, name, /*create=*/false);
    if (!slot) return false;

    uint64_t seq = slot->writeSeq.load(std::memory_order_acquire);
    if (seq == 0) return false;   // nothing written yet

    uint32_t bufIdx = slot->writeIdx.load(std::memory_order_acquire);
    int      si     = slotIndex(arena, slot);
    uint8_t* src    = arena->buf(si, bufIdx);

    // ZERO COPY: wrap a cv::Mat header around the shared buffer — no pixels
    // are copied.  The buffer is safe for at least 2 more writer frames
    // because we use triple buffering.
    out = cv::Mat(slot->height, slot->width, slot->type, src);
    return true;
}

void shmArenaSetShutdown(ShmArena* arena) {
    if (arena) arena->header()->shutdown.store(1, std::memory_order_release);
}

bool shmArenaIsShutdown(ShmArena* arena) {
    return arena && arena->header()->shutdown.load(std::memory_order_acquire) != 0;
}

// ============================================================================
// Throughput
// ============================================================================

void shmArenaRecordThroughput(ShmArena* arena, const std::string& name,
                               uint64_t durationNs) {
    if (!arena) return;

    auto* ts = findTputSlot(arena, name, /*create=*/true);
    if (!ts) return;

    ts->callCount.fetch_add(1, std::memory_order_relaxed);
    ts->totalNs.fetch_add(durationNs, std::memory_order_relaxed);

    // Lock-free min
    uint64_t curMin = ts->minNs.load(std::memory_order_relaxed);
    while (durationNs < curMin &&
           !ts->minNs.compare_exchange_weak(curMin, durationNs,
                                            std::memory_order_relaxed))
    { /* retry */ }

    // Lock-free max
    uint64_t curMax = ts->maxNs.load(std::memory_order_relaxed);
    while (durationNs > curMax &&
           !ts->maxNs.compare_exchange_weak(curMax, durationNs,
                                            std::memory_order_relaxed))
    { /* retry */ }
}

std::vector<ShmThroughputData> shmArenaReadThroughput(ShmArena* arena) {
    std::vector<ShmThroughputData> result;
    if (!arena) return result;

    for (int i = 0; i < arena->maxSlots; i++) {
        auto* ts = arena->tput(i);
        if (ts->active.load(std::memory_order_acquire) != 2) continue;

        ShmThroughputData d;
        d.name      = ts->name;
        d.callCount = ts->callCount.load(std::memory_order_relaxed);
        d.totalNs   = ts->totalNs.load(std::memory_order_relaxed);
        d.minNs     = ts->minNs.load(std::memory_order_relaxed);
        d.maxNs     = ts->maxNs.load(std::memory_order_relaxed);
        result.push_back(std::move(d));
    }
    return result;
}

} // namespace visionpipe

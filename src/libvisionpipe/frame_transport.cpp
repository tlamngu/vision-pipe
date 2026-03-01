#include "libvisionpipe/frame_transport.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>

namespace visionpipe {

// ============================================================================
// Helpers
// ============================================================================

std::string buildShmName(const std::string& sessionId, const std::string& sinkName) {
    // POSIX shm names must start with / and contain no other /
    return std::string(SHM_PREFIX) + sessionId + "_" + sinkName;
}

// ============================================================================
// FrameShmProducer
// ============================================================================

FrameShmProducer::FrameShmProducer() = default;

FrameShmProducer::~FrameShmProducer() {
    close();
}

bool FrameShmProducer::open(const std::string& shmName) {
    _shmName = shmName;

    // Create shared memory
    _fd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
    if (_fd < 0) {
        std::cerr << "[FrameShmProducer] shm_open failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Set size
    if (ftruncate(_fd, static_cast<off_t>(shmTotalSize())) != 0) {
        std::cerr << "[FrameShmProducer] ftruncate failed: " << strerror(errno) << std::endl;
        ::close(_fd);
        _fd = -1;
        shm_unlink(shmName.c_str());
        return false;
    }

    // Map
    _mapped = mmap(nullptr, shmTotalSize(), PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
    if (_mapped == MAP_FAILED) {
        std::cerr << "[FrameShmProducer] mmap failed: " << strerror(errno) << std::endl;
        ::close(_fd);
        _fd = -1;
        shm_unlink(shmName.c_str());
        _mapped = nullptr;
        return false;
    }

    // Initialize header
    auto* hdr = reinterpret_cast<FrameShmHeader*>(_mapped);
    hdr->magic = SHM_MAGIC;
    hdr->frameSeq.store(0, std::memory_order_release);
    hdr->rows = 0;
    hdr->cols = 0;
    hdr->type = 0;
    hdr->step = 0;
    hdr->dataSize = 0;
    hdr->writerLock.store(0, std::memory_order_release);
    hdr->producerPid = static_cast<int32_t>(getpid());

    return true;
}

void FrameShmProducer::writeFrame(const uint8_t* data, int rows, int cols, int type, int step) {
    if (!_mapped) return;

    auto* hdr = reinterpret_cast<FrameShmHeader*>(_mapped);
    uint8_t* frameData = reinterpret_cast<uint8_t*>(_mapped) + sizeof(FrameShmHeader);

    uint32_t dataSize = static_cast<uint32_t>(rows) * static_cast<uint32_t>(step);
    if (dataSize > MAX_FRAME_BYTES) {
        std::cerr << "[FrameShmProducer] Frame too large: " << dataSize << " > " << MAX_FRAME_BYTES << std::endl;
        return;
    }

    // Acquire writer lock
    int32_t expected = 0;
    while (!hdr->writerLock.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
        expected = 0;
        std::this_thread::yield();
    }

    // Copy frame data
    if (step == cols * static_cast<int>((type & 0x7) == 0 ? 1 : (type & 0x7) == 1 ? 1 : (type & 0x7) == 2 ? 2 : (type & 0x7) == 3 ? 2 : (type & 0x7) == 4 ? 4 : (type & 0x7) == 5 ? 4 : (type & 0x7) == 6 ? 8 : 1) * (((type >> 3) & 0x7) + 1)) {
        // Continuous data - single memcpy
        std::memcpy(frameData, data, dataSize);
    } else {
        // Non-continuous - copy row by row
        for (int r = 0; r < rows; ++r) {
            std::memcpy(frameData + r * step, data + r * step, step);
        }
    }

    // Update header
    hdr->rows = rows;
    hdr->cols = cols;
    hdr->type = type;
    hdr->step = step;
    hdr->dataSize = dataSize;

    // Release writer lock and increment sequence
    hdr->writerLock.store(0, std::memory_order_release);
    hdr->frameSeq.fetch_add(1, std::memory_order_release);
}

void FrameShmProducer::close() {
    if (_mapped) {
        munmap(_mapped, shmTotalSize());
        _mapped = nullptr;
    }
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
    if (!_shmName.empty()) {
        shm_unlink(_shmName.c_str());
        _shmName.clear();
    }
}

// ============================================================================
// FrameShmConsumer
// ============================================================================

FrameShmConsumer::FrameShmConsumer() = default;

FrameShmConsumer::~FrameShmConsumer() {
    close();
}

bool FrameShmConsumer::open(const std::string& shmName, int timeoutMs) {
    _shmName = shmName;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    // Retry loop - wait for producer to create the shm
    while (true) {
        _fd = shm_open(shmName.c_str(), O_RDONLY, 0);
        if (_fd >= 0) break;

        if (timeoutMs == 0 || std::chrono::steady_clock::now() >= deadline) {
            std::cerr << "[FrameShmConsumer] shm_open failed: " << strerror(errno) << std::endl;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Map read-only
    _mapped = mmap(nullptr, shmTotalSize(), PROT_READ, MAP_SHARED, _fd, 0);
    if (_mapped == MAP_FAILED) {
        std::cerr << "[FrameShmConsumer] mmap failed: " << strerror(errno) << std::endl;
        ::close(_fd);
        _fd = -1;
        _mapped = nullptr;
        return false;
    }

    // Verify magic
    auto* hdr = reinterpret_cast<const FrameShmHeader*>(_mapped);
    if (hdr->magic != SHM_MAGIC) {
        std::cerr << "[FrameShmConsumer] Invalid shm magic (region not initialized?)" << std::endl;
        close();
        return false;
    }

    return true;
}

bool FrameShmConsumer::readFrame(int& rows, int& cols, int& type, int& step,
                                  const uint8_t*& data, uint64_t& seq) {
    if (!_mapped) return false;

    auto* hdr = reinterpret_cast<const FrameShmHeader*>(_mapped);

    // Wait for writer to finish (spin on lock)
    int spinCount = 0;
    while (hdr->writerLock.load(std::memory_order_acquire) != 0) {
        if (++spinCount > 10000) {
            std::this_thread::yield();
            spinCount = 0;
        }
    }

    uint64_t curSeq = hdr->frameSeq.load(std::memory_order_acquire);
    if (curSeq == 0) return false; // No frames yet

    rows = hdr->rows;
    cols = hdr->cols;
    type = hdr->type;
    step = hdr->step;
    seq = curSeq;
    data = reinterpret_cast<const uint8_t*>(_mapped) + sizeof(FrameShmHeader);

    _lastSeq = curSeq;
    return true;
}

bool FrameShmConsumer::hasNewFrame() const {
    if (!_mapped) return false;
    auto* hdr = reinterpret_cast<const FrameShmHeader*>(_mapped);
    return hdr->frameSeq.load(std::memory_order_acquire) > _lastSeq;
}

void FrameShmConsumer::close() {
    if (_mapped) {
        munmap(const_cast<void*>(_mapped), shmTotalSize());
        _mapped = nullptr;
    }
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
    _shmName.clear();
}

} // namespace visionpipe

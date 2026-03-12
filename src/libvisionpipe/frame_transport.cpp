#include "libvisionpipe/frame_transport.h"

#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>

// ============================================================================
// Platform includes
// ============================================================================
#if defined(_WIN32)
  // Windows: CreateFileMapping / MapViewOfFile
  // (windows.h already included via header)
  #include <process.h>   // _getpid()
#else
  // POSIX (Linux + macOS): shm_open / mmap
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <cerrno>
  #include <csignal>   // kill()
#endif

namespace visionpipe {

// ============================================================================
// Helpers
// ============================================================================

std::string buildShmName(const std::string& sessionId, const std::string& sinkName) {
    return std::string(SHM_PREFIX) + sessionId + "_" + sinkName;
}

std::string buildIox2ChannelName(const std::string& sessionId, const std::string& sinkName) {
    // Iceoryx2 service names must not begin with '/'; return a plain
    // "<sessionId>_<sinkName>" key.  The iox2 publisher layer prepends "/vp_".
    return sessionId + "_" + sinkName;
}

// Helper: get current process ID portably
static int32_t portableGetPid() {
#if defined(_WIN32)
    return static_cast<int32_t>(_getpid());
#else
    return static_cast<int32_t>(getpid());
#endif
}

// ============================================================================
// FrameShmProducer
// ============================================================================

FrameShmProducer::FrameShmProducer() = default;

FrameShmProducer::~FrameShmProducer() {
    close();
}

#if defined(_WIN32)
// --- Windows implementation ---

bool FrameShmProducer::open(const std::string& shmName) {
    _shmName = shmName;

    DWORD sizeHigh = static_cast<DWORD>((shmTotalSize() >> 32) & 0xFFFFFFFF);
    DWORD sizeLow  = static_cast<DWORD>(shmTotalSize() & 0xFFFFFFFF);

    _hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,   // use paging file
        NULL,                   // default security
        PAGE_READWRITE,
        sizeHigh, sizeLow,
        shmName.c_str()
    );

    if (_hMapFile == NULL) {
        std::cerr << "[FrameShmProducer] CreateFileMapping failed: " << GetLastError() << std::endl;
        return false;
    }

    _mapped = MapViewOfFile(_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, shmTotalSize());
    if (_mapped == NULL) {
        std::cerr << "[FrameShmProducer] MapViewOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(_hMapFile);
        _hMapFile = NULL;
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
    hdr->producerPid = portableGetPid();

    return true;
}

void FrameShmProducer::close() {
    if (_mapped) {
        UnmapViewOfFile(_mapped);
        _mapped = nullptr;
    }
    if (_hMapFile != NULL) {
        CloseHandle(_hMapFile);
        _hMapFile = NULL;
    }
    _shmName.clear();
}

#else
// --- POSIX (Linux + macOS) implementation ---

bool FrameShmProducer::open(const std::string& shmName) {
    _shmName = shmName;

    _fd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
    if (_fd < 0) {
        std::cerr << "[FrameShmProducer] shm_open failed: " << strerror(errno) << std::endl;
        return false;
    }

    if (ftruncate(_fd, static_cast<off_t>(shmTotalSize())) != 0) {
        std::cerr << "[FrameShmProducer] ftruncate failed: " << strerror(errno) << std::endl;
        ::close(_fd);
        _fd = -1;
        shm_unlink(shmName.c_str());
        return false;
    }

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
    hdr->producerPid = portableGetPid();

    return true;
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

#endif // _WIN32

// --- Platform-independent writeFrame ---

void FrameShmProducer::writeFrame(const uint8_t* data, int rows, int cols, int type, int step) {
    if (!_mapped) return;

    auto* hdr = reinterpret_cast<FrameShmHeader*>(_mapped);
    uint8_t* frameData = reinterpret_cast<uint8_t*>(_mapped) + sizeof(FrameShmHeader);

    uint32_t dataSize = static_cast<uint32_t>(rows) * static_cast<uint32_t>(step);
    if (dataSize > MAX_FRAME_BYTES) {
        std::cerr << "[FrameShmProducer] Frame too large: " << dataSize
                  << " > " << MAX_FRAME_BYTES << std::endl;
        return;
    }

    // Acquire writer lock (spin)
    int32_t expected = 0;
    while (!hdr->writerLock.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
        expected = 0;
        std::this_thread::yield();
    }

    // Copy frame data (always using step-based copy for correctness)
    std::memcpy(frameData, data, dataSize);

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

// ============================================================================
// FrameShmConsumer
// ============================================================================

FrameShmConsumer::FrameShmConsumer() = default;

FrameShmConsumer::~FrameShmConsumer() {
    close();
}

#if defined(_WIN32)
// --- Windows implementation ---

bool FrameShmConsumer::open(const std::string& shmName, int timeoutMs) {
    _shmName = shmName;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (true) {
        _hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, shmName.c_str());
        if (_hMapFile != NULL) break;

        if (timeoutMs == 0 || std::chrono::steady_clock::now() >= deadline) {
            std::cerr << "[FrameShmConsumer] OpenFileMapping failed: " << GetLastError() << std::endl;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    _mapped = MapViewOfFile(_hMapFile, FILE_MAP_READ, 0, 0, shmTotalSize());
    if (_mapped == NULL) {
        std::cerr << "[FrameShmConsumer] MapViewOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(_hMapFile);
        _hMapFile = NULL;
        return false;
    }

    auto* hdr = reinterpret_cast<const FrameShmHeader*>(_mapped);
    if (hdr->magic != SHM_MAGIC) {
        std::cerr << "[FrameShmConsumer] Invalid shm magic (region not initialized?)" << std::endl;
        close();
        return false;
    }

    return true;
}

void FrameShmConsumer::close() {
    if (_mapped) {
        UnmapViewOfFile(_mapped);
        _mapped = nullptr;
    }
    if (_hMapFile != NULL) {
        CloseHandle(_hMapFile);
        _hMapFile = NULL;
    }
    _shmName.clear();
}

#else
// --- POSIX (Linux + macOS) implementation ---

bool FrameShmConsumer::open(const std::string& shmName, int timeoutMs) {
    _shmName = shmName;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (true) {
        _fd = shm_open(shmName.c_str(), O_RDONLY, 0);
        if (_fd >= 0) {
            // Guard against the race between shm_open(O_CREAT) and ftruncate
            // in the producer: if the SHM object exists but has not yet been
            // sized, mmap would succeed but any access beyond offset 0 yields
            // SIGBUS.  Check the size before mapping and retry if too small.
            struct stat sb;
            if (::fstat(_fd, &sb) != 0 ||
                static_cast<size_t>(sb.st_size) < shmTotalSize()) {
                ::close(_fd);
                _fd = -1;
                // fall through to the timeout/sleep logic below
            } else {
                break;  // file is properly sized, proceed to mmap
            }
        }

        if (timeoutMs == 0 || std::chrono::steady_clock::now() >= deadline) {
            std::cerr << "[FrameShmConsumer] shm_open/fstat failed: " << strerror(errno) << std::endl;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    _mapped = mmap(nullptr, shmTotalSize(), PROT_READ, MAP_SHARED, _fd, 0);
    if (_mapped == MAP_FAILED) {
        std::cerr << "[FrameShmConsumer] mmap failed: " << strerror(errno) << std::endl;
        ::close(_fd);
        _fd = -1;
        _mapped = nullptr;
        return false;
    }

    auto* hdr = reinterpret_cast<const FrameShmHeader*>(_mapped);
    if (hdr->magic != SHM_MAGIC) {
        std::cerr << "[FrameShmConsumer] Invalid shm magic (region not initialized?)" << std::endl;
        close();
        return false;
    }

    return true;
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

bool FrameShmConsumer::isProducerAlive() const {
    if (!_mapped) return false;
    const auto* hdr = reinterpret_cast<const FrameShmHeader*>(_mapped);
    pid_t pid = static_cast<pid_t>(hdr->producerPid);
    if (pid <= 0) return true;           // unknown, assume alive
    if (::kill(pid, 0) == 0) return true; // process exists
    return errno != ESRCH;               // EPERM = exists but no permission
}

#endif // _WIN32

// --- Platform-independent read methods ---

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

} // namespace visionpipe

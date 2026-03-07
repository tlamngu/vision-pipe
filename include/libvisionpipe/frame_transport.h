#ifndef LIBVISIONPIPE_FRAME_TRANSPORT_H
#define LIBVISIONPIPE_FRAME_TRANSPORT_H

/**
 * @file frame_transport.h
 * @brief Zero-copy shared memory frame transport between visionpipe process and clients
 *
 * Cross-platform support:
 *   - Linux/macOS: POSIX shared memory (shm_open/mmap)
 *   - Windows:     Named shared memory (CreateFileMapping/MapViewOfFile)
 *
 * Protocol:
 *   - Each frame_sink("name") in a .vsp script creates a shared memory region
 *   - The region contains a header + frame data buffer
 *   - Producer (visionpipe) writes frames, consumer (libvisionpipe client) reads them
 *   - Synchronization via atomic frame counter + spin-lock in shared memory
 */

#include <string>
#include <cstdint>
#include <cstddef>
#include <atomic>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

namespace visionpipe {

// Shared memory segment name prefix
// On POSIX this becomes "/visionpipe_sink_...", on Windows "Local\\visionpipe_sink_..."
#if defined(_WIN32)
constexpr const char* SHM_PREFIX = "Local\\visionpipe_sink_";
#else
constexpr const char* SHM_PREFIX = "/visionpipe_sink_";
#endif

// Maximum frame size (default 4K resolution * 4 channels = ~33MB)
constexpr size_t MAX_FRAME_BYTES = 3840 * 2160 * 4;

// Magic value to verify shared memory is valid
constexpr uint32_t SHM_MAGIC = 0x56505346; // "VPSF" = VisionPipe Shared Frame

/**
 * @brief Header structure in shared memory (placed at the start of the mapped region)
 *
 * Layout: [FrameShmHeader][frame_data_bytes...]
 *
 * NOTE: This struct uses std::atomic for lock-free synchronization across processes.
 * std::atomic<T> is lock-free for 32/64-bit integers on all major platforms (x86, ARM).
 */
struct FrameShmHeader {
    uint32_t magic;                  // SHM_MAGIC
    std::atomic<uint64_t> frameSeq;  // Monotonically increasing frame counter
    int32_t rows;                    // cv::Mat rows
    int32_t cols;                    // cv::Mat cols
    int32_t type;                    // cv::Mat type (e.g. CV_8UC3)
    int32_t step;                    // cv::Mat step[0] (bytes per row)
    uint32_t dataSize;               // Actual frame data size in bytes
    std::atomic<int32_t> writerLock; // Simple spin-lock: 0=free, 1=writing
    int32_t producerPid;             // PID of the visionpipe process
    uint8_t _pad[16];               // Reserved for future use
};

/**
 * @brief Total shared memory size for a sink
 */
constexpr size_t shmTotalSize() {
    return sizeof(FrameShmHeader) + MAX_FRAME_BYTES;
}

/**
 * @brief Build the shared memory name for a given session + sink
 * @param sessionId Unique session identifier
 * @param sinkName Sink name from frame_sink("name")
 */
std::string buildShmName(const std::string& sessionId, const std::string& sinkName);

/**
 * @brief Build the Iceoryx2 channel name for a given session + sink.
 *
 * Returns "<sessionId>_<sinkName>".  The iox2 transport layer prepends
 * "/vp_" to form the actual Iceoryx2 service name, e.g.
 * "/vp_<sessionId>_<sinkName>".
 *
 * Both the publisher (frame_sink with VISIONPIPE_IPC_USE_ICEORYX2) and the
 * subscriber (libvisionpipe client) must use this function to derive matching
 * channel names.
 *
 * @param sessionId Unique session identifier
 * @param sinkName  Sink name from frame_sink("name")
 */
std::string buildIox2ChannelName(const std::string& sessionId, const std::string& sinkName);

/**
 * @brief Producer side: create and map shared memory for writing frames
 */
class FrameShmProducer {
public:
    FrameShmProducer();
    ~FrameShmProducer();

    // Non-copyable
    FrameShmProducer(const FrameShmProducer&) = delete;
    FrameShmProducer& operator=(const FrameShmProducer&) = delete;

    /**
     * @brief Create/open shared memory for a sink
     * @param shmName Full shm name (from buildShmName)
     * @return true on success
     */
    bool open(const std::string& shmName);

    /**
     * @brief Write a frame into shared memory
     * @param data Pointer to raw pixel data (must be at least rows*step bytes)
     * @param rows Frame rows
     * @param cols Frame cols
     * @param type cv::Mat type
     * @param step Bytes per row
     */
    void writeFrame(const uint8_t* data, int rows, int cols, int type, int step);

    /**
     * @brief Close and unlink/destroy shared memory
     */
    void close();

    bool isOpen() const { return _mapped != nullptr; }

private:
#if defined(_WIN32)
    HANDLE _hMapFile = NULL;
#else
    int _fd = -1;
#endif
    void* _mapped = nullptr;
    std::string _shmName;
};

/**
 * @brief Consumer side: open and read frames from shared memory
 */
class FrameShmConsumer {
public:
    FrameShmConsumer();
    ~FrameShmConsumer();

    // Non-copyable
    FrameShmConsumer(const FrameShmConsumer&) = delete;
    FrameShmConsumer& operator=(const FrameShmConsumer&) = delete;

    /**
     * @brief Open existing shared memory (created by producer)
     * @param shmName Full shm name
     * @param timeoutMs Timeout in ms to wait for the shm to appear (0 = no wait)
     * @return true on success
     */
    bool open(const std::string& shmName, int timeoutMs = 5000);

    /**
     * @brief Read current frame from shared memory
     * @param[out] rows Output rows
     * @param[out] cols Output cols
     * @param[out] type Output Mat type
     * @param[out] step Output step
     * @param[out] data Pointer set to frame data in shared memory (zero-copy!)
     * @param[out] seq Frame sequence number
     * @return true if a frame is available
     */
    bool readFrame(int& rows, int& cols, int& type, int& step,
                   const uint8_t*& data, uint64_t& seq);

    /**
     * @brief Get last read sequence number
     */
    uint64_t lastSeq() const { return _lastSeq; }

    /**
     * @brief Check if a new frame is available (seq > lastSeq)
     */
    bool hasNewFrame() const;

    /**
     * @brief Close shared memory mapping
     */
    void close();

    bool isOpen() const { return _mapped != nullptr; }

    /**
     * @brief Check whether the producer process is still alive.
     *
     * Uses kill(pid, 0) against the producerPid stored in the SHM header.
     * Returns true if the process is alive or if liveness cannot be
     * determined (e.g. producerPid is 0 or EPERM).  Returns false only when
     * the OS confirms the PID no longer exists (ESRCH).
     *
     * Call this periodically (e.g. once per second) to detect dead producers
     * so the consumer can disconnect and reconnect to a new SHM segment.
     */
    bool isProducerAlive() const;

private:
#if defined(_WIN32)
    HANDLE _hMapFile = NULL;
#else
    int _fd = -1;
#endif
    void* _mapped = nullptr;
    std::string _shmName;
    uint64_t _lastSeq = 0;
};

} // namespace visionpipe

#endif // LIBVISIONPIPE_FRAME_TRANSPORT_H

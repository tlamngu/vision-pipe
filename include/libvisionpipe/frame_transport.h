#ifndef LIBVISIONPIPE_FRAME_TRANSPORT_H
#define LIBVISIONPIPE_FRAME_TRANSPORT_H

/**
 * @file frame_transport.h
 * @brief Zero-copy shared memory frame transport between visionpipe process and clients
 *
 * Uses POSIX shared memory (shm_open/mmap) for zero-copy cv::Mat transfer.
 * Protocol:
 *   - Each frame_sink("name") in a .vsp script creates a shared memory region
 *   - The region contains a header + frame data buffer
 *   - Producer (visionpipe) writes frames, consumer (libvisionpipe client) reads them
 *   - Synchronization via atomic frame counter + mutex in shared memory
 */

#include <string>
#include <cstdint>
#include <cstddef>
#include <atomic>

namespace visionpipe {

// Shared memory segment name prefix
constexpr const char* SHM_PREFIX = "/visionpipe_sink_";

// Maximum frame size (default 4K resolution * 4 channels = ~33MB)
constexpr size_t MAX_FRAME_BYTES = 3840 * 2160 * 4;

// Magic value to verify shared memory is valid
constexpr uint32_t SHM_MAGIC = 0x56505346; // "VPSF" = VisionPipe Shared Frame

/**
 * @brief Header structure in shared memory (placed at the start of the mmap region)
 *
 * Layout: [FrameShmHeader][frame_data_bytes...]
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
    uint8_t _pad[16];               // Reserved
};

/**
 * @brief Total shared memory size for a sink
 */
constexpr size_t shmTotalSize() {
    return sizeof(FrameShmHeader) + MAX_FRAME_BYTES;
}

/**
 * @brief Build the POSIX shm name for a given session + sink
 * @param sessionId Unique session identifier
 * @param sinkName Sink name from frame_sink("name")
 */
std::string buildShmName(const std::string& sessionId, const std::string& sinkName);

/**
 * @brief Producer side: create and map shared memory for writing frames
 */
class FrameShmProducer {
public:
    FrameShmProducer();
    ~FrameShmProducer();

    /**
     * @brief Create/open shared memory for a sink
     * @param shmName Full shm name (from buildShmName)
     * @return true on success
     */
    bool open(const std::string& shmName);

    /**
     * @brief Write a frame into shared memory (zero-copy when possible)
     * @param data Pointer to raw pixel data
     * @param rows Frame rows
     * @param cols Frame cols
     * @param type cv::Mat type
     * @param step Bytes per row
     */
    void writeFrame(const uint8_t* data, int rows, int cols, int type, int step);

    /**
     * @brief Close and unlink shared memory
     */
    void close();

    bool isOpen() const { return _mapped != nullptr; }

private:
    int _fd = -1;
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
     * @param[out] data Pointer set to the frame data in shared memory (zero-copy!)
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

private:
    int _fd = -1;
    void* _mapped = nullptr;
    std::string _shmName;
    uint64_t _lastSeq = 0;
};

} // namespace visionpipe

#endif // LIBVISIONPIPE_FRAME_TRANSPORT_H

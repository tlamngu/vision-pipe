#ifndef VISIONPIPE_SHM_FRAME_TRANSPORT_H
#define VISIONPIPE_SHM_FRAME_TRANSPORT_H

// ── Iceoryx2 backend redirect ────────────────────────────────────────────────
// When -DVISIONPIPE_IPC_USE_ICEORYX2 is set, all shmFrameXxx() symbols are
// provided as inlines by iceoryx_frame_transport.h.  The POSIX shm
// declarations below are skipped so there is no symbol conflict.
#ifdef VISIONPIPE_IPC_USE_ICEORYX2
#  include "extensions/iceoryx/iceoryx_frame_transport.h"

// Alias the iceoryx2 transport functions under the shm_ names so existing
// code (shm_items.cpp, interpreter.cpp, etc.) compiles unchanged.
namespace visionpipe {

inline bool shmFrameCreate(const std::string& n, int w, int h, int t) {
    return iox2_transport::iox2FrameCreate(n, w, h, t);
}
inline bool shmFrameWrite(const std::string& n, const cv::Mat& f) {
    return iox2_transport::iox2FrameWrite(n, f);
}
inline bool shmFrameRead(const std::string& n, cv::Mat& out) {
    return iox2_transport::iox2FrameRead(n, out);
}
inline void shmFrameSetShutdown(const std::string& n) {
    iox2_transport::iox2FrameSetShutdown(n);
}
inline bool shmFrameIsShutdown(const std::string& n) {
    return iox2_transport::iox2FrameIsShutdown(n);
}
inline void shmFrameDestroy(const std::string& n) {
    iox2_transport::iox2FrameDestroy(n);
}
inline void shmFrameDestroyAll() {
    iox2_transport::iox2FrameDestroyAll();
}
inline std::vector<std::string> shmFrameListRegions() {
    return iox2_transport::iox2FrameListChannels();
}
/// Expose the reader-side sequence number for per-turn dedup in ShmReadItem.
/// Returns iox2FrameGetSeq(): seq of the last frame received from this channel.
/// Returns 0 if no frame has been received yet.
inline uint64_t shmFrameGetSeq(const std::string& n) {
    return iox2_transport::iox2FrameGetSeq(n);
}

} // namespace visionpipe

#else  // VISIONPIPE_IPC_USE_ICEORYX2 — default POSIX shm backend
// ────────────────────────────────────────────────────────────────────────────

/**
 * @file shm_frame_transport.h
 * @brief POSIX shared-memory frame transport for inter-process cv::Mat exchange.
 *
 * Provides a zero-copy (single memcpy) double-buffered frame transport using
 * POSIX shm_open/mmap.  A writer process writes frames to a named region; any
 * number of reader processes can retrieve the latest frame.
 *
 * Typical flow inside a .vsp script:
 *
 *   # Parent process (setup)
 *   shm_create("left_cam", 1640, 1232, 3)       # CV_8UC3
 *
 *   # Child process (capture loop, via exec_fork)
 *   video_cap("/dev/video9", "v4l2_native")
 *   debayer("GRBG")
 *   shm_write("left_cam")
 *
 *   # Parent process (main loop)
 *   shm_read("left_cam") -> global "frame_l"
 */

#include <opencv2/core/mat.hpp>
#include <string>
#include <cstdint>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace visionpipe {

// ============================================================================
// Shared memory layout (POD, mapped directly into the shm region)
// ============================================================================

/**
 * @brief Header structure at the start of each shared memory frame region.
 *
 * Uses a simple sequence-number protocol:
 *   Writer:  writes pixel data → increments writeSeq (release fence)
 *   Reader:  compares readSeq vs writeSeq → memcpy if new data → stores readSeq
 */
struct ShmFrameHeader {
    // ── Synchronisation ──
    std::atomic<uint64_t> writeSeq;  ///< bumped by writer after each frame write
    uint64_t _pad0;                  ///< cache-line padding

    // ── Shutdown flag ──
    std::atomic<int32_t>  shutdown;  ///< set to 1 by parent; children check it

    // ── Frame metadata (written by shm_create, updated by writer if needed) ──
    int32_t  width;
    int32_t  height;
    int32_t  type;           ///< cv::Mat type (e.g. CV_8UC3)
    int32_t  channels;
    uint64_t dataSize;       ///< byte length of the pixel data region

    // ── Double buffer ──
    // Two pixel buffers follow the header.  The writer alternates between
    // them (writeSeq & 1 selects the "current" buffer).  Readers always
    // read from the buffer that the latest completed write targeted.
    //
    // bufferOffset[0] and bufferOffset[1] are byte offsets from the start of
    // the mapping to the first pixel of each buffer.
    uint64_t bufferOffset[2];
};

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Create (or open if it exists) a named shared-memory frame region.
 *
 * The region is sized to hold the header + two frame buffers of the given
 * dimensions.  If the region already exists with matching size it is reused.
 *
 * @param name     POSIX shm name (will be prefixed with /vp_)
 * @param width    Frame width in pixels
 * @param height   Frame height in pixels
 * @param type     OpenCV Mat type (e.g. CV_8UC3, CV_8UC1)
 * @return true on success
 */
bool shmFrameCreate(const std::string& name, int width, int height, int type);

/**
 * @brief Write a cv::Mat frame to the named shared-memory region.
 *
 * Performs one memcpy into the inactive buffer, then atomically bumps the
 * sequence counter so readers see the new frame.
 *
 * @param name  POSIX shm name (same as passed to shmFrameCreate)
 * @param frame Frame to publish (must match the region's dimensions/type)
 * @return true on success
 */
bool shmFrameWrite(const std::string& name, const cv::Mat& frame);

/**
 * @brief Read the latest frame from the named shared-memory region.
 *
 * If no new frame has been written since the last read, the previous frame
 * is returned.  The data is memcpy'd into @p out so the caller owns the
 * buffer.
 *
 * @param name  POSIX shm name
 * @param[out] out    Destination Mat (reallocated if necessary)
 * @return true on success (even if frame is the same as last read)
 */
bool shmFrameRead(const std::string& name, cv::Mat& out);

/**
 * @brief Set the shutdown flag in a named shared-memory region.
 *
 * Child processes should poll this flag via shmFrameIsShutdown() in their
 * capture loop.
 */
void shmFrameSetShutdown(const std::string& name);

/**
 * @brief Check whether the shutdown flag has been set.
 */
bool shmFrameIsShutdown(const std::string& name);

/**
 * @brief Unmap and unlink a named shared-memory region.
 */
void shmFrameDestroy(const std::string& name);

/**
 * @brief Unmap and unlink ALL regions created by this process.
 */
void shmFrameDestroyAll();

/**
 * @brief Get a list of all active shm region names in this process.
 */
std::vector<std::string> shmFrameListRegions();

} // namespace visionpipe

#endif // VISIONPIPE_IPC_USE_ICEORYX2 (else-branch end)
#endif // VISIONPIPE_SHM_FRAME_TRANSPORT_H

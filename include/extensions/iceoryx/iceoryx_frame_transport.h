#pragma once
#ifndef VISIONPIPE_ICEORYX_FRAME_TRANSPORT_H
#define VISIONPIPE_ICEORYX_FRAME_TRANSPORT_H

/**
 * @file iceoryx_frame_transport.h
 * @brief Iceoryx2-backed frame transport API for inter-process cv::Mat exchange.
 *
 * Provides the same surface as `utils/shm_frame_transport.h` but replaces
 * POSIX shm_open + mmap with Iceoryx2 zero-copy IPC.
 *
 * When VISIONPIPE_IPC_USE_ICEORYX2 is defined, `shm_frame_transport.cpp`
 * delegates all calls here instead of using its built-in POSIX shm logic.
 *
 * Publishers (writers) run inside exec_fork child processes or standalone
 * producer pipelines.  Subscribers (readers) live in the parent or consumer
 * pipelines.  Iceoryx2 handles the memory routing; both sides call the same
 * named-channel API.
 *
 * Design notes
 * ─────────────
 * • Service name : "/vp_<name>"  (e.g. "/vp_left_cam")
 * • Payload type : iox::Slice<uint8_t>  — variable-length pixel data
 * • Prefix       : a serialised IceoryxFrameMeta struct embedded as the
 *                  first bytes of every slice so no user-header ABI is needed.
 * • History      : publishers keep 2 samples in-flight; subscribers hold 1.
 * • Requires     : Iceoryx2 >= 0.4  with C++ bindings (iceoryx2-cxx).
 */

#include <opencv2/core/mat.hpp>
#include <string>
#include <cstdint>
#include <vector>

namespace visionpipe {
namespace iox2_transport {

// ============================================================================
// Frame metadata prefix embedded at the start of every Iceoryx2 sample slice
// ============================================================================

/**
 * @brief POD header prepended to every Iceoryx2 slice payload.
 *
 * Layout of each published sample:
 *   [ IceoryxFrameMeta | <width*height*channels bytes of pixel data> ]
 */
struct IceoryxFrameMeta {
    uint64_t seqNum;    ///< Monotonic frame counter (writer-side)
    int32_t  width;
    int32_t  height;
    int32_t  type;      ///< cv::Mat type (CV_8UC1, CV_8UC3, etc.)
    int32_t  channels;
    uint64_t dataSize;  ///< Pixel data byte length (follows this struct)
    uint8_t  _pad[8];   ///< Reserved / alignment
};

// ============================================================================
// Channel lifecycle
// ============================================================================

/**
 * @brief Create (or open) a named Iceoryx2 publish/subscribe channel.
 *
 * On the publisher side this creates (or re-opens) the service.  On the
 * subscriber side this also opens the service.  Must be called by both
 * producer and consumer before writing or reading.
 *
 * @param name      Channel name (will be prefixed with /vp_)
 * @param width     Frame width  (used to pre-compute max slice capacity)
 * @param height    Frame height
 * @param type      cv::Mat type
 * @return true on success
 */
bool iox2FrameCreate(const std::string& name, int width, int height, int type);

// ============================================================================
// Write / Read
// ============================================================================

/**
 * @brief Publish a cv::Mat via the named Iceoryx2 channel.
 *
 * The frame data is zero-copied into an Iceoryx2 shared-memory sample and
 * sent to all active subscribers.
 *
 * @param name   Channel name (same as passed to iox2FrameCreate)
 * @param frame  Frame to publish
 * @return true on success
 */
bool iox2FrameWrite(const std::string& name, const cv::Mat& frame);

/**
 * @brief Receive the latest frame from the named Iceoryx2 channel.
 *
 * Drains any queued samples and copies the most-recent one into @p out.
 * If no new sample has arrived the call returns false; the caller may
 * continue using the previous frame.
 *
 * @param name    Channel name
 * @param[out] out  Destination Mat
 * @return true if a new frame was received
 */
bool iox2FrameRead(const std::string& name, cv::Mat& out);

/**
 * @brief Return the sequence number of the last frame received for a channel.
 *
 * This is the seqNum from the IceoryxFrameMeta of the most recently drained
 * sample.  Returns 0 if no frame has been received yet on this channel.
 *
 * Callers (e.g. ShmReadItem) use this as a per-turn deduplication key:
 * if the seq hasn't changed since the last fetch, the cached Mat can be
 * returned without touching the iceoryx2 subscriber queue at all.
 */
uint64_t iox2FrameGetSeq(const std::string& name);

// ============================================================================
// Shutdown / cleanup
// ============================================================================

/**
 * @brief Signal shutdown on the named channel.
 *
 * Internally publishes a zero-length sample with seqNum == UINT64_MAX.
 * Consumers detect this via iox2FrameIsShutdown().
 */
void iox2FrameSetShutdown(const std::string& name);

/**
 * @brief Return true if a shutdown signal has been received on this channel.
 */
bool iox2FrameIsShutdown(const std::string& name);

/**
 * @brief Tear down the named channel (publisher and/or subscriber).
 */
void iox2FrameDestroy(const std::string& name);

/**
 * @brief Tear down ALL channels owned by this process.
 */
void iox2FrameDestroyAll();

/**
 * @brief List all active channel names in this process.
 */
std::vector<std::string> iox2FrameListChannels();

} // namespace iox2_transport
} // namespace visionpipe

// ============================================================================
// NOTE: The shmFrameXxx() → iox2FrameXxx() alias inlines are injected by
// include/utils/shm_frame_transport.h when VISIONPIPE_IPC_USE_ICEORYX2 is
// defined.  Do NOT add them here to avoid duplicate definitions.
// ============================================================================

#endif // VISIONPIPE_ICEORYX_FRAME_TRANSPORT_H

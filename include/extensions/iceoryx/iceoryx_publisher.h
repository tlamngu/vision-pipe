#pragma once
#ifndef VISIONPIPE_ICEORYX_PUBLISHER_H
#define VISIONPIPE_ICEORYX_PUBLISHER_H

/**
 * @file iceoryx_publisher.h
 * @brief Low-level Iceoryx2 publisher/subscriber session management.
 *
 * Uses an opaque handle (PIMPL) so iceoryx2-cxx types live exclusively in
 * the .cpp files and this header remains includable without iceoryx2 present.
 * External callers should use the iox2FrameXxx() helpers in
 * iceoryx_frame_transport.h; this header is internal to the Iceoryx2 backend.
 *
 * Requires: iceoryx2-cxx >= 0.4
 */

#ifdef VISIONPIPE_ICEORYX2_ENABLED

#include "extensions/iceoryx/iceoryx_frame_transport.h"  // IceoryxFrameMeta, std::string, std::vector

#include <opencv2/core/mat.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace visionpipe {
namespace iox2_transport {

// ============================================================================
// Maximum frame payload capacity
// ============================================================================
// Default: 4096 × 3072 × 3 bytes ≈ 36 MB  (covers up to ~12 MP colour frames).
// Override at compile-time via -DIOX2_MAX_FRAME_BYTES=<n> if needed.
#ifndef IOX2_MAX_FRAME_BYTES
#define IOX2_MAX_FRAME_BYTES (36 * 1024 * 1024UL)
#endif

// ============================================================================
// Abstract channel session (concrete type lives in iceoryx_publisher.cpp)
// ============================================================================

/**
 * @brief Abstract base for an Iceoryx2 channel session.
 *
 * The concrete implementation (Iox2ChannelSessionImpl in iceoryx_publisher.cpp)
 * owns the actual iceoryx2-cxx Node + Publisher + Subscriber objects.
 * Using a virtual interface keeps all iceoryx2-cxx types out of this header.
 */
struct Iox2ChannelSession {
    std::string name;
    std::atomic<uint64_t> writeSeq{0};
    std::atomic<bool>     shutdown{false};

    // Cached frame geometry (updated on first write/receive)
    int    cachedWidth    = 0;
    int    cachedHeight   = 0;
    int    cachedType     = 0;
    int    cachedChannels = 0;
    size_t maxSliceLen    = IOX2_MAX_FRAME_BYTES + sizeof(IceoryxFrameMeta);

    // ── Per-process subscriber frame cache ───────────────────────────────
    // All interpreter threads in a process share the same Iox2ChannelSession
    // (and therefore the same iceoryx2 subscriber queue).  The first thread
    // that calls receive() on a turn drains the queue; subsequent threads
    // would see an empty queue and get nothing.
    //
    // lastReadSeq / lastReadMat fix this: receive() always caches the
    // most-recently received frame so later callers can return it without
    // a second queue drain.  Exposed via iox2FrameGetSeq() so the
    // per-turn dedup in ShmReadItem can avoid calling receive() at all.
    std::atomic<uint64_t> lastReadSeq{0};   ///< seqNum from IceoryxFrameMeta of last received frame
    cv::Mat               lastReadMat;      ///< pixel data of the last received frame
    std::mutex            lastReadMtx;      ///< protects lastReadMat

    virtual ~Iox2ChannelSession() = default;

    /** Publish pixel data with the given metadata header. */
    virtual bool publish(const void* data, size_t dataBytes,
                         const IceoryxFrameMeta& meta) = 0;

    /** Drain the subscriber queue; copy the latest frame into @p out.
     *  @return true if a new frame was received. */
    virtual bool receive(cv::Mat& out) = 0;

    /** Send the shutdown sentinel (seqNum == UINT64_MAX). */
    virtual void sendShutdown() = 0;

protected:
    Iox2ChannelSession() = default;
    Iox2ChannelSession(const Iox2ChannelSession&) = delete;
    Iox2ChannelSession& operator=(const Iox2ChannelSession&) = delete;
};

// ============================================================================
// Channel registry helpers  (implemented in iceoryx_publisher.cpp)
// ============================================================================

/** Look up an existing channel by name; nullptr if not open. */
Iox2ChannelSession* iox2ChannelGet(const std::string& name);

/**
 * Open (or reuse) a channel for the given frame geometry.
 * Creates both a Publisher and a Subscriber on the named Iceoryx2 service.
 * @return Session pointer, or nullptr on failure.
 */
Iox2ChannelSession* iox2ChannelCreate(const std::string& name,
                                      int width, int height, int type);

/** Destroy a named channel session. */
void iox2ChannelRemove(const std::string& name);

/** Destroy all channel sessions owned by this process. */
void iox2ChannelRemoveAll();

/**
 * @brief Reset the Iceoryx2 node AND channel registry for a forked child.
 *
 * After fork(), both the parent and the child share the same g_node and
 * g_channels state.  Calling this in the child process immediately after
 * fork() destroys the inherited sessions (without closing the file-descriptor
 * in the parent) and releases the singleton IpcNode so the next iox2 call
 * creates a fresh node + new service connections unique to the child's PID.
 *
 * Must be called BEFORE any iox2FrameWrite / iox2ChannelCreate in the child.
 */
void iox2NodeReset();

} // namespace iox2_transport
} // namespace visionpipe

#endif // VISIONPIPE_ICEORYX2_ENABLED
#endif // VISIONPIPE_ICEORYX_PUBLISHER_H

/**
 * @file iceoryx_frame_transport.cpp
 * @brief Iceoryx2-backed implementation of the VisionPipe frame transport API.
 *
 * Delegates to the concrete Iox2ChannelSession (owned by iceoryx_publisher.cpp)
 * via the virtual interface so no iceoryx2-cxx types appear in this file.
 *
 * When VISIONPIPE_IPC_USE_ICEORYX2 is defined, shm_frame_transport.h provides
 * inline shmFrameXxx() → iox2FrameXxx() aliases so all callers
 * (shm_items.cpp, interpreter.cpp, etc.) transparently use the Iceoryx2 backend.
 *
 * Requires: VISIONPIPE_ICEORYX2_ENABLED  (iceoryx2-cxx >= 0.4)
 */

#ifdef VISIONPIPE_ICEORYX2_ENABLED

#include "extensions/iceoryx/iceoryx_frame_transport.h"
#include "extensions/iceoryx/iceoryx_publisher.h"

#include <opencv2/core/mat.hpp>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include <mutex>

namespace visionpipe {
namespace iox2_transport {

// ============================================================================
// iox2FrameCreate
// ============================================================================

bool iox2FrameCreate(const std::string& name, int width, int height, int type) {
    auto* session = iox2ChannelCreate(name, width, height, type);
    if (!session) {
        std::cerr << "[iox2_transport] iox2FrameCreate: failed for channel '"
                  << name << "'\n";
        return false;
    }
    return true;
}

// ============================================================================
// iox2FrameWrite  (publisher side)
// ============================================================================

bool iox2FrameWrite(const std::string& name, const cv::Mat& frame) {
    if (frame.empty()) return false;

    auto* session = iox2ChannelGet(name);
    if (!session) {
        // Auto-create using the frame's actual geometry
        session = iox2ChannelCreate(name, frame.cols, frame.rows, frame.type());
        if (!session) return false;
    }

    // Ensure contiguous data
    cv::Mat cont = frame.isContinuous() ? frame : frame.clone();
    const size_t dataBytes = cont.total() * cont.elemSize();

    IceoryxFrameMeta meta{};
    meta.seqNum   = ++session->writeSeq;
    meta.width    = cont.cols;
    meta.height   = cont.rows;
    meta.type     = cont.type();
    meta.channels = cont.channels();
    meta.dataSize = static_cast<uint64_t>(dataBytes);

    return session->publish(cont.data, dataBytes, meta);
}

// ============================================================================
// iox2FrameRead  (subscriber side)
// ============================================================================

bool iox2FrameRead(const std::string& name, cv::Mat& out) {
    auto* session = iox2ChannelGet(name);
    if (!session) {
        // Lazy open: geometry unknown yet; create with placeholder values.
        // Actual geometry is filled in on the first received sample.
        session = iox2ChannelCreate(name, 0, 0, CV_8UC3);
        if (!session) return false;
    }
    return session->receive(out);
}

// ============================================================================
// Shutdown helpers
// ============================================================================

void iox2FrameSetShutdown(const std::string& name) {
    auto* session = iox2ChannelGet(name);
    if (session) session->sendShutdown();
}

bool iox2FrameIsShutdown(const std::string& name) {
    auto* session = iox2ChannelGet(name);
    return session && session->shutdown.load(std::memory_order_acquire);
}

uint64_t iox2FrameGetSeq(const std::string& name) {
    auto* session = iox2ChannelGet(name);
    if (!session) return 0;
    return session->lastReadSeq.load(std::memory_order_acquire);
}

// ============================================================================
// Destroy
// ============================================================================

void iox2FrameDestroy(const std::string& name) {
    iox2ChannelRemove(name);
}

void iox2FrameDestroyAll() {
    iox2ChannelRemoveAll();
}

// ============================================================================
// List channels  (stub — registry is private to iceoryx_publisher.cpp)
// ============================================================================

std::vector<std::string> iox2FrameListChannels() {
    return {};   // Extend if needed by exposing the registry iterator
}

} // namespace iox2_transport
} // namespace visionpipe

#endif // VISIONPIPE_ICEORYX2_ENABLED

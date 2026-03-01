#ifndef VISIONPIPE_FRAME_SINK_ITEMS_H
#define VISIONPIPE_FRAME_SINK_ITEMS_H

/**
 * @file frame_sink_items.h
 * @brief frame_sink() interpreter item - publishes frames to shared memory
 *
 * Used inside .vsp scripts to expose frames to libvisionpipe clients:
 *
 *   video_cap(0)
 *   resize(640, 480)
 *   frame_sink("output")   # Makes this frame available to client via shm
 *
 * The session ID is set via environment variable VISIONPIPE_SESSION_ID.
 */

#include "interpreter/item_registry.h"
#include "libvisionpipe/frame_transport.h"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace visionpipe {

/**
 * @brief Global registry of active frame sink producers (one per sink name)
 */
class FrameSinkRegistry {
public:
    static FrameSinkRegistry& instance();

    /**
     * @brief Set the session ID (from env or runtime)
     */
    void setSessionId(const std::string& sessionId);
    const std::string& sessionId() const { return _sessionId; }

    /**
     * @brief Get or create a producer for the given sink name
     */
    std::shared_ptr<FrameShmProducer> getProducer(const std::string& sinkName);

    /**
     * @brief Close all producers
     */
    void closeAll();

private:
    FrameSinkRegistry() = default;
    std::string _sessionId;
    std::mutex _mutex;
    std::unordered_map<std::string, std::shared_ptr<FrameShmProducer>> _producers;
};

// ============================================================================
// Interpreter Items
// ============================================================================

/**
 * @brief Publishes the current frame to a named shared-memory sink
 *
 * Parameters:
 * - name: Sink name identifier (string)
 *
 * Example (in .vsp):
 *   video_cap(0)
 *   resize(640, 480)
 *   frame_sink("processed_output")
 */
class FrameSinkItem : public InterpreterItem {
public:
    FrameSinkItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Closes a specific frame sink
 *
 * Parameters:
 * - name: Sink name to close
 */
class FrameSinkCloseItem : public InterpreterItem {
public:
    FrameSinkCloseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Register frame_sink items
 */
void registerFrameSinkItems(ItemRegistry& registry);

} // namespace visionpipe

#endif // VISIONPIPE_FRAME_SINK_ITEMS_H

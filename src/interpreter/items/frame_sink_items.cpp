#include "interpreter/items/frame_sink_items.h"
#include <cstdlib>
#include <iostream>
#include <unistd.h>

namespace visionpipe {

// ============================================================================
// FrameSinkRegistry
// ============================================================================

FrameSinkRegistry& FrameSinkRegistry::instance() {
    static FrameSinkRegistry inst;
    return inst;
}

void FrameSinkRegistry::setSessionId(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(_mutex);
    _sessionId = sessionId;
}

std::shared_ptr<FrameShmProducer> FrameSinkRegistry::getProducer(const std::string& sinkName) {
    std::lock_guard<std::mutex> lock(_mutex);

    // Auto-detect session ID from env if not set
    if (_sessionId.empty()) {
        const char* env = std::getenv("VISIONPIPE_SESSION_ID");
        if (env && env[0]) {
            _sessionId = env;
        } else {
            // Generate from PID
            _sessionId = std::to_string(getpid());
        }
    }

    auto it = _producers.find(sinkName);
    if (it != _producers.end() && it->second->isOpen()) {
        return it->second;
    }

    // Create new producer
    auto producer = std::make_shared<FrameShmProducer>();
    std::string shmName = buildShmName(_sessionId, sinkName);

    if (!producer->open(shmName)) {
        std::cerr << "[frame_sink] Failed to open shm: " << shmName << std::endl;
        return nullptr;
    }

    std::cout << "[frame_sink] Created sink '" << sinkName << "' (shm: " << shmName << ")" << std::endl;
    _producers[sinkName] = producer;
    return producer;
}

void FrameSinkRegistry::closeAll() {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& [name, prod] : _producers) {
        prod->close();
    }
    _producers.clear();
}

// ============================================================================
// FrameSinkItem
// ============================================================================

FrameSinkItem::FrameSinkItem() {
    _functionName = "frame_sink";
    _description = "Publishes the current frame to a named shared-memory sink for external consumption (libvisionpipe)";
    _category = "io";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Sink name identifier")
    };
    _example = R"(frame_sink("output"))";
    _returnType = "mat";
    _tags = {"sink", "ipc", "shared_memory", "libvisionpipe"};
}

ExecutionResult FrameSinkItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("frame_sink: no frame available (currentMat is empty)");
    }

    std::string sinkName = args[0].asString();

    auto producer = FrameSinkRegistry::instance().getProducer(sinkName);
    if (!producer) {
        return ExecutionResult::fail("frame_sink: failed to create producer for '" + sinkName + "'");
    }

    // Ensure continuous memory layout
    cv::Mat continuous;
    if (ctx.currentMat.isContinuous()) {
        continuous = ctx.currentMat;
    } else {
        continuous = ctx.currentMat.clone();
    }

    producer->writeFrame(
        continuous.data,
        continuous.rows,
        continuous.cols,
        continuous.type(),
        static_cast<int>(continuous.step[0])
    );

    // Pass-through: frame_sink does not modify the current mat
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// FrameSinkCloseItem
// ============================================================================

FrameSinkCloseItem::FrameSinkCloseItem() {
    _functionName = "frame_sink_close";
    _description = "Closes a named frame sink and releases shared memory";
    _category = "io";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Sink name to close")
    };
    _example = R"(frame_sink_close("output"))";
    _returnType = "void";
    _tags = {"sink", "close", "ipc"};
}

ExecutionResult FrameSinkCloseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string sinkName = args[0].asString();
    auto& reg = FrameSinkRegistry::instance();
    // Close just calls closeAll for now; a per-sink close can be added later
    // For now just log
    std::cout << "[frame_sink_close] Closing sink '" << sinkName << "'" << std::endl;
    reg.closeAll();
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Registration
// ============================================================================

void registerFrameSinkItems(ItemRegistry& registry) {
    registry.add<FrameSinkItem>();
    registry.add<FrameSinkCloseItem>();
}

} // namespace visionpipe

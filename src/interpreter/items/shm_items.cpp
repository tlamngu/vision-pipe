#include "interpreter/items/shm_items.h"
#include "utils/shm_frame_transport.h"
#include <opencv2/core.hpp>
#include <mutex>
#include <unordered_map>

namespace visionpipe {

#ifdef VISIONPIPE_IPC_USE_ICEORYX2
// ============================================================================
// Per-process, per-channel frame cache for iceoryx2
//
// Background: all interpreter threads in one process share the same iceoryx2
// subscriber session (and therefore the same queue).  Without this cache, the
// first caller (e.g. brightness_adjustment) drains the queue and the second
// caller (e.g. display) finds nothing.
//
// The session-level fix in receive() already handles the correctness problem
// by returning the cached lastReadMat when the queue is empty.  This additional
// process-level cache is a performance optimization: it lets subsequent callers
// skip the subscriber receive() call entirely by checking the reader-side
// sequence number (iox2FrameGetSeq / shmFrameGetSeq).
//
// • Key   : channel name
// • Value : {seqNum of last cached frame, Mat}
// ============================================================================
namespace {
struct Iox2FrameCache {
    std::mutex                                                    mtx;
    std::unordered_map<std::string, std::pair<uint64_t, cv::Mat>> entries;
};

Iox2FrameCache& iox2TurnCache() {
    static Iox2FrameCache c;
    return c;
}
} // anonymous namespace
#endif // VISIONPIPE_IPC_USE_ICEORYX2

// ============================================================================
// ShmCreateItem
// ============================================================================

ShmCreateItem::ShmCreateItem() {
    _functionName = "shm_create";
    _description  = "Create a named POSIX shared-memory region for inter-process "
                    "frame exchange (double-buffered).  Call once during setup.";
    _category = "video_io";
    _params = {
        ParamDef::required("name",     BaseType::STRING, "Shared-memory region name"),
        ParamDef::required("width",    BaseType::INT,    "Frame width in pixels"),
        ParamDef::required("height",   BaseType::INT,    "Frame height in pixels"),
        ParamDef::optional("channels", BaseType::INT,    "Number of channels (1=gray, 3=BGR)", 3)
    };
    _example    = "shm_create(\"left_cam\", 1640, 1232, 3)";
    _returnType = "void";
    _tags       = {"shm", "shared_memory", "ipc", "multiprocess"};
}

ExecutionResult ShmCreateItem::execute(const std::vector<RuntimeValue>& args,
                                        ExecutionContext& /*ctx*/) {
    std::string name = args[0].asString();
    int width   = static_cast<int>(args[1].asNumber());
    int height  = static_cast<int>(args[2].asNumber());
    int channels = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 3;

    int cvType;
    switch (channels) {
        case 1:  cvType = CV_8UC1; break;
        case 3:  cvType = CV_8UC3; break;
        case 4:  cvType = CV_8UC4; break;
        default: cvType = CV_8UC3; break;
    }

    if (!shmFrameCreate(name, width, height, cvType)) {
        return ExecutionResult::fail("shm_create: failed to create region '" + name + "'");
    }
    return ExecutionResult::ok(cv::Mat());
}

// ============================================================================
// ShmWriteItem
// ============================================================================

ShmWriteItem::ShmWriteItem() {
    _functionName = "shm_write";
    _description  = "Write the current frame to a named shared-memory region.  "
                    "Used inside exec_fork pipelines to publish captured frames.";
    _category = "video_io";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Shared-memory region name")
    };
    _example    = "shm_write(\"left_cam\")";
    _returnType = "mat";
    _tags       = {"shm", "shared_memory", "ipc", "write", "multiprocess"};
}

ExecutionResult ShmWriteItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("shm_write: no frame to write");
    }
    std::string name = args[0].asString();
    if (!shmFrameWrite(name, ctx.currentMat)) {
        return ExecutionResult::fail("shm_write: failed to write to '" + name + "'");
    }
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ShmReadItem
// ============================================================================

ShmReadItem::ShmReadItem() {
    _functionName = "shm_read";
    _description  = "Read the latest frame from a named shared-memory region.  "
                    "Returns the most recent frame published by a shm_write in "
                    "another process (e.g. an exec_fork capture pipeline).";
    _category = "video_io";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Shared-memory region name")
    };
    _example    = "shm_read(\"left_cam\")";
    _returnType = "mat";
    _tags       = {"shm", "shared_memory", "ipc", "read", "multiprocess"};
}

ExecutionResult ShmReadItem::execute(const std::vector<RuntimeValue>& args,
                                      ExecutionContext& ctx) {
    std::string name = args[0].asString();

#ifdef VISIONPIPE_IPC_USE_ICEORYX2
    // Per-turn dedup: if the reader-side seq for this channel hasn't changed
    // since another thread/pipeline already fetched it, return the cached Mat
    // without touching the iceoryx2 subscriber queue at all.
    uint64_t curSeq = shmFrameGetSeq(name);
    if (curSeq > 0) {
        std::lock_guard<std::mutex> lk(iox2TurnCache().mtx);
        auto it = iox2TurnCache().entries.find(name);
        if (it != iox2TurnCache().entries.end() && it->second.first == curSeq) {
            ctx.currentMat = it->second.second;
            return ExecutionResult::ok(ctx.currentMat);
        }
    }
#endif

    cv::Mat frame;
    if (!shmFrameRead(name, frame)) {
        // No frame available yet – pass through (don't fail on first iteration).
        return ExecutionResult::ok(ctx.currentMat);
    }

#ifdef VISIONPIPE_IPC_USE_ICEORYX2
    // Write-through: cache by the post-receive seq so subsequent callers
    // in the same process turn skip the subscriber queue entirely.
    uint64_t newSeq = shmFrameGetSeq(name);   // updated by receive() above
    if (newSeq > 0) {
        std::lock_guard<std::mutex> lk(iox2TurnCache().mtx);
        iox2TurnCache().entries[name] = {newSeq, frame};
    }
#endif

    return ExecutionResult::ok(frame);
}

// ============================================================================
// Registration
// ============================================================================

void registerShmItems(ItemRegistry& registry) {
    registry.add(std::make_shared<ShmCreateItem>());
    registry.add(std::make_shared<ShmWriteItem>());
    registry.add(std::make_shared<ShmReadItem>());
}

} // namespace visionpipe

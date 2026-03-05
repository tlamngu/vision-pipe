#include "interpreter/items/shm_items.h"
#include "utils/shm_frame_transport.h"
#include <opencv2/core.hpp>

namespace visionpipe {

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
    cv::Mat frame;
    if (!shmFrameRead(name, frame)) {
        // No frame available yet – pass through (don't fail on first iteration).
        return ExecutionResult::ok(ctx.currentMat);
    }
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

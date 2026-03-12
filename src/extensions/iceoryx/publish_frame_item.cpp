/**
 * @file publish_frame_item.cpp
 * @brief "iceoryx_publish" and "iceoryx_subscribe" interpreter items.
 *
 * Provides two VisionPipe script functions:
 *
 *   iceoryx_publish(name)     – publish currentMat through Iceoryx2 IPC.
 *   iceoryx_subscribe(name)   – receive the latest frame via Iceoryx2 IPC.
 *
 * These items are compiled only when VISIONPIPE_ICEORYX2_ENABLED is defined
 * (i.e. when CMake was configured with -DVISIONPIPE_WITH_ICEORYX2=ON).
 * They complement the shm_write / shm_read items: when
 * -DVISIONPIPE_IPC_USE_ICEORYX2=ON is also set, shm_write/read transparently
 * delegate to Iceoryx2 as well, so iceoryx_publish and iceoryx_subscribe are
 * useful when you want _explicit_ Iceoryx2 items alongside the shm aliases.
 *
 * Registration:
 *   This file exposes registerIceoryxPublishItems(registry) which must be
 *   called from all_items.cpp when VISIONPIPE_ICEORYX2_ENABLED is defined.
 */

#ifdef VISIONPIPE_ICEORYX2_ENABLED

#include "extensions/iceoryx/publish_frame_item.h"
#include "extensions/iceoryx/iceoryx_publisher.h"
#include "extensions/iceoryx/iceoryx_frame_transport.h"

#include <opencv2/core.hpp>
#include <iostream>

namespace visionpipe {

// ============================================================================
// IceoryxPublishItem — iceoryx_publish(name)
// ============================================================================

IceoryxPublishItem::IceoryxPublishItem() {
    _functionName = "iceoryx_publish";
    _description  = "Publish the current frame to a named Iceoryx2 IPC channel. "
                    "Any process subscribed to the same channel name will receive "
                    "the frame with zero-copy shared-memory semantics.";
    _category = "video_io";
    _params = {
        ParamDef::required("name", BaseType::STRING,
                           "Iceoryx2 channel name (e.g. \"left_cam\")"),
        ParamDef::optional("width",    BaseType::INT,
                           "Frame width for implicit channel creation (0=auto)", 0),
        ParamDef::optional("height",   BaseType::INT,
                           "Frame height for implicit channel creation (0=auto)", 0),
        ParamDef::optional("channels", BaseType::INT,
                           "Number of channels for implicit creation (0=auto)", 0),
    };
    _example    = "iceoryx_publish(\"left_cam\")";
    _returnType = "mat";
    _tags       = {"iceoryx2", "ipc", "publish", "shared_memory", "multiprocess"};
}

ExecutionResult IceoryxPublishItem::execute(const std::vector<RuntimeValue>& args,
                                            ExecutionContext& ctx) {
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("iceoryx_publish: no frame to publish");
    }

    std::string name = args[0].asString();

    // Optional explicit geometry (used to pre-create the channel on first call)
    int w = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    int h = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;
    int c = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;

    // If the channel doesn't exist yet, create it now using either the
    // explicit geometry or the geometry inferred from currentMat.
    if (!iox2_transport::iox2ChannelGet(name)) {
        int fw = (w > 0) ? w : ctx.currentMat.cols;
        int fh = (h > 0) ? h : ctx.currentMat.rows;
        int ft = (c > 0) ? (c == 1 ? CV_8UC1 : (c == 4 ? CV_8UC4 : CV_8UC3))
                          : ctx.currentMat.type();
        if (!iox2_transport::iox2FrameCreate(name, fw, fh, ft)) {
            return ExecutionResult::fail(
                "iceoryx_publish: failed to create channel '" + name + "'");
        }
    }

    if (!iox2_transport::iox2FrameWrite(name, ctx.currentMat)) {
        return ExecutionResult::fail(
            "iceoryx_publish: failed to publish to channel '" + name + "'");
    }

    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// IceoryxSubscribeItem — iceoryx_subscribe(name)
// ============================================================================

IceoryxSubscribeItem::IceoryxSubscribeItem() {
    _functionName = "iceoryx_subscribe";
    _description  = "Receive the latest frame from a named Iceoryx2 IPC channel. "
                    "If no new frame has been published since the last call the "
                    "previous frame is passed through unchanged.";
    _category = "video_io";
    _params = {
        ParamDef::required("name", BaseType::STRING,
                           "Iceoryx2 channel name (must match the publisher)"),
    };
    _example    = "iceoryx_subscribe(\"left_cam\")";
    _returnType = "mat";
    _tags       = {"iceoryx2", "ipc", "subscribe", "shared_memory", "multiprocess"};
}

ExecutionResult IceoryxSubscribeItem::execute(const std::vector<RuntimeValue>& args,
                                              ExecutionContext& ctx) {
    std::string name = args[0].asString();

    cv::Mat frame;
    if (iox2_transport::iox2FrameRead(name, frame) && !frame.empty()) {
        return ExecutionResult::ok(frame);
    }
    // No new frame — pass through current
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Registration
// ============================================================================

void registerIceoryxPublishItems(ItemRegistry& registry) {
    registry.add(std::make_shared<IceoryxPublishItem>());
    registry.add(std::make_shared<IceoryxSubscribeItem>());
}

} // namespace visionpipe

#endif // VISIONPIPE_ICEORYX2_ENABLED

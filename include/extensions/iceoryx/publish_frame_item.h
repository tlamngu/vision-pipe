#pragma once
#ifndef VISIONPIPE_PUBLISH_FRAME_ITEM_H
#define VISIONPIPE_PUBLISH_FRAME_ITEM_H

/**
 * @file publish_frame_item.h
 * @brief Interpreter items for explicit Iceoryx2 IPC frame exchange.
 *
 * Provides:
 *   iceoryx_publish(name [, width, height, channels])  – publish via Iceoryx2
 *   iceoryx_subscribe(name)                            – receive via Iceoryx2
 *
 * Only compiled when VISIONPIPE_ICEORYX2_ENABLED is defined.
 */

#ifdef VISIONPIPE_ICEORYX2_ENABLED

#include "interpreter/item_registry.h"

namespace visionpipe {

// ── iceoryx_publish ─────────────────────────────────────────────────────────

class IceoryxPublishItem : public InterpreterItem {
public:
    IceoryxPublishItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
};

// ── iceoryx_subscribe ───────────────────────────────────────────────────────

class IceoryxSubscribeItem : public InterpreterItem {
public:
    IceoryxSubscribeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
};

// ── Registration ────────────────────────────────────────────────────────────

void registerIceoryxPublishItems(ItemRegistry& registry);

} // namespace visionpipe

#endif // VISIONPIPE_ICEORYX2_ENABLED
#endif // VISIONPIPE_PUBLISH_FRAME_ITEM_H

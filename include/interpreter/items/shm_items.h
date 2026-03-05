#ifndef VISIONPIPE_SHM_ITEMS_H
#define VISIONPIPE_SHM_ITEMS_H

/**
 * @file shm_items.h
 * @brief Interpreter items for POSIX shared-memory frame exchange.
 *
 * Provides:
 *   shm_create(name, width, height, channels)  – allocate a named shm region
 *   shm_write(name)                            – publish currentMat to shm
 *   shm_read(name)                             – fetch latest frame from shm
 */

#include "interpreter/item_registry.h"

namespace visionpipe {

// ── shm_create ──────────────────────────────────────────────────────────────

class ShmCreateItem : public InterpreterItem {
public:
    ShmCreateItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
};

// ── shm_write ───────────────────────────────────────────────────────────────

class ShmWriteItem : public InterpreterItem {
public:
    ShmWriteItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
};

// ── shm_read ────────────────────────────────────────────────────────────────

class ShmReadItem : public InterpreterItem {
public:
    ShmReadItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args,
                            ExecutionContext& ctx) override;
};

// ── Registration ────────────────────────────────────────────────────────────

void registerShmItems(ItemRegistry& registry);

} // namespace visionpipe

#endif // VISIONPIPE_SHM_ITEMS_H

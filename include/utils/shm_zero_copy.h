#pragma once

/**
 * @file shm_zero_copy.h
 * @brief Zero-copy anonymous shared-memory arena for inter-process cv::Mat exchange.
 *
 * Unlike the POSIX shm_open transport (which creates named files under
 * /dev/shm and performs memcpy on both write AND read), this arena:
 *
 *   1. Uses mmap(MAP_SHARED | MAP_ANONYMOUS) — pure RAM, no filesystem paths.
 *   2. Is allocated BEFORE fork(), so parent and children share the mapping.
 *   3. Provides true zero-copy reads: the returned cv::Mat header wraps a
 *      pointer directly into the arena buffer.
 *   4. Triple-buffered per slot so the reader's Mat remains valid while
 *      the writer continues producing (safe for at least 2 writer frames).
 *   5. Fully lock-free hot path — synchronisation via atomics only.
 *
 * Typical lifecycle:
 *
 *   Parent (before fork):
 *     arena = shmArenaCreate();   // anonymous mmap, 8 slots, 8 MB/slot
 *
 *   Parent: fork() → child inherits the mmap automatically.
 *
 *   Child (write loop):
 *     shmArenaWrite(arena, "frame_l", mat);  // one memcpy into arena buffer
 *     shmArenaRecordThroughput(arena, "capture_left", durationNs);
 *
 *   Parent (read loop):
 *     shmArenaRead(arena, "frame_l", out);  // ZERO COPY — out.data → arena
 *
 *   Shutdown:
 *     shmArenaSetShutdown(arena);   // child checks shmArenaIsShutdown()
 *     shmArenaDestroy(arena);       // munmap
 */

#include <opencv2/core/mat.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace visionpipe {

/// Opaque arena handle (per-process bookkeeping; the actual data lives in the
/// anonymous mmap which is shared across fork() children).
struct ShmArena;

/// Create an arena backed by anonymous mmap (pure RAM, no filesystem).
/// Must be called BEFORE fork() so children inherit the mapping.
/// @param maxSlots      Maximum named frame channels (default 8).
/// @param maxFrameBytes Maximum byte size per individual frame buffer (default 32 MB).
///                      Raise this if you use large sensors (e.g. 3280×2464 BGR ≈ 24 MB).
/// @return Arena handle, or nullptr on failure.
ShmArena* shmArenaCreate(int maxSlots = 8,
                          size_t maxFrameBytes = 32UL * 1024 * 1024);

/// Destroy the arena (munmap).  Only the parent should call this after all
/// children have exited.  Children that call _exit() never reach destructors
/// so the arena remains valid.
void shmArenaDestroy(ShmArena* arena);

/// Write a frame to a named slot (one memcpy from Mat data → arena buffer).
/// The slot is auto-created on the first call.
/// @return false if the arena is full or the frame exceeds maxFrameBytes.
bool shmArenaWrite(ShmArena* arena, const std::string& name, const cv::Mat& frame);

/// Zero-copy read: @p out receives a cv::Mat header whose data pointer
/// references the arena buffer directly — no pixel copy.  The data is valid
/// until the writer completes 2 more frames (triple-buffering guarantee).
/// @return false if no frame has been written yet to this slot.
bool shmArenaRead(ShmArena* arena, const std::string& name, cv::Mat& out);

/// Signal all processes sharing this arena to shut down.
void shmArenaSetShutdown(ShmArena* arena);

/// Check if the arena has been shut down.
bool shmArenaIsShutdown(ShmArena* arena);

/// Return the current monotonic write-sequence number for a named slot.
/// Returns 0 if the slot does not exist yet.  Cheap: single atomic load after
/// a linear scan over at most 8 slot headers.
uint64_t shmArenaGetSeq(ShmArena* arena, const std::string& name);

// ============================================================================
// Fork-child throughput tracking (shared via arena)
// ============================================================================

/// Record one pipeline-execution sample for a named pipeline.
/// Called by the fork child after each iteration.
void shmArenaRecordThroughput(ShmArena* arena,
                               const std::string& pipelineName,
                               uint64_t durationNs);

/// Snapshot of throughput counters for one fork-child pipeline.
struct ShmThroughputData {
    std::string name;
    uint64_t callCount;
    uint64_t totalNs;
    uint64_t minNs;
    uint64_t maxNs;
    // 8-bucket latency histogram matching ShmThroughputSlot::latBuckets:
    //   [0]<1ms  [1]1-2ms  [2]2-4ms  [3]4-8ms
    //   [4]8-16ms  [5]16-32ms  [6]32-64ms  [7]>=64ms
    uint32_t latBuckets[8];
};

/// Read all active throughput entries from the arena.
/// The parent's throughput printer calls this to merge fork-child stats.
std::vector<ShmThroughputData> shmArenaReadThroughput(ShmArena* arena);

/// Compute the write-to-read latency for a named frame slot and record it as a
/// throughput sample named "ipc:<name>" in the arena's throughput table.
/// Call this from the reader side (parent) after a successful shmArenaRead.
/// The IPC latency rows appear in the throughput/latency table as fork:ipc:<name>.
void shmArenaRecordReadLatency(ShmArena* arena, const std::string& name);

} // namespace visionpipe

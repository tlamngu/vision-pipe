#include "interpreter/cache_manager.h"
#include "utils/shm_zero_copy.h"
#include <sstream>
#include <chrono>
#include <shared_mutex>

namespace visionpipe {

CacheManager::CacheManager()
    : _sharedGlobal(std::make_shared<GlobalCacheData>()) {
    // Start with one local scope
    _localScopes.push_back({});
}

CacheManager::CacheManager(std::shared_ptr<GlobalCacheData> sharedGlobal)
    : _sharedGlobal(std::move(sharedGlobal)) {
    // Start with one local scope
    _localScopes.push_back({});
}

// ============================================================================
// Global cache operations
// ============================================================================

void CacheManager::setGlobal(const std::string& id, const cv::Mat& mat, const std::string& source) {
    std::unique_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
    
    CacheEntry entry;
    entry.mat = mat;
    entry.source = source;
    entry.timestamp = 0;
    entry.accessCount = 0;
    entry.isGlobal = true;
    entry.shmSeq = 0;  // Not sourced from SHM; clear so getGlobal always re-reads
    
    _sharedGlobal->entries[id] = std::move(entry);

    // In fork child processes, write the frame into the anonymous-mmap arena
    // so the parent (and its threads) can read it via zero-copy getGlobal().
    if (_isForkChild && _shmArena && !mat.empty()) {
        lock.unlock();  // don't hold lock during memcpy into arena
        shmArenaWrite(_shmArena, id, mat);
    }
}

cv::Mat CacheManager::getGlobal(const std::string& id) const {
    // When fork children are running, use a seq-number write-through cache:
    //
    //   1. Read the SHM slot's writeSeq (one atomic load — very cheap).
    //   2. If the in-process shared cache already holds a copy with the same
    //      seq, return it immediately — no arena read, no clone.
    //   3. Otherwise read from the arena (zero-copy header), clone to own
    //      the data (arena triple-buffer may be recycled after 2 more writes),
    //      and write it back with the seq so the next caller (e.g. the main
    //      display loop after brightness_adjustment) gets a cache hit.
    //
    // Because all threads share _sharedGlobal, the first caller in any turn
    // that reads a new frame pays one shmArenaRead + clone; every subsequent
    // caller in the same turn (same writeSeq) pays only one shared_lock read.
    if (_hasForkChildren && _shmArena) {
        uint64_t shmSeq = shmArenaGetSeq(_shmArena, id);
        if (shmSeq > 0) {
            // Fast path: in-process cache has the current frame already.
            {
                std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
                auto it = _sharedGlobal->entries.find(id);
                if (it != _sharedGlobal->entries.end() &&
                    it->second.shmSeq == shmSeq) {
                    const_cast<CacheEntry&>(it->second).accessCount++;
                    return it->second.mat;  // cache hit — no SHM read
                }
            }

            // Slow path: read from arena, clone, and write-through.
            cv::Mat shmMat;
            if (shmArenaRead(_shmArena, id, shmMat)) {
                shmArenaRecordReadLatency(_shmArena, id);
                // Clone so the stored Mat owns its data; the arena triple-
                // buffer is only guaranteed stable for 2 more writer frames.
                cv::Mat owned = shmMat.clone();
                {
                    std::unique_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
                    CacheEntry& entry    = _sharedGlobal->entries[id];
                    entry.mat            = owned;
                    entry.shmSeq         = shmSeq;
                    entry.isGlobal       = true;
                    entry.accessCount    = 1;
                }
                return owned;
            }
        }
        // shmSeq==0 means the child hasn't written this slot yet; fall through
        // to the in-process cache (may have a pre-fork value) or return empty.
    }

    // Secondary SHM read path for fork grandchildren:
    // _hasForkChildren is intentionally false in fork children (see execExecFork),
    // but a fork-of-a-fork (e.g. chessboard_loop inside camera_loop) still has
    // _shmArena inherited from its parent and needs to read frames that a sibling
    // fork child (e.g. camera_fetch) writes into the arena.
    //
    // Writers identify their entries with shmSeq=0 in the in-process map so
    // we detect them by that sentinel and return without an arena round-trip.
    if (_shmArena && !_hasForkChildren) {
        uint64_t shmSeq = shmArenaGetSeq(_shmArena, id);
        if (shmSeq > 0) {
            // Fast path: in-process cache may already have a current copy.
            {
                std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
                auto it = _sharedGlobal->entries.find(id);
                if (it != _sharedGlobal->entries.end()) {
                    // shmSeq==0 on the entry marks a locally-written frame
                    // (the writer process itself); always treat it as current.
                    if (it->second.shmSeq == 0 || it->second.shmSeq == shmSeq) {
                        const_cast<CacheEntry&>(it->second).accessCount++;
                        return it->second.mat;
                    }
                }
            }
            // Slow path: read the updated frame from the arena.
            cv::Mat shmMat;
            if (shmArenaRead(_shmArena, id, shmMat)) {
                cv::Mat owned = shmMat.clone();
                {
                    std::unique_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
                    CacheEntry& entry = _sharedGlobal->entries[id];
                    entry.mat         = owned;
                    entry.shmSeq      = shmSeq;
                    entry.isGlobal    = true;
                    entry.accessCount = 1;
                }
                return owned;
            }
        }
    }

    // Fallback: in-process shared map (pre-fork values or direct setGlobal calls).
    std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
    auto it = _sharedGlobal->entries.find(id);
    if (it != _sharedGlobal->entries.end()) {
        const_cast<CacheEntry&>(it->second).accessCount++;
        return it->second.mat;
    }
    return cv::Mat();
}

bool CacheManager::hasGlobal(const std::string& id) const {
    // When an SHM arena is attached, check it first — this covers fork
    // grandchildren (e.g. chessboard_loop) that have _hasForkChildren=false
    // but still need to detect frames written by a sibling fork child
    // (e.g. camera_fetch) into the shared arena.
    if (_shmArena && shmArenaGetSeq(_shmArena, id) > 0) return true;
    std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
    return _sharedGlobal->entries.count(id) > 0;
}

void CacheManager::removeGlobal(const std::string& id) {
    std::unique_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
    _sharedGlobal->entries.erase(id);
}

void CacheManager::clearGlobal() {
    std::unique_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
    _sharedGlobal->entries.clear();
}

std::vector<std::string> CacheManager::getGlobalIds() const {
    std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
    
    std::vector<std::string> ids;
    ids.reserve(_sharedGlobal->entries.size());
    for (const auto& [id, _] : _sharedGlobal->entries) {
        ids.push_back(id);
    }
    return ids;
}

// ============================================================================
// Local cache operations
// ============================================================================

size_t CacheManager::pushScope() {
    _localScopes.push_back({});
    return _localScopes.size() - 1;
}

void CacheManager::popScope() {
    if (_localScopes.size() > 1) {
        _localScopes.pop_back();
    } else {
        _localScopes.back().clear();
    }
}

void CacheManager::setLocal(const std::string& id, const cv::Mat& mat, const std::string& source) {
    if (_localScopes.empty()) {
        _localScopes.push_back({});
    }
    
    CacheEntry entry;
    // Shallow copy: local scopes are strictly per-thread, and most items return
    // a newly-allocated cv::Mat rather than modifying the buffer in-place, so
    // there is no risk of the caller mutating the stored Mat through a shared
    // buffer.  Callers that genuinely need isolation (e.g. CacheItem) already
    // clone before calling set(), so double-cloning is avoided here.
    entry.mat = mat;
    entry.source = source;
    entry.timestamp = 0;   // Timestamps are metadata; skip the syscall in the hot path.
    entry.accessCount = 0;
    entry.isGlobal = false;
    
    _localScopes.back()[id] = std::move(entry);
}

cv::Mat CacheManager::getLocal(const std::string& id) const {
    // Search from innermost scope outward
    for (auto it = _localScopes.rbegin(); it != _localScopes.rend(); ++it) {
        auto entryIt = it->find(id);
        if (entryIt != it->end()) {
            const_cast<CacheEntry&>(entryIt->second).accessCount++;
            return entryIt->second.mat;
        }
    }
    return cv::Mat();
}

bool CacheManager::hasLocal(const std::string& id) const {
    for (auto it = _localScopes.rbegin(); it != _localScopes.rend(); ++it) {
        if (it->count(id) > 0) {
            return true;
        }
    }
    return false;
}

void CacheManager::clearLocalScope() {
    if (!_localScopes.empty()) {
        _localScopes.back().clear();
    }
}

void CacheManager::resetLocalScopes() {
    _localScopes.clear();
    _localScopes.push_back({});
}

// ============================================================================
// Unified cache operations
// ============================================================================

void CacheManager::set(const std::string& id, const cv::Mat& mat, bool isGlobal, const std::string& source) {
    if (isGlobal) {
        setGlobal(id, mat, source);
    } else {
        setLocal(id, mat, source);
    }
}

cv::Mat CacheManager::get(const std::string& id) const {
    // Check local first
    cv::Mat localMat = getLocal(id);
    if (!localMat.empty()) {
        return localMat;
    }
    
    // Fall back to global
    return getGlobal(id);
}

bool CacheManager::has(const std::string& id) const {
    return hasLocal(id) || hasGlobal(id);
}

std::optional<CacheEntry> CacheManager::getEntry(const std::string& id) const {
    // Check local first (no locking — local scopes are per-thread)
    {
        for (auto it = _localScopes.rbegin(); it != _localScopes.rend(); ++it) {
            auto entryIt = it->find(id);
            if (entryIt != it->end()) {
                return entryIt->second;
            }
        }
    }
    
    // Check global
    {
        std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
        auto it = _sharedGlobal->entries.find(id);
        if (it != _sharedGlobal->entries.end()) {
            return it->second;
        }
    }
    
    return std::nullopt;
}

// ============================================================================
// Numeric key support
// ============================================================================

void CacheManager::set(uint16_t key, const cv::Mat& mat) {
    setLocal("__num_" + std::to_string(key), mat);
}

cv::Mat CacheManager::get(uint16_t key) const {
    return get("__num_" + std::to_string(key));
}

bool CacheManager::has(uint16_t key) const {
    return has("__num_" + std::to_string(key));
}

// ============================================================================
// Statistics and debugging
// ============================================================================

CacheManager::Stats CacheManager::getStats() const {
    Stats stats = {};
    
    {
        std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
        stats.globalEntryCount = _sharedGlobal->entries.size();
        for (const auto& [_, entry] : _sharedGlobal->entries) {
            stats.totalMemoryBytes += estimateMatMemory(entry.mat);
        }
    }
    
    {
        stats.scopeDepth = _localScopes.size();
        for (const auto& scope : _localScopes) {
            stats.localEntryCount += scope.size();
            for (const auto& [_, entry] : scope) {
                stats.totalMemoryBytes += estimateMatMemory(entry.mat);
            }
        }
    }
    
    return stats;
}

std::string CacheManager::debugString() const {
    std::ostringstream oss;
    
    auto stats = getStats();
    oss << "CacheManager {\n";
    oss << "  Global entries: " << stats.globalEntryCount << "\n";
    oss << "  Local entries: " << stats.localEntryCount << "\n";
    oss << "  Scope depth: " << stats.scopeDepth << "\n";
    oss << "  Total memory: " << (stats.totalMemoryBytes / 1024.0 / 1024.0) << " MB\n";
    
    oss << "  Global cache:\n";
    {
        std::shared_lock<std::shared_mutex> lock(_sharedGlobal->mutex);
        for (const auto& [id, entry] : _sharedGlobal->entries) {
            oss << "    [" << id << "] " 
                << entry.mat.cols << "x" << entry.mat.rows 
                << " (accessed " << entry.accessCount << " times)\n";
        }
    }
    
    oss << "  Local scopes:\n";
    {

        for (size_t i = 0; i < _localScopes.size(); ++i) {
            oss << "    Scope " << i << ":\n";
            for (const auto& [id, entry] : _localScopes[i]) {
                oss << "      [" << id << "] " 
                    << entry.mat.cols << "x" << entry.mat.rows << "\n";
            }
        }
    }
    
    oss << "}\n";
    return oss.str();
}

size_t CacheManager::estimateMatMemory(const cv::Mat& mat) {
    if (mat.empty()) return 0;
    return mat.total() * mat.elemSize();
}

uint64_t CacheManager::currentTimestamp() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace visionpipe

#ifndef VISIONPIPE_CACHE_MANAGER_H
#define VISIONPIPE_CACHE_MANAGER_H

#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <optional>
#include <atomic>
#include <opencv2/core/mat.hpp>

namespace visionpipe {

// Forward declaration — full definition in utils/shm_zero_copy.h
struct ShmArena;

/**
 * @brief Cache entry with metadata
 */
struct CacheEntry {
    cv::Mat mat;
    std::string source;         // Pipeline/function that created this entry
    uint64_t timestamp;         // Creation timestamp
    uint32_t accessCount;       // Number of times accessed
    bool isGlobal;              // Whether this is a global cache entry
    uint64_t shmSeq = 0;        // SHM writeSeq when this entry was last populated
                                // from the arena (0 = not from SHM / stale)
    CacheEntry() : timestamp(0), accessCount(0), isGlobal(false), shmSeq(0) {}
};

/**
 * @brief Shared storage for the global cache, so multiple CacheManager instances
 *        (e.g. per-thread child interpreters) can all read/write the same global data.
 */
struct GlobalCacheData {
    // shared_mutex allows concurrent reads (e.g. multiple exec_multi workers
    // all reading the same ccm_matrix) while serialising writes.
    mutable std::shared_mutex mutex;
    std::unordered_map<std::string, CacheEntry> entries;
};

/**
 * @brief Manages frame/Mat caching for the interpreter
 * 
 * Supports both local (pipeline-scoped) and global caches.
 * Thread-safe for concurrent access.
 * 
 * Multiple CacheManager instances can share the same GlobalCacheData
 * (e.g. for parallel pipeline execution via exec_multi).
 */
class CacheManager {
public:
    /// Creates a standalone CacheManager with its own private global cache.
    CacheManager();

    /// Creates a CacheManager whose global cache is shared with other managers.
    explicit CacheManager(std::shared_ptr<GlobalCacheData> sharedGlobal);

    ~CacheManager() = default;

    /// Returns the underlying shared global cache storage.
    std::shared_ptr<GlobalCacheData> getGlobalData() const { return _sharedGlobal; }

    /// Replaces the global cache backing store (safe to call before the manager
    /// is accessed by any thread).
    void replaceGlobalData(std::shared_ptr<GlobalCacheData> sharedGlobal) {
        _sharedGlobal = std::move(sharedGlobal);
    }

    /// Mark this CacheManager as running inside a fork() child process.
    /// When set, setGlobal() will also write frames to the SHM arena.
    void setForkChild(bool v) { _isForkChild = v; }

    /// Mark this CacheManager as having active fork children.
    /// When set, getGlobal() reads from the SHM arena (zero-copy).
    void setHasForkChildren(bool v) { _hasForkChildren = v; }

    /// Set the shared-memory arena (created before fork, inherited by children).
    void setShmArena(ShmArena* arena) { _shmArena = arena; }

    /// Configure the arena that will be created on the first exec_fork call.
    /// maxFrameBytes: maximum frame size in bytes (0 = use compile-time default).
    /// maxSlots:      maximum named channels   (0 = use compile-time default).
    /// Must be called BEFORE exec_fork (i.e. during pipeline setup).
    void setPendingShmConfig(size_t maxFrameBytes, int maxSlots = 0) {
        _pendingShmMaxFrameBytes = maxFrameBytes;
        _pendingShmMaxSlots      = maxSlots;
    }

    /// Returns the user-requested frame-byte limit (0 = use default).
    size_t pendingShmMaxFrameBytes() const { return _pendingShmMaxFrameBytes; }

    /// Returns the user-requested slot count (0 = use default).
    int pendingShmMaxSlots() const { return _pendingShmMaxSlots; }
    
    // =========================================================================
    // Global cache operations
    // =========================================================================
    
    /**
     * @brief Store a Mat in the global cache
     * @param id Cache identifier
     * @param mat Mat to cache
     * @param source Source identifier (pipeline name, etc.)
     */
    void setGlobal(const std::string& id, const cv::Mat& mat, const std::string& source = "");
    
    /**
     * @brief Get a Mat from the global cache
     * @param id Cache identifier
     * @return Mat if found, empty Mat otherwise
     */
    cv::Mat getGlobal(const std::string& id) const;
    
    /**
     * @brief Check if a global cache entry exists
     */
    bool hasGlobal(const std::string& id) const;
    
    /**
     * @brief Remove a global cache entry
     */
    void removeGlobal(const std::string& id);
    
    /**
     * @brief Clear all global cache entries
     */
    void clearGlobal();
    
    /**
     * @brief Get all global cache IDs
     */
    std::vector<std::string> getGlobalIds() const;
    
    // =========================================================================
    // Local cache operations (pipeline-scoped)
    // =========================================================================
    
    /**
     * @brief Push a new local scope
     * @return Scope ID
     */
    size_t pushScope();
    
    /**
     * @brief Pop the current local scope and clear its cache
     */
    void popScope();
    
    /**
     * @brief Store a Mat in the current local scope
     */
    void setLocal(const std::string& id, const cv::Mat& mat, const std::string& source = "");
    
    /**
     * @brief Get a Mat from local cache (searches from current scope upward)
     */
    cv::Mat getLocal(const std::string& id) const;
    
    /**
     * @brief Check if a local cache entry exists
     */
    bool hasLocal(const std::string& id) const;
    
    /**
     * @brief Clear the current local scope
     */
    void clearLocalScope();

    /**
     * @brief Reset local scope stack to a single empty scope.
     * Used when reusing a child interpreter across exec_multi iterations
     * to avoid growing stale scope data between frames.
     */
    void resetLocalScopes();
    
    // =========================================================================
    // Unified cache operations (searches local first, then global)
    // =========================================================================
    
    /**
     * @brief Store a Mat (local by default, global if specified)
     */
    void set(const std::string& id, const cv::Mat& mat, bool isGlobal = false, const std::string& source = "");
    
    /**
     * @brief Get a Mat (searches local first, then global)
     */
    cv::Mat get(const std::string& id) const;
    
    /**
     * @brief Check if a cache entry exists (local or global)
     */
    bool has(const std::string& id) const;
    
    /**
     * @brief Get cache entry with metadata
     */
    std::optional<CacheEntry> getEntry(const std::string& id) const;
    
    // =========================================================================
    // Numeric key support (for compatibility with existing Pipeline)
    // =========================================================================
    
    /**
     * @brief Store using numeric key (converts to string)
     */
    void set(uint16_t key, const cv::Mat& mat);
    
    /**
     * @brief Get using numeric key
     */
    cv::Mat get(uint16_t key) const;
    
    /**
     * @brief Check using numeric key
     */
    bool has(uint16_t key) const;
    
    // =========================================================================
    // Statistics and debugging
    // =========================================================================
    
    /**
     * @brief Get cache statistics
     */
    struct Stats {
        size_t globalEntryCount;
        size_t localEntryCount;
        size_t scopeDepth;
        size_t totalMemoryBytes;
    };
    
    Stats getStats() const;
    
    /**
     * @brief Print cache state for debugging
     */
    std::string debugString() const;
    
private:
    // Global cache (shared across all pipelines and, optionally, across threads)
    std::shared_ptr<GlobalCacheData> _sharedGlobal;

    // SHM bridge flags
    bool _isForkChild     = false;  ///< True in fork() child — setGlobal writes to arena
    bool _hasForkChildren = false;  ///< True in parent — getGlobal reads from arena
    ShmArena* _shmArena   = nullptr; ///< Anonymous mmap arena (set before fork)

    // Pending arena configuration (set by set_shm_size() before exec_fork)
    size_t _pendingShmMaxFrameBytes = 0;  ///< 0 = use compile-time default (32 MB)
    int    _pendingShmMaxSlots      = 0;  ///< 0 = use compile-time default (8)

    // Local cache stack — strictly owned by this CacheManager instance.
    // No mutex needed: local scopes are only accessed by the thread that owns
    // this CacheManager (each child interpreter has its own instance).
    std::vector<std::unordered_map<std::string, CacheEntry>> _localScopes;
    
    // Helper to estimate Mat memory
    static size_t estimateMatMemory(const cv::Mat& mat);
    
    // Get current timestamp
    static uint64_t currentTimestamp();
};

} // namespace visionpipe

#endif // VISIONPIPE_CACHE_MANAGER_H

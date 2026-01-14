#ifndef VISIONPIPE_CACHE_MANAGER_H
#define VISIONPIPE_CACHE_MANAGER_H

#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <optional>
#include <opencv2/core/mat.hpp>

namespace visionpipe {

/**
 * @brief Cache entry with metadata
 */
struct CacheEntry {
    cv::Mat mat;
    std::string source;         // Pipeline/function that created this entry
    uint64_t timestamp;         // Creation timestamp
    uint32_t accessCount;       // Number of times accessed
    bool isGlobal;              // Whether this is a global cache entry
    
    CacheEntry() : timestamp(0), accessCount(0), isGlobal(false) {}
};

/**
 * @brief Manages frame/Mat caching for the interpreter
 * 
 * Supports both local (pipeline-scoped) and global caches.
 * Thread-safe for concurrent access.
 */
class CacheManager {
public:
    CacheManager();
    ~CacheManager() = default;
    
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
    // Global cache (shared across all pipelines)
    mutable std::mutex _globalMutex;
    std::unordered_map<std::string, CacheEntry> _globalCache;
    
    // Local cache stack (one map per scope level)
    mutable std::mutex _localMutex;
    std::vector<std::unordered_map<std::string, CacheEntry>> _localScopes;
    
    // Helper to estimate Mat memory
    static size_t estimateMatMemory(const cv::Mat& mat);
    
    // Get current timestamp
    static uint64_t currentTimestamp();
};

} // namespace visionpipe

#endif // VISIONPIPE_CACHE_MANAGER_H

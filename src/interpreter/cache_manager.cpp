#include "interpreter/cache_manager.h"
#include <sstream>
#include <chrono>

namespace visionpipe {

CacheManager::CacheManager() {
    // Start with one local scope
    _localScopes.push_back({});
}

// ============================================================================
// Global cache operations
// ============================================================================

void CacheManager::setGlobal(const std::string& id, const cv::Mat& mat, const std::string& source) {
    std::lock_guard<std::mutex> lock(_globalMutex);
    
    CacheEntry entry;
    entry.mat = mat.clone();  // Always clone to ensure ownership
    entry.source = source;
    entry.timestamp = currentTimestamp();
    entry.accessCount = 0;
    entry.isGlobal = true;
    
    _globalCache[id] = std::move(entry);
}

cv::Mat CacheManager::getGlobal(const std::string& id) const {
    std::lock_guard<std::mutex> lock(_globalMutex);
    
    auto it = _globalCache.find(id);
    if (it != _globalCache.end()) {
        // Update access count (mutable in const context)
        const_cast<CacheEntry&>(it->second).accessCount++;
        return it->second.mat;
    }
    return cv::Mat();
}

bool CacheManager::hasGlobal(const std::string& id) const {
    std::lock_guard<std::mutex> lock(_globalMutex);
    return _globalCache.count(id) > 0;
}

void CacheManager::removeGlobal(const std::string& id) {
    std::lock_guard<std::mutex> lock(_globalMutex);
    _globalCache.erase(id);
}

void CacheManager::clearGlobal() {
    std::lock_guard<std::mutex> lock(_globalMutex);
    _globalCache.clear();
}

std::vector<std::string> CacheManager::getGlobalIds() const {
    std::lock_guard<std::mutex> lock(_globalMutex);
    
    std::vector<std::string> ids;
    ids.reserve(_globalCache.size());
    for (const auto& [id, _] : _globalCache) {
        ids.push_back(id);
    }
    return ids;
}

// ============================================================================
// Local cache operations
// ============================================================================

size_t CacheManager::pushScope() {
    std::lock_guard<std::mutex> lock(_localMutex);
    _localScopes.push_back({});
    return _localScopes.size() - 1;
}

void CacheManager::popScope() {
    std::lock_guard<std::mutex> lock(_localMutex);
    if (_localScopes.size() > 1) {
        _localScopes.pop_back();
    } else {
        // Clear the last scope but don't remove it
        _localScopes.back().clear();
    }
}

void CacheManager::setLocal(const std::string& id, const cv::Mat& mat, const std::string& source) {
    std::lock_guard<std::mutex> lock(_localMutex);
    
    if (_localScopes.empty()) {
        _localScopes.push_back({});
    }
    
    CacheEntry entry;
    entry.mat = mat.clone();
    entry.source = source;
    entry.timestamp = currentTimestamp();
    entry.accessCount = 0;
    entry.isGlobal = false;
    
    _localScopes.back()[id] = std::move(entry);
}

cv::Mat CacheManager::getLocal(const std::string& id) const {
    std::lock_guard<std::mutex> lock(_localMutex);
    
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
    std::lock_guard<std::mutex> lock(_localMutex);
    
    for (auto it = _localScopes.rbegin(); it != _localScopes.rend(); ++it) {
        if (it->count(id) > 0) {
            return true;
        }
    }
    return false;
}

void CacheManager::clearLocalScope() {
    std::lock_guard<std::mutex> lock(_localMutex);
    if (!_localScopes.empty()) {
        _localScopes.back().clear();
    }
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
    // Check local first
    {
        std::lock_guard<std::mutex> lock(_localMutex);
        for (auto it = _localScopes.rbegin(); it != _localScopes.rend(); ++it) {
            auto entryIt = it->find(id);
            if (entryIt != it->end()) {
                return entryIt->second;
            }
        }
    }
    
    // Check global
    {
        std::lock_guard<std::mutex> lock(_globalMutex);
        auto it = _globalCache.find(id);
        if (it != _globalCache.end()) {
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
        std::lock_guard<std::mutex> lock(_globalMutex);
        stats.globalEntryCount = _globalCache.size();
        for (const auto& [_, entry] : _globalCache) {
            stats.totalMemoryBytes += estimateMatMemory(entry.mat);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(_localMutex);
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
        std::lock_guard<std::mutex> lock(_globalMutex);
        for (const auto& [id, entry] : _globalCache) {
            oss << "    [" << id << "] " 
                << entry.mat.cols << "x" << entry.mat.rows 
                << " (accessed " << entry.accessCount << " times)\n";
        }
    }
    
    oss << "  Local scopes:\n";
    {
        std::lock_guard<std::mutex> lock(_localMutex);
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

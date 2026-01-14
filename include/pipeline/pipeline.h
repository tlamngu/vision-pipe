#ifndef PIPELINE_H
#define PIPELINE_H

#define VISION_SDK_PIPELINE_VERSION "1.0.0"

#include <vector>
#include <map>
#include <string>
#include <memory> // For std::shared_ptr
#include <opencv2/core/mat.hpp> // For cv::Mat

// Forward declaration
class PipelineItem; 

class Pipeline {
public:
    std::string _id; // Added: Identifier for the pipeline
    std::vector<std::shared_ptr<PipelineItem>> _items;
    std::map<uint16_t, cv::Mat> _matCache; // Mat cache for inter-item communication

    Pipeline(std::string id = ""); // Modified: Constructor to accept an ID
    ~Pipeline() = default;

    // Adds an item to the pipeline. Takes ownership if raw pointer is provided.
    void addItem(PipelineItem* item); 
    // Adds an already managed shared_ptr item to the pipeline.
    void addSharedItem(std::shared_ptr<PipelineItem> item);
    
    // Runs the pipeline with the given input Mat.
    // Output Mat is the result of the last item.
    // Cache is NOT automatically cleared by run() to support pre-population.
    cv::Mat run(cv::Mat input);

    // Clears all items from the pipeline.
    void clearItems();

    // Clears the internal Mat cache.
    void clearCache();
    bool getMatFromCache(uint16_t key, cv::Mat& mat) const { // const if it doesn't modify Pipeline state
        auto it = _matCache.find(key);
        if (it != _matCache.end()) {
            mat = it->second; // Or it->second.clone() if you want to return a copy
            return true;
        }
        return false;
    }
    // Added: Pre-populates the cache with a given Mat.
    void addMatToCache(uint16_t cacheKey, const cv::Mat& mat);
};

#endif // PIPELINE_H
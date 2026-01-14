#include "pipeline/pipeline.h"
#include "pipeline/pipeline_item.h" 
#include <iostream>     

// Modified constructor
Pipeline::Pipeline(std::string id) : _id(std::move(id)) {}

void Pipeline::addItem(PipelineItem* item) {
    if (item) {
        _items.push_back(std::shared_ptr<PipelineItem>(item));
    } else {
        std::cerr << "Pipeline [" << _id << "]: Attempted to add a null item (raw pointer)." << std::endl;
    }
}

void Pipeline::addSharedItem(std::shared_ptr<PipelineItem> item) {
    if (item) {
        _items.push_back(item);
    } else {
         std::cerr << "Pipeline [" << _id << "]: Attempted to add a null item (shared_ptr)." << std::endl;
    }
}

cv::Mat Pipeline::run(cv::Mat input) {
    // _matCache is NOT cleared here automatically to allow pre-population via addMatToCache.
    // User should call clearCache() explicitly if a fresh state is needed before a run.

    cv::Mat currentMat = input;
    for (size_t i = 0; i < _items.size(); ++i) {
        const auto& item = _items[i]; 
        if (item) {
#ifdef DEBUG_PIPELINE
            std::cout << "Pipeline [" << _id << "]: Executing item [" << item->_id << "] (" << i+1 << "/" << _items.size() << ")" << std::endl;
#endif
            currentMat = item->forward(currentMat, _matCache);
            if (currentMat.empty() && i < _items.size() -1) {
#ifdef DEBUG_PIPELINE
                std::cout << "Pipeline [" << _id << "]: Item [" << item->_id << "] returned an empty Mat. Subsequent items may be affected." << std::endl;
#endif
            }
        } else {
            std::cerr << "Pipeline [" << _id << "]: Encountered a null item at index " << i << ". Skipping." << std::endl;
        }
    }
    return currentMat;
}

void Pipeline::clearItems() {
    _items.clear();
}

void Pipeline::clearCache() {
    _matCache.clear();
#ifdef DEBUG_PIPELINE
    std::cout << "Pipeline [" << _id << "]: Mat cache cleared." << std::endl;
#endif
}

// Added method
void Pipeline::addMatToCache(uint16_t cacheKey, const cv::Mat& mat) {
    if (mat.empty()) {
#ifdef DEBUG_PIPELINE
        std::cout << "Pipeline [" << _id << "]: Attempted to add an empty Mat to cache for key " << cacheKey << ". Skipping." << std::endl;
#endif
        return;
    }
    // cv::Mat assignment is a shallow copy (data is shared, ref-counted)
    // If 'mat' is temporary, its data will be effectively moved or copied by OpenCV if needed.
    // For safety against 'mat' going out of scope if it's a temporary that doesn't own data,
    // one might consider mat.clone(), but typically direct assignment is fine and efficient.
    _matCache[cacheKey] = mat; 
#ifdef DEBUG_PIPELINE
    std::cout << "Pipeline [" << _id << "]: Added Mat to cache with key " << cacheKey << std::endl;
#endif
}


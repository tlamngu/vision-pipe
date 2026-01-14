#include "pipeline/pipeline_item.h"

PipelineItem::PipelineItem(uint16_t id) : _id(id) {}

cv::Mat PipelineItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat>& matCache) {
#ifdef DEBUG_PIPELINE
    std::cout << "PipelineItem [" << _id << "] (Base): Forwarding. Input empty: " << _input.empty() << std::endl;
#endif
    return _input;
}
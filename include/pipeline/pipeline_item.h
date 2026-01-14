#ifndef PIPELINE_ITEM_H
#define PIPELINE_ITEM_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core/ocl.hpp> // For cv::ocl::isEnable() and UMat
#include <map>
#include <string>
#include <iostream> // For std::cout, std::cerr
#include <cstdint>  // For uint16_t

// --- Debug Configuration ---
// To disable debug messages for the pipeline, either:
// 1. Define NDEBUG globally (e.g., compiler flag -DNDEBUG for release builds)
// 2. Uncomment the following line:
// #define NDEBUG

#ifndef NDEBUG // If NDEBUG is not defined (i.e., debug mode or NDEBUG not explicitly set)
// #define DEBUG_PIPELINE // Enable detailed pipeline item logging
#endif

// To force debug messages even if NDEBUG is defined (e.g., for specific testing):
// #define DEBUG_PIPELINE
// --- End Debug Configuration ---

// Forward declaration
struct CapReadItem; 

class PipelineItem {
public:
    uint16_t _id;
    PipelineItem(uint16_t id);
    virtual ~PipelineItem() = default;

    // Processes the input Mat and returns an output Mat.
    // Implementations may use OpenCL (UMat) internally if cv::ocl::haveOpenCL() && cv::ocl::useOpenCL().
    virtual cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat>& matCache);
};

#endif // PIPELINE_ITEM_H
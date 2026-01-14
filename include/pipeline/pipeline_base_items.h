#ifndef PIPELINE_BASE_ITEMS_H
#define PIPELINE_BASE_ITEMS_H

#include "pipeline_item.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/calib3d.hpp>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <chrono> // Required for timing

// --- CapReadItem ---
// Reads frames from a cv::VideoCapture source.
struct CapReadItem : PipelineItem
{
public:
    cv::VideoCapture *capSource;
    double captureWidth;
    double captureHeight;
    double FPS;
    char captureFormat[4];

    CapReadItem(uint16_t id, cv::VideoCapture *cap, double width, double height, double fps = 30.0, bool tryAutoModes = false);
    CapReadItem(uint16_t id, cv::VideoCapture *cap); // Simplified constructor
    ~CapReadItem() override = default;

    bool setCameraSetting(cv::VideoCaptureProperties prop, double value);
    double getCameraSetting(cv::VideoCaptureProperties prop);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- SaveImageItem ---
// Saves the input image to a file.
struct SaveImageItem : PipelineItem
{
public:
    std::string _filename;
    SaveImageItem(uint16_t id, std::string filename);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- ResizeItem ---
// Resizes the input image. Supports OpenCL.
struct ResizeItem : PipelineItem
{
public:
    int _width;
    int _height;
    ResizeItem(uint16_t id, int width, int height);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- GaussianBlurItem ---
// Applies Gaussian blur. Supports OpenCL.
struct GaussianBlurItem : PipelineItem
{
public:
    int _kernelSize;
    GaussianBlurItem(uint16_t id, int kernelSize);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- CannyEdgeItem ---
// Detects edges using Canny algorithm. Supports OpenCL for prerequisite GaussianBlur and Sobel.
struct CannyEdgeItem : PipelineItem
{
public:
    double _threshold1;
    double _threshold2;
    CannyEdgeItem(uint16_t id, double threshold1, double threshold2);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- ColorConvertItem ---
// Converts image color space. Supports OpenCL.
struct ColorConvertItem : PipelineItem
{
public:
    cv::ColorConversionCodes _colorCode;
    ColorConvertItem(uint16_t id, cv::ColorConversionCodes colorCode);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- CacheMatItem ---
// Caches the input Mat for later retrieval by LoadMatItem.
struct CacheMatItem : PipelineItem
{
public:
    uint16_t _cacheKey;
    CacheMatItem(uint16_t id, uint16_t cacheKey);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- LoadMatItem ---
// Loads a Mat from cache using a key.
struct LoadMatItem : PipelineItem
{
public:
    uint16_t _cacheKey;
    LoadMatItem(uint16_t id, uint16_t cacheKey);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- WriteVideoItem ---


// --- WriteVideoItem class definition (from user) ---

struct WriteVideoItem : PipelineItem
{
public:
    // --- Configuration (set at construction) ---
    std::string _filename;
    int _fourcc;
    cv::Size _initialFrameSizeHint;
    bool _initialIsColorHint;
    double _targetFpsActual;

    // --- Internal State ---
    cv::VideoWriter _writer;
    bool _firstFrameProcessed;
    bool _writerOpenedSuccessfully;
    long _frameCount;

    std::chrono::steady_clock::time_point _startTime;
    std::chrono::steady_clock::duration _frameDuration;
    std::chrono::steady_clock::time_point _nextFrameWriteTime;

    cv::Mat _lastValidFrameToRepeat;
    cv::Size _actualWriterFrameSize;
    bool _actualWriterIsColor;

    std::vector<cv::Mat> _frameBuffer; // Kept for compatibility, unused for buffering all frames

private:
    bool _isFinalized;

public:
    WriteVideoItem(uint16_t id, const std::string &filename, int fourcc, double targetFpsHint = 30.0,
                   cv::Size frameSizeHint = cv::Size(0, 0), bool isColorHint = true);
    ~WriteVideoItem() override;
    cv::Mat forward(cv::Mat _input, [[maybe_unused]] std::map<uint16_t, cv::Mat> &matCache) override;
    void finalize();
};

// --- ThresholdItem ---
// Applies fixed-level thresholding. Supports OpenCL.
struct ThresholdItem : PipelineItem
{
public:
    double _threshVal;
    double _maxVal;
    cv::ThresholdTypes _type;
    ThresholdItem(uint16_t id, double threshVal, double maxVal, cv::ThresholdTypes type);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- ErodeItem ---
// Erodes an image. Supports OpenCL.
struct ErodeItem : PipelineItem
{
public:
    cv::Mat _kernel;
    int _iterations;
    ErodeItem(uint16_t id, int kernelSize = 3, int iterations = 1);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- DilateItem ---
// Dilates an image. Supports OpenCL.
struct DilateItem : PipelineItem
{
public:
    cv::Mat _kernel;
    int _iterations;
    DilateItem(uint16_t id, int kernelSize = 3, int iterations = 1);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- StereoSBGMItem ---
// Computes stereo disparity using SGBM. CPU-based, but converts input/output for OCL pipeline.
struct StereoSBGMItem : PipelineItem
{
public:
    uint16_t _leftImageCacheKey;
    uint16_t _rightImageCacheKey;
    cv::Ptr<cv::StereoSGBM> _sgbm;

    StereoSBGMItem(uint16_t id,
                   uint16_t leftCacheKey,
                   uint16_t rightCacheKey,
                   int minDisparity = 0,
                   int numDisparities = 16 * 6,
                   int blockSize = 7,
                   int P1 = 8 * 3 * 7 * 7,
                   int P2 = 32 * 3 * 7 * 7,
                   int disp12MaxDiff = 1,
                   int preFilterCap = 63,
                   int uniquenessRatio = 10,
                   int speckleWindowSize = 100,
                   int speckleRange = 32,
                   int mode = cv::StereoSGBM::MODE_SGBM_3WAY);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- AutoTuneCameraItem ---
// Attempts to auto-tune a camera property of a CapReadItem.
struct AutoTuneCameraItem : PipelineItem
{
public:
    CapReadItem *_capSourceItem;
    bool _tuned;
    double _targetMetricValue;
    int _maxIterations;
    cv::VideoCaptureProperties _propToTune;
    bool _propIsLogScale;

    AutoTuneCameraItem(uint16_t id, CapReadItem *capItem,
                       cv::VideoCaptureProperties prop = cv::CAP_PROP_EXPOSURE,
                       double targetVal = 128.0, int maxIterations = 10, bool propIsLogScale = false);

    double evaluateBrightness(const cv::Mat &frame); // Can use OCL internally
    void tuneCamera();
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- ImageRectificationItem ---
// Rectifies an image using pre-computed calibration maps. Supports OpenCL for remap.
struct ImageRectificationItem : PipelineItem
{
public:
    cv::Mat _map1_cpu, _map2_cpu;  // CPU maps
    cv::UMat _map1_ocl, _map2_ocl; // OCL maps
    cv::Size _imageSize;
    bool _initialized;
    bool _oclInitialized;

    ImageRectificationItem(uint16_t id, const std::string &calibrationFilePath, bool isLeftCamera);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- NEW ITEMS ---

// --- AdaptiveThresholdItem ---
// Applies adaptive thresholding. Supports OpenCL.
struct AdaptiveThresholdItem : PipelineItem
{
public:
    double _maxValue;
    cv::AdaptiveThresholdTypes _adaptiveMethod;
    cv::ThresholdTypes _thresholdType;
    int _blockSize;
    double _C; // Constant subtracted from the mean or weighted mean

    AdaptiveThresholdItem(uint16_t id, double maxValue, cv::AdaptiveThresholdTypes adaptiveMethod,
                          cv::ThresholdTypes thresholdType, int blockSize, double C);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- MorphologyExItem ---
// Performs advanced morphological operations like opening, closing, gradient, etc. Supports OpenCL.
struct MorphologyExItem : PipelineItem
{
public:
    cv::MorphTypes _op;
    cv::Mat _kernel;
    cv::Point _anchor;
    int _iterations;
    int _borderType;
    cv::Scalar _borderValue;

    MorphologyExItem(uint16_t id, cv::MorphTypes op, int kernelSize = 3, int iterations = 1,
                     cv::Point anchor = cv::Point(-1, -1), int borderType = cv::BORDER_CONSTANT,
                     const cv::Scalar &borderValue = cv::morphologyDefaultBorderValue());
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- HistogramEqualizeItem ---
// Performs histogram equalization on grayscale images. Supports OpenCL.
// For color images, it converts to grayscale first.
struct HistogramEqualizeItem : PipelineItem
{
public:
    HistogramEqualizeItem(uint16_t id);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- StereoSADItem ---
// Computes stereo disparity using SAD (StereoBM). CPU-based.
struct StereoSADItem : PipelineItem
{
public:
    uint16_t _leftImageCacheKey;
    uint16_t _rightImageCacheKey;
    cv::Ptr<cv::StereoBM> _sbm;
    int _minDisparity;
    int _numDisparities;

    StereoSADItem(uint16_t id, uint16_t leftCacheKey, uint16_t rightCacheKey,
                  int minDisparity = 0, int numDisparities = 16 * 6, int blockSize = 15,
                  // Additional StereoBM parameters can be added here
                  int speckleWindowSize = 0, int speckleRange = 0, int disp12MaxDiff = -1);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- MedianFilterItem ---
// Applies a median blur filter. Supports OpenCL.
struct MedianFilterItem : PipelineItem
{
public:
    int _ksize; // Aperture linear size; it must be odd and greater than 1, for example: 3, 5, 7 ...

    MedianFilterItem(uint16_t id, int ksize);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- SpeckleFilterItem ---
// Filters speckles from a disparity map (typically CV_16S). CPU-based.
struct SpeckleFilterItem : PipelineItem
{
public:
    int _newVal;         // The disparity value used for rejected pixels.
    int _maxSpeckleSize; // The Emaximum speckle size to consider it a speckle.
    int _maxDiff;        // Maximum disparity variation allowed in a speckle.
    // This item typically operates on CV_16S disparity maps where disparities are scaled by 16.
    // _maxDiff should be provided as an unscaled disparity difference (e.g., 1, 2). It will be scaled internally.

    SpeckleFilterItem(uint16_t id, int newVal, int maxSpeckleSize, int maxDiff);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- HoleFillingItem ---
// Fills holes in a disparity map (typically CV_16S). CPU-based.


struct HoleFillingItem : PipelineItem
{
public:
    int _minDisparityValue;     // The minimum valid disparity value (unscaled, e.g., 0).
                                // Pixels with scaled_value < _minDisparityValue * 16 are considered holes.
    // 1D Propagation (Pass 1 & 2)
    bool _propagateLeftToRight; 
    bool _propagateRightToLeft; 

    // 2D Iterative Median Fill (Pass 3)
    bool _enable2DFill;             // Master switch for 2D neighborhood-based filling.
    int _2dNeighborhoodSize;        // Size of the square neighborhood for 2D fill (e.g., 5 for 5x5, must be odd & >=3).
    int _2dFillIterations;          // Number of iterations for the 2D median fill. More iterations allow better propagation.

    // Final Smoothing (Pass 4 - internal to this item)
    bool _applyMedianBlurPost;      // Apply a median blur after all filling stages.
    int _medianBlurKernelSize;      // Kernel size for the final median blur (e.g., 3 or 5, must be odd & >=3).


    HoleFillingItem(uint16_t id,
                    int minDisparityValue = 0,
                    bool propagateLtoR = true,
                    bool propagateRtoL = true,
                    bool enable2DFill = true,
                    int neighborhoodSize = 5,
                    int fillIterations = 3, // Default to a few iterations for better results
                    bool applyMedianBlur = true,
                    int medianBlurKernel = 3);

    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;

private:
    // Helper for 2D fill - remains the same
    short getMedianNeighborValue(const cv::Mat& map, int r, int c, int neighborhoodSize, short scaledMinValidDisparity);
};

// --- DisparityConfidenceItem ---
// Calculates a confidence map for a disparity map using SAD. CPU-based.
// Input disparity map is expected to be CV_16S. Left/Right images are CV_8UC1.
struct DisparityConfidenceItem : PipelineItem
{
public:
    uint16_t _leftImageCacheKey;
    uint16_t _rightImageCacheKey;
    int _windowSize;       // SAD window size, must be odd.
    float _disparityScale; // Scale factor for disparity values (e.g., 1.0/16.0 for CV_16S maps from OpenCV).

    DisparityConfidenceItem(uint16_t id, uint16_t leftCacheKey, uint16_t rightCacheKey, int windowSize = 7, float disparityScale = 1.0f / 16.0f);
    cv::Mat forward(cv::Mat _inputDisparityMap, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- LeftRightConsistencyCheckItem ---
// Performs a left-right consistency check to filter disparities. CPU-based.
// Expects input (left-to-right) disparity map and a cached right-to-left disparity map. Both typically CV_16S.
struct LeftRightConsistencyCheckItem : PipelineItem
{
public:
    uint16_t _rightToLeftDisparityCacheKey;
    float _threshold;               // Maximum allowed difference between D_LR and D_RL (unscaled).
    int _minDisparityValueForInput; // Min disparity of the input D_LR map (unscaled, e.g. 0)
                                    // Used to determine the fill value for invalidated pixels.
    float _disparityScale;          // Scale factor for disparity values (e.g., 1.0/16.0 for CV_16S maps).

    LeftRightConsistencyCheckItem(uint16_t id, uint16_t rightToLeftDispKey, float threshold = 1.0f, int minDispValForInput = 0, float disparityScale = 1.0f / 16.0f);
    cv::Mat forward(cv::Mat _inputDisparityMapLR, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- NormalizeDisparityMapItem ---
// Normalizes a disparity map (often CV_16S or CV_32F) to CV_8U for visualization.
// Supports OpenCL for convertTo.
struct NormalizeDisparityMapItem : PipelineItem
{
public:
    double _alpha;               // Value to scale to (typically 255)
    double _beta;                // Value to add after scaling (typically 0)
    int _targetType;             // Target type, usually CV_8U
    double _minDispOverride;     // Optional: if > 0, use this as min disparity for normalization. Otherwise, calculate from data.
    double _maxDispOverride;     // Optional: if > 0, use this as max disparity for normalization. Otherwise, calculate from data.
                                 // Note: For CV_16S maps, these overrides should be scaled values (e.g., min_disp_val * 16.0)
    bool _scaleInputDisparities; // If true, and input is CV_16S, divide by 16.0 before normalization.

    NormalizeDisparityMapItem(uint16_t id, double alpha = 255.0, double beta = 0.0, int targetType = CV_8U,
                              bool scaleCV16S = true, double minDispOverride = -1.0, double maxDispOverride = -1.0);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- ApplyColorMapItem ---
// Applies a pseudo-color map to a single-channel input image.
// Typically used for visualizing disparity maps or heatmaps. Supports OpenCL.
struct ApplyColorMapItem : PipelineItem
{
public:
    cv::ColormapTypes _colormap;

    ApplyColorMapItem(uint16_t id, cv::ColormapTypes colormap = cv::COLORMAP_JET);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

struct JoinFramesHorizontallyItem : PipelineItem
{
public:
    uint16_t _leftImageCacheKey;
    uint16_t _rightImageCacheKey;
    int _outputType; // Desired output type, e.g., CV_8UC3. If -1, tries to match input.

    JoinFramesHorizontallyItem(uint16_t id, uint16_t leftCacheKey, uint16_t rightCacheKey, int outputType = -1);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// ... (after ApplyColorMapItem or JoinFramesHorizontallyItem) ...

// --- DrawTextItem ---
// Draws text on the input image.
struct DrawTextItem : PipelineItem
{
public:
    std::string _text;
    cv::Point _org; // Bottom-left corner of the text string in the image.
    cv::HersheyFonts _fontFace;
    double _fontScale;
    cv::Scalar _color;
    int _thickness;
    int _lineType;
    bool _bottomLeftOrigin;

    DrawTextItem(uint16_t id, const std::string &text, cv::Point org,
                 cv::HersheyFonts fontFace = cv::FONT_HERSHEY_SIMPLEX, double fontScale = 1.0,
                 cv::Scalar color = cv::Scalar(255, 255, 255), int thickness = 1,
                 int lineType = cv::LINE_AA, bool bottomLeftOrigin = false);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- DrawRectangleItem ---
// Draws a rectangle on the input image.
struct DrawRectangleItem : PipelineItem
{
public:
    cv::Rect _rect; // Can also use two cv::Point for pt1 and pt2
    cv::Scalar _color;
    int _thickness;
    int _lineType;
    int _shift;

    DrawRectangleItem(uint16_t id, cv::Rect rect,
                      cv::Scalar color = cv::Scalar(255, 0, 0), int thickness = 1,
                      int lineType = cv::LINE_8, int shift = 0);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- DrawCircleItem ---
// Draws a circle on the input image.
struct DrawCircleItem : PipelineItem
{
public:
    cv::Point _center;
    int _radius;
    cv::Scalar _color;
    int _thickness;
    int _lineType;
    int _shift;

    DrawCircleItem(uint16_t id, cv::Point center, int radius,
                   cv::Scalar color = cv::Scalar(0, 0, 255), int thickness = 1,
                   int lineType = cv::LINE_8, int shift = 0);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

// --- DrawArrowedLineItem ---
// Draws an arrowed line on the input image.
struct DrawArrowedLineItem : PipelineItem
{
public:
    cv::Point _pt1;
    cv::Point _pt2;
    cv::Scalar _color;
    int _thickness;
    int _line_type;
    int _shift;
    double _tipLength;

    DrawArrowedLineItem(uint16_t id, cv::Point pt1, cv::Point pt2,
                        cv::Scalar color = cv::Scalar(0, 255, 0), int thickness = 1,
                        int line_type = cv::LINE_8, int shift = 0, double tipLength = 0.1);
    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};



struct PredictiveHoleFillingItem : PipelineItem
{
public:
    int _minDisparityValue;         // Unscaled minimum valid disparity.
    
    // Hole Region Parameters
    int _minHoleRegionSize;         // Minimum number of pixels for a hole to be processed by predictive fill. Smaller holes might use a simpler fill.
    int _maxHoleRegionSize;         // Maximum number of pixels for a hole. Very large holes might be skipped or use a simpler fill to save computation.

    // Boundary and Surface Fitting Parameters
    int _minBoundaryPixelsForFit;   // Minimum valid boundary pixels around a hole region required to attempt a surface fit.
    int _boundarySearchRadius;      // When filling a pixel within a hole, how far to look for boundary pixels to contribute to local prediction. (pixels)
                                    // If 0, uses all boundary pixels of the region for a global region fit.
                                    // If >0, performs a local fit/interpolation for each hole pixel using boundary pixels within this radius.

    // Filling Strategy
    // enum class FillMode { PLANE_FIT, WEIGHTED_BOUNDARY_AVG }; // Example, could be more
    // FillMode _fillMode; // For now, we'll focus on one primary strategy (e.g. local weighted boundary average or simplified plane)

    // Fallback and Post-processing
    bool _enableSimpleFillForSmallHoles; // Use a quick median/average for holes smaller than _minHoleRegionSize
    int _smallHoleFillNeighborhood;    // Neighborhood for simple fill of small holes.
    bool _applyMedianBlurPost;      
    int _medianBlurKernelSize;      

    PredictiveHoleFillingItem(uint16_t id,
                              int minDisparityValue = 0,
                              int minHoleRegionSize = 5,
                              int maxHoleRegionSize = 5000, // Avoid extremely large holes
                              int minBoundaryPixelsForFit = 10,
                              int boundarySearchRadius = 0, // Default to global region fit
                              bool enableSimpleFillForSmallHoles = true,
                              int smallHoleFillNeighborhood = 3,
                              bool applyMedianBlur = false, // Often, good predictive fill doesn't need aggressive blur
                              int medianBlurKernel = 3);

    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;

private:
    struct HoleRegionInfo {
        std::vector<cv::Point> pixels; // Pixels belonging to this hole
        std::vector<cv::Point> boundaryPixels; // Valid pixels adjacent to this hole
        cv::Rect boundingBox;
        int id;
    };

    // Helper to fill small holes quickly
    void simpleLocalFill(cv::Mat& map, const cv::Point& holePixel, int neighborhoodSize, short scaledMinValidDisparity);
    
    // Helper for plane fitting (simplified version focusing on weighted average for now for speed)
    // bool estimateLocalPlane(const std::vector<cv::Point>& points, const cv::Mat& disparityMap,
    //                         cv::Vec3f& planeCoefficients, short scaledMinValidDisparity);
};


struct DHoleFillingItem : PipelineItem // Diagonal Hole Filling Item
{
public:
    int _minDisparityValue;     // Unscaled minimum valid disparity.
    
    bool _propagateLtoR;        // Enable Left-to-Right horizontal pass
    bool _propagateRtoL;        // Enable Right-to-Left horizontal pass
    bool _propagateTLtoBR;      // Enable Top-Left to Bottom-Right diagonal pass
    bool _propagateBRtoTL;      // Enable Bottom-Right to Top-Left diagonal pass 
                                // (or TRtoBL, we need two opposing diagonal directions)

    // Optional final smoothing (can be useful after propagation)
    bool _applyMedianBlurPost;
    int _medianBlurKernelSize;

    DHoleFillingItem(uint16_t id,
                     int minDisparityValue = 0,
                     bool propagateLtoR = true,
                     bool propagateRtoL = true,
                     bool propagateTLtoBR = true,
                     bool propagateBRtoTL = true, // Changed from TRtoBL for one common implementation style
                     bool applyMedianBlur = false,
                     int medianBlurKernel = 3);

    cv::Mat forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache) override;
};

#endif // PIPELINE_BASE_ITEMS_H

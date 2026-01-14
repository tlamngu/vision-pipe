#include "pipeline/pipeline_base_items.h"
// #define LOG_PREFIX "PIPELINE_BASE_ITEM [" << _id << "] ('" << _filename << "'): "
// --- CapReadItem Implementation ---
CapReadItem::CapReadItem(uint16_t id, cv::VideoCapture *cap, double width, double height, double fps, bool tryAutoModes)
    : PipelineItem(id), capSource(cap), captureWidth(width), captureHeight(height), FPS(fps), captureFormat{'M', 'J', 'P', 'G'}
{
    if (!capSource || !capSource->isOpened())
    {
        std::cerr << "CapReadItem [" << _id << "]: Error - Video source not open on construction." << std::endl;
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "CapReadItem [" << _id << "]: Configuring camera: "
                  << width << "x" << height << " @ " << fps << "FPS, Format: "
                  << captureFormat[0] << captureFormat[1] << captureFormat[2] << captureFormat[3] << std::endl;
#endif
        setCameraSetting(cv::CAP_PROP_FRAME_WIDTH, captureWidth);
        setCameraSetting(cv::CAP_PROP_FRAME_HEIGHT, captureHeight);
        setCameraSetting(cv::CAP_PROP_FPS, FPS);
        setCameraSetting(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc(captureFormat[0], captureFormat[1], captureFormat[2], captureFormat[3]));

        if (tryAutoModes)
        {
#ifdef DEBUG_PIPELINE
            std::cout << "CapReadItem [" << _id << "]: Attempting to set auto modes." << std::endl;
#endif
            setCameraSetting(cv::CAP_PROP_AUTO_EXPOSURE, 0.75);
            setCameraSetting(cv::CAP_PROP_AUTO_WB, 1.0);
            setCameraSetting(cv::CAP_PROP_GAIN, 0);
        }
    }
}

CapReadItem::CapReadItem(uint16_t id, cv::VideoCapture *cap)
    : PipelineItem(id), capSource(cap), captureWidth(0), captureHeight(0), FPS(0), captureFormat{'M', 'J', 'P', 'G'}
{
    if (!capSource || !capSource->isOpened())
    {
        std::cerr << "CapReadItem [" << _id << "]: Error - Video source not open on construction." << std::endl;
    }
    // User is expected to configure if using this constructor, or rely on defaults.
}

bool CapReadItem::setCameraSetting(cv::VideoCaptureProperties prop, double value)
{
    if (capSource && capSource->isOpened())
    {
        bool success = capSource->set(prop, value);
#ifdef DEBUG_PIPELINE
        if (!success)
        {
            std::cout << "CapReadItem [" << _id << "]: Failed to set property " << static_cast<int>(prop) << " to " << value << std::endl;
        }
        else
        {
            std::cout << "CapReadItem [" << _id << "]: Successfully set property " << static_cast<int>(prop) << " to " << value << std::endl;
        }
#endif
        return success;
    }
    else
    {
        std::cerr << "CapReadItem [" << _id << "]: Error - Unable to set camera property, source not open." << std::endl;
        return false;
    }
}

double CapReadItem::getCameraSetting(cv::VideoCaptureProperties prop)
{
    if (capSource && capSource->isOpened())
    {
        return capSource->get(prop);
    }
    else
    {
        std::cerr << "CapReadItem [" << _id << "]: Error - Unable to get camera property, source not open." << std::endl;
        return -1;
    }
}

cv::Mat CapReadItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    cv::Mat frame;
    if (capSource && capSource->isOpened())
    {
        if (!capSource->read(frame))
        {
#ifdef DEBUG_PIPELINE
            // This can be noisy if it's just end of stream
            // std::cout << "CapReadItem [" << _id << "]: Failed to read frame or end of stream." << std::endl;
#endif
        }
        // Note: frame is cv::Mat. If OCL is used downstream, it will be converted.
        return frame;
    }
    else
    {
        std::cerr << "CapReadItem [" << _id << "]: Error - Video source not open. Returning empty Mat." << std::endl;
        return cv::Mat();
    }
}

// --- SaveImageItem Implementation ---
SaveImageItem::SaveImageItem(uint16_t id, std::string filename) : PipelineItem(id), _filename(filename) {}

cv::Mat SaveImageItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (!_input.empty())
    {
        // cv::imwrite can take UMat directly as InputArray
        bool success = cv::imwrite(_filename, _input);
#ifdef DEBUG_PIPELINE
        if (success)
        {
            std::cout << "SaveImageItem [" << _id << "]: Image saved to " << _filename << std::endl;
        }
        else
        {
            std::cerr << "SaveImageItem [" << _id << "]: Error - Failed to save image to " << _filename << std::endl;
        }
#else
        if (!success)
        {
            std::cerr << "SaveImageItem [" << _id << "]: Error - Failed to save image to " << _filename << std::endl;
        }
#endif
    }
    else
    {
        std::cerr << "SaveImageItem [" << _id << "]: Error - Input image is empty. Cannot save." << std::endl;
    }
    return _input; // Pass through
}

// --- ResizeItem Implementation ---
ResizeItem::ResizeItem(uint16_t id, int width, int height) : PipelineItem(id), _width(width), _height(height) {}

cv::Mat ResizeItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "ResizeItem [" << _id << "]: Warning - Input image is empty. Cannot resize." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ResizeItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_resizedImage;
        cv::resize(u_input, u_resizedImage, cv::Size(_width, _height));
        return u_resizedImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ResizeItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat resizedImage;
        cv::resize(_input, resizedImage, cv::Size(_width, _height));
        return resizedImage;
    }
}

// --- GaussianBlurItem Implementation ---
GaussianBlurItem::GaussianBlurItem(uint16_t id, int kernelSize) : PipelineItem(id), _kernelSize(kernelSize)
{
    if (_kernelSize <= 0)
    {
        std::cerr << "GaussianBlurItem [" << _id << "]: Error - Kernel size must be positive. Using 3." << std::endl;
        _kernelSize = 3;
    }
    else if (_kernelSize % 2 == 0)
    {
        _kernelSize++; // Ensure kernel size is odd
#ifdef DEBUG_PIPELINE
        std::cout << "GaussianBlurItem [" << _id << "]: Kernel size " << _kernelSize - 1 << " is even. Adjusted to " << _kernelSize << "." << std::endl;
#endif
    }
}

cv::Mat GaussianBlurItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "GaussianBlurItem [" << _id << "]: Warning - Input image is empty. Cannot blur." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "GaussianBlurItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_blurredImage;
        cv::GaussianBlur(u_input, u_blurredImage, cv::Size(_kernelSize, _kernelSize), 0);
        return u_blurredImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "GaussianBlurItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat blurredImage;
        cv::GaussianBlur(_input, blurredImage, cv::Size(_kernelSize, _kernelSize), 0);
        return blurredImage;
    }
}

// --- CannyEdgeItem Implementation ---
CannyEdgeItem::CannyEdgeItem(uint16_t id, double threshold1, double threshold2) : PipelineItem(id), _threshold1(threshold1), _threshold2(threshold2) {}

cv::Mat CannyEdgeItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "CannyEdgeItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    // Canny internally uses Sobel and non-maximum suppression.
    // Color conversion, if needed, can also use OCL.
    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "CannyEdgeItem [" << _id << "]: Using OpenCL path (internally for components)." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_grayInput;
        if (u_input.channels() == 3)
        {
            cv::cvtColor(u_input, u_grayInput, cv::COLOR_BGR2GRAY);
        }
        else
        {
            u_grayInput = u_input;
        }
        cv::UMat u_edges;
        // Note: OpenCV's Canny doesn't have a direct UMat override for the full Canny,
        // but its internal components (like Sobel, GaussianBlur if not applied before) might use OCL.
        // For full UMat Canny, one might need to implement it using UMat versions of Sobel, magnitude, NMS.
        // However, passing UMat to cv::Canny usually works as it takes InputArray.
        cv::Canny(u_grayInput, u_edges, _threshold1, _threshold2);
        return u_edges.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "CannyEdgeItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat grayInput = _input;
        if (_input.channels() == 3)
        {
            cv::cvtColor(_input, grayInput, cv::COLOR_BGR2GRAY);
        }
        cv::Mat edges;
        cv::Canny(grayInput, edges, _threshold1, _threshold2);
        return edges;
    }
}

// --- ColorConvertItem Implementation ---
ColorConvertItem::ColorConvertItem(uint16_t id, cv::ColorConversionCodes colorCode) : PipelineItem(id), _colorCode(colorCode) {}

cv::Mat ColorConvertItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "ColorConvertItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ColorConvertItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_convertedImage;
        cv::cvtColor(u_input, u_convertedImage, _colorCode);
        return u_convertedImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ColorConvertItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat convertedImage;
        cv::cvtColor(_input, convertedImage, _colorCode);
        return convertedImage;
    }
}

// --- CacheMatItem Implementation ---
CacheMatItem::CacheMatItem(uint16_t id, uint16_t cacheKey) : PipelineItem(id), _cacheKey(cacheKey) {}

cv::Mat CacheMatItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (!_input.empty())
    {
        // Mat cache stores cv::Mat. If _input is from an OCL item, it's already Mat.
        matCache[_cacheKey] = _input.clone();
#ifdef DEBUG_PIPELINE
        std::cout << "CacheMatItem [" << _id << "]: Cached Mat with key " << _cacheKey << ". Size: " << _input.cols << "x" << _input.rows << std::endl;
#endif
    }
    else
    {
        std::cerr << "CacheMatItem [" << _id << "]: Warning - Attempting to cache an empty Mat for key " << _cacheKey << std::endl;
    }
    return _input; // Pass through
}

// --- LoadMatItem Implementation ---
LoadMatItem::LoadMatItem(uint16_t id, uint16_t cacheKey) : PipelineItem(id), _cacheKey(cacheKey) {}

cv::Mat LoadMatItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    auto it = matCache.find(_cacheKey);
    if (it != matCache.end() && !it->second.empty())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "LoadMatItem [" << _id << "]: Loaded Mat with key " << _cacheKey << ". Size: " << it->second.cols << "x" << it->second.rows << std::endl;
#endif
        return it->second.clone(); // Return a clone to prevent unintended modification of cache
    }
    else
    {
        std::cerr << "LoadMatItem [" << _id << "]: Error - Mat with key " << _cacheKey << " not found in cache or is empty. Returning original input." << std::endl;
        return _input;
    }
}

// --- WriteVideoItem Implementation ---

WriteVideoItem::WriteVideoItem(uint16_t id, const std::string &filename, int fourcc, double targetFpsHint,
                               cv::Size frameSize, bool isColor)
    : PipelineItem(id),
      _writer(),
      _filename(filename),
      _fourcc(fourcc),
      _initialFrameSizeHint(frameSize),
      _initialIsColorHint(isColor),
      _firstFrameProcessed(false),
      _frameCount(0),
      _targetFpsActual(targetFpsHint), // Will be validated
      _frameDuration(),
      _nextFrameWriteTime(),
      _startTime(),
      _lastValidFrameToRepeat(),
      _frameBuffer(),
      _isFinalized(false),
      _writerOpenedSuccessfully(false),
      _actualWriterFrameSize(),
      _actualWriterIsColor()
{
    double fpsToUse = targetFpsHint;

    if (fpsToUse <= 0.0)
    {
        std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Warning - Target FPS hint " << targetFpsHint
                  << " is invalid. Defaulting to 30.0 FPS." << std::endl;
        fpsToUse = 30.0;
    }

    std::chrono::duration<double, std::ratio<1>> seconds_per_frame(1.0 / fpsToUse);
    _frameDuration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(seconds_per_frame);

    if (_frameDuration.count() <= 0)
    {
        std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Warning - FPS " << fpsToUse
                  << " (from hint " << targetFpsHint << ") results in a zero or negative frame duration. Defaulting to 30.0 FPS and recalculating duration." << std::endl;
        fpsToUse = 30.0;
        seconds_per_frame = std::chrono::duration<double, std::ratio<1>>(1.0 / fpsToUse);
        _frameDuration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(seconds_per_frame);

        if (_frameDuration.count() <= 0)
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Critical - Frame duration is still zero or negative even with 30.0 FPS. "
                      << "Video writing will likely be problematic or fail. Check system clock capabilities." << std::endl;
        }
    }
    _targetFpsActual = fpsToUse;

#ifdef DEBUG_PIPELINE
    std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Constructed. Target FPS: " << std::fixed << std::setprecision(2) << _targetFpsActual
              << ", Initial Hint Size: " << _initialFrameSizeHint.width << "x" << _initialFrameSizeHint.height
              << ", Initial Hint IsColor: " << (_initialIsColorHint ? "true" : "false")
              << ", Frame Duration (ns): " << std::chrono::duration_cast<std::chrono::nanoseconds>(_frameDuration).count()
              << std::endl;
#endif
}

WriteVideoItem::~WriteVideoItem()
{
    finalize();
}

cv::Mat WriteVideoItem::forward(cv::Mat _input, [[maybe_unused]] std::map<uint16_t, cv::Mat> &matCache)
{
    if (_isFinalized)
    {
        return _input;
    }

    cv::Mat currentFrameForProcessing;

    if (!_input.empty())
    {
        currentFrameForProcessing = _input.clone();
    }
    else if (_writerOpenedSuccessfully && !_lastValidFrameToRepeat.empty())
    {
        currentFrameForProcessing = _lastValidFrameToRepeat.clone();
    }
    else
    {
        if (!_firstFrameProcessed && (_initialFrameSizeHint.width == 0 || _initialFrameSizeHint.height == 0)) {
            // Not initialized, input is empty, and no valid initial size hint to create a dummy frame for opening.
            // Cannot initialize writer yet.
#ifdef DEBUG_PIPELINE
            // std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Input empty, not initialized, and no frame size hint. Waiting for a frame." << std::endl;
#endif
            return _input;
        }
        // If writer is open, currentFrameForProcessing will be empty, and we'll just advance time for writes.
        // If writer is not open, but we have size hints, we might try to open with a dummy frame.
        // However, current logic relies on a non-empty currentFrameForProcessing for first frame logic.
    }

    if (!_firstFrameProcessed) // Attempt to initialize and open the writer
    {
        // If currentFrameForProcessing is empty at this point, it means _input was empty,
        // and we didn't have _lastValidFrameToRepeat. We cannot initialize without a frame
        // unless we were to construct a dummy black frame based on hints (not current logic).
        if (currentFrameForProcessing.empty()) {
             // This case implies _input was empty and we couldn't use _lastValidFrameToRepeat.
             // If _initialFrameSizeHint is valid, one *could* create a dummy frame here to open the writer.
             // For now, we require a real frame to pass through.
#ifdef DEBUG_PIPELINE
            // std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "First frame processing: input frame is empty, cannot initialize writer." << std::endl;
#endif
            return _input; // Cannot initialize yet.
        }

        _actualWriterFrameSize = _initialFrameSizeHint;
        if (_actualWriterFrameSize.width == 0 || _actualWriterFrameSize.height == 0)
        {
            _actualWriterFrameSize = currentFrameForProcessing.size();
        }
        _actualWriterIsColor = _initialIsColorHint; // Using hint primarily.

        if (_actualWriterFrameSize.width <= 0 || _actualWriterFrameSize.height <= 0) // Check for valid positive size
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Error - Invalid frame size (" << _actualWriterFrameSize.width << "x" << _actualWriterFrameSize.height
                      << ") determined for writer. Cannot open writer." << std::endl;
            finalize(); // Finalize to prevent further attempts.
            return _input;
        }

        try
        {
            cv::Mat firstFrameConformed = currentFrameForProcessing.clone();
            if (firstFrameConformed.size() != _actualWriterFrameSize)
            {
                cv::resize(firstFrameConformed, firstFrameConformed, _actualWriterFrameSize);
            }
            if (_actualWriterIsColor && firstFrameConformed.channels() == 1)
            {
                cv::cvtColor(firstFrameConformed, firstFrameConformed, cv::COLOR_GRAY2BGR);
            }
            else if (!_actualWriterIsColor && firstFrameConformed.channels() >= 3) // Handles 3 and 4 channel
            {
                cv::cvtColor(firstFrameConformed, firstFrameConformed, cv::COLOR_BGR2GRAY);
            }
             // Ensure the conformed frame isn't empty (e.g. resize to 0x0 if something went very wrong)
            if(firstFrameConformed.empty()){
                std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Error - First frame became empty after conformance. Cannot open writer." << std::endl;
                finalize();
                return _input;
            }
            _lastValidFrameToRepeat = firstFrameConformed;
        }
        catch (const cv::Exception &e)
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "OpenCV exception during first frame conformance: " << e.what() << std::endl;
            finalize();
            return _input;
        }

#ifdef DEBUG_PIPELINE
        std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Attempting to open writer. Params - Size: " << _actualWriterFrameSize
                  << ", IsColor: " << _actualWriterIsColor << ", TargetFPS: " << _targetFpsActual << std::endl;
#endif

        if (_frameDuration.count() <= 0) {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): Critical - Frame duration is zero or negative before opening writer. Finalizing." << std::endl;
            finalize();
            return _input;
        }
        
        _firstFrameProcessed = true; // Mark that we've processed parameters and are attempting to open.
                                     // Statistics in finalize() can use this.
        if (_writer.open(_filename, _fourcc, _targetFpsActual, _actualWriterFrameSize, _actualWriterIsColor))
        {
            _writerOpenedSuccessfully = true;
            _startTime = std::chrono::steady_clock::now();
            _nextFrameWriteTime = _startTime;
#ifdef DEBUG_PIPELINE
            std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Video writer opened successfully." << std::endl;
#endif
        }
        else
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Error - Could not open video writer. Check codec, permissions, path, FPS, and frame size/color compatibility." << std::endl;
            finalize(); // Finalize as writer could not be opened.
            return _input;
        }
    }

    // Conform subsequent frames or a new _input frame if writer is open
    if (_writerOpenedSuccessfully && !currentFrameForProcessing.empty())
    {
        try
        {
            if (currentFrameForProcessing.size() != _actualWriterFrameSize)
            {
                cv::resize(currentFrameForProcessing, currentFrameForProcessing, _actualWriterFrameSize, 0, 0, cv::INTER_LINEAR);
            }
            if (_actualWriterIsColor && currentFrameForProcessing.channels() == 1)
            {
                cv::cvtColor(currentFrameForProcessing, currentFrameForProcessing, cv::COLOR_GRAY2BGR);
            }
            else if (!_actualWriterIsColor && currentFrameForProcessing.channels() >= 3)
            {
                cv::cvtColor(currentFrameForProcessing, currentFrameForProcessing, cv::COLOR_BGR2GRAY);
            }
            if(currentFrameForProcessing.empty()){
                std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Warning - Frame became empty after conformance. Repeating last valid frame." << std::endl;
                // Do not update _lastValidFrameToRepeat if current frame is bad
            } else {
                _lastValidFrameToRepeat = currentFrameForProcessing;
            }
        }
        catch (const cv::Exception &e)
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "OpenCV exception during frame conformance: " << e.what() << ". Repeating last valid frame." << std::endl;
        }
    }

    if (_writerOpenedSuccessfully)
    {
        if (_frameDuration.count() <= 0)
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Critical - _frameDuration is zero during processing. Finalizing." << std::endl;
            finalize();
            return _input;
        }

        auto currentTime = std::chrono::steady_clock::now();

        if (_lastValidFrameToRepeat.empty())
        {
#ifdef DEBUG_PIPELINE
            // std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Writer is open, but no valid frame available to write. Advancing schedule." << std::endl;
#endif
            // Advance schedule even if no frame to write, to keep time.
            // This loop ensures _nextFrameWriteTime is always >= currentTime or slightly past it.
            while (_nextFrameWriteTime <= currentTime)
            {
                _nextFrameWriteTime += _frameDuration;
            }
        }
        else
        {
            while (_nextFrameWriteTime <= currentTime)
            {
                try
                {
                    if (!_writer.isOpened()) { // Defensive check
                        std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Error - Writer is not open before write attempt in forward(). Frame: " << _frameCount +1 << ". Finalizing." << std::endl;
                        finalize(); 
                        return _input; 
                    }
                    _writer.write(_lastValidFrameToRepeat);
                    _frameCount++;
                }
                catch (const cv::Exception &e)
                {
                    std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "OpenCV exception during write frame " << (_frameCount + 1) << ": " << e.what() << ". Skipping write, advancing schedule." << std::endl;
                    // Consider if this should be more fatal, e.g., finalize after N errors. For now, just log and continue.
                }
                _nextFrameWriteTime += _frameDuration;
            }
        }
    }
    return _input;
}

void WriteVideoItem::finalize()
{
    if (_isFinalized)
    {
        return;
    }
    _isFinalized = true;

#ifdef DEBUG_PIPELINE
    std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Finalizing video..." << std::endl;
#endif

    auto finalizationTime = std::chrono::steady_clock::now();

    if (_writerOpenedSuccessfully)
    {
        if (_frameDuration.count() <= 0) {
             std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Warning - _frameDuration is zero or negative during finalize. Skipping pending writes." << std::endl;
        } else if (!_lastValidFrameToRepeat.empty())
        {
            long framesWrittenInFinalize = 0;
            // Limit frames in finalize: max 5s of video or 1000 frames, whichever is smaller. Min 1 frame if FPS is very low.
            const long maxFramesInFinalizeLoop = (_targetFpsActual > 0.01) ? std::min(static_cast<long>(_targetFpsActual * 5.0), 1000L) : ( _targetFpsActual > 0 ? 1L : 150L );


#ifdef DEBUG_PIPELINE
            if (_frameDuration.count() > 0) {
                 std::chrono::steady_clock::duration pending_duration = finalizationTime - _nextFrameWriteTime;
                 if (pending_duration.count() > 0) { // Check if there's actually pending time
                    long num_pending_frames = static_cast<long>(pending_duration.count() / _frameDuration.count()); // Approximate
                    std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): "
                              << "Finalize: Calculated ~" << num_pending_frames << " pending frames to write. Will limit to " << maxFramesInFinalizeLoop << "." << std::endl;
                 } else {
                    std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): "
                              << "Finalize: No pending time for frames. _nextFrameWriteTime is ahead or at finalizationTime." << std::endl;
                 }
            }
#endif
            while (_nextFrameWriteTime <= finalizationTime)
            {
                if (framesWrittenInFinalize >= maxFramesInFinalizeLoop) {
                    std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): "
                              << "Warning - Reached max frame write limit (" << maxFramesInFinalizeLoop
                              << ") during finalize. Some trailing frames may be omitted." << std::endl;
                    break;
                }
                try
                {
                    if (!_writer.isOpened()) { 
                        std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Error - Writer is not open before finalize write attempt (frame " << _frameCount +1 << "). Aborting finalize writes." << std::endl;
                        break; 
                    }
                    _writer.write(_lastValidFrameToRepeat);
                    _frameCount++;
                    framesWrittenInFinalize++;
                }
                catch (const cv::Exception &e)
                {
                    std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "OpenCV exception during finalize (writing pending frame " << (_frameCount + 1) << "): " << e.what() << ". Aborting finalize writes." << std::endl;
                    break; // Stop trying to write if one fails in finalize
                }
                _nextFrameWriteTime += _frameDuration;
            }
        }
        else if (_frameCount > 0) // Writer was open, frames written, but now no valid frame.
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Warning - Finalizing, but _lastValidFrameToRepeat is empty. Some trailing frames might be missing." << std::endl;
        }
    }

    // Print statistics
    if (_firstFrameProcessed) 
    {
        // Calculate elapsed_seconds only if writer was opened and startTime initialized meaningfully
        std::chrono::duration<double> elapsed_seconds = (_writerOpenedSuccessfully && _startTime.time_since_epoch().count() != 0) 
                                                        ? (finalizationTime - _startTime) 
                                                        : std::chrono::duration<double>(0);
        double actual_avg_written_fps = 0.0;
        if (elapsed_seconds.count() > std::numeric_limits<double>::epsilon() && _frameCount > 0)
        {
            actual_avg_written_fps = static_cast<double>(_frameCount) / elapsed_seconds.count();
        }

        std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Finalized." << std::endl;
        std::cout << "  Total frames written: " << _frameCount << std::endl;
        std::cout << "  Target FPS for writer: " << std::fixed << std::setprecision(2) << _targetFpsActual << std::endl;
        std::cout << "  Actual Writer Params (attempted/used) - Size: " << _actualWriterFrameSize.width << "x" << _actualWriterFrameSize.height
                  << ", IsColor: " << (_actualWriterIsColor ? "true" : "false") << std::endl;
        if (_writerOpenedSuccessfully) { // Only show duration/FPS stats if writer was successfully used
             std::cout << "  Total active writing duration: " << std::fixed << std::setprecision(3) << elapsed_seconds.count() << "s" << std::endl;
            if (actual_avg_written_fps > 0.0) {
                std::cout << "  Approx. Avg actual written FPS: " << std::fixed << std::setprecision(2) << actual_avg_written_fps << std::endl;
            }
        } else {
            std::cout << "  Writer was not opened successfully or no frames written." << std::endl;
        }
    }
    else
    {
        std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Finalized. No frames were processed (writer not initialized)." << std::endl;
    }

    if (_writer.isOpened())
    {
        try
        {
#ifdef DEBUG_PIPELINE
            std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Attempting to release video writer..." << std::endl;
#endif
            _writer.release();
#ifdef DEBUG_PIPELINE
            std::cout << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Video writer released." << std::endl;
#endif
        }
        catch (const cv::Exception &e)
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "OpenCV exception during writer.release(): " << e.what() << std::endl;
        }
        catch (const std::exception &e) 
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Standard exception during writer.release(): " << e.what() << std::endl;
        }
        catch (...) 
        {
            std::cerr << "WriteVideoItem [" << _id << "] ('" << _filename << "'): " << "Unknown exception during writer.release()." << std::endl;
        }
    }
    _writerOpenedSuccessfully = false; 

    _frameBuffer.clear(); 
}

// --- ErodeItem Implementation ---
ErodeItem::ErodeItem(uint16_t id, int kernelSize, int iterations)
    : PipelineItem(id), _iterations(iterations)
{
    _kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
}

cv::Mat ErodeItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "ErodeItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ErodeItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_erodedImage;
        // Kernel is Mat, but erode can take it as InputArray
        cv::erode(u_input, u_erodedImage, _kernel, cv::Point(-1, -1), _iterations);
        return u_erodedImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ErodeItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat erodedImage;
        cv::erode(_input, erodedImage, _kernel, cv::Point(-1, -1), _iterations);
        return erodedImage;
    }
}

// --- DilateItem Implementation ---
DilateItem::DilateItem(uint16_t id, int kernelSize, int iterations)
    : PipelineItem(id), _iterations(iterations)
{
    _kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
}

cv::Mat DilateItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "DilateItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }
    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "DilateItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_dilatedImage;
        cv::dilate(u_input, u_dilatedImage, _kernel, cv::Point(-1, -1), _iterations);
        return u_dilatedImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "DilateItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat dilatedImage;
        cv::dilate(_input, dilatedImage, _kernel, cv::Point(-1, -1), _iterations);
        return dilatedImage;
    }
}

// --- StereoSBGMItem Implementation ---
StereoSBGMItem::StereoSBGMItem(uint16_t id, uint16_t leftCacheKey, uint16_t rightCacheKey,
                               int minDisparity, int numDisparities, int blockSize,
                               int P1, int P2, int disp12MaxDiff,
                               int preFilterCap, int uniquenessRatio,
                               int speckleWindowSize, int speckleRange,
                               int mode)
    : PipelineItem(id), _leftImageCacheKey(leftCacheKey), _rightImageCacheKey(rightCacheKey)
{
    _sgbm = cv::StereoSGBM::create(minDisparity, numDisparities, blockSize,
                                   P1, P2, disp12MaxDiff, preFilterCap, uniquenessRatio,
                                   speckleWindowSize, speckleRange, mode);
    if (!_sgbm)
    {
        std::cerr << "StereoSBGMItem [" << _id << "]: Error - Failed to create StereoSGBM instance." << std::endl;
    }
#ifdef DEBUG_PIPELINE
    else {
        std::cout << "StereoSBGMItem [" << _id << "]: StereoSGBM instance created. "
                  << "minDisparity=" << minDisparity << ", numDisparities=" << numDisparities
                  << ", blockSize=" << blockSize << ", P1=" << P1 << ", P2=" << P2
                  << ", disp12MaxDiff=" << disp12MaxDiff << ", preFilterCap=" << preFilterCap
                  << ", uniquenessRatio=" << uniquenessRatio << ", speckleWindowSize=" << speckleWindowSize
                  << ", speckleRange=" << speckleRange << ", mode=" << mode << std::endl;
    }
#endif
}

cv::Mat StereoSBGMItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    auto itLeft = matCache.find(_leftImageCacheKey);
    auto itRight = matCache.find(_rightImageCacheKey);

    if (itLeft == matCache.end() || itRight == matCache.end() || itLeft->second.empty() || itRight->second.empty())
    {
        std::cerr << "StereoSBGMItem [" << _id << "]: Error - Left (" << _leftImageCacheKey
                  << ") or Right (" << _rightImageCacheKey << ") image not found/empty in cache." << std::endl;
        return cv::Mat(); // Return empty mat, previous _input is irrelevant here
    }

    // StereoSGBM is primarily CPU. Inputs are Mats.
    cv::Mat leftImage_cpu = itLeft->second;
    cv::Mat rightImage_cpu = itRight->second;
    cv::Mat leftGray_cpu, rightGray_cpu;

    // Ensure inputs are grayscale (CV_8UC1) as SGBM expects this
    bool oclActive = cv::ocl::useOpenCL(); // For potential color conversion

    if (leftImage_cpu.channels() != 1 || leftImage_cpu.depth() != CV_8U)
    {
#ifdef DEBUG_PIPELINE
        std::cout << "StereoSBGMItem [" << _id << "]: Converting left input image (type: " << leftImage_cpu.type() << ") to CV_8UC1." << std::endl;
#endif
        if (leftImage_cpu.channels() >= 3) { // BGR, BGRA, etc.
            if (oclActive)
                cv::cvtColor(leftImage_cpu.getUMat(cv::ACCESS_READ), leftGray_cpu, cv::COLOR_BGR2GRAY);
            else
                cv::cvtColor(leftImage_cpu, leftGray_cpu, cv::COLOR_BGR2GRAY);
        } else { // Some other non-CV_8UC1 format
            leftImage_cpu.convertTo(leftGray_cpu, CV_8U); // Convert depth, assuming single channel
            if (leftGray_cpu.channels() >=3) { // If convertTo somehow made it multi-channel (unlikely for depth conversion)
                 cv::cvtColor(leftGray_cpu, leftGray_cpu, cv::COLOR_BGR2GRAY);
            }
        }
    }
    else
    {
        leftGray_cpu = leftImage_cpu;
    }

    if (rightImage_cpu.channels() != 1 || rightImage_cpu.depth() != CV_8U)
    {
#ifdef DEBUG_PIPELINE
        std::cout << "StereoSBGMItem [" << _id << "]: Converting right input image (type: " << rightImage_cpu.type() << ") to CV_8UC1." << std::endl;
#endif
        if (rightImage_cpu.channels() >= 3) {
            if (oclActive)
                cv::cvtColor(rightImage_cpu.getUMat(cv::ACCESS_READ), rightGray_cpu, cv::COLOR_BGR2GRAY);
            else
                cv::cvtColor(rightImage_cpu, rightGray_cpu, cv::COLOR_BGR2GRAY);
        } else {
            rightImage_cpu.convertTo(rightGray_cpu, CV_8U);
             if (rightGray_cpu.channels() >=3) {
                 cv::cvtColor(rightGray_cpu, rightGray_cpu, cv::COLOR_BGR2GRAY);
            }
        }
    }
    else
    {
        rightGray_cpu = rightImage_cpu;
    }

    if (leftGray_cpu.empty() || rightGray_cpu.empty() || leftGray_cpu.size() != rightGray_cpu.size() || leftGray_cpu.type() != CV_8UC1 || rightGray_cpu.type() != CV_8UC1 )
    {
        std::cerr << "StereoSBGMItem [" << _id << "]: Error - Left/Right images mismatch size/type or are empty after conversion. Both must be CV_8UC1." << std::endl;
        std::cerr << "  Left: " << leftGray_cpu.size() << " type " << leftGray_cpu.type()
                  << ", Right: " << rightGray_cpu.size() << " type " << rightGray_cpu.type() << std::endl;
        return cv::Mat();
    }
    if (!_sgbm)
    {
        std::cerr << "StereoSBGMItem [" << _id << "]: Error - StereoSGBM not initialized." << std::endl;
        return cv::Mat();
    }

    cv::Mat disparityS16_cpu;
#ifdef DEBUG_PIPELINE
    std::cout << "StereoSBGMItem [" << _id << "]: Computing SGBM disparity with CV_8UC1 inputs. Left: " << leftGray_cpu.size() << ", Right: " << rightGray_cpu.size() << std::endl;
#endif
    try {
        _sgbm->compute(leftGray_cpu, rightGray_cpu, disparityS16_cpu);
    } catch (const cv::Exception& e) {
        std::cerr << "StereoSBGMItem [" << _id << "]: OpenCV Exception during SGBM compute: " << e.what() << std::endl;
        return cv::Mat();
    }
    

#ifdef DEBUG_PIPELINE
    if (!disparityS16_cpu.empty()) {
        double minVal, maxVal;
        cv::minMaxLoc(disparityS16_cpu, &minVal, &maxVal);
        std::cout << "StereoSBGMItem [" << _id << "]: SGBM computation complete. Output type: " << disparityS16_cpu.type()
                  << " (expected CV_16S=" << CV_16S << "). Size: " << disparityS16_cpu.size()
                  << ". MinVal: " << minVal << ", MaxVal: " << maxVal << "." << std::endl;
    } else {
        std::cout << "StereoSBGMItem [" << _id << "]: SGBM computation resulted in an empty map." << std::endl;
    }
#endif

    // Return the raw CV_16S disparity map.
    // Disparity values are scaled by 16.
    // Invalid disparities are typically (minDisparity - 1) * 16.
    return disparityS16_cpu;
}


// --- AutoTuneCameraItem Implementation ---
AutoTuneCameraItem::AutoTuneCameraItem(uint16_t id, CapReadItem *capItem,
                                       cv::VideoCaptureProperties prop,
                                       double targetVal, int maxIterations, bool propIsLogScale)
    : PipelineItem(id), _capSourceItem(capItem), _tuned(false),
      _targetMetricValue(targetVal), _maxIterations(maxIterations), _propToTune(prop), _propIsLogScale(propIsLogScale)
{
    if (!_capSourceItem)
    {
        std::cerr << "AutoTuneCameraItem [" << _id << "]: Error - CapReadItem pointer is null." << std::endl;
    }
}

double AutoTuneCameraItem::evaluateBrightness(const cv::Mat &frame)
{
    if (frame.empty())
        return 0.0;

    cv::Scalar meanBrightness;
    if (cv::ocl::useOpenCL())
    {
        cv::UMat u_frame = frame.getUMat(cv::ACCESS_READ);
        cv::UMat u_grayFrame;
        if (u_frame.channels() == 3)
            cv::cvtColor(u_frame, u_grayFrame, cv::COLOR_BGR2GRAY);
        else
            u_grayFrame = u_frame;
        meanBrightness = cv::mean(u_grayFrame);
    }
    else
    {
        cv::Mat grayFrame;
        if (frame.channels() == 3)
            cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
        else
            grayFrame = frame;
        meanBrightness = cv::mean(grayFrame);
    }
    return meanBrightness[0];
}

void AutoTuneCameraItem::tuneCamera()
{
    if (!_capSourceItem || !_capSourceItem->capSource || !_capSourceItem->capSource->isOpened())
    {
        std::cerr << "AutoTuneCameraItem [" << _id << "]: Camera source not available for tuning." << std::endl;
        _tuned = true;
        return;
    }
#ifdef DEBUG_PIPELINE
    std::cout << "AutoTuneCameraItem [" << _id << "]: Starting tuning for prop " << static_cast<int>(_propToTune) << "..." << std::endl;
#endif
    for (int i = 0; i < _maxIterations; ++i)
    {
        cv::Mat frame;
        // Grab a few frames to let settings stabilize and discard stale frames
        for (int j = 0; j < 3; ++j)
        {
            if (!_capSourceItem->capSource->grab())
            {
                std::cerr << "AutoTuneCameraItem [" << _id << "]: Failed to grab frame during pre-roll for tuning." << std::endl;
                _tuned = true;
                return;
            }
        }
        if (_capSourceItem->capSource->retrieve(frame) && !frame.empty())
        {
            double currentFrameMetric = evaluateBrightness(frame);
            double currentPropSetting = _capSourceItem->getCameraSetting(_propToTune);
#ifdef DEBUG_PIPELINE
            std::cout << "AutoTuneCameraItem [" << _id << "]: Iter " << i << ", Prop " << static_cast<int>(_propToTune) << " Setting: " << currentPropSetting
                      << ", Measured Brightness: " << currentFrameMetric << ", Target: " << _targetMetricValue << std::endl;
#endif
            double error = _targetMetricValue - currentFrameMetric;
            if (std::abs(error) < 10.0)
            { // Threshold for "good enough"
#ifdef DEBUG_PIPELINE
                std::cout << "AutoTuneCameraItem [" << _id << "]: Target metric reached." << std::endl;
#endif
                _tuned = true;
                break;
            }

            double newPropSetting = currentPropSetting;
            if (_propIsLogScale)
            {
                // For log scale properties (like some exposure controls), adjust by steps
                newPropSetting = currentPropSetting + (error > 0 ? 1 : -1); // Adjust by one step
                // Clamping might be needed depending on the property's valid range
                // Example for typical log exposure:
                // newPropSetting = std::max(-10.0, std::min(0.0, newPropSetting));
            }
            else if (_propToTune == cv::CAP_PROP_BRIGHTNESS)
            {
                newPropSetting = currentPropSetting + error * 0.1;               // Proportional adjustment
                newPropSetting = std::max(0.0, std::min(255.0, newPropSetting)); // Clamp for typical brightness
            }
            else if (_propToTune == cv::CAP_PROP_EXPOSURE && !_propIsLogScale)
            {                            // Linear exposure scale
                double gainFactor = 0.1; // How aggressively to adjust
                if (std::abs(currentPropSetting) > 1e-5)
                {                                                      // Relative adjustment if current setting is not zero
                    double relativeError = error / currentFrameMetric; // How far off in relative terms
                    newPropSetting = currentPropSetting * (1.0 + relativeError * gainFactor);
                }
                else
                {                                                        // Absolute adjustment if current setting is zero or very small
                    newPropSetting = currentPropSetting + error * 0.001; // Small absolute step
                }
                // Clamping for typical linear exposure values (e.g., in seconds or ms) might be needed
                // newPropSetting = std::max(0.0001, std::min(1.0, newPropSetting));
            }
            else
            { // Generic proportional adjustment for other properties
                double gain = 0.01;
                if (std::abs(currentPropSetting) > 1e-5)
                    gain = std::abs(error / currentPropSetting) * 0.1;
                gain = std::max(0.001, std::min(0.2, gain));
                newPropSetting = currentPropSetting + error * gain * (std::abs(currentPropSetting) > 1e-5 ? std::abs(currentPropSetting) : 1.0);
            }

            if (std::abs(newPropSetting - currentPropSetting) < 1e-4 && std::abs(error) >= 10.0)
            { // Avoid tiny changes if error still large
#ifdef DEBUG_PIPELINE
                std::cout << "AutoTuneCameraItem [" << _id << "]: No significant change in prop setting (" << newPropSetting << " vs " << currentPropSetting << "), but error (" << error << ") persists. Stopping." << std::endl;
#endif
                _tuned = true;
                break;
            }
            _capSourceItem->setCameraSetting(_propToTune, newPropSetting);
            // Add a small delay to allow camera settings to take effect
            // std::this_thread::sleep_for(std::chrono::milliseconds(30)); // Requires <thread> and <chrono>
        }
        else
        {
            std::cerr << "AutoTuneCameraItem [" << _id << "]: Failed to grab/retrieve frame during tuning." << std::endl;
            _tuned = true; // Stop if frames can't be read
            break;
        }
    }
    _tuned = true;
#ifdef DEBUG_PIPELINE
    std::cout << "AutoTuneCameraItem [" << _id << "]: Tuning finished. Final prop " << static_cast<int>(_propToTune)
              << " setting: " << _capSourceItem->getCameraSetting(_propToTune) << std::endl;
#endif
}

cv::Mat AutoTuneCameraItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (!_tuned && _capSourceItem)
    {
        tuneCamera();
    }
    return _input; // Pass through input, this item modifies CapSourceItem state
}

// --- ImageRectificationItem Implementation ---
ImageRectificationItem::ImageRectificationItem(uint16_t id, const std::string &calibrationFilePath, bool isLeftCamera)
    : PipelineItem(id), _initialized(false), _oclInitialized(false)
{
    cv::FileStorage fs(calibrationFilePath, cv::FileStorage::READ);
    if (!fs.isOpened())
    {
        std::cerr << "ImageRectificationItem [" << _id << "]: Error - Failed to open calibration file: " << calibrationFilePath << std::endl;
        return;
    }

    cv::Mat M_cpu, D_cpu, R_calib, P_calib; // Use temporary CPU Mats for reading
    int w = 0, h = 0;

    fs["imageSizeWidth"] >> w;
    fs["imageSizeHeight"] >> h;

    if (w == 0 || h == 0)
    {
        cv::Size calibSize;
        fs["imageSize"] >> calibSize;
        if (calibSize.width > 0 && calibSize.height > 0)
        {
            w = calibSize.width;
            h = calibSize.height;
        }
        else
        {
            std::cerr << "ImageRectificationItem [" << _id << "]: Error - imageSizeWidth/Height or imageSize not found or zero in calibration file." << std::endl;
            fs.release();
            return;
        }
    }
    _imageSize = cv::Size(w, h);

    if (isLeftCamera)
    {
        fs["cameraMatrix1"] >> M_cpu;
        fs["distCoeffs1"] >> D_cpu;
        fs["R1"] >> R_calib;
        fs["P1"] >> P_calib;
        if (M_cpu.empty() || D_cpu.empty() || R_calib.empty() || P_calib.empty())
        {
            std::cerr << "ImageRectificationItem [" << _id << "]: Error - Missing left camera calibration data (M1, D1, R1, P1) in file." << std::endl;
            fs.release();
            return;
        }
    }
    else
    {
        fs["cameraMatrix2"] >> M_cpu;
        fs["distCoeffs2"] >> D_cpu;
        fs["R2"] >> R_calib;
        fs["P2"] >> P_calib;
        if (M_cpu.empty() || D_cpu.empty() || R_calib.empty() || P_calib.empty())
        {
            std::cerr << "ImageRectificationItem [" << _id << "]: Error - Missing right camera calibration data (M2, D2, R2, P2) in file." << std::endl;
            fs.release();
            return;
        }
    }
    fs.release();

    // Initialize CPU maps
    cv::initUndistortRectifyMap(M_cpu, D_cpu, R_calib, P_calib, _imageSize, CV_16SC2, _map1_cpu, _map2_cpu);
    _initialized = true;

#ifdef DEBUG_PIPELINE
    std::cout << "ImageRectificationItem [" << _id << "]: CPU maps initialized for " << (isLeftCamera ? "LEFT" : "RIGHT")
              << " camera with image size " << _imageSize << std::endl;
#endif

    // If OpenCL is available and enabled, initialize OCL maps
    if (cv::ocl::haveOpenCL() && cv::ocl::useOpenCL())
    {
        _map1_ocl = _map1_cpu.getUMat(cv::ACCESS_READ);
        _map2_ocl = _map2_cpu.getUMat(cv::ACCESS_READ);
        _oclInitialized = true;
#ifdef DEBUG_PIPELINE
        std::cout << "ImageRectificationItem [" << _id << "]: OCL maps initialized." << std::endl;
#endif
    }
}

cv::Mat ImageRectificationItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (!_initialized || _input.empty())
    {
        if (!_initialized && !_input.empty())
            std::cerr << "ImageRectificationItem [" << _id << "]: Not initialized, cannot rectify." << std::endl;
        else if (_input.empty())
            std::cerr << "ImageRectificationItem [" << _id << "]: Input image is empty." << std::endl;
        return _input;
    }

    cv::Mat currentInput = _input; // Input is already Mat
    if (currentInput.size() != _imageSize)
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ImageRectificationItem [" << _id << "]: Warning - Input image size " << currentInput.size()
                  << " does not match calibration image size " << _imageSize << ". Resizing input." << std::endl;
#endif
        // Resize can use OCL
        if (cv::ocl::useOpenCL() && _oclInitialized)
        {
            cv::UMat u_temp;
            cv::resize(currentInput.getUMat(cv::ACCESS_READ), u_temp, _imageSize);
            currentInput = u_temp.getMat(cv::ACCESS_READ).clone();
        }
        else
        {
            cv::resize(currentInput, currentInput, _imageSize);
        }
    }

    cv::Mat rectifiedImage;
    if (cv::ocl::useOpenCL() && _oclInitialized && !_map1_ocl.empty() && !_map2_ocl.empty())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ImageRectificationItem [" << _id << "]: Using OpenCL for remap." << std::endl;
#endif
        cv::UMat u_input = currentInput.getUMat(cv::ACCESS_READ);
        cv::UMat u_rectifiedImage;
        cv::remap(u_input, u_rectifiedImage, _map1_ocl, _map2_ocl, cv::INTER_LINEAR);
        rectifiedImage = u_rectifiedImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
        if (_map1_cpu.empty() || _map2_cpu.empty())
        {
            std::cerr << "ImageRectificationItem [" << _id << "]: CPU maps are empty, cannot rectify." << std::endl;
            return _input;
        }
#ifdef DEBUG_PIPELINE
        std::cout << "ImageRectificationItem [" << _id << "]: Using CPU for remap." << std::endl;
#endif
        cv::remap(currentInput, rectifiedImage, _map1_cpu, _map2_cpu, cv::INTER_LINEAR);
    }
    return rectifiedImage;
}

// --- AdaptiveThresholdItem Implementation ---
AdaptiveThresholdItem::AdaptiveThresholdItem(uint16_t id, double maxValue, cv::AdaptiveThresholdTypes adaptiveMethod,
                                             cv::ThresholdTypes thresholdType, int blockSize, double C)
    : PipelineItem(id), _maxValue(maxValue), _adaptiveMethod(adaptiveMethod),
      _thresholdType(thresholdType), _blockSize(blockSize), _C(C)
{
    if (_blockSize <= 1 || _blockSize % 2 == 0)
    {
        std::cerr << "AdaptiveThresholdItem [" << _id << "]: Warning - Block size must be >1 and odd. Adjusting " << _blockSize << " to ";
        _blockSize = (_blockSize <= 1) ? 3 : _blockSize + 1;
        std::cerr << _blockSize << "." << std::endl;
    }
}

cv::Mat AdaptiveThresholdItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "AdaptiveThresholdItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "AdaptiveThresholdItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_grayInput, u_thresholdedImage;
        if (u_input.channels() == 3)
        {
            cv::cvtColor(u_input, u_grayInput, cv::COLOR_BGR2GRAY);
        }
        else
        {
            u_grayInput = u_input;
        }
        cv::adaptiveThreshold(u_grayInput, u_thresholdedImage, _maxValue, _adaptiveMethod, _thresholdType, _blockSize, _C);
        return u_thresholdedImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "AdaptiveThresholdItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat grayInput = _input;
        if (_input.channels() == 3)
        {
            cv::cvtColor(_input, grayInput, cv::COLOR_BGR2GRAY);
        }
        cv::Mat thresholdedImage;
        cv::adaptiveThreshold(grayInput, thresholdedImage, _maxValue, _adaptiveMethod, _thresholdType, _blockSize, _C);
        return thresholdedImage;
    }
}

// --- MorphologyExItem Implementation ---
MorphologyExItem::MorphologyExItem(uint16_t id, cv::MorphTypes op, int kernelSize, int iterations,
                                   cv::Point anchor, int borderType, const cv::Scalar &borderValue)
    : PipelineItem(id), _op(op), _anchor(anchor), _iterations(iterations), _borderType(borderType), _borderValue(borderValue)
{
    _kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
}

cv::Mat MorphologyExItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "MorphologyExItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "MorphologyExItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_outputImage;
        cv::morphologyEx(u_input, u_outputImage, _op, _kernel, _anchor, _iterations, _borderType, _borderValue);
        return u_outputImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "MorphologyExItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat outputImage;
        cv::morphologyEx(_input, outputImage, _op, _kernel, _anchor, _iterations, _borderType, _borderValue);
        return outputImage;
    }
}

// --- HistogramEqualizeItem Implementation ---
HistogramEqualizeItem::HistogramEqualizeItem(uint16_t id) : PipelineItem(id) {}

cv::Mat HistogramEqualizeItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "HistogramEqualizeItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "HistogramEqualizeItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_grayInput, u_equalizedImage;
        if (u_input.channels() == 3)
        {
            cv::cvtColor(u_input, u_grayInput, cv::COLOR_BGR2GRAY);
        }
        else if (u_input.channels() == 1)
        {
            u_grayInput = u_input;
        }
        else
        {
            std::cerr << "HistogramEqualizeItem [" << _id << "]: Error - Input image must be 1 or 3 channels. Found " << u_input.channels() << std::endl;
            return _input; // Or original image if it's gray
        }
        cv::equalizeHist(u_grayInput, u_equalizedImage);
        return u_equalizedImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "HistogramEqualizeItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat grayInput, equalizedImage;
        if (_input.channels() == 3)
        {
            cv::cvtColor(_input, grayInput, cv::COLOR_BGR2GRAY);
        }
        else if (_input.channels() == 1)
        {
            grayInput = _input;
        }
        else
        {
            std::cerr << "HistogramEqualizeItem [" << _id << "]: Error - Input image must be 1 or 3 channels. Found " << _input.channels() << std::endl;
            return _input;
        }
        cv::equalizeHist(grayInput, equalizedImage);
        return equalizedImage;
    }
}

// --- StereoSADItem Implementation ---
StereoSADItem::StereoSADItem(uint16_t id, uint16_t leftCacheKey, uint16_t rightCacheKey,
                             int minDisparity, int numDisparities, int blockSize,
                             int speckleWindowSize, int speckleRange, int disp12MaxDiff)
    : PipelineItem(id), _leftImageCacheKey(leftCacheKey), _rightImageCacheKey(rightCacheKey),
      _minDisparity(minDisparity), _numDisparities(numDisparities)
{
    if (numDisparities <= 0 || numDisparities % 16 != 0)
    {
        std::cerr << "StereoSADItem [" << _id << "]: Warning - numDisparities must be > 0 and multiple of 16. Adjusted "
                  << numDisparities << " to ";
        numDisparities = (numDisparities <= 0) ? 16 : ((numDisparities / 16) + (numDisparities % 16 != 0 ? 1 : 0)) * 16;
        if (numDisparities == 0)
            numDisparities = 16; // Ensure it's at least 16
        std::cerr << numDisparities << "." << std::endl;
        _numDisparities = numDisparities;
    }
    if (blockSize <= 0 || blockSize % 2 == 0)
    {
        std::cerr << "StereoSADItem [" << _id << "]: Warning - blockSize must be odd and positive. Adjusted "
                  << blockSize << " to ";
        blockSize = (blockSize <= 0) ? 5 : blockSize + (blockSize % 2 == 0 ? 1 : 0);
        std::cerr << blockSize << "." << std::endl;
    }

    _sbm = cv::StereoBM::create(_numDisparities, blockSize);
    if (!_sbm)
    {
        std::cerr << "StereoSADItem [" << _id << "]: Error - Failed to create StereoBM instance." << std::endl;
        return;
    }
    _sbm->setMinDisparity(_minDisparity);
    // Set other SBM parameters if provided and valid
    if (speckleWindowSize > 0)
        _sbm->setSpeckleWindowSize(speckleWindowSize);
    if (speckleRange > 0)
        _sbm->setSpeckleRange(speckleRange);
    _sbm->setDisp12MaxDiff(disp12MaxDiff); // -1 to disable, 0 for default if speckle removal is on
    // Can add more setters: setPreFilterCap, setPreFilterSize, setPreFilterType,
    // setSmallerBlockSize, setTextureThreshold, setUniquenessRatio
}

cv::Mat StereoSADItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    auto itLeft = matCache.find(_leftImageCacheKey);
    auto itRight = matCache.find(_rightImageCacheKey);

    if (itLeft == matCache.end() || itRight == matCache.end() || itLeft->second.empty() || itRight->second.empty())
    {
        std::cerr << "StereoSADItem [" << _id << "]: Error - Left (" << _leftImageCacheKey
                  << ") or Right (" << _rightImageCacheKey << ") image not found/empty in cache." << std::endl;
        return cv::Mat(); // Return empty mat
    }

    cv::Mat leftImage_cpu = itLeft->second;
    cv::Mat rightImage_cpu = itRight->second;
    cv::Mat leftGray_cpu, rightGray_cpu;

    bool oclActive = cv::ocl::useOpenCL();

    if (leftImage_cpu.channels() != 1)
    {
        if (oclActive)
            cv::cvtColor(leftImage_cpu.getUMat(cv::ACCESS_READ), leftGray_cpu, cv::COLOR_BGR2GRAY);
        else
            cv::cvtColor(leftImage_cpu, leftGray_cpu, cv::COLOR_BGR2GRAY);
    }
    else
    {
        leftGray_cpu = leftImage_cpu;
    }
    if (rightImage_cpu.channels() != 1)
    {
        if (oclActive)
            cv::cvtColor(rightImage_cpu.getUMat(cv::ACCESS_READ), rightGray_cpu, cv::COLOR_BGR2GRAY);
        else
            cv::cvtColor(rightImage_cpu, rightGray_cpu, cv::COLOR_BGR2GRAY);
    }
    else
    {
        rightGray_cpu = rightImage_cpu;
    }

    if (leftGray_cpu.size() != rightGray_cpu.size() || leftGray_cpu.type() != CV_8UC1 || rightGray_cpu.type() != CV_8UC1)
    {
        std::cerr << "StereoSADItem [" << _id << "]: Error - Left/Right images must be CV_8UC1 and same size." << std::endl;
        std::cerr << "  Left: " << leftGray_cpu.size() << " type " << leftGray_cpu.type()
                  << ", Right: " << rightGray_cpu.size() << " type " << rightGray_cpu.type() << std::endl;
        return cv::Mat();
    }
    if (!_sbm)
    {
        std::cerr << "StereoSADItem [" << _id << "]: Error - StereoBM not initialized." << std::endl;
        return cv::Mat();
    }

    cv::Mat disparityS16_cpu;
    _sbm->compute(leftGray_cpu, rightGray_cpu, disparityS16_cpu); // Output is CV_16S

    // The output is CV_16S. Normalization for visualization can be a separate pipeline item.
    return disparityS16_cpu;
}

// --- MedianFilterItem Implementation ---
MedianFilterItem::MedianFilterItem(uint16_t id, int ksize) : PipelineItem(id), _ksize(ksize)
{
    if (_ksize <= 1 || _ksize % 2 == 0)
    {
        std::cerr << "MedianFilterItem [" << _id << "]: Warning - ksize must be odd and >1. Adjusted " << _ksize << " to ";
        _ksize = (_ksize <= 1) ? 3 : _ksize + (_ksize % 2 == 0 ? 1 : 0);
        std::cerr << _ksize << "." << std::endl;
    }
}

cv::Mat MedianFilterItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "MedianFilterItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "MedianFilterItem [" << _id << "]: Using OpenCL." << std::endl;
#endif
        cv::UMat u_input = _input.getUMat(cv::ACCESS_READ);
        cv::UMat u_blurredImage;
        cv::medianBlur(u_input, u_blurredImage, _ksize);
        return u_blurredImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "MedianFilterItem [" << _id << "]: Using CPU." << std::endl;
#endif
        cv::Mat blurredImage;
        cv::medianBlur(_input, blurredImage, _ksize);
        return blurredImage;
    }
}

// --- SpeckleFilterItem Implementation ---
SpeckleFilterItem::SpeckleFilterItem(uint16_t id, int newVal, int maxSpeckleSize, int maxDiff)
    : PipelineItem(id), _newVal(newVal), _maxSpeckleSize(maxSpeckleSize), _maxDiff(maxDiff * 16)
{
    // _maxDiff is scaled because cv::filterSpeckles expects scaled diff for CV_16S maps
    // _newVal is also a scaled disparity value (newValue*16 + (newValue<0?-subpixel_val:subpixel_val))
    // For simplicity, we assume newVal is passed as the direct value to set in the CV_16S map.
}

cv::Mat SpeckleFilterItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "SpeckleFilterItem [" << _id << "]: Warning - Input disparity map is empty." << std::endl;
        return _input;
    }
    if (_input.type() != CV_16S)
    {
        std::cerr << "SpeckleFilterItem [" << _id << "]: Warning - Input map is not CV_16S. Speckle filtering might not work as expected. Type: " << _input.type() << std::endl;
        // Continue, but it might fail or give bad results.
    }

    // cv::filterSpeckles operates on cv::Mat directly.
    // If OpenCL was used before, _input is already a Mat.
    cv::Mat disparityMap = _input.clone(); // filterSpeckles modifies in-place
#ifdef DEBUG_PIPELINE
    std::cout << "SpeckleFilterItem [" << _id << "]: Applying filterSpeckles. NewVal: " << _newVal
              << ", MaxSize: " << _maxSpeckleSize << ", MaxDiff (scaled): " << _maxDiff << std::endl;
#endif
    cv::filterSpeckles(disparityMap, _newVal, _maxSpeckleSize, _maxDiff);
    return disparityMap;
}


// --- HoleFillingItem Implementation ---
HoleFillingItem::HoleFillingItem(uint16_t id, int minDisparityValue,
                                 bool propagateLtoR, bool propagateRtoL,
                                 bool enable2DFill, int neighborhoodSize,
                                 int fillIterations, // New parameter
                                 bool applyMedianBlur, int medianBlurKernel)
    : PipelineItem(id),
      _minDisparityValue(minDisparityValue),
      _propagateLeftToRight(propagateLtoR),
      _propagateRightToLeft(propagateRtoL),
      _enable2DFill(enable2DFill),
      _2dNeighborhoodSize(neighborhoodSize),
      _2dFillIterations(std::max(1, fillIterations)), // Ensure at least 1 iteration if 2D fill is enabled
      _applyMedianBlurPost(applyMedianBlur),
      _medianBlurKernelSize(medianBlurKernel)
{
    if (_2dNeighborhoodSize % 2 == 0 || _2dNeighborhoodSize < 3) {
        std::cerr << "HoleFillingItem [" << _id << "]: Warning - 2D Neighborhood size must be odd and >= 3. Setting to 3." << std::endl;
        _2dNeighborhoodSize = 3;
    }
     if (_2dFillIterations <= 0 && _enable2DFill) {
        std::cerr << "HoleFillingItem [" << _id << "]: Warning - 2D Fill Iterations must be > 0 if 2D fill is enabled. Setting to 1." << std::endl;
        _2dFillIterations = 1;
    }
    if (_medianBlurKernelSize % 2 == 0 || _medianBlurKernelSize < 3) {
        std::cerr << "HoleFillingItem [" << _id << "]: Warning - Median blur kernel size must be odd and >= 3. Setting to 3." << std::endl;
        _medianBlurKernelSize = 3;
    }
}

// Helper function to get median of valid neighbors - NO CHANGES NEEDED
short HoleFillingItem::getMedianNeighborValue(const cv::Mat& map, int r, int c, int neighborhoodSize, short scaledMinValidDisparity) {
    std::vector<short> neighbors;
    int halfSize = neighborhoodSize / 2;

    for (int dr = -halfSize; dr <= halfSize; ++dr) {
        for (int dc = -halfSize; dc <= halfSize; ++dc) {
            if (dr == 0 && dc == 0) continue; // Skip the pixel itself

            int nr = r + dr;
            int nc = c + dc;

            if (nr >= 0 && nr < map.rows && nc >= 0 && nc < map.cols) {
                short val = map.at<short>(nr, nc);
                if (val >= scaledMinValidDisparity) {
                    neighbors.push_back(val);
                }
            }
        }
    }

    if (neighbors.empty()) {
        return -1; // Sentinel for no valid neighbors
    }

    std::sort(neighbors.begin(), neighbors.end());
    return neighbors[neighbors.size() / 2]; // Median
}


cv::Mat HoleFillingItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "HoleFillingItem [" << _id << "]: Warning - Input disparity map is empty." << std::endl;
        return _input;
    }
    if (_input.type() != CV_16S)
    {
        std::cerr << "HoleFillingItem [" << _id << "]: Error - Input map must be CV_16S. Type: " << _input.type() << std::endl;
        return _input;
    }

    cv::Mat filledMap = _input.clone(); // Start with a copy. This will be modified by 1D passes.
    short scaledMinValidDisparity = static_cast<short>(_minDisparityValue * 16);

#ifdef DEBUG_PIPELINE
    std::cout << "HoleFillingItem [" << _id << "]: Filling holes. MinValidDisp (unscaled): " << _minDisparityValue
              << ", MinValidDisp (scaled): " << scaledMinValidDisparity
              << ", L-R: " << _propagateLeftToRight << ", R-L: " << _propagateRightToLeft
              << ", 2DFill: " << _enable2DFill << " (Size: " << _2dNeighborhoodSize << ", Iterations: " << _2dFillIterations << ")"
              << ", MedianBlurPost: " << _applyMedianBlurPost << " (Kernel: " << _medianBlurKernelSize << ")"
              << std::endl;
#endif

    // Pass 1: Horizontal Left-to-Right propagation (optional)
    if (_propagateLeftToRight)
    {
        for (int r = 0; r < filledMap.rows; ++r)
        {
            short lastValidDisp = -1; 
            for (int c = 0; c < filledMap.cols; ++c)
            {
                short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity)
                {
                    lastValidDisp = currentDisp;
                }
                else // Hole
                {
                    if (lastValidDisp != -1)
                    {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
    }

    // Pass 2: Horizontal Right-to-Left propagation (optional)
    if (_propagateRightToLeft)
    {
        for (int r = 0; r < filledMap.rows; ++r)
        {
            short lastValidDisp = -1;
            for (int c = filledMap.cols - 1; c >= 0; --c)
            {
                short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity)
                {
                    lastValidDisp = currentDisp;
                }
                else // Hole
                {
                    if (lastValidDisp != -1)
                    {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
    }

    // Pass 3: Iterative 2D Neighborhood Fill (optional)
    if (_enable2DFill && _2dFillIterations > 0)
    {
        // currentPassMap holds the state at the START of the current iteration
        // (i.e., result from previous iteration or 1D passes for the first iteration)
        cv::Mat currentPassMap = filledMap.clone(); 
        cv::Mat nextPassMap = filledMap.clone(); // nextPassMap is where we write results FOR the current iteration

        for (int iter = 0; iter < _2dFillIterations; ++iter)
        {
            bool changedInThisIteration = false;
            for (int r = 0; r < currentPassMap.rows; ++r)
            {
                for (int c = 0; c < currentPassMap.cols; ++c)
                {
                    // Check for holes in currentPassMap (values from previous iteration)
                    if (currentPassMap.at<short>(r, c) < scaledMinValidDisparity) 
                    {
                        // Neighbors are also read from currentPassMap
                        short medianVal = getMedianNeighborValue(currentPassMap, r, c, _2dNeighborhoodSize, scaledMinValidDisparity);
                        
                        if (medianVal != -1) // If valid neighbors were found
                        {
                            // Write the new value to nextPassMap for this iteration
                            nextPassMap.at<short>(r, c) = medianVal;
                            // A hole was filled, so it definitely changed.
                            changedInThisIteration = true; 
                        }
                        else
                        {
                            // No valid median found, the hole persists in nextPassMap for this iteration
                            // (it was already a hole in currentPassMap, and nextPassMap is a clone)
                            // No change needed for nextPassMap.at<short>(r,c) here if it's already a clone.
                            // To be explicit:
                            nextPassMap.at<short>(r, c) = currentPassMap.at<short>(r, c);
                        }
                    }
                    else
                    {
                        // If it's not a hole, it remains unchanged for the next iteration's start.
                        // No change needed for nextPassMap.at<short>(r,c) here if it's already a clone.
                        // To be explicit:
                        nextPassMap.at<short>(r, c) = currentPassMap.at<short>(r, c);
                    }
                }
            }
            
            // Prepare for the next iteration: the result of this iteration (nextPassMap)
            // becomes the input for the next one (currentPassMap).
            // Using copyTo is generally preferred over .clone() in a loop if dimensions don't change.
            nextPassMap.copyTo(currentPassMap);

            if (!changedInThisIteration && iter > 0) { 
                // Optimization: if no pixels changed values during this iteration
                // (and it's not the very first 2D iteration which might fill holes left by 1D passes),
                // then further iterations won't help.
#ifdef DEBUG_PIPELINE
                std::cout << "HoleFillingItem [" << _id << "]: 2D fill converged after " << iter + 1 << " iterations." << std::endl;
#endif
                break;
            }
        }
        // The result of all iterations is in currentPassMap, assign it back to filledMap
        currentPassMap.copyTo(filledMap); 
    }

    // Pass 4: Optional Median Blur for final smoothing (internal to this item)
    if (_applyMedianBlurPost)
    {
        if (filledMap.rows > 0 && filledMap.cols > 0) 
        {
             cv::medianBlur(filledMap, filledMap, _medianBlurKernelSize);
        }
        else
        {
            std::cerr << "HoleFillingItem [" << _id << "]: Warning - Map is empty before median blur, skipping." << std::endl;
        }
    }

    return filledMap;
}

// --- DisparityConfidenceItem Implementation ---
DisparityConfidenceItem::DisparityConfidenceItem(uint16_t id, uint16_t leftCacheKey, uint16_t rightCacheKey, int windowSize, float disparityScale)
    : PipelineItem(id), _leftImageCacheKey(leftCacheKey), _rightImageCacheKey(rightCacheKey), _windowSize(windowSize), _disparityScale(disparityScale)
{
    if (_windowSize <= 0 || _windowSize % 2 == 0)
    {
        std::cerr << "DisparityConfidenceItem [" << _id << "]: Warning - windowSize must be odd and positive. Adjusted "
                  << _windowSize << " to ";
        _windowSize = (_windowSize <= 0) ? 3 : _windowSize + (_windowSize % 2 == 0 ? 1 : 0);
        std::cerr << _windowSize << "." << std::endl;
    }
}

cv::Mat DisparityConfidenceItem::forward(cv::Mat _inputDisparityMap, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_inputDisparityMap.empty())
    {
        std::cerr << "DisparityConfidenceItem [" << _id << "]: Input disparity map is empty." << std::endl;
        return cv::Mat();
    }
    if (_inputDisparityMap.type() != CV_16S && _inputDisparityMap.type() != CV_32F)
    {
        std::cerr << "DisparityConfidenceItem [" << _id << "]: Warning - Input disparity map type " << _inputDisparityMap.type()
                  << " not CV_16S or CV_32F. Confidence calculation might be incorrect." << std::endl;
    }

    auto itLeft = matCache.find(_leftImageCacheKey);
    auto itRight = matCache.find(_rightImageCacheKey);

    if (itLeft == matCache.end() || itRight == matCache.end() || itLeft->second.empty() || itRight->second.empty())
    {
        std::cerr << "DisparityConfidenceItem [" << _id << "]: Error - Left (" << _leftImageCacheKey
                  << ") or Right (" << _rightImageCacheKey << ") image not found/empty in cache." << std::endl;
        return cv::Mat();
    }

    cv::Mat leftImg = itLeft->second;
    cv::Mat rightImg = itRight->second;

    if (leftImg.type() != CV_8UC1 || rightImg.type() != CV_8UC1)
    {
        std::cerr << "DisparityConfidenceItem [" << _id << "]: Error - Left/Right images must be CV_8UC1." << std::endl;
        // Could attempt to convert, but for now, require grayscale
        return cv::Mat();
    }
    if (leftImg.size() != rightImg.size() || leftImg.size() != _inputDisparityMap.size())
    {
        std::cerr << "DisparityConfidenceItem [" << _id << "]: Error - Image and disparity map sizes mismatch." << std::endl;
        return cv::Mat();
    }

    cv::Mat confidenceMap = cv::Mat::zeros(_inputDisparityMap.size(), CV_8UC1);
    int halfWin = _windowSize / 2;
    float maxSAD = static_cast<float>(_windowSize * _windowSize * 255);

    for (int r = halfWin; r < _inputDisparityMap.rows - halfWin; ++r)
    {
        for (int c = halfWin; c < _inputDisparityMap.cols - halfWin; ++c)
        {
            float disp;
            if (_inputDisparityMap.type() == CV_16S)
            {
                disp = static_cast<float>(_inputDisparityMap.at<short>(r, c)) * _disparityScale;
            }
            else
            {                                                                // CV_32F
                disp = _inputDisparityMap.at<float>(r, c) * _disparityScale; // Assuming _disparityScale is 1.0 if map already scaled
            }

            int right_c = cvRound(c - disp);

            if (right_c >= halfWin && right_c < _inputDisparityMap.cols - halfWin)
            {
                float currentSAD = 0;
                for (int wy = -halfWin; wy <= halfWin; ++wy)
                {
                    for (int wx = -halfWin; wx <= halfWin; ++wx)
                    {
                        currentSAD += std::abs(static_cast<float>(leftImg.at<uchar>(r + wy, c + wx)) -
                                               static_cast<float>(rightImg.at<uchar>(r + wy, right_c + wx)));
                    }
                }
                // Normalize SAD to confidence (0-255, higher is better)
                // (1 - SAD/maxSAD) * 255
                confidenceMap.at<uchar>(r, c) = cv::saturate_cast<uchar>(255.0f * (1.0f - (currentSAD / maxSAD)));
            }
            else
            {
                confidenceMap.at<uchar>(r, c) = 0; // Low confidence for out-of-bounds disparity
            }
        }
    }
    return confidenceMap;
}

// --- LeftRightConsistencyCheckItem Implementation ---
LeftRightConsistencyCheckItem::LeftRightConsistencyCheckItem(uint16_t id, uint16_t rightToLeftDispKey, float threshold, int minDispValForInput, float disparityScale)
    : PipelineItem(id), _rightToLeftDisparityCacheKey(rightToLeftDispKey), _threshold(threshold),
      _minDisparityValueForInput(minDispValForInput), _disparityScale(disparityScale) {}

cv::Mat LeftRightConsistencyCheckItem::forward(cv::Mat _inputDisparityMapLR, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_inputDisparityMapLR.empty())
    {
        std::cerr << "LeftRightConsistencyCheckItem [" << _id << "]: Input L-R disparity map is empty." << std::endl;
        return cv::Mat();
    }

    auto itRL = matCache.find(_rightToLeftDisparityCacheKey);
    if (itRL == matCache.end() || itRL->second.empty())
    {
        std::cerr << "LeftRightConsistencyCheckItem [" << _id << "]: Right-to-Left disparity map (key "
                  << _rightToLeftDisparityCacheKey << ") not found or empty in cache." << std::endl;
        return _inputDisparityMapLR; // Pass through if D_RL is missing
    }
    cv::Mat disparityMapRL = itRL->second;

    if (_inputDisparityMapLR.type() != disparityMapRL.type() || _inputDisparityMapLR.size() != disparityMapRL.size())
    {
        std::cerr << "LeftRightConsistencyCheckItem [" << _id << "]: L-R and R-L disparity maps type/size mismatch." << std::endl;
        std::cerr << "  LR: " << _inputDisparityMapLR.size() << " type " << _inputDisparityMapLR.type()
                  << ", RL: " << disparityMapRL.size() << " type " << disparityMapRL.type() << std::endl;
        return _inputDisparityMapLR;
    }

    bool isTypeS16 = (_inputDisparityMapLR.type() == CV_16S);
    if (!isTypeS16 && _inputDisparityMapLR.type() != CV_32F)
    {
        std::cerr << "LeftRightConsistencyCheckItem [" << _id << "]: Unsupported disparity map type: " << _inputDisparityMapLR.type() << ". Expected CV_16S or CV_32F." << std::endl;
        return _inputDisparityMapLR;
    }

    cv::Mat filteredMap = _inputDisparityMapLR.clone();
    short invalidDispValS16 = static_cast<short>((_minDisparityValueForInput - 1) * (isTypeS16 ? (1.0f / _disparityScale) : 1.0f));
    float invalidDispValF32 = static_cast<float>(_minDisparityValueForInput - 1);

#ifdef DEBUG_PIPELINE
    std::cout << "LeftRightConsistencyCheckItem [" << _id << "]: Checking consistency. Threshold (unscaled): " << _threshold
              << ", Scaled threshold for S16: " << _threshold / _disparityScale << std::endl;
#endif

    for (int r = 0; r < filteredMap.rows; ++r)
    {
        for (int c = 0; c < filteredMap.cols; ++c)
        {
            float dispLR;
            if (isTypeS16)
            {
                dispLR = static_cast<float>(filteredMap.at<short>(r, c)) * _disparityScale;
            }
            else
            {
                dispLR = filteredMap.at<float>(r, c); // Assume already scaled if F32
            }

            // Check if current disparity is valid before proceeding
            // For CV_16S, min valid is often minDisparity * 16.
            // For CV_32F, min valid is often minDisparity.
            bool currentLRValid = isTypeS16 ? (filteredMap.at<short>(r, c) >= static_cast<short>(_minDisparityValueForInput / _disparityScale))
                                            : (filteredMap.at<float>(r, c) >= static_cast<float>(_minDisparityValueForInput));
            if (!currentLRValid)
                continue;

            int c_RL = cvRound(c - dispLR); // Corresponding column in Right image frame for D_RL lookup

            if (c_RL >= 0 && c_RL < disparityMapRL.cols)
            {
                float dispRL;
                if (isTypeS16)
                {
                    dispRL = static_cast<float>(disparityMapRL.at<short>(r, c_RL)) * _disparityScale;
                }
                else
                {
                    dispRL = disparityMapRL.at<float>(r, c_RL);
                }

                bool currentRLValid = isTypeS16 ? (disparityMapRL.at<short>(r, c_RL) >= static_cast<short>(_minDisparityValueForInput / _disparityScale))
                                                : (disparityMapRL.at<float>(r, c_RL) >= static_cast<float>(_minDisparityValueForInput));

                if (!currentRLValid || std::abs(dispLR - dispRL) > _threshold)
                { // Standard D_L(p) vs D_R(p') check
                    // if (!currentRLValid || std::abs(dispLR + dispRL) > _threshold) { // If D_R is D_R(p') = - D_L(p)
                    if (isTypeS16)
                    {
                        filteredMap.at<short>(r, c) = invalidDispValS16;
                    }
                    else
                    {
                        filteredMap.at<float>(r, c) = invalidDispValF32;
                    }
                }
            }
            else
            { // Out of bounds for D_RL lookup
                if (isTypeS16)
                {
                    filteredMap.at<short>(r, c) = invalidDispValS16;
                }
                else
                {
                    filteredMap.at<float>(r, c) = invalidDispValF32;
                }
            }
        }
    }
    return filteredMap;
}

// --- NormalizeDisparityMapItem Implementation ---
NormalizeDisparityMapItem::NormalizeDisparityMapItem(uint16_t id, double alpha, double beta, int targetType,
                                                     bool scaleCV16S, double minDispOverride, double maxDispOverride)
    : PipelineItem(id), _alpha(alpha), _beta(beta), _targetType(targetType),
      _scaleInputDisparities(scaleCV16S), _minDispOverride(minDispOverride), _maxDispOverride(maxDispOverride) {}

cv::Mat NormalizeDisparityMapItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "NormalizeDisparityMapItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    cv::Mat toNormalize = _input;
    if (_input.type() == CV_16S && _scaleInputDisparities)
    {
        _input.convertTo(toNormalize, CV_32F, 1.0 / 16.0);
#ifdef DEBUG_PIPELINE
        std::cout << "NormalizeDisparityMapItem [" << _id << "]: Scaled CV_16S input to CV_32F by 1/16.0." << std::endl;
#endif
    }
    else if (_input.type() != CV_32F && _input.type() != CV_8U && _input.type() != CV_16S /*handled above or passed through if not scaling*/)
    {
        // Convert other types to CV_32F for robust normalization
        _input.convertTo(toNormalize, CV_32F);
#ifdef DEBUG_PIPELINE
        std::cout << "NormalizeDisparityMapItem [" << _id << "]: Converted input type " << _input.type() << " to CV_32F." << std::endl;
#endif
    }

    double minVal, maxVal;
    if (_minDispOverride >= 0 && _maxDispOverride > _minDispOverride)
    {
        minVal = _minDispOverride;
        maxVal = _maxDispOverride;
#ifdef DEBUG_PIPELINE
        std::cout << "NormalizeDisparityMapItem [" << _id << "]: Using override min/max: " << minVal << ", " << maxVal << std::endl;
#endif
    }
    else
    {
        cv::Mat mask; // Consider only valid disparities if a known invalid value exists
        // This is complex if we don't know the min_disp of the map.
        // For simplicity, normalize over the whole range unless overridden.
        // A more robust version would take min_disparity as param to create a mask.
        cv::minMaxLoc(toNormalize, &minVal, &maxVal, nullptr, nullptr, mask.empty() ? cv::noArray() : mask);
#ifdef DEBUG_PIPELINE
        std::cout << "NormalizeDisparityMapItem [" << _id << "]: Calculated min/max: " << minVal << ", " << maxVal << std::endl;
#endif
    }

    if (maxVal <= minVal)
    { // Avoid division by zero or negative range
#ifdef DEBUG_PIPELINE
        std::cout << "NormalizeDisparityMapItem [" << _id << "]: maxVal <= minVal. Outputting zero mat or original if target type matches." << std::endl;
#endif
        if (toNormalize.type() == _targetType)
            return toNormalize.clone(); // Or just pass through if already target type
        cv::Mat result = cv::Mat::zeros(toNormalize.size(), _targetType);
        // If minVal == maxVal, could fill with a specific value e.g. corresponding to minVal
        if (minVal == maxVal)
        {
            double scaledVal = (minVal - minVal) * (_alpha / 1.0) + _beta; // Effectively _beta if range is 0
            if (maxVal - minVal > 0)
            { // Should not happen here, but for safety
                scaledVal = (minVal - minVal) * (_alpha / (maxVal - minVal)) + _beta;
            }
            result.setTo(cv::Scalar::all(cv::saturate_cast<uchar>(scaledVal))); // Assuming CV_8U common target
        }
        return result;
    }

    cv::Mat normalizedMap;
    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "NormalizeDisparityMapItem [" << _id << "]: Using OpenCL for convertTo." << std::endl;
#endif
        cv::UMat u_toNormalize = toNormalize.getUMat(cv::ACCESS_READ);
        cv::UMat u_normalized;
        // Formula: output = (input - minVal) * alpha / (maxVal - minVal) + beta
        u_toNormalize.convertTo(u_normalized, _targetType, _alpha / (maxVal - minVal), _beta - minVal * _alpha / (maxVal - minVal));
        normalizedMap = u_normalized.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "NormalizeDisparityMapItem [" << _id << "]: Using CPU for convertTo." << std::endl;
#endif
        toNormalize.convertTo(normalizedMap, _targetType, _alpha / (maxVal - minVal), _beta - minVal * _alpha / (maxVal - minVal));
    }
    return normalizedMap;
}

// --- ApplyColorMapItem Implementation ---
ApplyColorMapItem::ApplyColorMapItem(uint16_t id, cv::ColormapTypes colormap)
    : PipelineItem(id), _colormap(colormap) {}

cv::Mat ApplyColorMapItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
    {
        std::cerr << "ApplyColorMapItem [" << _id << "]: Warning - Input image is empty." << std::endl;
        return _input;
    }

    cv::Mat inputToColor;
    // applyColorMap expects CV_8UC1.
    // If input is not, attempt conversion.
    if (_input.type() == CV_8UC1)
    {
        inputToColor = _input;
    }
    else if (_input.type() == CV_16S || _input.type() == CV_32F || _input.channels() != 1)
    {
        // If it's a depth map (16S, 32F) or multi-channel, it needs normalization first.
        // This item assumes the input is ALREADY SUITABLE or has been normalized to CV_8U by a previous step.
        // For robustness, we can try a simple normalization here if not 8UC1,
        // but it's better practice to have a dedicated NormalizeDisparityMapItem before this.
#ifdef DEBUG_PIPELINE
        std::cout << "ApplyColorMapItem [" << _id << "]: Input type is " << _input.type()
                  << ". Attempting conversion/normalization to CV_8UC1 for colormapping." << std::endl;
#endif
        cv::Mat temp;
        if (_input.channels() == 3 || _input.channels() == 4)
        {                                                   // Convert color to gray first
            cv::cvtColor(_input, temp, cv::COLOR_BGR2GRAY); // Assuming BGR if 3 channels
        }
        else
        {
            temp = _input.clone(); // Use a clone if single channel but not 8UC1
        }

        // Normalize to 0-255 range and convert to CV_8U
        double minVal, maxVal;
        cv::minMaxLoc(temp, &minVal, &maxVal);
        if (maxVal > minVal)
        {
            temp.convertTo(inputToColor, CV_8UC1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
        }
        else
        {                                          // Handle case where minVal == maxVal (e.g., black image)
            temp.convertTo(inputToColor, CV_8UC1); // Simple conversion, likely results in 0 or a fixed value
        }
    }
    else
    {
        std::cerr << "ApplyColorMapItem [" << _id << "]: Warning - Input image type " << _input.type()
                  << " is not CV_8UC1. Colormapping might fail or produce unexpected results." << std::endl;
        inputToColor = _input; // Proceed, but OpenCV might complain or misinterpret
    }

    if (inputToColor.empty() || inputToColor.type() != CV_8UC1)
    {
        std::cerr << "ApplyColorMapItem [" << _id << "]: Error - Could not prepare a CV_8UC1 input for colormapping." << std::endl;
        return _input; // Return original input on failure to prepare
    }

    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ApplyColorMapItem [" << _id << "]: Using OpenCL path for applyColorMap." << std::endl;
#endif
        cv::UMat u_input = inputToColor.getUMat(cv::ACCESS_READ);
        cv::UMat u_coloredImage;
        cv::applyColorMap(u_input, u_coloredImage, _colormap);
        return u_coloredImage.getMat(cv::ACCESS_READ).clone();
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "ApplyColorMapItem [" << _id << "]: Using CPU for applyColorMap." << std::endl;
#endif
        cv::Mat coloredImage;
        cv::applyColorMap(inputToColor, coloredImage, _colormap);
        return coloredImage;
    }
}

JoinFramesHorizontallyItem::JoinFramesHorizontallyItem(uint16_t id, uint16_t leftCacheKey, uint16_t rightCacheKey, int outputType)
    : PipelineItem(id), _leftImageCacheKey(leftCacheKey), _rightImageCacheKey(rightCacheKey), _outputType(outputType) {}

cv::Mat JoinFramesHorizontallyItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    auto itLeft = matCache.find(_leftImageCacheKey);
    auto itRight = matCache.find(_rightImageCacheKey);

    if (itLeft == matCache.end() || itLeft->second.empty())
    {
        std::cerr << "JoinFramesHorizontallyItem [" << _id << "]: Error - Left image (key "
                  << _leftImageCacheKey << ") not found or empty in cache." << std::endl;
        return _input; // Or return an empty Mat
    }
    if (itRight == matCache.end() || itRight->second.empty())
    {
        std::cerr << "JoinFramesHorizontallyItem [" << _id << "]: Error - Right image (key "
                  << _rightImageCacheKey << ") not found or empty in cache." << std::endl;
        return _input; // Or return an empty Mat
    }

    cv::Mat leftImg = itLeft->second;
    cv::Mat rightImg = itRight->second;

    // --- Prepare images for joining ---
    cv::Mat leftPrepared, rightPrepared;

    // 1. Match Height: Resize the shorter image to match the taller one, or a common height.
    // For simplicity, let's resize the one with smaller height to match the other.
    // A more robust approach might involve resizing both to a predefined target height if needed.
    if (leftImg.rows != rightImg.rows)
    {
#ifdef DEBUG_PIPELINE
        std::cout << "JoinFramesHorizontallyItem [" << _id << "]: Heights mismatch ("
                  << leftImg.rows << " vs " << rightImg.rows << "). Resizing." << std::endl;
#endif
        if (leftImg.rows < rightImg.rows)
        {
            cv::resize(leftImg, leftPrepared, cv::Size(leftImg.cols * rightImg.rows / leftImg.rows, rightImg.rows));
            rightPrepared = rightImg.clone(); // Use clone if rightImg is used directly
        }
        else
        {
            cv::resize(rightImg, rightPrepared, cv::Size(rightImg.cols * leftImg.rows / rightImg.rows, leftImg.rows));
            leftPrepared = leftImg.clone(); // Use clone if leftImg is used directly
        }
    }
    else
    {
        leftPrepared = leftImg.clone(); // Use clones to ensure data independence from cache
        rightPrepared = rightImg.clone();
    }

    // 2. Match Type & Channels (e.g., for display, usually CV_8UC3)
    int targetType = _outputType;
    if (targetType == -1)
    { // If no specific output type, try to make them compatible
        if (leftPrepared.type() != rightPrepared.type())
        {
            // Default to CV_8UC3 if types differ, assuming for visualization
            targetType = CV_8UC3;
#ifdef DEBUG_PIPELINE
            std::cout << "JoinFramesHorizontallyItem [" << _id << "]: Types mismatch ("
                      << leftPrepared.type() << " vs " << rightPrepared.type() << "). Converting to CV_8UC3." << std::endl;
#endif
        }
        else
        {
            targetType = leftPrepared.type(); // Use their common type
        }
    }

    if (leftPrepared.type() != targetType)
    {
        if (leftPrepared.channels() == 1 && (CV_MAT_CN(targetType) == 3 || CV_MAT_CN(targetType) == 4))
        {
            cv::cvtColor(leftPrepared, leftPrepared, cv::COLOR_GRAY2BGR);
        }
        leftPrepared.convertTo(leftPrepared, CV_MAT_DEPTH(targetType)); // Convert depth if needed
        if (leftPrepared.type() != targetType)
        { // Final check, if cvtColor didn't make it BGR for example
            // This part can be tricky if targetType is e.g. CV_8UC3 and input was CV_16UC1
            // A more specific conversion strategy might be needed based on common use cases
            if (CV_MAT_CN(leftPrepared.type()) != CV_MAT_CN(targetType) && CV_MAT_CN(targetType) == 3)
            {
                // Fallback: if it's still single channel and target is 3 channel, try GRAY2BGR
                if (leftPrepared.depth() == CV_8U)
                    cv::cvtColor(leftPrepared, leftPrepared, cv::COLOR_GRAY2BGR);
                else
                { /* More complex case */
                }
            }
        }
    }
    if (rightPrepared.type() != targetType)
    {
        if (rightPrepared.channels() == 1 && (CV_MAT_CN(targetType) == 3 || CV_MAT_CN(targetType) == 4))
        {
            cv::cvtColor(rightPrepared, rightPrepared, cv::COLOR_GRAY2BGR);
        }
        rightPrepared.convertTo(rightPrepared, CV_MAT_DEPTH(targetType));
        if (rightPrepared.type() != targetType)
        {
            if (CV_MAT_CN(rightPrepared.type()) != CV_MAT_CN(targetType) && CV_MAT_CN(targetType) == 3)
            {
                if (rightPrepared.depth() == CV_8U)
                    cv::cvtColor(rightPrepared, rightPrepared, cv::COLOR_GRAY2BGR);
                else
                { /* More complex case */
                }
            }
        }
    }

    // After conversions, re-check height equality, as cvtColor doesn't change size but convertTo might if not handled carefully.
    // (Our resize was done first, so this should ideally still hold unless convertTo had weird effects not intended here)
    if (leftPrepared.rows != rightPrepared.rows)
    {
        std::cerr << "JoinFramesHorizontallyItem [" << _id << "]: Error - Heights became mismatched after type conversion. This is unexpected." << std::endl;
        return _input;
    }
    if (leftPrepared.type() != rightPrepared.type())
    {
        std::cerr << "JoinFramesHorizontallyItem [" << _id << "]: Error - Types still mismatched after conversion attempts ("
                  << leftPrepared.type() << " vs " << rightPrepared.type() << "). Cannot join." << std::endl;
        return _input;
    }

    // --- Perform Joining ---
    cv::Mat joinedImage;
    if (cv::ocl::useOpenCL())
    {
#ifdef DEBUG_PIPELINE
        std::cout << "JoinFramesHorizontallyItem [" << _id << "]: Using OpenCL for hconcat." << std::endl;
#endif
        cv::UMat u_left = leftPrepared.getUMat(cv::ACCESS_READ);
        cv::UMat u_right = rightPrepared.getUMat(cv::ACCESS_READ);
        cv::UMat u_joined;
        try
        {
            cv::hconcat(u_left, u_right, u_joined);
            joinedImage = u_joined.getMat(cv::ACCESS_READ).clone();
        }
        catch (const cv::Exception &e)
        {
            std::cerr << "JoinFramesHorizontallyItem [" << _id << "]: OpenCL hconcat failed: " << e.what() << ". Falling back to CPU." << std::endl;
            cv::hconcat(leftPrepared, rightPrepared, joinedImage); // CPU fallback
        }
    }
    else
    {
#ifdef DEBUG_PIPELINE
        std::cout << "JoinFramesHorizontallyItem [" << _id << "]: Using CPU for hconcat." << std::endl;
#endif
        cv::hconcat(leftPrepared, rightPrepared, joinedImage);
    }

    return joinedImage;
}

// --- DrawTextItem Implementation ---
DrawTextItem::DrawTextItem(uint16_t id, const std::string &text, cv::Point org,
                           cv::HersheyFonts fontFace, double fontScale, cv::Scalar color,
                           int thickness, int lineType, bool bottomLeftOrigin)
    : PipelineItem(id), _text(text), _org(org), _fontFace(fontFace),
      _fontScale(fontScale), _color(color), _thickness(thickness),
      _lineType(lineType), _bottomLeftOrigin(bottomLeftOrigin) {}

cv::Mat DrawTextItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
        return _input;
    cv::Mat outputMat = _input.clone(); // Draw on a copy
    // Ensure output is color if drawing with color
    if (outputMat.channels() == 1 && (_color[0] != _color[1] || _color[0] != _color[2]))
    { // Crude check for non-grayscale color
        cv::cvtColor(outputMat, outputMat, cv::COLOR_GRAY2BGR);
    }
    cv::putText(outputMat, _text, _org, _fontFace, _fontScale, _color, _thickness, _lineType, _bottomLeftOrigin);
    return outputMat;
}

// --- DrawRectangleItem Implementation ---
DrawRectangleItem::DrawRectangleItem(uint16_t id, cv::Rect rect, cv::Scalar color,
                                     int thickness, int lineType, int shift)
    : PipelineItem(id), _rect(rect), _color(color), _thickness(thickness),
      _lineType(lineType), _shift(shift) {}

cv::Mat DrawRectangleItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
        return _input;
    cv::Mat outputMat = _input.clone();
    if (outputMat.channels() == 1 && (_color[0] != _color[1] || _color[0] != _color[2]))
    {
        cv::cvtColor(outputMat, outputMat, cv::COLOR_GRAY2BGR);
    }
    cv::rectangle(outputMat, _rect, _color, _thickness, _lineType, _shift);
    return outputMat;
}

// --- DrawCircleItem Implementation ---
DrawCircleItem::DrawCircleItem(uint16_t id, cv::Point center, int radius, cv::Scalar color,
                               int thickness, int lineType, int shift)
    : PipelineItem(id), _center(center), _radius(radius), _color(color),
      _thickness(thickness), _lineType(lineType), _shift(shift) {}

cv::Mat DrawCircleItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
        return _input;
    cv::Mat outputMat = _input.clone();
    if (outputMat.channels() == 1 && (_color[0] != _color[1] || _color[0] != _color[2]))
    {
        cv::cvtColor(outputMat, outputMat, cv::COLOR_GRAY2BGR);
    }
    cv::circle(outputMat, _center, _radius, _color, _thickness, _lineType, _shift);
    return outputMat;
}

// --- DrawArrowedLineItem Implementation ---
DrawArrowedLineItem::DrawArrowedLineItem(uint16_t id, cv::Point pt1, cv::Point pt2, cv::Scalar color,
                                         int thickness, int line_type, int shift, double tipLength)
    : PipelineItem(id), _pt1(pt1), _pt2(pt2), _color(color), _thickness(thickness),
      _line_type(line_type), _shift(shift), _tipLength(tipLength) {}

cv::Mat DrawArrowedLineItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty())
        return _input;
    cv::Mat outputMat = _input.clone();
    if (outputMat.channels() == 1 && (_color[0] != _color[1] || _color[0] != _color[2]))
    {
        cv::cvtColor(outputMat, outputMat, cv::COLOR_GRAY2BGR);
    }
    cv::arrowedLine(outputMat, _pt1, _pt2, _color, _thickness, _line_type, _shift, _tipLength);
    return outputMat;
}




// --- PredictiveHoleFillingItem Implementation ---

PredictiveHoleFillingItem::PredictiveHoleFillingItem(uint16_t id, int minDisparityValue,
                                                 int minHoleRegionSize, int maxHoleRegionSize,
                                                 int minBoundaryPixelsForFit, int boundarySearchRadius,
                                                 bool enableSimpleFillForSmallHoles, int smallHoleFillNeighborhood,
                                                 bool applyMedianBlur, int medianBlurKernel)
    : PipelineItem(id),
      _minDisparityValue(minDisparityValue),
      _minHoleRegionSize(std::max(1, minHoleRegionSize)),
      _maxHoleRegionSize(std::max(minHoleRegionSize, maxHoleRegionSize)),
      _minBoundaryPixelsForFit(std::max(3, minBoundaryPixelsForFit)), // Need at least 3 for a plane, more for robust avg
      _boundarySearchRadius(std::max(0, boundarySearchRadius)),
      _enableSimpleFillForSmallHoles(enableSimpleFillForSmallHoles),
      _smallHoleFillNeighborhood(smallHoleFillNeighborhood),
      _applyMedianBlurPost(applyMedianBlur),
      _medianBlurKernelSize(medianBlurKernel)
{
    if (_smallHoleFillNeighborhood % 2 == 0 || _smallHoleFillNeighborhood < 3) {
        std::cerr << "PredictiveHoleFillingItem [" << _id << "]: Warning - Small hole fill neighborhood must be odd and >= 3. Setting to 3." << std::endl;
        _smallHoleFillNeighborhood = 3;
    }
    if (_medianBlurKernelSize % 2 == 0 || _medianBlurKernelSize < 3) {
        std::cerr << "PredictiveHoleFillingItem [" << _id << "]: Warning - Median blur kernel size must be odd and >= 3. Setting to 3." << std::endl;
        _medianBlurKernelSize = 3;
    }
     if (_boundarySearchRadius > 0 && _boundarySearchRadius < 3) {
         std::cerr << "PredictiveHoleFillingItem [" << _id << "]: Warning - Non-zero boundarySearchRadius should ideally be >=3. Using " << _boundarySearchRadius << "." << std::endl;
    }
}

void PredictiveHoleFillingItem::simpleLocalFill(cv::Mat& map, const cv::Point& holePixel, int neighborhoodSize, short scaledMinValidDisparity) {
    std::vector<short> neighbors;
    int halfSize = neighborhoodSize / 2;
    for (int dr = -halfSize; dr <= halfSize; ++dr) {
        for (int dc = -halfSize; dc <= halfSize; ++dc) {
            if (dr == 0 && dc == 0) continue;
            cv::Point npt = holePixel + cv::Point(dc, dr);
            if (npt.x >= 0 && npt.x < map.cols && npt.y >= 0 && npt.y < map.rows) {
                short val = map.at<short>(npt);
                if (val >= scaledMinValidDisparity) {
                    neighbors.push_back(val);
                }
            }
        }
    }
    if (!neighbors.empty()) {
        std::sort(neighbors.begin(), neighbors.end());
        map.at<short>(holePixel) = neighbors[neighbors.size() / 2]; // Median
    }
}


cv::Mat PredictiveHoleFillingItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty()) {
        std::cerr << "PredictiveHoleFillingItem [" << _id << "]: Warning - Input disparity map is empty." << std::endl;
        return _input;
    }
    if (_input.type() != CV_16S) {
        std::cerr << "PredictiveHoleFillingItem [" << _id << "]: Error - Input map must be CV_16S. Type: " << _input.type() << std::endl;
        return _input;
    }

    cv::Mat filledMap = _input.clone();
    short scaledMinValidDisparity = static_cast<short>(_minDisparityValue * 16);
    const float epsilon = 1e-6f;

#ifdef DEBUG_PIPELINE
    std::cout << "PredictiveHoleFillingItem [" << _id << "]: Filling holes. MinValidDisp (scaled): " << scaledMinValidDisparity
              << ", MinRegionSize: " << _minHoleRegionSize << ", MaxRegionSize: " << _maxHoleRegionSize
              << ", MinBoundaryForFit: " << _minBoundaryPixelsForFit << ", BoundarySearchRadius: " << _boundarySearchRadius
              << ", SimpleFillSmall: " << _enableSimpleFillForSmallHoles << " (Nh: " << _smallHoleFillNeighborhood << ")"
              << ", MedianBlurPost: " << _applyMedianBlurPost << " (Kernel: " << _medianBlurKernelSize << ")"
              << std::endl;
#endif

    cv::Mat holeMask(filledMap.size(), CV_8U);
    for (int r = 0; r < filledMap.rows; ++r) {
        for (int c = 0; c < filledMap.cols; ++c) {
            if (filledMap.at<short>(r, c) < scaledMinValidDisparity) {
                holeMask.at<uchar>(r, c) = 255; // Mark as hole
            } else {
                holeMask.at<uchar>(r, c) = 0;   // Mark as valid
            }
        }
    }

    cv::Mat labels, stats, centroids;
    int numLabels = cv::connectedComponentsWithStats(holeMask, labels, stats, centroids, 8, CV_32S);

    std::vector<HoleRegionInfo> holeRegions(numLabels);

    // Collect hole region pixels and identify boundary pixels
    for (int r = 0; r < filledMap.rows; ++r) {
        for (int c = 0; c < filledMap.cols; ++c) {
            int label = labels.at<int>(r, c);
            if (label > 0) { // label 0 is the background (valid pixels)
                if(holeRegions[label].pixels.empty()) { // First time seeing this label
                    holeRegions[label].id = label;
                    holeRegions[label].boundingBox = cv::Rect(
                        stats.at<int>(label, cv::CC_STAT_LEFT),
                        stats.at<int>(label, cv::CC_STAT_TOP),
                        stats.at<int>(label, cv::CC_STAT_WIDTH),
                        stats.at<int>(label, cv::CC_STAT_HEIGHT)
                    );
                }
                holeRegions[label].pixels.push_back(cv::Point(c, r));

                // Check 8-connectivity neighbors for boundary pixels
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = -1; dc <= 1; ++dc) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = r + dr;
                        int nc = c + dc;
                        if (nr >= 0 && nr < filledMap.rows && nc >= 0 && nc < filledMap.cols) {
                            if (labels.at<int>(nr, nc) == 0) { // Neighbor is a valid pixel
                                // Ensure not already added (simple check, could use std::set for perfect uniqueness)
                                bool found = false;
                                for(const auto& bp : holeRegions[label].boundaryPixels) {
                                    if(bp.x == nc && bp.y == nr) {found = true; break;}
                                }
                                if(!found) holeRegions[label].boundaryPixels.push_back(cv::Point(nc, nr));
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Process each hole region
    for (int i = 1; i < numLabels; ++i) { // Start from 1, label 0 is background
        HoleRegionInfo& region = holeRegions[i];
        int regionSize = stats.at<int>(i, cv::CC_STAT_AREA);

        if (regionSize == 0 || region.pixels.empty()) continue; // Should not happen if stats are correct

        if (regionSize > _maxHoleRegionSize) {
#ifdef DEBUG_PIPELINE
            std::cout << "PredictiveHoleFillingItem [" << _id << "]: Skipping large hole region " << i << " (size: " << regionSize << ")" << std::endl;
#endif
            continue; 
        }

        if (_enableSimpleFillForSmallHoles && regionSize < _minHoleRegionSize) {
            for (const auto& holePx : region.pixels) {
                simpleLocalFill(filledMap, holePx, _smallHoleFillNeighborhood, scaledMinValidDisparity);
            }
            continue; 
        }

        if (region.boundaryPixels.size() < static_cast<size_t>(_minBoundaryPixelsForFit)) {
#ifdef DEBUG_PIPELINE
            std::cout << "PredictiveHoleFillingItem [" << _id << "]: Skipping region " << i << " due to insufficient boundary pixels (" 
                      << region.boundaryPixels.size() << "/" << _minBoundaryPixelsForFit << ")" << std::endl;
#endif
            // Optionally, could apply simpleLocalFill here as a fallback
            if (_enableSimpleFillForSmallHoles) { // Re-use this flag for fallback on failed fit
                 for (const auto& holePx : region.pixels) {
                    simpleLocalFill(filledMap, holePx, _smallHoleFillNeighborhood, scaledMinValidDisparity);
                }
            }
            continue;
        }

        // --- Main Predictive Fill Logic ---
        // For simplicity and "faster compute", we'll implement a weighted average of boundary pixels.
        // A true plane fit per region is more complex.
        // If _boundarySearchRadius > 0, we do a local weighted average for each hole pixel.
        // If _boundarySearchRadius == 0, we use all boundary pixels of the region for each hole pixel (less local).

        for (const auto& holePx : region.pixels) {
            double totalWeight = 0.0;
            double weightedSum = 0.0;
            int contributingBoundaryCount = 0;

            const std::vector<cv::Point>& boundarySet = region.boundaryPixels;

            for (const auto& boundaryPx : boundarySet) {
                double distSq = std::pow(holePx.x - boundaryPx.x, 2) + std::pow(holePx.y - boundaryPx.y, 2);
                
                if (_boundarySearchRadius > 0 && distSq > std::pow(_boundarySearchRadius,2) ) {
                    continue; // Boundary pixel is outside the local search radius for this hole pixel
                }

                short dispVal = filledMap.at<short>(boundaryPx); // Boundary pixels are valid by definition here
                // (though filledMap might have been modified by previous small hole fills, so re-check could be added if strict)
                if (dispVal < scaledMinValidDisparity) continue; // Should not happen if boundary logic is correct for initial state

                double weight = 1.0 / (distSq + epsilon);
                weightedSum += static_cast<double>(dispVal) * weight;
                totalWeight += weight;
                contributingBoundaryCount++;
            }

            if (contributingBoundaryCount > 0 && totalWeight > epsilon) {
                 // Clamp the fill value to be within a reasonable range of its contributing neighbors
                short minNeighborDisp = -1, maxNeighborDisp = -1;
                if (_boundarySearchRadius == 0 && !boundarySet.empty()){ // Global boundary set
                     minNeighborDisp = filledMap.at<short>(boundarySet[0]); maxNeighborDisp = minNeighborDisp;
                    for(const auto& bp : boundarySet) {
                        short val = filledMap.at<short>(bp);
                        if(val < minNeighborDisp) minNeighborDisp = val;
                        if(val > maxNeighborDisp) maxNeighborDisp = val;
                    }
                }
                // For local, clamping is harder without re-iterating contributing neighbors.
                // For now, a simple global min/max based on all boundary pixels for the region.

                short filledValue = static_cast<short>(std::round(weightedSum / totalWeight));
                
                // Basic clamping against overall min/max disparity seen on boundary (if available)
                // This prevents wild extrapolations if weights lead to it.
                if (minNeighborDisp != -1 && maxNeighborDisp != -1) { // If calculated
                     filledValue = std::max(minNeighborDisp, std::min(filledValue, maxNeighborDisp));
                }
                // More robust clamping: ensure it's not less than min valid overall
                filledValue = std::max(scaledMinValidDisparity, filledValue);

                filledMap.at<short>(holePx) = filledValue;
            } else {
                // Fallback if no contributing boundary pixels found for this specific hole pixel (e.g. due to radius)
                // Or if it's a very complex hole.
                // Could leave it, or apply simpleLocalFill. For now, leave it or rely on prior small hole fill.
                if (_enableSimpleFillForSmallHoles) { // Re-use flag
                     simpleLocalFill(filledMap, holePx, _smallHoleFillNeighborhood, scaledMinValidDisparity);
                }
            }
        }
    }

    // Final Median Blur (Optional)
    if (_applyMedianBlurPost) {
        if (filledMap.rows > 0 && filledMap.cols > 0) {
             cv::medianBlur(filledMap, filledMap, _medianBlurKernelSize);
        } else {
            std::cerr << "PredictiveHoleFillingItem [" << _id << "]: Warning - Map is empty before median blur, skipping." << std::endl;
        }
    }

    return filledMap;
}

// --- DHoleFillingItem Implementation ---
DHoleFillingItem::DHoleFillingItem(uint16_t id, int minDisparityValue,
                                   bool propagateLtoR, bool propagateRtoL,
                                   bool propagateTLtoBR, bool propagateBRtoTL,
                                   bool applyMedianBlur, int medianBlurKernel)
    : PipelineItem(id),
      _minDisparityValue(minDisparityValue),
      _propagateLtoR(propagateLtoR),
      _propagateRtoL(propagateRtoL),
      _propagateTLtoBR(propagateTLtoBR),
      _propagateBRtoTL(propagateBRtoTL),
      _applyMedianBlurPost(applyMedianBlur),
      _medianBlurKernelSize(medianBlurKernel)
{
    if (_medianBlurKernelSize % 2 == 0 || _medianBlurKernelSize < 3) {
        std::cerr << "DHoleFillingItem [" << _id << "]: Warning - Median blur kernel size must be odd and >= 3. Setting to 3." << std::endl;
        _medianBlurKernelSize = 3;
    }
}

cv::Mat DHoleFillingItem::forward(cv::Mat _input, std::map<uint16_t, cv::Mat> &matCache)
{
    if (_input.empty()) {
        std::cerr << "DHoleFillingItem [" << _id << "]: Warning - Input disparity map is empty." << std::endl;
        return _input;
    }
    if (_input.type() != CV_16S) {
        std::cerr << "DHoleFillingItem [" << _id << "]: Error - Input map must be CV_16S. Type: " << _input.type() << std::endl;
        return _input;
    }

    cv::Mat filledMap = _input.clone();
    short scaledMinValidDisparity = static_cast<short>(_minDisparityValue * 16);
    int rows = filledMap.rows;
    int cols = filledMap.cols;

#ifdef DEBUG_PIPELINE
    std::cout << "DHoleFillingItem [" << _id << "]: Filling holes. MinValidDisp (scaled): " << scaledMinValidDisparity
              << ", LtoR: " << _propagateLtoR << ", RtoL: " << _propagateRtoL
              << ", TLtoBR: " << _propagateTLtoBR << ", BRtoTL: " << _propagateBRtoTL
              << ", MedianBlurPost: " << _applyMedianBlurPost << " (Kernel: " << _medianBlurKernelSize << ")"
              << std::endl;
#endif

    // Pass 1: Left-to-Right (Horizontal)
    if (_propagateLtoR) {
        for (int r = 0; r < rows; ++r) {
            short lastValidDisp = -1; // Sentinel for no valid disparity found yet
            for (int c = 0; c < cols; ++c) {
                short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity) {
                    lastValidDisp = currentDisp;
                } else { // Hole
                    if (lastValidDisp != -1) {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
    }

    // Pass 2: Right-to-Left (Horizontal)
    if (_propagateRtoL) {
        for (int r = 0; r < rows; ++r) {
            short lastValidDisp = -1;
            for (int c = cols - 1; c >= 0; --c) {
                short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity) {
                    lastValidDisp = currentDisp;
                } else { // Hole
                    if (lastValidDisp != -1) {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
    }

    // Pass 3: Top-Left to Bottom-Right (Diagonals: r-c = const)
    // More accurately, this is propagating "downwards and rightwards"
    // We iterate through all possible starting points on the top and left edges.
    if (_propagateTLtoBR) {
        // Diagonals starting from top row (r=0, c=0 to cols-1)
        for (int start_c = 0; start_c < cols; ++start_c) {
            short lastValidDisp = -1;
            for (int r = 0, c = start_c; r < rows && c < cols; ++r, ++c) {
                short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity) {
                    lastValidDisp = currentDisp;
                } else {
                    if (lastValidDisp != -1) {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
        // Diagonals starting from left column (c=0, r=1 to rows-1, as r=0,c=0 is covered)
        for (int start_r = 1; start_r < rows; ++start_r) {
            short lastValidDisp = -1;
            for (int r = start_r, c = 0; r < rows && c < cols; ++r, ++c) {
                 short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity) {
                    lastValidDisp = currentDisp;
                } else {
                    if (lastValidDisp != -1) {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
    }

    // Pass 4: Bottom-Right to Top-Left (Diagonals: r-c = const, but propagating backwards)
    // This is propagating "upwards and leftwards"
    // We iterate through all possible starting points on the bottom and right edges.
    if (_propagateBRtoTL) {
        // Diagonals starting from bottom row (r=rows-1, c=0 to cols-1)
        for (int start_c = 0; start_c < cols; ++start_c) {
            short lastValidDisp = -1;
            for (int r = rows - 1, c = start_c; r >= 0 && c >= 0; --r, --c) {
                short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity) {
                    lastValidDisp = currentDisp;
                } else {
                    if (lastValidDisp != -1) {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
        // Diagonals starting from right column (c=cols-1, r=0 to rows-2, as r=rows-1,c=cols-1 is covered)
        for (int start_r = 0; start_r < rows - 1; ++start_r) { // rows-2 because rows-1 is covered
            short lastValidDisp = -1;
            for (int r = start_r, c = cols - 1; r >= 0 && c >= 0; --r, --c) {
                short currentDisp = filledMap.at<short>(r, c);
                if (currentDisp >= scaledMinValidDisparity) {
                    lastValidDisp = currentDisp;
                } else {
                    if (lastValidDisp != -1) {
                        filledMap.at<short>(r, c) = lastValidDisp;
                    }
                }
            }
        }
    }

    // Note: The two diagonal passes above cover one set of diagonals (r-c=const) in both directions.
    // To cover the other set of diagonals (r+c=const), we'd need:
    // Pass 5: Top-Right to Bottom-Left (TR-BL)
    // Pass 6: Bottom-Left to Top-Right (BL-TR)
    // For simplicity and to keep it to 4 main directions, the current implementation is a common approach.
    // If you want true 8-directional propagation, you'd add these.
    // Let's stick to the 4 passes (2 horizontal, 2 along one diagonal orientation but opposite directions of propagation)
    // as defined by the bool flags for now. The naming _propagateTLtoBR and _propagateBRtoTL implies this.

    // Optional Median Blur
    if (_applyMedianBlurPost) {
        if (filledMap.rows > 0 && filledMap.cols > 0) {
             cv::medianBlur(filledMap, filledMap, _medianBlurKernelSize);
        } else {
            std::cerr << "DHoleFillingItem [" << _id << "]: Warning - Map is empty before median blur, skipping." << std::endl;
        }
    }

    return filledMap;
}
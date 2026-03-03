/*
 * gpu_stabilization_items.cpp
 *
 * OpenCL-accelerated horizon lock and full 2-D video stabilization.
 *
 * GPU dispatch strategy:
 *   - goodFeaturesToTrack  → OpenCV T-API OpenCL (UMat)
 *   - calcOpticalFlowPyrLK → OpenCV T-API OpenCL (UMat)
 *   - estimateAffinePartial2D → CPU (small point set, negligible)
 *   - warpAffine            → native OpenCL kernel (primary),
 *                              T-API cv::warpAffine(UMat) (fallback),
 *                              CPU cv::warpAffine (final fallback).
 */

#include "interpreter/items/gpu_stabilization_items.h"
#include <cmath>
#include <iostream>
#include <algorithm>

#ifdef VISIONPIPE_OPENCL_ENABLED
#include <opencv2/core/ocl.hpp>
#endif

namespace visionpipe {

// ============================================================================
// Native OpenCL warp kernel — affine warp with BORDER_REPLICATE
//
// Performs   dst(x,y) = src( M_inv * [x, y, 1]^T )
// where M_inv is the 2×3 inverse-affine matrix, bilinear interpolation,
// with border-replicate (clamp coordinates to [0, w-1] / [0, h-1]).
//
// This avoids UMat upload/download overhead on Mali/embedded SoCs where the
// T-API cv::warpAffine path serialises through cl_mem mapping.
// ============================================================================
#ifdef VISIONPIPE_OPENCL_ENABLED

static const char GPU_WARP_AFFINE_CL[] = R"CL(
/*
 * Affine warp with bilinear interpolation and border-replicate.
 *
 * Works on 8U images with 1, 3, or 4 channels.
 * M is a 6-element float array: the INVERSE affine matrix
 *   [m0  m1  m2]    →  src_x = m0*dst_x + m1*dst_y + m2
 *   [m3  m4  m5]    →  src_y = m3*dst_x + m4*dst_y + m5
 *
 * Argument layout:
 *   ReadOnly(src)   → ptr, step, offset, rows, cols
 *   WriteOnly(dst)  → ptr, step, offset, rows, cols
 *   PtrReadOnly(M)  → ptr  (6 floats)
 *   channels        → int
 */
__kernel void gpu_warp_affine_replicate(
    __global const uchar* src,  int src_step,  int src_offset,
    int src_rows, int src_cols,
    __global       uchar* dst,  int dst_step,  int dst_offset,
    int dst_rows, int dst_cols,
    __global const float* M,
    int channels)
{
    const int dx = get_global_id(0);
    const int dy = get_global_id(1);
    if (dx >= dst_cols || dy >= dst_rows) return;

    /* Compute source coordinate via inverse affine */
    float sx = M[0] * (float)dx + M[1] * (float)dy + M[2];
    float sy = M[3] * (float)dx + M[4] * (float)dy + M[5];

    /* Bilinear interpolation with border-replicate (clamp) */
    int x0 = (int)floor(sx);
    int y0 = (int)floor(sy);
    float fx = sx - (float)x0;
    float fy = sy - (float)y0;

    /* Clamp to image bounds (replicate border) */
    int x0c = clamp(x0,     0, src_cols - 1);
    int x1c = clamp(x0 + 1, 0, src_cols - 1);
    int y0c = clamp(y0,     0, src_rows - 1);
    int y1c = clamp(y0 + 1, 0, src_rows - 1);

    float w00 = (1.0f - fx) * (1.0f - fy);
    float w01 = fx           * (1.0f - fy);
    float w10 = (1.0f - fx) * fy;
    float w11 = fx           * fy;

    __global const uchar* r00 = src + src_offset + (size_t)y0c * src_step + x0c * channels;
    __global const uchar* r01 = src + src_offset + (size_t)y0c * src_step + x1c * channels;
    __global const uchar* r10 = src + src_offset + (size_t)y1c * src_step + x0c * channels;
    __global const uchar* r11 = src + src_offset + (size_t)y1c * src_step + x1c * channels;

    __global uchar* dp = dst + dst_offset + (size_t)dy * dst_step + dx * channels;

    for (int c = 0; c < channels; c++) {
        float v = w00 * (float)r00[c]
                + w01 * (float)r01[c]
                + w10 * (float)r10[c]
                + w11 * (float)r11[c];
        dp[c] = convert_uchar_sat(v + 0.5f);
    }
}
)CL";

#endif // VISIONPIPE_OPENCL_ENABLED

// ============================================================================
// Registration
// ============================================================================

void registerGpuStabilizationItems(ItemRegistry& registry) {
    registry.add<GpuHorizonLockItem>();
    registry.add<GpuVideoStabilizeItem>();
}

// ============================================================================
// Shared helpers
// ============================================================================

/// Detect Shi-Tomasi corners (shared by both items)
static std::vector<cv::Point2f> _detectPts(const cv::Mat& gray,
                                            int maxPoints,
                                            double quality,
                                            double minDist) {
    std::vector<cv::Point2f> pts;
    cv::goodFeaturesToTrack(gray, pts, maxPoints, quality, minDist,
                            cv::noArray(), 3, false);
    return pts;
}

#ifdef VISIONPIPE_OPENCL_ENABLED
/// Optical flow via UMat (T-API OpenCL path)
static void _opticalFlowGpu(const cv::UMat& prevU, const cv::UMat& currU,
                             const std::vector<cv::Point2f>& prevPts,
                             std::vector<cv::Point2f>& currPts,
                             std::vector<uchar>& status,
                             std::vector<float>& err) {
    // OpenCV's T-API dispatches LK to OpenCL when inputs are UMat-backed
    cv::calcOpticalFlowPyrLK(
        prevU, currU,
        prevPts, currPts,
        status, err,
        cv::Size(21, 21), 3,
        cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01)
    );
}

/// Invert the forward 2×3 affine matrix for the native warp kernel.
/// M_fwd = [a  b  tx]   →  M_inv = (1/det) [d  -b  (b*ty - d*tx)]
///         [c  d  ty]                       [-c  a  (c*tx - a*ty)]
static void invertAffine2x3(const double* fwd, float* inv) {
    double a = fwd[0], b = fwd[1], tx = fwd[2];
    double c = fwd[3], d = fwd[4], ty = fwd[5];
    double det = a * d - b * c;
    if (std::abs(det) < 1e-12) det = 1e-12;  // degenerate guard
    double idet = 1.0 / det;
    inv[0] = static_cast<float>( d * idet);
    inv[1] = static_cast<float>(-b * idet);
    inv[2] = static_cast<float>((b * ty - d * tx) * idet);
    inv[3] = static_cast<float>(-c * idet);
    inv[4] = static_cast<float>( a * idet);
    inv[5] = static_cast<float>((c * tx - a * ty) * idet);
}

/// Build the forward affine matrix for rotation+scale+translate about a centre
static cv::Mat buildFullAffine(double corrA, double corrX, double corrY,
                                double scale, double cx, double cy) {
    double cosA = std::cos(corrA);
    double sinA = std::sin(corrA);
    cv::Mat M = (cv::Mat_<double>(2, 3) <<
        cosA, -sinA,  (1.0 - cosA) * cx + sinA * cy + corrX,
        sinA,  cosA, -sinA * cx + (1.0 - cosA) * cy + corrY
    );
    if (scale > 1.0 + 1e-4) {
        M.at<double>(0, 0) *= scale;
        M.at<double>(0, 1) *= scale;
        M.at<double>(1, 0) *= scale;
        M.at<double>(1, 1) *= scale;
        M.at<double>(0, 2) = scale * M.at<double>(0, 2) + (1.0 - scale) * cx;
        M.at<double>(1, 2) = scale * M.at<double>(1, 2) + (1.0 - scale) * cy;
    }
    return M;
}

#endif // VISIONPIPE_OPENCL_ENABLED

/// Build crop scale (no #ifdef needed)
static double computeCropScaleCpu(double absAngle, double W, double H) {
    double sinA = std::sin(absAngle);
    double cosA = std::cos(absAngle);
    double scaleW = cosA + (H / W) * sinA;
    double scaleH = cosA + (W / H) * sinA;
    double scale = std::max(scaleW, scaleH);
    return std::min(scale, 1.5);
}

/// Filter out bad optical-flow matches
static void filterGood(const std::vector<cv::Point2f>& prevPts,
                        const std::vector<cv::Point2f>& currPts,
                        const std::vector<uchar>& status,
                        std::vector<cv::Point2f>& goodPrev,
                        std::vector<cv::Point2f>& goodCurr) {
    goodPrev.clear();
    goodCurr.clear();
    goodPrev.reserve(prevPts.size());
    goodCurr.reserve(currPts.size());
    for (size_t i = 0; i < status.size(); ++i) {
        if (status[i]) {
            goodPrev.push_back(prevPts[i]);
            goodCurr.push_back(currPts[i]);
        }
    }
}

/// RANSAC rotation estimation from point pairs.  Returns raw delta in radians.
/// Also optionally outputs translation (dx, dy).
static double estimateRotation(const std::vector<cv::Point2f>& goodPrev,
                                const std::vector<cv::Point2f>& goodCurr,
                                double* outDx = nullptr,
                                double* outDy = nullptr) {
    double rawDa = 0.0, rawDx = 0.0, rawDy = 0.0;
    if (goodPrev.size() >= 4) {
        cv::Mat affine = cv::estimateAffinePartial2D(
            goodPrev, goodCurr, cv::noArray(), cv::RANSAC, 2.0);
        if (!affine.empty()) {
            rawDa = std::atan2(affine.at<double>(1, 0), affine.at<double>(0, 0));
            rawDx = affine.at<double>(0, 2);
            rawDy = affine.at<double>(1, 2);
            const double kMaxA  = 5.0 * CV_PI / 180.0;
            if (std::abs(rawDa) > kMaxA) rawDa = 0.0;
        }
    }
    if (outDx) *outDx = rawDx;
    if (outDy) *outDy = rawDy;
    return rawDa;
}

// ============================================================================
// GpuHorizonLockItem
// ============================================================================

GpuHorizonLockItem::GpuHorizonLockItem() {
    _functionName = "gpu_horizon_lock";
    _description  = "GPU-accelerated horizon lock (rotation-only stabilization). "
                    "Same algorithm as horizon_lock() but with feature detection, "
                    "optical flow, and the corrective warp dispatched through OpenCL "
                    "(native kernel with T-API fallback). Falls back to CPU if "
                    "OpenCL is unavailable.";
    _category = "stabilization";
    _params = {
        ParamDef::optional("delta_smooth",  BaseType::FLOAT, "EMA for delta noise filter [0,1) (default 0.5)", 0.5),
        ParamDef::optional("drift_decay",   BaseType::FLOAT, "Per-frame angle decay (0,1] (default 0.997)", 0.997),
        ParamDef::optional("max_points",    BaseType::INT,   "Max Shi-Tomasi corners (default 200)", 200),
        ParamDef::optional("quality",       BaseType::FLOAT, "Shi-Tomasi quality (default 0.01)", 0.01),
        ParamDef::optional("min_distance",  BaseType::FLOAT, "Min corner distance in px (default 10)", 10.0),
        ParamDef::optional("crop",          BaseType::BOOL,  "Scale-crop borders (default true)", true),
        ParamDef::optional("reset",         BaseType::BOOL,  "Reset state (default false)", false)
    };
    _example    = "video_cap(\"/dev/video0\") | gpu_horizon_lock()\n"
                  "video_cap(\"/dev/video0\") | gpu_horizon_lock(0.5, 0.997, 300)";
    _returnType = "mat";
    _tags       = {"gpu", "opencl", "stabilization", "horizon", "rotation", "lock", "steady"};
}

std::vector<cv::Point2f> GpuHorizonLockItem::detectPoints(
    const cv::Mat& gray, int maxPoints, double quality, double minDist) {
    return _detectPts(gray, maxPoints, quality, minDist);
}

#ifdef VISIONPIPE_OPENCL_ENABLED
cv::ocl::Program GpuHorizonLockItem::getWarpProgram(bool verbose) {
    std::lock_guard<std::mutex> lk(_warpMutex);
    if (_warpBuilt) return _warpProgram;
    _warpBuilt = true;
    cv::ocl::ProgramSource src(GPU_WARP_AFFINE_CL);
    cv::String log;
    _warpProgram = cv::ocl::Program(src, "", log);
    _warpOk = (_warpProgram.ptr() != nullptr);
    if (!_warpOk)
        std::cerr << "[GPU-HORIZON-LOCK] Warp kernel compile failed:\n" << log << std::endl;
    else if (verbose)
        std::cout << "[GPU-HORIZON-LOCK] Native warp kernel compiled" << std::endl;
    return _warpProgram;
}
#endif

ExecutionResult GpuHorizonLockItem::execute(const std::vector<RuntimeValue>& args,
                                             ExecutionContext& ctx) {
    if (ctx.currentMat.empty()) return ExecutionResult::ok(ctx.currentMat);

    // ---- args ----
    double deltaSmooth = args.size() > 0 ? args[0].asNumber() : 0.5;
    double driftDecay  = args.size() > 1 ? args[1].asNumber() : 0.997;
    int    maxPoints   = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 200;
    double quality     = args.size() > 3 ? args[3].asNumber() : 0.01;
    double minDistance = args.size() > 4 ? args[4].asNumber() : 10.0;
    bool   crop        = args.size() > 5 ? args[5].asBool()   : true;
    bool   reset       = args.size() > 6 ? args[6].asBool()   : false;

    deltaSmooth = std::max(0.0, std::min(deltaSmooth, 0.99));
    driftDecay  = std::max(0.9, std::min(driftDecay,  1.0));
    maxPoints   = std::max(10,  std::min(maxPoints,   2000));

    if (reset) {
        _prevGray.release();
        _prevPts.clear();
        _accumulatedAngle = 0.0;
        _smoothedDelta    = 0.0;
        _initialised      = false;
    }

    // ---- detect OpenCL availability once ----
    bool useOCL = false;
#ifdef VISIONPIPE_OPENCL_ENABLED
    try { useOCL = cv::ocl::useOpenCL() && cv::ocl::haveOpenCL(); } catch (...) {}
#endif

    // ---- grayscale ----
    cv::Mat gray;
    if (ctx.currentMat.channels() == 1)
        gray = ctx.currentMat;
    else
        cv::cvtColor(ctx.currentMat, gray, cv::COLOR_BGR2GRAY);

    // ---- first frame ----
    if (!_initialised || _prevGray.empty()) {
        _prevGray    = gray.clone();
        _prevPts     = detectPoints(gray, maxPoints, quality, minDistance);
        _initialised = true;
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ---- refresh sparse set ----
    if (static_cast<int>(_prevPts.size()) < maxPoints / 4)
        _prevPts = detectPoints(_prevGray, maxPoints, quality, minDistance);

    if (_prevPts.empty()) {
        _prevGray = gray.clone();
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ---- optical flow ----
    std::vector<cv::Point2f> currPts;
    std::vector<uchar>       status;
    std::vector<float>       err;

#ifdef VISIONPIPE_OPENCL_ENABLED
    if (useOCL) {
        _prevGray.copyTo(_prevGrayU);
        gray.copyTo(_currGrayU);
        _opticalFlowGpu(_prevGrayU, _currGrayU, _prevPts, currPts, status, err);
    } else
#endif
    {
        cv::calcOpticalFlowPyrLK(
            _prevGray, gray, _prevPts, currPts,
            status, err,
            cv::Size(21, 21), 3,
            cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01));
    }

    std::vector<cv::Point2f> goodPrev, goodCurr;
    filterGood(_prevPts, currPts, status, goodPrev, goodCurr);

    // ---- rotation estimation (CPU — tiny point set) ----
    double rawDelta = estimateRotation(goodPrev, goodCurr);

    // ---- accumulate ----
    _smoothedDelta    = deltaSmooth * _smoothedDelta + (1.0 - deltaSmooth) * rawDelta;
    _accumulatedAngle += _smoothedDelta;
    _accumulatedAngle *= driftDecay;

    _prevGray = gray.clone();
    _prevPts  = goodCurr;

    // ---- correction ----
    const double corrAngle = -_accumulatedAngle;
    if (std::abs(corrAngle) < 1e-7)
        return ExecutionResult::ok(ctx.currentMat);

    const double W = ctx.currentMat.cols;
    const double H = ctx.currentMat.rows;
    const double cx = W * 0.5, cy = H * 0.5;
    const double angleDeg = corrAngle * (180.0 / CV_PI);
    double scale = crop ? computeCropScaleCpu(std::abs(corrAngle), W, H) : 1.0;

    cv::Mat rotMat = cv::getRotationMatrix2D(
        cv::Point2f(static_cast<float>(cx), static_cast<float>(cy)),
        angleDeg, scale);

    // ---- GPU warp ----
#ifdef VISIONPIPE_OPENCL_ENABLED
    if (useOCL && ctx.currentMat.depth() == CV_8U) {
        cv::ocl::Program prog = getWarpProgram(ctx.verbose);
        if (_warpOk) {
            try {
                const int rows = ctx.currentMat.rows;
                const int cols = ctx.currentMat.cols;
                const int ch   = ctx.currentMat.channels();

                ctx.currentMat.copyTo(_srcU);
                _dstU.create(rows, cols, ctx.currentMat.type());

                // Invert the forward affine for the kernel
                float invM[6];
                double fwd[6] = {
                    rotMat.at<double>(0,0), rotMat.at<double>(0,1), rotMat.at<double>(0,2),
                    rotMat.at<double>(1,0), rotMat.at<double>(1,1), rotMat.at<double>(1,2)
                };
                invertAffine2x3(fwd, invM);

                cv::Mat invMat(1, 6, CV_32F, invM);
                cv::UMat invMU;
                invMat.copyTo(invMU);

                cv::ocl::Kernel kernel("gpu_warp_affine_replicate", prog);
                if (kernel.empty())
                    throw cv::Exception(cv::Error::StsError,
                        "gpu_warp_affine_replicate kernel missing",
                        __func__, __FILE__, __LINE__);

                int arg = 0;
                arg = kernel.set(arg, cv::ocl::KernelArg::ReadOnly(_srcU));
                arg = kernel.set(arg, cv::ocl::KernelArg::WriteOnly(_dstU));
                arg = kernel.set(arg, cv::ocl::KernelArg::PtrReadOnly(invMU));
                arg = kernel.set(arg, ch);

                size_t lsz[2] = {16, 16};
                size_t gsz[2] = {
                    (size_t)(((cols + 15) / 16) * 16),
                    (size_t)(((rows + 15) / 16) * 16)
                };
                if (!kernel.run(2, gsz, lsz, false))
                    throw cv::Exception(cv::Error::StsError,
                        "kernel.run() failed", __func__, __FILE__, __LINE__);

                cv::Mat result;
                _dstU.copyTo(result);
                if (ctx.verbose)
                    std::cout << "[GPU-HORIZON-LOCK] Native OCL warp "
                              << cols << "x" << rows << " " << ch << "ch" << std::endl;
                return ExecutionResult::ok(result);

            } catch (const cv::Exception& e) {
                if (ctx.verbose)
                    std::cout << "[GPU-HORIZON-LOCK] Native warp failed: "
                              << e.what() << " — trying T-API" << std::endl;
            }
        }

        // T-API fallback: UMat warpAffine
        try {
            ctx.currentMat.copyTo(_srcU);
            cv::UMat rotMatU;
            rotMat.copyTo(rotMatU);
            cv::warpAffine(_srcU, _dstU, rotMat,
                           ctx.currentMat.size(),
                           cv::INTER_LINEAR,
                           cv::BORDER_REPLICATE);
            cv::Mat result;
            _dstU.copyTo(result);
            if (ctx.verbose)
                std::cout << "[GPU-HORIZON-LOCK] T-API warp" << std::endl;
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            if (ctx.verbose)
                std::cout << "[GPU-HORIZON-LOCK] T-API warp failed: "
                          << e.what() << " — CPU fallback" << std::endl;
        }
    }
#endif

    // CPU fallback
    cv::Mat result;
    cv::warpAffine(ctx.currentMat, result, rotMat,
                   ctx.currentMat.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REPLICATE);
    return ExecutionResult::ok(result);
}

// ============================================================================
// GpuVideoStabilizeItem
// ============================================================================

GpuVideoStabilizeItem::GpuVideoStabilizeItem() {
    _functionName = "gpu_video_stabilize";
    _description  = "GPU-accelerated full 2-D video stabilizer (translation + rotation). "
                    "Same algorithm as video_stabilize() with feature detection, optical "
                    "flow, and the corrective warp dispatched through OpenCL "
                    "(native kernel with T-API fallback). Falls back to CPU if "
                    "OpenCL is unavailable.";
    _category = "stabilization";
    _params = {
        ParamDef::optional("delta_smooth",  BaseType::FLOAT, "EMA for delta noise filter [0,1) (default 0.5)", 0.5),
        ParamDef::optional("drift_decay",   BaseType::FLOAT, "Per-frame decay (0,1] (default 0.997)", 0.997),
        ParamDef::optional("max_points",    BaseType::INT,   "Max Shi-Tomasi corners (default 200)", 200),
        ParamDef::optional("quality",       BaseType::FLOAT, "Shi-Tomasi quality (default 0.01)", 0.01),
        ParamDef::optional("min_distance",  BaseType::FLOAT, "Min corner distance in px (default 10)", 10.0),
        ParamDef::optional("crop",          BaseType::BOOL,  "Scale-crop borders (default true)", true),
        ParamDef::optional("reset",         BaseType::BOOL,  "Reset state (default false)", false)
    };
    _example    = "video_cap(\"/dev/video0\") | gpu_video_stabilize()\n"
                  "video_cap(\"/dev/video0\") | gpu_video_stabilize(0.5, 0.997)\n"
                  "video_cap(\"/dev/video0\") | gpu_video_stabilize(0.5, 1.0)";
    _returnType = "mat";
    _tags       = {"gpu", "opencl", "stabilization", "video", "shake", "translation",
                   "rotation", "steady"};
}

std::vector<cv::Point2f> GpuVideoStabilizeItem::detectPoints(
    const cv::Mat& gray, int maxPoints, double quality, double minDist) {
    return _detectPts(gray, maxPoints, quality, minDist);
}

#ifdef VISIONPIPE_OPENCL_ENABLED
cv::ocl::Program GpuVideoStabilizeItem::getWarpProgram(bool verbose) {
    std::lock_guard<std::mutex> lk(_warpMutex);
    if (_warpBuilt) return _warpProgram;
    _warpBuilt = true;
    cv::ocl::ProgramSource src(GPU_WARP_AFFINE_CL);
    cv::String log;
    _warpProgram = cv::ocl::Program(src, "", log);
    _warpOk = (_warpProgram.ptr() != nullptr);
    if (!_warpOk)
        std::cerr << "[GPU-VIDEO-STABILIZE] Warp kernel compile failed:\n" << log << std::endl;
    else if (verbose)
        std::cout << "[GPU-VIDEO-STABILIZE] Native warp kernel compiled" << std::endl;
    return _warpProgram;
}
#endif

ExecutionResult GpuVideoStabilizeItem::execute(const std::vector<RuntimeValue>& args,
                                                ExecutionContext& ctx) {
    if (ctx.currentMat.empty()) return ExecutionResult::ok(ctx.currentMat);

    // ---- args ----
    double deltaSmooth = args.size() > 0 ? args[0].asNumber() : 0.5;
    double driftDecay  = args.size() > 1 ? args[1].asNumber() : 0.997;
    int    maxPoints   = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 200;
    double quality     = args.size() > 3 ? args[3].asNumber() : 0.01;
    double minDistance = args.size() > 4 ? args[4].asNumber() : 10.0;
    bool   crop        = args.size() > 5 ? args[5].asBool()   : true;
    bool   reset       = args.size() > 6 ? args[6].asBool()   : false;

    deltaSmooth = std::max(0.0, std::min(deltaSmooth, 0.99));
    driftDecay  = std::max(0.9, std::min(driftDecay,  1.0));
    maxPoints   = std::max(10,  std::min(maxPoints,   2000));

    if (reset) {
        _prevGray.release();
        _prevPts.clear();
        _accX = _accY = _accA = 0.0;
        _smoothDx = _smoothDy = _smoothDa = 0.0;
        _initialised = false;
    }

    bool useOCL = false;
#ifdef VISIONPIPE_OPENCL_ENABLED
    try { useOCL = cv::ocl::useOpenCL() && cv::ocl::haveOpenCL(); } catch (...) {}
#endif

    // ---- grayscale ----
    cv::Mat gray;
    if (ctx.currentMat.channels() == 1)
        gray = ctx.currentMat;
    else
        cv::cvtColor(ctx.currentMat, gray, cv::COLOR_BGR2GRAY);

    // ---- first frame ----
    if (!_initialised || _prevGray.empty()) {
        _prevGray    = gray.clone();
        _prevPts     = detectPoints(gray, maxPoints, quality, minDistance);
        _initialised = true;
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ---- refresh ----
    if (static_cast<int>(_prevPts.size()) < maxPoints / 4)
        _prevPts = detectPoints(_prevGray, maxPoints, quality, minDistance);

    if (_prevPts.empty()) {
        _prevGray = gray.clone();
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ---- optical flow ----
    std::vector<cv::Point2f> currPts;
    std::vector<uchar>       status;
    std::vector<float>       err;

#ifdef VISIONPIPE_OPENCL_ENABLED
    if (useOCL) {
        _prevGray.copyTo(_prevGrayU);
        gray.copyTo(_currGrayU);
        _opticalFlowGpu(_prevGrayU, _currGrayU, _prevPts, currPts, status, err);
    } else
#endif
    {
        cv::calcOpticalFlowPyrLK(
            _prevGray, gray, _prevPts, currPts,
            status, err,
            cv::Size(21, 21), 3,
            cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01));
    }

    std::vector<cv::Point2f> goodPrev, goodCurr;
    filterGood(_prevPts, currPts, status, goodPrev, goodCurr);

    // ---- estimate full 2-D motion (CPU) ----
    double rawDx = 0.0, rawDy = 0.0;
    double rawDa = estimateRotation(goodPrev, goodCurr, &rawDx, &rawDy);

    // Translation sanity clamp
    const double kMaxTx = std::max(ctx.currentMat.cols, ctx.currentMat.rows) * 0.1;
    if (std::abs(rawDx) > kMaxTx) rawDx = 0.0;
    if (std::abs(rawDy) > kMaxTx) rawDy = 0.0;

    // ---- EMA + accumulate + decay ----
    _smoothDx  = deltaSmooth * _smoothDx  + (1.0 - deltaSmooth) * rawDx;
    _smoothDy  = deltaSmooth * _smoothDy  + (1.0 - deltaSmooth) * rawDy;
    _smoothDa  = deltaSmooth * _smoothDa  + (1.0 - deltaSmooth) * rawDa;

    _accX += _smoothDx;  _accX *= driftDecay;
    _accY += _smoothDy;  _accY *= driftDecay;
    _accA += _smoothDa;  _accA *= driftDecay;

    _prevGray = gray.clone();
    _prevPts  = goodCurr;

    // ---- build correction matrix ----
    const double corrA = -_accA;
    const double corrX = -_accX;
    const double corrY = -_accY;

    if (std::abs(corrA) < 1e-7 && std::abs(corrX) < 0.1 && std::abs(corrY) < 0.1)
        return ExecutionResult::ok(ctx.currentMat);

    const double W  = ctx.currentMat.cols;
    const double H  = ctx.currentMat.rows;
    const double cx = W * 0.5, cy = H * 0.5;
    double scale = crop ? computeCropScaleCpu(std::abs(corrA), W, H) : 1.0;

    cv::Mat M = buildFullAffine(corrA, corrX, corrY, scale, cx, cy);

    // ---- GPU warp ----
#ifdef VISIONPIPE_OPENCL_ENABLED
    if (useOCL && ctx.currentMat.depth() == CV_8U) {
        cv::ocl::Program prog = getWarpProgram(ctx.verbose);
        if (_warpOk) {
            try {
                const int rows = ctx.currentMat.rows;
                const int cols = ctx.currentMat.cols;
                const int ch   = ctx.currentMat.channels();

                ctx.currentMat.copyTo(_srcU);
                _dstU.create(rows, cols, ctx.currentMat.type());

                float invM[6];
                double fwd[6] = {
                    M.at<double>(0,0), M.at<double>(0,1), M.at<double>(0,2),
                    M.at<double>(1,0), M.at<double>(1,1), M.at<double>(1,2)
                };
                invertAffine2x3(fwd, invM);

                cv::Mat invMat(1, 6, CV_32F, invM);
                cv::UMat invMU;
                invMat.copyTo(invMU);

                cv::ocl::Kernel kernel("gpu_warp_affine_replicate", prog);
                if (kernel.empty())
                    throw cv::Exception(cv::Error::StsError,
                        "gpu_warp_affine_replicate kernel missing",
                        __func__, __FILE__, __LINE__);

                int arg = 0;
                arg = kernel.set(arg, cv::ocl::KernelArg::ReadOnly(_srcU));
                arg = kernel.set(arg, cv::ocl::KernelArg::WriteOnly(_dstU));
                arg = kernel.set(arg, cv::ocl::KernelArg::PtrReadOnly(invMU));
                arg = kernel.set(arg, ch);

                size_t lsz[2] = {16, 16};
                size_t gsz[2] = {
                    (size_t)(((cols + 15) / 16) * 16),
                    (size_t)(((rows + 15) / 16) * 16)
                };
                if (!kernel.run(2, gsz, lsz, false))
                    throw cv::Exception(cv::Error::StsError,
                        "kernel.run() failed", __func__, __FILE__, __LINE__);

                cv::Mat result;
                _dstU.copyTo(result);
                if (ctx.verbose)
                    std::cout << "[GPU-VIDEO-STABILIZE] Native OCL warp "
                              << cols << "x" << rows << " " << ch << "ch" << std::endl;
                return ExecutionResult::ok(result);

            } catch (const cv::Exception& e) {
                if (ctx.verbose)
                    std::cout << "[GPU-VIDEO-STABILIZE] Native warp failed: "
                              << e.what() << " — trying T-API" << std::endl;
            }
        }

        // T-API fallback
        try {
            ctx.currentMat.copyTo(_srcU);
            cv::warpAffine(_srcU, _dstU, M,
                           ctx.currentMat.size(),
                           cv::INTER_LINEAR,
                           cv::BORDER_REPLICATE);
            cv::Mat result;
            _dstU.copyTo(result);
            if (ctx.verbose)
                std::cout << "[GPU-VIDEO-STABILIZE] T-API warp" << std::endl;
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            if (ctx.verbose)
                std::cout << "[GPU-VIDEO-STABILIZE] T-API warp failed: "
                          << e.what() << " — CPU fallback" << std::endl;
        }
    }
#endif

    // CPU fallback
    cv::Mat result;
    cv::warpAffine(ctx.currentMat, result, M,
                   ctx.currentMat.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REPLICATE);
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

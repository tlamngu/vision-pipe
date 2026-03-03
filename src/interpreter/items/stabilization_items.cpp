#include "interpreter/items/stabilization_items.h"
#include "interpreter/cache_manager.h"
#include <cmath>
#include <iostream>

namespace visionpipe {

void registerStabilizationItems(ItemRegistry& registry) {
    registry.add<HorizonLockItem>();
    registry.add<VideoStabilizeItem>();
    registry.add<StereoStabilizeItem>();
}

// ============================================================================
// HorizonLockItem
// ============================================================================

HorizonLockItem::HorizonLockItem() {
    _functionName = "horizon_lock";
    _description  = "Keeps the frame steady against rotational motion (horizon stabilizer). "
                    "Tracks feature points across frames, estimates per-frame rotation with "
                    "RANSAC partial-affine estimation, smooths the per-frame delta to reduce "
                    "noise, integrates into an accumulated angle, and applies the full "
                    "counter-rotation every frame — no lag, no overshoot.";
    _category = "stabilization";
    _params = {
        ParamDef::optional("delta_smooth",  BaseType::FLOAT, "EMA coefficient for raw delta noise filter [0,1). Higher = smoother, slower initial response (default 0.5)", 0.5),
        ParamDef::optional("drift_decay",   BaseType::FLOAT, "Per-frame decay of accumulated angle (0,1]. 1.0 = hard lock; ~0.997 = soft lock that accepts very slow tilts (default 0.997)", 0.997),
        ParamDef::optional("max_points",    BaseType::INT,   "Max Shi-Tomasi corners to track (default 200)", 200),
        ParamDef::optional("quality",       BaseType::FLOAT, "Shi-Tomasi quality level (default 0.01)", 0.01),
        ParamDef::optional("min_distance",  BaseType::FLOAT, "Min distance between corners in px (default 10)", 10.0),
        ParamDef::optional("crop",          BaseType::BOOL,  "Scale-crop to hide rotation borders (default true)", true),
        ParamDef::optional("reset",         BaseType::BOOL,  "Reset accumulated state and restart (default false)", false)
    };
    _example    = "video_cap(\"/dev/video0\") | horizon_lock()\n"
                  "// soft lock — accepts very slow intentional tilts:\n"
                  "video_cap(\"/dev/video0\") | horizon_lock(0.5, 0.997, 200, 0.01, 10, true)\n"
                  "// hard lock — fights all rotation permanently:\n"
                  "video_cap(\"/dev/video0\") | horizon_lock(0.5, 1.0)";
    _returnType = "mat";
    _tags       = {"stabilization", "horizon", "rotation", "lock", "steady", "video", "gyro"};
}

// ---------------------------------------------------------------------------

std::vector<cv::Point2f> HorizonLockItem::detectPoints(const cv::Mat& gray,
                                                        int maxPoints,
                                                        double quality,
                                                        double minDist) {
    std::vector<cv::Point2f> pts;
    cv::goodFeaturesToTrack(gray, pts,
                            maxPoints,
                            quality,
                            minDist,
                            cv::noArray(),
                            3,     // blockSize
                            false  // useHarrisDetector
    );
    return pts;
}

// ---------------------------------------------------------------------------

ExecutionResult HorizonLockItem::execute(const std::vector<RuntimeValue>& args,
                                          ExecutionContext& ctx) {
    if (ctx.currentMat.empty()) return ExecutionResult::ok(ctx.currentMat);

    // ------------------------------------------------------------------ args
    double deltaSmooth  = args.size() > 0 ? args[0].asNumber() : 0.5;
    double driftDecay   = args.size() > 1 ? args[1].asNumber() : 0.997;
    int    maxPoints    = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 200;
    double quality      = args.size() > 3 ? args[3].asNumber() : 0.01;
    double minDistance  = args.size() > 4 ? args[4].asNumber() : 10.0;
    bool   crop         = args.size() > 5 ? args[5].asBool()   : true;
    bool   reset        = args.size() > 6 ? args[6].asBool()   : false;

    deltaSmooth = std::max(0.0,  std::min(deltaSmooth, 0.99));
    driftDecay  = std::max(0.9,  std::min(driftDecay,  1.0));
    maxPoints   = std::max(10,   std::min(maxPoints,   2000));

    // ----------------------------------------------------------------- reset
    if (reset) {
        _prevGray.release();
        _prevPts.clear();
        _accumulatedAngle = 0.0;
        _smoothedDelta    = 0.0;
        _initialised      = false;
    }

    // --------------------------------------------------- grayscale of current
    cv::Mat gray;
    if (ctx.currentMat.channels() == 1) {
        gray = ctx.currentMat;
    } else {
        cv::cvtColor(ctx.currentMat, gray, cv::COLOR_BGR2GRAY);
    }

    // ---------------------------------------- first frame — just initialise
    if (!_initialised || _prevGray.empty()) {
        _prevGray     = gray.clone();
        _prevPts      = detectPoints(gray, maxPoints, quality, minDistance);
        _initialised  = true;
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ------------------------------------------ refresh points if too sparse
    if (static_cast<int>(_prevPts.size()) < maxPoints / 4) {
        _prevPts = detectPoints(_prevGray, maxPoints, quality, minDistance);
    }

    if (_prevPts.empty()) {
        _prevGray = gray.clone();
        return ExecutionResult::ok(ctx.currentMat);
    }

    // --------------------------------------- optical flow (Lucas-Kanade PyrLK)
    std::vector<cv::Point2f> currPts;
    std::vector<uchar>       status;
    std::vector<float>       err;

    cv::calcOpticalFlowPyrLK(
        _prevGray, gray,
        _prevPts, currPts,
        status, err,
        cv::Size(21, 21),
        3,
        cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01)
    );

    // Separate good matches
    std::vector<cv::Point2f> goodPrev, goodCurr;
    goodPrev.reserve(_prevPts.size());
    goodCurr.reserve(currPts.size());
    for (size_t i = 0; i < status.size(); ++i) {
        if (status[i]) {
            goodPrev.push_back(_prevPts[i]);
            goodCurr.push_back(currPts[i]);
        }
    }

    // ------------------------------------------- estimate rotation (RANSAC)
    // estimateAffinePartial2D finds M s.t. currPts ≈ M * prevPts
    // M = [cosθ  -sinθ  tx]   →  rotation angle = atan2(M[1,0], M[0,0])
    //     [sinθ   cosθ  ty]
    // A positive angle means the scene rotated counter-clockwise from prev to curr
    // (i.e. the camera tilted clockwise).
    double rawDelta = 0.0;

    if (goodPrev.size() >= 4) {
        cv::Mat affine = cv::estimateAffinePartial2D(
            goodPrev, goodCurr,
            cv::noArray(),
            cv::RANSAC,
            2.0   // reprojection threshold
        );
        if (!affine.empty()) {
            double cosA = affine.at<double>(0, 0);
            double sinA = affine.at<double>(1, 0);
            rawDelta    = std::atan2(sinA, cosA);

            // Sanity clamp: reject implausibly large per-frame jumps (> 5°)
            // which are almost certainly tracking failures, not real motion.
            const double kMaxDeltaRad = 5.0 * CV_PI / 180.0;
            if (std::abs(rawDelta) > kMaxDeltaRad) {
                rawDelta = 0.0;
            }
        }
    }

    // ---------------------------------------------- noise-filter the delta
    // EMA applied to the raw per-frame delta removes measurement noise WITHOUT
    // lagging the actual correction (we still apply the full accumulated angle).
    _smoothedDelta = deltaSmooth * _smoothedDelta + (1.0 - deltaSmooth) * rawDelta;

    // Integrate: accumulate the clean delta
    _accumulatedAngle += _smoothedDelta;

    // Soft-lock: very gradually forget so deliberate slow tilts are accepted.
    // driftDecay = 0.997 at 30 fps → time constant ≈ 1/(1-0.997)/30 ≈ 11 s.
    _accumulatedAngle *= driftDecay;

    // ------------------------------------------------- update previous state
    _prevGray = gray.clone();
    _prevPts  = goodCurr;

    // ---------------------------------- correction = full counter-rotation
    // correction is the FULL negative of the accumulated sensor rotation.
    // There is NO trajectory smoothing here, so there is zero additional lag
    // and zero overshoot.
    const double correctionAngle = -_accumulatedAngle;   // radians

    if (std::abs(correctionAngle) < 1e-7) {
        return ExecutionResult::ok(ctx.currentMat);
    }

    // --------------------------------------------------- build warp matrix
    const double angleDeg = correctionAngle * (180.0 / CV_PI);
    cv::Point2f  centre(ctx.currentMat.cols * 0.5f, ctx.currentMat.rows * 0.5f);

    double scale = 1.0;
    if (crop) {
        // Minimum scale so the rotated image fills the full W×H rectangle.
        double absA = std::abs(correctionAngle);
        double sinA = std::sin(absA);
        double cosA = std::cos(absA);
        double W = ctx.currentMat.cols;
        double H = ctx.currentMat.rows;
        // Derived from the inscribed rectangle formula:
        //   scale ≥ (W cosA + H sinA) / W  and  scale ≥ (H cosA + W sinA) / H
        double scaleW = cosA + (H / W) * sinA;
        double scaleH = cosA + (W / H) * sinA;
        scale = std::max(scaleW, scaleH);
        // Cap to avoid extreme zoom on large-angle correction (fallback to replicate border)
        scale = std::min(scale, 1.5);
    }

    cv::Mat rotMat = cv::getRotationMatrix2D(centre, angleDeg, scale);

    cv::Mat result;
    cv::warpAffine(ctx.currentMat, result, rotMat,
                   ctx.currentMat.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REPLICATE);

    return ExecutionResult::ok(result);
}

// ============================================================================
// VideoStabilizeItem
// ============================================================================

VideoStabilizeItem::VideoStabilizeItem() {
    _functionName = "video_stabilize";
    _description  = "Full 2-D video stabilizer: cancels translational shake AND rotational "
                    "jitter in a single warp. Tracks feature points, estimates Δx/Δy/Δθ "
                    "each frame via RANSAC partial-affine, noise-filters them with an EMA, "
                    "integrates into an accumulated trajectory, and counter-transforms the "
                    "frame with no lag and no overshoot.";
    _category = "stabilization";
    _params = {
        ParamDef::optional("delta_smooth",  BaseType::FLOAT, "EMA coefficient for per-frame delta noise filter [0,1). Higher = smoother (default 0.5)", 0.5),
        ParamDef::optional("drift_decay",   BaseType::FLOAT, "Per-frame decay of accumulated motion (0,1]. 1.0 = hard lock; ~0.997 = soft lock ~11s (default 0.997)", 0.997),
        ParamDef::optional("max_points",    BaseType::INT,   "Max Shi-Tomasi corners to track (default 200)", 200),
        ParamDef::optional("quality",       BaseType::FLOAT, "Shi-Tomasi quality level (default 0.01)", 0.01),
        ParamDef::optional("min_distance",  BaseType::FLOAT, "Min distance between corners in px (default 10)", 10.0),
        ParamDef::optional("crop",          BaseType::BOOL,  "Scale-crop to hide warp borders (default true)", true),
        ParamDef::optional("reset",         BaseType::BOOL,  "Reset accumulated state and restart (default false)", false)
    };
    _example    = "video_cap(\"/dev/video0\") | video_stabilize()\n"
                  "video_cap(\"/dev/video0\") | video_stabilize(0.5, 0.997)\n"
                  "// hard lock — no drift allowed:\n"
                  "video_cap(\"/dev/video0\") | video_stabilize(0.5, 1.0)";
    _returnType = "mat";
    _tags       = {"stabilization", "video", "shake", "translation", "rotation", "steady"};
}

// ---------------------------------------------------------------------------

std::vector<cv::Point2f> VideoStabilizeItem::detectPoints(const cv::Mat& gray,
                                                           int maxPoints,
                                                           double quality,
                                                           double minDist) {
    std::vector<cv::Point2f> pts;
    cv::goodFeaturesToTrack(gray, pts, maxPoints, quality, minDist,
                            cv::noArray(), 3, false);
    return pts;
}

// ---------------------------------------------------------------------------

ExecutionResult VideoStabilizeItem::execute(const std::vector<RuntimeValue>& args,
                                             ExecutionContext& ctx) {
    if (ctx.currentMat.empty()) return ExecutionResult::ok(ctx.currentMat);

    // ------------------------------------------------------------------ args
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

    // ----------------------------------------------------------------- reset
    if (reset) {
        _prevGray.release();
        _prevPts.clear();
        _accX = _accY = _accA = 0.0;
        _smoothDx = _smoothDy = _smoothDa = 0.0;
        _initialised = false;
    }

    // --------------------------------------------------- grayscale of current
    cv::Mat gray;
    if (ctx.currentMat.channels() == 1) {
        gray = ctx.currentMat;
    } else {
        cv::cvtColor(ctx.currentMat, gray, cv::COLOR_BGR2GRAY);
    }

    // ---------------------------------------- first frame — just initialise
    if (!_initialised || _prevGray.empty()) {
        _prevGray    = gray.clone();
        _prevPts     = detectPoints(gray, maxPoints, quality, minDistance);
        _initialised = true;
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ------------------------------------------ refresh points if too sparse
    if (static_cast<int>(_prevPts.size()) < maxPoints / 4) {
        _prevPts = detectPoints(_prevGray, maxPoints, quality, minDistance);
    }

    if (_prevPts.empty()) {
        _prevGray = gray.clone();
        return ExecutionResult::ok(ctx.currentMat);
    }

    // --------------------------------------- optical flow (Lucas-Kanade PyrLK)
    std::vector<cv::Point2f> currPts;
    std::vector<uchar>       status;
    std::vector<float>       err;

    cv::calcOpticalFlowPyrLK(
        _prevGray, gray,
        _prevPts, currPts,
        status, err,
        cv::Size(21, 21),
        3,
        cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01)
    );

    std::vector<cv::Point2f> goodPrev, goodCurr;
    goodPrev.reserve(_prevPts.size());
    goodCurr.reserve(currPts.size());
    for (size_t i = 0; i < status.size(); ++i) {
        if (status[i]) {
            goodPrev.push_back(_prevPts[i]);
            goodCurr.push_back(currPts[i]);
        }
    }

    // ------------------------------------------- estimate full 2-D transform
    // estimateAffinePartial2D: M = [cosθ  −sinθ  tx]
    //                              [sinθ   cosθ  ty]
    // prev → curr transform: currPts ≈ M * prevPts
    double rawDx = 0.0, rawDy = 0.0, rawDa = 0.0;

    if (goodPrev.size() >= 4) {
        cv::Mat affine = cv::estimateAffinePartial2D(
            goodPrev, goodCurr,
            cv::noArray(),
            cv::RANSAC,
            2.0
        );
        if (!affine.empty()) {
            rawDa = std::atan2(affine.at<double>(1, 0), affine.at<double>(0, 0));
            rawDx = affine.at<double>(0, 2);
            rawDy = affine.at<double>(1, 2);

            // Sanity clamps — tracking failures produce huge jumps
            const double kMaxAngle = 5.0  * CV_PI / 180.0;   // 5°
            const double kMaxTx    = std::max(ctx.currentMat.cols, ctx.currentMat.rows) * 0.1; // 10% frame dim
            if (std::abs(rawDa) > kMaxAngle) rawDa = 0.0;
            if (std::abs(rawDx) > kMaxTx)    rawDx = 0.0;
            if (std::abs(rawDy) > kMaxTx)    rawDy = 0.0;
        }
    }

    // ---------------------------------------------- EMA noise filter
    _smoothDx = deltaSmooth * _smoothDx + (1.0 - deltaSmooth) * rawDx;
    _smoothDy = deltaSmooth * _smoothDy + (1.0 - deltaSmooth) * rawDy;
    _smoothDa = deltaSmooth * _smoothDa + (1.0 - deltaSmooth) * rawDa;

    // Integrate
    _accX += _smoothDx;
    _accY += _smoothDy;
    _accA += _smoothDa;

    // Soft-lock decay
    _accX *= driftDecay;
    _accY *= driftDecay;
    _accA *= driftDecay;

    // ------------------------------------------------- update previous state
    _prevGray = gray.clone();
    _prevPts  = goodCurr;

    // ----------------------------------------- build counter-transform
    // Correction = negate accumulated trajectory
    const double corrA  = -_accA;
    const double corrX  = -_accX;
    const double corrY  = -_accY;

    if (std::abs(corrA) < 1e-7 && std::abs(corrX) < 0.1 && std::abs(corrY) < 0.1) {
        return ExecutionResult::ok(ctx.currentMat);
    }

    const double W = ctx.currentMat.cols;
    const double H = ctx.currentMat.rows;
    cv::Point2f  centre(W * 0.5f, H * 0.5f);

    // Build rotation component around centre
    const double cosA = std::cos(corrA);
    const double sinA = std::sin(corrA);

    // Full affine: rotate around centre + translate
    // M = R * (p - centre) + centre + t
    cv::Mat M = (cv::Mat_<double>(2, 3) <<
        cosA, -sinA,  (1.0 - cosA) * centre.x + sinA * centre.y + corrX,
        sinA,  cosA, -sinA * centre.x + (1.0 - cosA) * centre.y + corrY
    );

    double scale = 1.0;
    if (crop) {
        // Scale to cover the original frame after the combined rotation+translation warp.
        // Rotation part drives the scale; translations are handled by border replication.
        double absA  = std::abs(corrA);
        double sinAb = std::sin(absA);
        double cosAb = std::cos(absA);
        double scaleW = cosAb + (H / W) * sinAb;
        double scaleH = cosAb + (W / H) * sinAb;
        scale = std::max(scaleW, scaleH);
        scale = std::min(scale, 1.5);

        if (scale > 1.0 + 1e-4) {
            // Bake scale into M: scale about centre
            M.at<double>(0, 0) *= scale;
            M.at<double>(0, 1) *= scale;
            M.at<double>(1, 0) *= scale;
            M.at<double>(1, 1) *= scale;
            // Re-centre after scale: add (1-scale)*centre
            M.at<double>(0, 2) = scale * M.at<double>(0, 2) + (1.0 - scale) * centre.x;
            M.at<double>(1, 2) = scale * M.at<double>(1, 2) + (1.0 - scale) * centre.y;
        }
    }

    cv::Mat result;
    cv::warpAffine(ctx.currentMat, result, M,
                   ctx.currentMat.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REPLICATE);

    return ExecutionResult::ok(result);
}

// ============================================================================
// StereoStabilizeItem
// ============================================================================

StereoStabilizeItem::StereoStabilizeItem() {
    _functionName = "stereo_stabilize";
    _description  = "Stereo-synchronised 2-D video stabilizer: reads left frame from the "
                    "pipeline (currentMat) and right frame from cache, tracks feature points "
                    "in both views, estimates a single shared correction transform (Δx, Δy, Δθ) "
                    "from the pooled matches, and applies the identical counter-warp to both "
                    "frames. This preserves epipolar geometry for downstream stereo matching.";
    _category = "stabilization";
    _params = {
        ParamDef::required("right_cache",   BaseType::STRING, "Cache key of the right frame"),
        ParamDef::optional("output_cache",  BaseType::STRING, "Cache key for the stabilised right frame (default \"stabilized_right\")", std::string("stabilized_right")),
        ParamDef::optional("delta_smooth",  BaseType::FLOAT,  "EMA coefficient for per-frame delta noise [0,1) (default 0.5)", 0.5),
        ParamDef::optional("drift_decay",   BaseType::FLOAT,  "Per-frame decay ∈ (0,1]. 1.0 = hard lock (default 0.997)", 0.997),
        ParamDef::optional("max_points",    BaseType::INT,    "Max Shi-Tomasi corners per view (default 200)", 200),
        ParamDef::optional("quality",       BaseType::FLOAT,  "Shi-Tomasi quality level (default 0.01)", 0.01),
        ParamDef::optional("min_distance",  BaseType::FLOAT,  "Min corner distance in px (default 10)", 10.0),
        ParamDef::optional("crop",          BaseType::BOOL,   "Scale-crop to hide borders (default true)", true),
        ParamDef::optional("reset",         BaseType::BOOL,   "Clear state and restart (default false)", false)
    };
    _example    = "use(\"rect_l\")\n"
                  "stereo_stabilize(\"rect_r\", \"stab_r\")\n"
                  "// currentMat = stabilised left, cache \"stab_r\" = stabilised right";
    _returnType = "mat";
    _tags       = {"stabilization", "stereo", "video", "shake", "translation", "rotation", "sync"};
}

// ---------------------------------------------------------------------------

std::vector<cv::Point2f> StereoStabilizeItem::detectPoints(const cv::Mat& gray,
                                                            int maxPoints,
                                                            double quality,
                                                            double minDist) {
    std::vector<cv::Point2f> pts;
    cv::goodFeaturesToTrack(gray, pts, maxPoints, quality, minDist,
                            cv::noArray(), 3, false);
    return pts;
}

// ---------------------------------------------------------------------------

cv::Mat StereoStabilizeItem::buildCorrectionMatrix(double corrA, double corrX, double corrY,
                                                    double W, double H, bool crop) const {
    cv::Point2f centre(W * 0.5f, H * 0.5f);

    const double cosA = std::cos(corrA);
    const double sinA = std::sin(corrA);

    // Full affine: rotate around centre + translate
    cv::Mat M = (cv::Mat_<double>(2, 3) <<
        cosA, -sinA,  (1.0 - cosA) * centre.x + sinA * centre.y + corrX,
        sinA,  cosA, -sinA * centre.x + (1.0 - cosA) * centre.y + corrY
    );

    if (crop) {
        double absA  = std::abs(corrA);
        double sinAb = std::sin(absA);
        double cosAb = std::cos(absA);
        double scaleW = cosAb + (H / W) * sinAb;
        double scaleH = cosAb + (W / H) * sinAb;
        double scale = std::max(scaleW, scaleH);
        scale = std::min(scale, 1.5);

        if (scale > 1.0 + 1e-4) {
            M.at<double>(0, 0) *= scale;
            M.at<double>(0, 1) *= scale;
            M.at<double>(1, 0) *= scale;
            M.at<double>(1, 1) *= scale;
            M.at<double>(0, 2) = scale * M.at<double>(0, 2) + (1.0 - scale) * centre.x;
            M.at<double>(1, 2) = scale * M.at<double>(1, 2) + (1.0 - scale) * centre.y;
        }
    }

    return M;
}

// ---------------------------------------------------------------------------

ExecutionResult StereoStabilizeItem::execute(const std::vector<RuntimeValue>& args,
                                              ExecutionContext& ctx) {
    if (ctx.currentMat.empty())
        return ExecutionResult::ok(ctx.currentMat);

    // ------------------------------------------------------------------ args
    std::string rightCache  = args[0].asString();
    std::string outputCache = args.size() > 1 ? args[1].asString() : "stabilized_right";
    double deltaSmooth = args.size() > 2 ? args[2].asNumber() : 0.5;
    double driftDecay  = args.size() > 3 ? args[3].asNumber() : 0.997;
    int    maxPoints   = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 200;
    double quality     = args.size() > 5 ? args[5].asNumber() : 0.01;
    double minDistance = args.size() > 6 ? args[6].asNumber() : 10.0;
    bool   crop        = args.size() > 7 ? args[7].asBool()   : true;
    bool   reset       = args.size() > 8 ? args[8].asBool()   : false;

    deltaSmooth = std::max(0.0, std::min(deltaSmooth, 0.99));
    driftDecay  = std::max(0.9, std::min(driftDecay,  1.0));
    maxPoints   = std::max(10,  std::min(maxPoints,   2000));

    // --------------------------------------------------------- fetch right frame
    cv::Mat rightMat = ctx.cacheManager->get(rightCache);
    if (rightMat.empty()) {
        return ExecutionResult::fail("stereo_stabilize: right frame not found in cache: " + rightCache);
    }

    // ----------------------------------------------------------------- reset
    if (reset) {
        _prevGrayL.release();
        _prevGrayR.release();
        _prevPtsL.clear();
        _prevPtsR.clear();
        _accX = _accY = _accA = 0.0;
        _smoothDx = _smoothDy = _smoothDa = 0.0;
        _initialised = false;
    }

    // ------------------------------------------------ grayscale of both views
    cv::Mat grayL, grayR;
    if (ctx.currentMat.channels() == 1) grayL = ctx.currentMat;
    else cv::cvtColor(ctx.currentMat, grayL, cv::COLOR_BGR2GRAY);

    if (rightMat.channels() == 1) grayR = rightMat;
    else cv::cvtColor(rightMat, grayR, cv::COLOR_BGR2GRAY);

    // ---------------------------------------- first frame — just initialise
    if (!_initialised || _prevGrayL.empty() || _prevGrayR.empty()) {
        _prevGrayL = grayL.clone();
        _prevGrayR = grayR.clone();
        _prevPtsL  = detectPoints(grayL, maxPoints, quality, minDistance);
        _prevPtsR  = detectPoints(grayR, maxPoints, quality, minDistance);
        _initialised = true;
        // Store unmodified right to output cache on first frame
        ctx.cacheManager->set(outputCache, rightMat);
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ------------------------------------------ refresh points if too sparse
    if (static_cast<int>(_prevPtsL.size()) < maxPoints / 4) {
        _prevPtsL = detectPoints(_prevGrayL, maxPoints, quality, minDistance);
    }
    if (static_cast<int>(_prevPtsR.size()) < maxPoints / 4) {
        _prevPtsR = detectPoints(_prevGrayR, maxPoints, quality, minDistance);
    }

    // If both views have no points, pass through
    if (_prevPtsL.empty() && _prevPtsR.empty()) {
        _prevGrayL = grayL.clone();
        _prevGrayR = grayR.clone();
        ctx.cacheManager->set(outputCache, rightMat);
        return ExecutionResult::ok(ctx.currentMat);
    }

    // ======================================================================
    // Track points in BOTH views and pool the good matches
    // ======================================================================

    std::vector<cv::Point2f> pooledPrev, pooledCurr;

    // --- Left view tracking ---
    if (!_prevPtsL.empty()) {
        std::vector<cv::Point2f> currPtsL;
        std::vector<uchar>       statusL;
        std::vector<float>       errL;

        cv::calcOpticalFlowPyrLK(
            _prevGrayL, grayL,
            _prevPtsL, currPtsL,
            statusL, errL,
            cv::Size(21, 21), 3,
            cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01)
        );

        std::vector<cv::Point2f> goodPrevL, goodCurrL;
        for (size_t i = 0; i < statusL.size(); ++i) {
            if (statusL[i]) {
                goodPrevL.push_back(_prevPtsL[i]);
                goodCurrL.push_back(currPtsL[i]);
            }
        }

        // Add to pooled set
        pooledPrev.insert(pooledPrev.end(), goodPrevL.begin(), goodPrevL.end());
        pooledCurr.insert(pooledCurr.end(), goodCurrL.begin(), goodCurrL.end());

        // Update left tracking points with the good current ones
        _prevPtsL = goodCurrL;
    }

    // --- Right view tracking ---
    if (!_prevPtsR.empty()) {
        std::vector<cv::Point2f> currPtsR;
        std::vector<uchar>       statusR;
        std::vector<float>       errR;

        cv::calcOpticalFlowPyrLK(
            _prevGrayR, grayR,
            _prevPtsR, currPtsR,
            statusR, errR,
            cv::Size(21, 21), 3,
            cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 30, 0.01)
        );

        std::vector<cv::Point2f> goodPrevR, goodCurrR;
        for (size_t i = 0; i < statusR.size(); ++i) {
            if (statusR[i]) {
                goodPrevR.push_back(_prevPtsR[i]);
                goodCurrR.push_back(currPtsR[i]);
            }
        }

        // Add to pooled set
        pooledPrev.insert(pooledPrev.end(), goodPrevR.begin(), goodPrevR.end());
        pooledCurr.insert(pooledCurr.end(), goodCurrR.begin(), goodCurrR.end());

        // Update right tracking points
        _prevPtsR = goodCurrR;
    }

    // ======================================================================
    // Estimate SHARED transform from the pooled tracking plane
    // ======================================================================
    double rawDx = 0.0, rawDy = 0.0, rawDa = 0.0;

    if (pooledPrev.size() >= 4) {
        cv::Mat affine = cv::estimateAffinePartial2D(
            pooledPrev, pooledCurr,
            cv::noArray(),
            cv::RANSAC,
            2.0
        );
        if (!affine.empty()) {
            rawDa = std::atan2(affine.at<double>(1, 0), affine.at<double>(0, 0));
            rawDx = affine.at<double>(0, 2);
            rawDy = affine.at<double>(1, 2);

            // Sanity clamps
            const double kMaxAngle = 5.0 * CV_PI / 180.0;
            const double kMaxTx = std::max(ctx.currentMat.cols, ctx.currentMat.rows) * 0.1;
            if (std::abs(rawDa) > kMaxAngle) rawDa = 0.0;
            if (std::abs(rawDx) > kMaxTx)    rawDx = 0.0;
            if (std::abs(rawDy) > kMaxTx)    rawDy = 0.0;
        }
    }

    // ---------------------------------------------- EMA noise filter
    _smoothDx = deltaSmooth * _smoothDx + (1.0 - deltaSmooth) * rawDx;
    _smoothDy = deltaSmooth * _smoothDy + (1.0 - deltaSmooth) * rawDy;
    _smoothDa = deltaSmooth * _smoothDa + (1.0 - deltaSmooth) * rawDa;

    // Integrate
    _accX += _smoothDx;
    _accY += _smoothDy;
    _accA += _smoothDa;

    // Soft-lock decay
    _accX *= driftDecay;
    _accY *= driftDecay;
    _accA *= driftDecay;

    // ------------------------------------------------- update previous state
    _prevGrayL = grayL.clone();
    _prevGrayR = grayR.clone();

    // ======================================================================
    // Apply the SAME counter-transform to both views
    // ======================================================================
    const double corrA = -_accA;
    const double corrX = -_accX;
    const double corrY = -_accY;

    if (std::abs(corrA) < 1e-7 && std::abs(corrX) < 0.1 && std::abs(corrY) < 0.1) {
        // No meaningful correction — pass through both frames
        ctx.cacheManager->set(outputCache, rightMat);
        return ExecutionResult::ok(ctx.currentMat);
    }

    const double W = ctx.currentMat.cols;
    const double H = ctx.currentMat.rows;

    cv::Mat M = buildCorrectionMatrix(corrA, corrX, corrY, W, H, crop);

    // Warp left frame
    cv::Mat resultL;
    cv::warpAffine(ctx.currentMat, resultL, M,
                   ctx.currentMat.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REPLICATE);

    // Warp right frame with the exact same matrix
    cv::Mat resultR;
    cv::warpAffine(rightMat, resultR, M,
                   rightMat.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REPLICATE);

    // Store stabilised right frame into cache
    ctx.cacheManager->set(outputCache, resultR);

    return ExecutionResult::ok(resultL);
}

} // namespace visionpipe
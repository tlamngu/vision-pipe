/*
 * gpu_debayer_items.cpp
 *
 * GPU-accelerated Bayer demosaicing for VisionPipe.
 *
 * Primary path: native OpenCL Malvar-He-Cutler kernel (debayer_kernel.cl),
 * adapted from cldemosaic (https://github.com/nevion/cldemosaic, MIT License).
 *   "HIGH-QUALITY LINEAR INTERPOLATION FOR DEMOSAICING OF BAYER-PATTERNED
 *    COLOR IMAGES", H. S. Malvar, L.-w. He, R. Cutler, 2004.
 *
 * Fallback paths (for vng / ea / bilinear algorithms):
 *   OpenCV T-API (cv::UMat + cvtColor) when OpenCL is available.
 *   CPU cvtColor when OpenCL is unavailable.
 */

#include "interpreter/items/gpu_debayer_items.h"
#include "utils/camera_device_manager.h"
#include <iostream>
#include <algorithm>
#include <cstdio>

#ifdef VISIONPIPE_OPENCL_ENABLED
#include <opencv2/core/ocl.hpp>
#endif

namespace visionpipe {

// ============================================================================
// OpenCL kernel source — Malvar-He-Cutler demosaicing
// Adapted from cldemosaic: https://github.com/nevion/cldemosaic (MIT License)
// Copyright 2015 Jason Newton <nevion@gmail.com>
// ============================================================================
#ifdef VISIONPIPE_OPENCL_ENABLED

static const char DEBAYER_CL_SOURCE[] = R"CL(
#ifndef TILE_ROWS
#define TILE_ROWS 4
#endif
#ifndef TILE_COLS
#define TILE_COLS 32
#endif

#define KERNEL_SIZE 5
#define HALF_K      2

#define APRON_ROWS  (TILE_ROWS + KERNEL_SIZE - 1)
#define APRON_COLS  (TILE_COLS + KERNEL_SIZE - 1)
#define N_APRON     (APRON_ROWS * APRON_COLS)
#define N_TILE      (TILE_ROWS * TILE_COLS)

#define BAYER_RGGB 0
#define BAYER_GRBG 1
#define BAYER_GBRG 2
#define BAYER_BGGR 3

/* Reflect-101 border (BORDER_REFLECT_101): idx=-1->1, idx=len->len-2 */
int reflect101(int idx, int len)
{
    if (idx < 0)    idx = -idx;
    if (idx >= len) idx = 2 * (len - 1) - idx;
    return idx;
}

/*
 * Tile-based Malvar-He-Cutler demosaicing kernel.
 *
 * Argument layout (cv::ocl::KernelArg::ReadOnly/WriteOnly convention):
 *   ReadOnly(src)  -> ptr, step_bytes, offset_bytes, rows, cols
 *   WriteOnly(dst) -> ptr, step_bytes, offset_bytes, rows, cols
 *   pattern        -> int (BAYER_RGGB=0, BAYER_GRBG=1, BAYER_GBRG=2, BAYER_BGGR=3)
 */
__kernel
__attribute__((reqd_work_group_size(TILE_COLS, TILE_ROWS, 1)))
void malvar_he_cutler_demosaic(
    __global const uchar* src, int src_step, int src_offset,
    int src_rows, int src_cols,
    __global       uchar* dst, int dst_step, int dst_offset,
    int dst_rows,  int dst_cols,
    int pattern)
{
    const int tile_col_block = get_group_id(0);
    const int tile_row_block = get_group_id(1);
    const int tile_col       = get_local_id(0);
    const int tile_row       = get_local_id(1);
    const int g_c            = get_global_id(0);
    const int g_r            = get_global_id(1);

    const bool valid = (g_r < src_rows) & (g_c < src_cols);

    /* Shared-memory apron: tile + 2-pixel border on each side */
    __local int apron[APRON_ROWS][APRON_COLS];

    const int flat_id = tile_row * TILE_COLS + tile_col;
    for (int task = flat_id; task < N_APRON; task += N_TILE)
    {
        const int ar = task / APRON_COLS;
        const int ac = task % APRON_COLS;

        int gr = (ar + tile_row_block * TILE_ROWS) - HALF_K;
        int gc = (ac + tile_col_block * TILE_COLS) - HALF_K;

        gr = reflect101(gr, src_rows);
        gc = reflect101(gc, src_cols);

        apron[ar][ac] = (int)src[src_offset + gr * src_step + gc];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    if (!valid) return;

    /* Apron indices (i=col, j=row — paper convention) */
    const int i = tile_col + HALF_K;
    const int j = tile_row + HALF_K;

#define F(_i, _j)  apron[(_j)][(_i)]

    const int Fij = F(i, j);

    /* R1: symmetric 4,2,-1 cross — Green at R or B locations */
    const int R1 =
        (4*F(i,j)
         + 2*(F(i-1,j) + F(i+1,j) + F(i,j-1) + F(i,j+1))
         - F(i-2,j) - F(i+2,j) - F(i,j-2) - F(i,j+2)
        ) / 8;

    /* R2: left-right symmetric (theta) — R at G in red row, B at G in blue row */
    const int R2 =
        (  8*(F(i-1,j) + F(i+1,j))
         + 10*F(i,j)
         + F(i,j-2) + F(i,j+2)
         - 2*(F(i-1,j-1) + F(i+1,j-1) + F(i-1,j+1) + F(i+1,j+1)
              + F(i-2,j)  + F(i+2,j))
        ) / 16;

    /* R3: top-bottom symmetric (phi) — R at G in blue row, B at G in red row */
    const int R3 =
        (  8*(F(i,j-1) + F(i,j+1))
         + 10*F(i,j)
         + F(i-2,j) + F(i+2,j)
         - 2*(F(i-1,j-1) + F(i+1,j-1) + F(i-1,j+1) + F(i+1,j+1)
              + F(i,j-2)  + F(i,j+2))
        ) / 16;

    /* R4: symmetric 3/2 checker — R at B locations, B at R locations */
    const int R4 =
        (  12*F(i,j)
         - 3*(F(i-2,j) + F(i+2,j) + F(i,j-2) + F(i,j+2))
         + 4*(F(i-1,j-1) + F(i+1,j-1) + F(i-1,j+1) + F(i+1,j+1))
        ) / 16;

#undef F

    /* Determine Bayer sub-pixel location */
    const int red_col  = (pattern == BAYER_GRBG) | (pattern == BAYER_BGGR);
    const int red_row  = (pattern == BAYER_GBRG) | (pattern == BAYER_BGGR);

    const int r_mod_2 = g_r & 1;
    const int c_mod_2 = g_c & 1;

    const int in_red_row    = (r_mod_2 == red_row);
    const int in_blue_row   = 1 - in_red_row;
    const int is_red_pixel  = (r_mod_2 == red_row)  & (c_mod_2 == red_col);
    const int is_blue_pixel = (r_mod_2 != red_row)  & (c_mod_2 != red_col);
    const int is_green      = !(is_red_pixel | is_blue_pixel);

    /* Reconstruct R, G, B with saturation */
    const uchar R = (uchar)clamp(
        Fij * is_red_pixel
        + R4 * is_blue_pixel
        + R2 * (is_green & in_red_row)
        + R3 * (is_green & in_blue_row),
        0, 255);

    const uchar G = (uchar)clamp(
        Fij * is_green
        + R1 * (!is_green),
        0, 255);

    const uchar B = (uchar)clamp(
        Fij * is_blue_pixel
        + R4 * is_red_pixel
        + R3 * (is_green & in_red_row)
        + R2 * (is_green & in_blue_row),
        0, 255);

    /* Write BGR output (OpenCV convention) */
    const int dst_idx = dst_offset + g_r * dst_step + g_c * 3;
    dst[dst_idx + 0] = B;
    dst[dst_idx + 1] = G;
    dst[dst_idx + 2] = R;
}
)CL";

// Tile dimensions (must match reqd_work_group_size above)
static constexpr int MALVAR_TILE_COLS = 32;
static constexpr int MALVAR_TILE_ROWS = 8;

#endif // VISIONPIPE_OPENCL_ENABLED

// ============================================================================
// Registration
// ============================================================================

void registerGpuDebayerItems(ItemRegistry& registry) {
    registry.add<GpuDebayerItem>();
}

// ============================================================================
// Helper: Bayer pattern string -> cldemosaic enum
//   RGGB=0  GRBG=1  GBRG=2  BGGR=3
// ============================================================================
static int patternEnum(const std::string& pattern)
{
    // Match OpenCV Bayer naming convention (BayerXY = colour at row 1):
    //   "RGGB" → OpenCV BayerRG → sensor BGGR (kernel enum 3)
    //   "BGGR" → OpenCV BayerBG → sensor RGGB (kernel enum 0)
    //   "GRBG" → OpenCV BayerGR → sensor GBRG (kernel enum 2)
    //   "GBRG" → OpenCV BayerGB → sensor GRBG (kernel enum 1)
    if (pattern == "RGGB") return 3;
    if (pattern == "GRBG") return 2;
    if (pattern == "GBRG") return 1;
    if (pattern == "BGGR") return 0;
    return -1;
}

// ============================================================================
// Helper: pattern + algorithm + output -> OpenCV conversion code
// ============================================================================
static int resolveBayerCode(const std::string& pattern,
                            const std::string& algorithm,
                            const std::string& output)
{
    if (output == "bgr") {
        if (algorithm == "bilinear") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR;
        } else if (algorithm == "vng") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR_VNG;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR_VNG;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR_VNG;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR_VNG;
        } else if (algorithm == "ea") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR_EA;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR_EA;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR_EA;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR_EA;
        }
    } else if (output == "rgb") {
        if (algorithm == "bilinear") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2RGB;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2RGB;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2RGB;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2RGB;
        } else if (algorithm == "vng") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2RGB_VNG;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2RGB_VNG;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2RGB_VNG;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2RGB_VNG;
        } else if (algorithm == "ea") {
            if (pattern == "RGGB") return cv::COLOR_BayerRG2RGB_EA;
            if (pattern == "BGGR") return cv::COLOR_BayerBG2RGB_EA;
            if (pattern == "GBRG") return cv::COLOR_BayerGB2RGB_EA;
            if (pattern == "GRBG") return cv::COLOR_BayerGR2RGB_EA;
        }
    }
    return -1;
}

// ============================================================================
// Constructor
// ============================================================================

GpuDebayerItem::GpuDebayerItem() {
    _functionName = "gpu_debayer";
    _description  =
        "GPU-accelerated Bayer demosaicing. "
        "Default algorithm \"malvar\" runs a native OpenCL kernel implementing "
        "the Malvar-He-Cutler high-quality linear interpolation (2004), "
        "adapted from cldemosaic (https://github.com/nevion/cldemosaic). "
        "Also supports \"bilinear\", \"vng\", and \"ea\" via OpenCV T-API. "
        "Falls back to CPU when OpenCL is unavailable.";
    _category = "color";
    _params = {
        ParamDef::optional("pattern",
            BaseType::STRING,
            "Bayer tile pattern: RGGB, BGGR, GBRG, GRBG, or \"auto\"",
            "RGGB"),
        ParamDef::optional("algorithm",
            BaseType::STRING,
            "Demosaic algorithm: malvar (native OpenCL, default), bilinear, vng, ea",
            "malvar"),
        ParamDef::optional("output",
            BaseType::STRING,
            "Output color order: bgr or rgb",
            "bgr"),
        ParamDef::optional("bit_shift",
            BaseType::INT,
            "Right-shift for 10/12-bit packed inputs (e.g. 2 for 10-bit in 16-bit container)",
            0)
    };
    _example    = R"(gpu_debayer("auto") | gpu_debayer("RGGB", "malvar") | gpu_debayer("RGGB", "ea", "bgr", 2))";
    _returnType = "mat";
    _tags       = {"gpu", "opencl", "debayer", "demosaic", "bayer", "color",
                   "malvar", "malvar-he-cutler", "acceleration"};
}

// ============================================================================
// Lazy OCL program compilation (cached, thread-safe)
// ============================================================================

#ifdef VISIONPIPE_OPENCL_ENABLED

cv::ocl::Program GpuDebayerItem::getMalvarProgram(bool verbose)
{
    std::lock_guard<std::mutex> lk(_programMutex);

    if (_programBuilt) return _malvarProgram;
    _programBuilt = true;

    char opts[128];
    std::snprintf(opts, sizeof(opts),
        "-D TILE_COLS=%d -D TILE_ROWS=%d",
        MALVAR_TILE_COLS, MALVAR_TILE_ROWS);

    cv::ocl::ProgramSource source(DEBAYER_CL_SOURCE);
    cv::String build_log;
    _malvarProgram = cv::ocl::Program(source, opts, build_log);

    if (_malvarProgram.ptr() == nullptr) {
        std::cerr << "[GPU-DEBAYER] Malvar-He-Cutler kernel compile failed:\n"
                  << build_log << std::endl;
        _programOk = false;
    } else {
        _programOk = true;
        if (verbose) {
            std::cout << "[GPU-DEBAYER] Malvar-He-Cutler kernel compiled"
                      << " (tile " << MALVAR_TILE_COLS << "x" << MALVAR_TILE_ROWS << ")"
                      << std::endl;
            if (!build_log.empty())
                std::cout << "[GPU-DEBAYER] Build log: " << build_log << std::endl;
        }
    }
    return _malvarProgram;
}

#endif // VISIONPIPE_OPENCL_ENABLED

// ============================================================================
// execute()
// ============================================================================

ExecutionResult GpuDebayerItem::execute(const std::vector<RuntimeValue>& args,
                                        ExecutionContext& ctx)
{
    if (ctx.currentMat.empty())
        return ExecutionResult::ok(ctx.currentMat);

    // ---- Parse arguments ----
    std::string pattern   = args.size() > 0 ? args[0].asString() : "RGGB";
    std::string algorithm = args.size() > 1 ? args[1].asString() : "malvar";
    std::string output    = args.size() > 2 ? args[2].asString() : "bgr";
    int         bitShift  = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;

    std::transform(pattern.begin(),   pattern.end(),   pattern.begin(),   ::toupper);
    std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(), ::tolower);
    std::transform(output.begin(),    output.end(),    output.begin(),    ::tolower);

    // ---- Auto-detect Bayer pattern ----
    if (pattern == "AUTO") {
        if (ctx.lastSourceId.empty()) {
            return ExecutionResult::fail(
                "gpu_debayer(\"auto\"): no source ID in context — "
                "run video_cap or v4l2_setup before gpu_debayer");
        }
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
        pattern = CameraDeviceManager::forSource(ctx.lastSourceId).getV4L2BayerPattern(ctx.lastSourceId);
#endif
        if (pattern.empty())
            pattern = CameraDeviceManager::forSource(ctx.lastSourceId).getBayerPattern(ctx.lastSourceId);
        if (pattern.empty()) {
            return ExecutionResult::fail(
                "gpu_debayer(\"auto\"): could not detect Bayer pattern for source '"
                + ctx.lastSourceId + "'");
        }
        if (ctx.verbose)
            std::cout << "[GPU-DEBAYER] auto-detected pattern=" << pattern
                      << " for source=" << ctx.lastSourceId << std::endl;
    }

    // ---- Prepare 8-bit single-channel input ----
    cv::Mat input = ctx.currentMat;

    if (bitShift > 0 && input.depth() == CV_16U) {
        cv::Mat tmp;
        input.convertTo(tmp, CV_8U, 1.0 / (1 << bitShift));
        input = tmp;
    } else if (input.depth() == CV_16U) {
        double minVal, maxVal;
        cv::minMaxLoc(input, &minVal, &maxVal);
        input.convertTo(input, CV_8U, maxVal > 0 ? 255.0 / maxVal : 1.0);
    } else if (input.depth() != CV_8U) {
        input.convertTo(input, CV_8U);
    }

    if (input.channels() != 1) {
        return ExecutionResult::fail(
            "gpu_debayer: input must be single-channel (got "
            + std::to_string(input.channels()) + " channels)");
    }

    // ===================================================================
    //  Path 1 — Native OpenCL Malvar-He-Cutler kernel
    // ===================================================================
#ifdef VISIONPIPE_OPENCL_ENABLED
    bool haveOCL = false;
    try { haveOCL = cv::ocl::useOpenCL() && cv::ocl::haveOpenCL(); }
    catch (...) {}

    if (haveOCL && algorithm == "malvar")
    {
        cv::ocl::Program prog = getMalvarProgram(ctx.verbose);
        if (_programOk) {
            try {
                const int rows = input.rows;
                const int cols = input.cols;

                int pattEnum = patternEnum(pattern);
                if (pattEnum < 0) {
                    return ExecutionResult::fail(
                        "gpu_debayer(malvar): unknown pattern \"" + pattern + "\"");
                }

                // Persistent GPU buffers: reuse _srcU/_dstU across frames
                // to avoid per-frame clCreateBuffer/clReleaseMemObject overhead.
                // On SoC unified memory, copyTo into existing UMat is a fast
                // memcpy (no reallocation when size matches).
                input.copyTo(_srcU);                       // reuses buffer
                _dstU.create(rows, cols, CV_8UC3);         // no-op if same size

                cv::ocl::Kernel kernel("malvar_he_cutler_demosaic", prog);
                if (kernel.empty()) {
                    throw cv::Exception(cv::Error::StsError,
                        "Kernel lookup returned empty", __func__, __FILE__, __LINE__);
                }

                int arg = 0;
                arg = kernel.set(arg, cv::ocl::KernelArg::ReadOnly(_srcU));
                arg = kernel.set(arg, cv::ocl::KernelArg::WriteOnly(_dstU));
                arg = kernel.set(arg, pattEnum);

                size_t lsz[2]  = { (size_t)MALVAR_TILE_COLS, (size_t)MALVAR_TILE_ROWS };
                size_t gsz[2]  = {
                    (size_t)(((cols + MALVAR_TILE_COLS - 1) / MALVAR_TILE_COLS) * MALVAR_TILE_COLS),
                    (size_t)(((rows + MALVAR_TILE_ROWS - 1) / MALVAR_TILE_ROWS) * MALVAR_TILE_ROWS)
                };

                // Non-blocking: getMat() below implicitly waits for completion
                bool ok = kernel.run(2, gsz, lsz, /*sync=*/false);
                if (!ok) {
                    throw cv::Exception(cv::Error::StsError,
                        "kernel.run() returned false", __func__, __FILE__, __LINE__);
                }

                cv::Mat result;
                _dstU.copyTo(result);

                // Kernel always outputs BGR; convert if RGB requested
                if (output == "rgb")
                    cv::cvtColor(result, result, cv::COLOR_BGR2RGB);

                if (ctx.verbose)
                    std::cout << "[GPU-DEBAYER] Malvar-He-Cutler OCL: pattern=" << pattern
                              << " " << cols << "x" << rows << std::endl;
                return ExecutionResult::ok(result);

            } catch (const cv::Exception& e) {
                if (ctx.verbose)
                    std::cout << "[GPU-DEBAYER] Native kernel error: " << e.what()
                              << " — falling back to OpenCV T-API" << std::endl;
            }
        }
    }

    // ===================================================================
    //  Path 2 — OpenCV T-API (cv::UMat + cvtColor)
    // ===================================================================
    if (haveOCL)
    {
        // "malvar" falls back to "ea" (highest quality OpenCV algorithm)
        const std::string cv_algo = (algorithm == "malvar") ? "ea" : algorithm;
        int code = resolveBayerCode(pattern, cv_algo, output);
        if (code >= 0) {
            try {
                input.copyTo(_srcU);           // reuses existing buffer
                cv::cvtColor(_srcU, _dstU, code);
                cv::Mat result;
                _dstU.copyTo(result);
                if (ctx.verbose)
                    std::cout << "[GPU-DEBAYER] OpenCV T-API: pattern=" << pattern
                              << " algo=" << cv_algo
                              << " " << input.cols << "x" << input.rows << std::endl;
                return ExecutionResult::ok(result);
            } catch (const cv::Exception& e) {
                if (ctx.verbose)
                    std::cout << "[GPU-DEBAYER] T-API error: " << e.what()
                              << " — falling back to CPU" << std::endl;
            }
        }
    }
    if (!haveOCL ) {
        if (ctx.verbose)
            std::cout << "[GPU-DEBAYER] OpenCL not available. Falling back to CPU." << std::endl;
    }
#endif // VISIONPIPE_OPENCL_ENABLED

    // ===================================================================
    //  Path 3 — CPU fallback
    // ===================================================================
    {
        const std::string cv_algo = (algorithm == "malvar") ? "ea" : algorithm;
        int code = resolveBayerCode(pattern, cv_algo, output);
        if (code < 0) {
            return ExecutionResult::fail(
                "gpu_debayer: unsupported combination pattern=\"" + pattern
                + "\" algorithm=\"" + algorithm + "\" output=\"" + output + "\"");
        }
        try {
            cv::Mat result;
            cv::cvtColor(input, result, code);
            if (ctx.verbose)
                std::cout << "[GPU-DEBAYER] CPU fallback: pattern=" << pattern
                          << " algo=" << cv_algo
                          << " " << input.cols << "x" << input.rows << std::endl;
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            return ExecutionResult::fail(
                "gpu_debayer: OpenCV error: " + std::string(e.what()));
        }
    }
}

} // namespace visionpipe

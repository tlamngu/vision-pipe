/*
 * gpu_transform_items.cpp
 *
 * OpenCL-accelerated per-pixel colour/channel transform and matrix multiply.
 *
 *  gpu_transform(matrix_cache, [depth])
 *    Applies a cached float matrix M to every pixel vector:
 *      dst_pixel = M * [src_pixel | 1]   (bias column if m.cols == src_ch + 1)
 *      dst_pixel = M * src_pixel          (no bias if m.cols == src_ch)
 *    Native OpenCL kernel → OpenCV T-API (UMat+cv::transform) → CPU fallback.
 *
 *  gpu_matrix_mul(src2_cache, [alpha, src3_cache, beta, flags])
 *    dst = alpha * op(A) * op(B) + beta * C
 *    Tiled OpenCL GEMM kernel → OpenCV T-API (UMat+cv::gemm) → CPU fallback.
 */

#include "interpreter/items/gpu_transform_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <algorithm>
#include <cstdio>

#ifdef VISIONPIPE_OPENCL_ENABLED
#include <opencv2/core/ocl.hpp>
#endif

namespace visionpipe {

// ============================================================================
// OpenCL source — gpu_color_transform
//
// Applies an (m x n) or (m x n+1) float matrix to every pixel of a CV_32F
// image with n-channels, producing an m-channel CV_32F result.
//
// Argument layout (KernelArg convention):
//   ReadOnly(src)    → ptr, step_bytes, offset_bytes, rows, cols
//   WriteOnly(dst)   → ptr, step_bytes, offset_bytes, rows, cols
//   PtrReadOnly(M)   → ptr   (raw cl_mem, no extra scalars)
//   src_channels, dst_channels, m_cols  (int scalars)
// ============================================================================
#ifdef VISIONPIPE_OPENCL_ENABLED

static const char GPU_TRANSFORM_CL[] = R"CL(
/*
 * Per-pixel color/channel linear transform.
 * src and dst are CV_32F images (n and m channels respectively).
 * M is a row-major float array: dst_channels rows, m_cols columns.
 * m_cols == src_channels       → no bias
 * m_cols == src_channels + 1  → last column is additive bias
 */
__kernel void gpu_color_transform(
    __global const uchar* src,  int src_step,  int src_offset,
    int src_rows, int src_cols,
    __global uchar*       dst,  int dst_step,  int dst_offset,
    int dst_rows, int dst_cols,
    __global const float* M,
    int src_channels,
    int dst_channels,
    int m_cols)
{
    const int c = get_global_id(0);
    const int r = get_global_id(1);
    if (r >= src_rows || c >= src_cols) return;

    /* src/dst pointers must be interpreted as float arrays */
    __global const float* src_row =
        (__global const float*)( src + src_offset + (size_t)r * src_step );
    __global float* dst_row =
        (__global float*)( dst + dst_offset + (size_t)r * dst_step );

    /* Read source pixel (up to 4 channels) */
    float inp[4] = { 0.f, 0.f, 0.f, 0.f };
    for (int k = 0; k < src_channels && k < 4; k++)
        inp[k] = src_row[c * src_channels + k];

    /* Multiply and accumulate */
    for (int o = 0; o < dst_channels && o < 4; o++) {
        __global const float* Mrow = M + (size_t)o * m_cols;
        float val = 0.f;
        for (int k = 0; k < src_channels && k < 4; k++)
            val += Mrow[k] * inp[k];
        if (m_cols > src_channels)
            val += Mrow[src_channels]; /* bias */
        dst_row[c * dst_channels + o] = val;
    }
}

/*
 * Per-pixel color transform working directly on CV_8U images.
 * Avoids CPU-side float <-> uint8 conversion: ~9x less data movement.
 * Argument layout identical to gpu_color_transform.
 */
__kernel void gpu_color_transform_8u(
    __global const uchar* src,  int src_step,  int src_offset,
    int src_rows, int src_cols,
    __global uchar*       dst,  int dst_step,  int dst_offset,
    int dst_rows, int dst_cols,
    __global const float* M,
    int src_channels,
    int dst_channels,
    int m_cols)
{
    const int c = get_global_id(0);
    const int r = get_global_id(1);
    if (r >= src_rows || c >= src_cols) return;

    __global const uchar* sp =
        src + src_offset + (size_t)r * src_step + c * src_channels;
    __global uchar* dp =
        dst + dst_offset + (size_t)r * dst_step + c * dst_channels;

    /* Read source pixel bytes -> float in registers */
    float inp[4] = { 0.f, 0.f, 0.f, 0.f };
    for (int k = 0; k < src_channels && k < 4; k++)
        inp[k] = (float)sp[k];

    /* Matrix multiply + saturate + write bytes */
    for (int o = 0; o < dst_channels && o < 4; o++) {
        __global const float* Mrow = M + (size_t)o * m_cols;
        float val = 0.f;
        for (int k = 0; k < src_channels && k < 4; k++)
            val += Mrow[k] * inp[k];
        if (m_cols > src_channels)
            val += Mrow[src_channels]; /* bias */
        dp[o] = convert_uchar_sat(val);
    }
}
)CL";

// ============================================================================
// OpenCL source — gpu_matrix_mul (tiled GEMM)
//
// Computes D = alpha * A * B   (float32, 2-D single-channel matrices).
// Bias / addend C is handled on the host after this kernel.
//
// Argument layout:
//   ReadOnly(A)   → ptr, step_bytes, offset_bytes, A_rows, A_cols
//   ReadOnly(B)   → ptr, step_bytes, offset_bytes, B_rows, B_cols
//   WriteOnly(D)  → ptr, step_bytes, offset_bytes, D_rows, D_cols
//   float alpha
// ============================================================================

static const char GPU_MATMUL_CL[] = R"CL(
#define GEMM_TILE 16

__kernel
__attribute__((reqd_work_group_size(GEMM_TILE, GEMM_TILE, 1)))
void gpu_matrix_mul(
    __global const uchar* A, int A_step, int A_offset, int A_rows, int A_cols,
    __global const uchar* B, int B_step, int B_offset, int B_rows, int B_cols,
    __global uchar*       D, int D_step, int D_offset, int D_rows, int D_cols,
    float alpha)
{
    /* Tiled matrix multiplication: D = alpha * A * B
     * A: M x K  (A_rows x A_cols)
     * B: K x N  (B_rows x B_cols)
     * D: M x N  (A_rows x B_cols)
     */
    __local float tA[GEMM_TILE][GEMM_TILE];
    __local float tB[GEMM_TILE][GEMM_TILE];

    const int row = get_global_id(1);  /* output row  (0..M-1) */
    const int col = get_global_id(0);  /* output col  (0..N-1) */
    const int lr  = get_local_id(1);
    const int lc  = get_local_id(0);

    const int M = A_rows;
    const int K = A_cols;  /* == B_rows */
    const int N = B_cols;

    float acc = 0.f;
    const int n_tiles = (K + GEMM_TILE - 1) / GEMM_TILE;

    for (int t = 0; t < n_tiles; t++) {
        /* Load tile from A */
        int a_col = t * GEMM_TILE + lc;
        if (row < M && a_col < K)
            tA[lr][lc] = *((__global const float*)(A + A_offset + (size_t)row * A_step) + a_col);
        else
            tA[lr][lc] = 0.f;

        /* Load tile from B */
        int b_row = t * GEMM_TILE + lr;
        if (b_row < K && col < N)
            tB[lr][lc] = *((__global const float*)(B + B_offset + (size_t)b_row * B_step) + col);
        else
            tB[lr][lc] = 0.f;

        barrier(CLK_LOCAL_MEM_FENCE);

        for (int k = 0; k < GEMM_TILE; k++)
            acc += tA[lr][k] * tB[k][lc];

        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if (row < M && col < N) {
        __global float* d_row = (__global float*)(D + D_offset + (size_t)row * D_step);
        d_row[col] = alpha * acc;
    }
}
)CL";

#endif // VISIONPIPE_OPENCL_ENABLED

// ============================================================================
// Registration
// ============================================================================

void registerGpuTransformItems(ItemRegistry& registry) {
    registry.add<GpuTransformItem>();
    registry.add<GpuMatrixMulItem>();
}

// ============================================================================
// Utility
// ============================================================================

static int parseDepth(const std::string& s) {
    if (s == "8u"  || s == "8U")  return CV_8U;
    if (s == "16u" || s == "16U") return CV_16U;
    if (s == "32f" || s == "32F") return CV_32F;
    return -1; // "same"
}

// ============================================================================
// GpuTransformItem — constructor
// ============================================================================

GpuTransformItem::GpuTransformItem() {
    _functionName = "gpu_transform";
    _description  =
        "GPU-accelerated per-pixel colour/channel linear transform. "
        "Applies a cached float matrix M to every pixel vector: "
        "  dst_pixel = M * pixel        (M.cols == src_channels) "
        "  dst_pixel = M * [pixel|1]    (M.cols == src_channels+1, with bias). "
        "Primary: native OpenCL kernel. "
        "Fallback: OpenCV T-API (cv::transform + UMat). "
        "CPU fallback: cv::transform.";
    _category   = "color";
    _params = {
        ParamDef::required("matrix_cache",
            BaseType::STRING,
            "Cache key of the float transform matrix (CV_32F, m x n or m x n+1)"),
        ParamDef::optional("depth",
            BaseType::STRING,
            "Output depth: \"8u\", \"16u\", \"32f\", or \"same\" (default)",
            std::string("same"))
    };
    _example    = R"(gpu_transform("CCM") | gpu_transform("CCM3x3", "8u"))";
    _returnType = "mat";
    _tags       = {"gpu", "opencl", "color", "transform", "matrix", "ccm",
                   "channel", "acceleration"};
}

// ============================================================================
// GpuTransformItem — lazy OCL program
// ============================================================================

#ifdef VISIONPIPE_OPENCL_ENABLED
cv::ocl::Program GpuTransformItem::getTransformProgram(bool verbose)
{
    std::lock_guard<std::mutex> lk(_programMutex);
    if (_programBuilt) return _program;
    _programBuilt = true;

    cv::ocl::ProgramSource src(GPU_TRANSFORM_CL);
    cv::String log;
    _program  = cv::ocl::Program(src, "", log);
    _programOk = (_program.ptr() != nullptr);

    if (!_programOk)
        std::cerr << "[GPU-TRANSFORM] Compile failed:\n" << log << std::endl;
    else if (verbose)
        std::cout << "[GPU-TRANSFORM] gpu_color_transform kernel compiled" << std::endl;
    return _program;
}
#endif

// ============================================================================
// GpuTransformItem — execute
// ============================================================================

ExecutionResult GpuTransformItem::execute(const std::vector<RuntimeValue>& args,
                                          ExecutionContext& ctx)
{
    if (ctx.currentMat.empty())
        return ExecutionResult::ok(ctx.currentMat);

    // ---- arguments ----
    if (args.empty())
        return ExecutionResult::fail("gpu_transform: matrix_cache argument required");
    std::string cacheKey = args[0].asString();
    std::string depthStr = args.size() > 1 ? args[1].asString() : "same";

    // ---- load transform matrix ----
    cv::Mat M = ctx.cacheManager->get(cacheKey);
    if (M.empty())
        return ExecutionResult::fail("gpu_transform: matrix not found in cache: " + cacheKey);
    if (M.depth() != CV_32F) {
        M.convertTo(M, CV_32F);
    }
    if (M.channels() != 1)
        return ExecutionResult::fail("gpu_transform: matrix must be single-channel");

    const int src_ch  = ctx.currentMat.channels();
    const int dst_ch  = M.rows;
    const int m_cols  = M.cols;

    if (m_cols != src_ch && m_cols != src_ch + 1)
        return ExecutionResult::fail(
            "gpu_transform: matrix cols (" + std::to_string(m_cols) +
            ") must equal src_channels (" + std::to_string(src_ch) +
            ") or src_channels+1");
    if (dst_ch < 1 || dst_ch > 4)
        return ExecutionResult::fail("gpu_transform: dst_channels out of range (1-4)");

    // ---- output depth ----
    int inputDepth  = ctx.currentMat.depth();
    int outputDepth = (depthStr == "same") ? inputDepth : parseDepth(depthStr);
    if (outputDepth < 0) outputDepth = inputDepth;

#ifdef VISIONPIPE_OPENCL_ENABLED
    bool haveOCL = false;
    try { haveOCL = cv::ocl::useOpenCL() && cv::ocl::haveOpenCL(); } catch (...) {}

    // ===== Fast path: 8U→8U native kernel (no float conversion) =====
    // Eliminates CPU-side 8U↔32F conversion: ~9× less data movement.
    if (haveOCL && inputDepth == CV_8U && outputDepth == CV_8U) {
        cv::ocl::Program prog = getTransformProgram(ctx.verbose);
        if (_programOk) {
            try {
                const int rows = ctx.currentMat.rows;
                const int cols = ctx.currentMat.cols;

                ctx.currentMat.copyTo(_srcU);                    // 8U upload
                _dstU.create(rows, cols,
                             CV_MAKE_TYPE(CV_8U, dst_ch));

                // Cache transform matrix on GPU — only re-upload when changed
                if (cacheKey != _cachedMKey || M.data != _cachedMData) {
                    M.copyTo(_MU);
                    _cachedMKey  = cacheKey;
                    _cachedMData = M.data;
                }

                cv::ocl::Kernel kernel("gpu_color_transform_8u", prog);
                if (kernel.empty())
                    throw cv::Exception(cv::Error::StsError,
                        "gpu_color_transform_8u kernel not found",
                        __func__, __FILE__, __LINE__);

                int arg = 0;
                arg = kernel.set(arg, cv::ocl::KernelArg::ReadOnly(_srcU));
                arg = kernel.set(arg, cv::ocl::KernelArg::WriteOnly(_dstU));
                arg = kernel.set(arg, cv::ocl::KernelArg::PtrReadOnly(_MU));
                arg = kernel.set(arg, src_ch);
                arg = kernel.set(arg, dst_ch);
                arg = kernel.set(arg, m_cols);

                size_t lsz[2] = { 16, 16 };
                size_t gsz[2] = {
                    (size_t)(((cols + 15) / 16) * 16),
                    (size_t)(((rows + 15) / 16) * 16)
                };
                bool ok = kernel.run(2, gsz, lsz, /*sync=*/false);
                if (!ok)
                    throw cv::Exception(cv::Error::StsError,
                        "kernel.run() failed", __func__, __FILE__, __LINE__);

                cv::Mat result;
                _dstU.copyTo(result);

                if (ctx.verbose)
                    std::cout << "[GPU-TRANSFORM] Native OCL 8U: "
                              << src_ch << "→" << dst_ch << "ch "
                              << cols << "x" << rows << std::endl;
                return ExecutionResult::ok(result);

            } catch (const cv::Exception& e) {
                if (ctx.verbose)
                    std::cout << "[GPU-TRANSFORM] 8U kernel error: " << e.what()
                              << " — falling back to float path" << std::endl;
            }
        }
    }
#endif

    // Convert input to float32 for the float kernel / CPU paths
    cv::Mat src32;
    if (inputDepth == CV_32F)
        src32 = ctx.currentMat;
    else
        ctx.currentMat.convertTo(src32, CV_MAKE_TYPE(CV_32F, src_ch));

#ifdef VISIONPIPE_OPENCL_ENABLED
    // ===== Path 1: Native OpenCL kernel (float) =====
    if (haveOCL) {
        cv::ocl::Program prog = getTransformProgram(ctx.verbose);
        if (_programOk) {
            try {
                const int rows = src32.rows;
                const int cols = src32.cols;

                // Persistent GPU buffers: reuse across frames to avoid
                // per-frame clCreateBuffer/clReleaseMemObject overhead.
                src32.copyTo(_srcU);                      // reuses buffer
                _dstU.create(rows, cols,
                             CV_MAKE_TYPE(CV_32F, dst_ch)); // no-op if same

                // Cache transform matrix on GPU — only re-upload when changed
                if (cacheKey != _cachedMKey || M.data != _cachedMData) {
                    M.copyTo(_MU);
                    _cachedMKey  = cacheKey;
                    _cachedMData = M.data;
                }

                cv::ocl::Kernel kernel("gpu_color_transform", prog);
                if (kernel.empty())
                    throw cv::Exception(cv::Error::StsError,
                        "gpu_color_transform kernel not found", __func__, __FILE__, __LINE__);

                int arg = 0;
                arg = kernel.set(arg, cv::ocl::KernelArg::ReadOnly(_srcU));
                arg = kernel.set(arg, cv::ocl::KernelArg::WriteOnly(_dstU));
                arg = kernel.set(arg, cv::ocl::KernelArg::PtrReadOnly(_MU));
                arg = kernel.set(arg, src_ch);
                arg = kernel.set(arg, dst_ch);
                arg = kernel.set(arg, m_cols);

                // Use explicit work-group size for better scheduling
                size_t lsz[2] = { 16, 16 };
                size_t gsz[2] = {
                    (size_t)(((cols + 15) / 16) * 16),
                    (size_t)(((rows + 15) / 16) * 16)
                };
                // Non-blocking: getMat() below implicitly waits
                bool ok = kernel.run(2, gsz, lsz, /*sync=*/false);
                if (!ok)
                    throw cv::Exception(cv::Error::StsError,
                        "kernel.run() failed", __func__, __FILE__, __LINE__);

                cv::Mat result32;
                _dstU.copyTo(result32);

                // Convert to requested output depth
                cv::Mat result;
                if (outputDepth == CV_32F) {
                    result = result32;
                } else {
                    double scale = (outputDepth == CV_8U) ? 255.0 : 65535.0;
                    if (inputDepth != CV_32F) scale = 1.0; // already in 0..255 range
                    result32.convertTo(result, CV_MAKE_TYPE(outputDepth, dst_ch), scale);
                }

                if (ctx.verbose)
                    std::cout << "[GPU-TRANSFORM] Native OCL: "
                              << src_ch << "→" << dst_ch << "ch "
                              << cols << "x" << rows << std::endl;
                return ExecutionResult::ok(result);

            } catch (const cv::Exception& e) {
                if (ctx.verbose)
                    std::cout << "[GPU-TRANSFORM] Native kernel error: " << e.what()
                              << " — falling back to T-API" << std::endl;
            }
        }
    }

    // ===== Path 2: OpenCV T-API (cv::transform + UMat) =====
    if (haveOCL) {
        try {
            src32.copyTo(_srcU);          // reuses existing buffer
            cv::transform(_srcU, _dstU, M);
            cv::Mat result32;
            _dstU.copyTo(result32);

            cv::Mat result;
            if (outputDepth == CV_32F) {
                result = result32;
            } else {
                double scale = (outputDepth == CV_8U) ? 255.0 : 65535.0;
                if (inputDepth != CV_32F) scale = 1.0;
                result32.convertTo(result, CV_MAKE_TYPE(outputDepth, dst_ch), scale);
            }

            if (ctx.verbose)
                std::cout << "[GPU-TRANSFORM] OpenCV T-API: "
                          << src_ch << "→" << dst_ch << "ch "
                          << src32.cols << "x" << src32.rows << std::endl;
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            if (ctx.verbose)
                std::cout << "[GPU-TRANSFORM] T-API error: " << e.what()
                          << " — falling back to CPU" << std::endl;
        }
    }
#endif // VISIONPIPE_OPENCL_ENABLED

    // ===== Path 3: CPU fallback =====
    {
        try {
            cv::Mat result32;
            cv::transform(src32, result32, M);

            cv::Mat result;
            if (outputDepth == CV_32F) {
                result = result32;
            } else {
                double scale = (outputDepth == CV_8U) ? 255.0 : 65535.0;
                if (inputDepth != CV_32F) scale = 1.0;
                result32.convertTo(result, CV_MAKE_TYPE(outputDepth, dst_ch), scale);
            }

            if (ctx.verbose)
                std::cout << "[GPU-TRANSFORM] CPU fallback: "
                          << src_ch << "→" << dst_ch << "ch "
                          << src32.cols << "x" << src32.rows << std::endl;
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            return ExecutionResult::fail(
                "gpu_transform: OpenCV error: " + std::string(e.what()));
        }
    }
}

// ============================================================================
// GpuMatrixMulItem — constructor
// ============================================================================

GpuMatrixMulItem::GpuMatrixMulItem() {
    _functionName = "gpu_matrix_mul";
    _description  =
        "GPU-accelerated general matrix multiplication (GEMM). "
        "Computes: dst = alpha * op(A) * op(B) + beta * C, "
        "where A = currentMat, B and C (optional) are loaded from cache. "
        "op() is controlled by the flags parameter. "
        "Input matrices are automatically converted to CV_32F. "
        "Primary: native OpenCL tiled GEMM kernel. "
        "Fallback: OpenCV T-API (cv::gemm + UMat). "
        "CPU fallback: cv::gemm.";
    _category   = "arithmetic";
    _params = {
        ParamDef::required("src2_cache",
            BaseType::STRING,
            "Cache key for matrix B (second operand)"),
        ParamDef::optional("alpha",
            BaseType::FLOAT,
            "Scale factor for the product A*B",
            1.0),
        ParamDef::optional("src3_cache",
            BaseType::STRING,
            "Cache key for addend matrix C (empty = no addend)",
            std::string("")),
        ParamDef::optional("beta",
            BaseType::FLOAT,
            "Scale factor for C",
            0.0),
        ParamDef::optional("flags",
            BaseType::STRING,
            "Transpose flags: \"none\", \"at\" (transpose A), \"bt\" (transpose B), \"atbt\"",
            std::string("none"))
    };
    _example    = R"(gpu_matrix_mul("B") | gpu_matrix_mul("W", 1.0, "", 0.0, "none") | gpu_matrix_mul("B", 2.0, "C", 1.0, "at"))";
    _returnType = "mat";
    _tags       = {"gpu", "opencl", "matrix", "multiply", "gemm",
                   "linear algebra", "acceleration"};
}

// ============================================================================
// GpuMatrixMulItem — lazy OCL program
// ============================================================================

#ifdef VISIONPIPE_OPENCL_ENABLED
cv::ocl::Program GpuMatrixMulItem::getGemmProgram(bool verbose)
{
    std::lock_guard<std::mutex> lk(_programMutex);
    if (_programBuilt) return _program;
    _programBuilt = true;

    cv::ocl::ProgramSource src(GPU_MATMUL_CL);
    cv::String log;
    _program   = cv::ocl::Program(src, "", log);
    _programOk = (_program.ptr() != nullptr);

    if (!_programOk)
        std::cerr << "[GPU-MATMUL] Compile failed:\n" << log << std::endl;
    else if (verbose)
        std::cout << "[GPU-MATMUL] gpu_matrix_mul (tiled GEMM) kernel compiled"
                  << std::endl;
    return _program;
}
#endif

// ============================================================================
// GpuMatrixMulItem — execute
// ============================================================================

ExecutionResult GpuMatrixMulItem::execute(const std::vector<RuntimeValue>& args,
                                          ExecutionContext& ctx)
{
    if (ctx.currentMat.empty())
        return ExecutionResult::ok(ctx.currentMat);

    // ---- arguments ----
    if (args.empty())
        return ExecutionResult::fail("gpu_matrix_mul: src2_cache argument required");

    std::string src2Key  = args[0].asString();
    double      alpha    = args.size() > 1 ? args[1].asNumber() : 1.0;
    std::string src3Key  = args.size() > 2 ? args[2].asString() : "";
    double      beta     = args.size() > 3 ? args[3].asNumber() : 0.0;
    std::string flagsStr = args.size() > 4 ? args[4].asString() : "none";

    // ---- load matrices ----
    cv::Mat B_mat = ctx.cacheManager->get(src2Key);
    if (B_mat.empty())
        return ExecutionResult::fail("gpu_matrix_mul: matrix B not found in cache: " + src2Key);

    cv::Mat C_mat;
    bool hasC = false;
    if (!src3Key.empty() && beta != 0.0) {
        C_mat = ctx.cacheManager->get(src3Key);
        hasC  = !C_mat.empty();
    }

    // ---- transpose flags ----
    int flags = 0;
    std::string fl = flagsStr;
    std::transform(fl.begin(), fl.end(), fl.begin(), ::tolower);
    if (fl.find("at") != std::string::npos) flags |= cv::GEMM_1_T;
    if (fl.find("bt") != std::string::npos) flags |= cv::GEMM_2_T;

    // ---- convert all to float32 ----
    cv::Mat A32, B32, C32;
    ctx.currentMat.convertTo(A32, CV_32F);
    B_mat.convertTo(B32, CV_32F);
    if (hasC) C_mat.convertTo(C32, CV_32F);

    // Apply transpose flags on host before kernel (simpler than in-kernel transpose)
    cv::Mat Aeff = (flags & cv::GEMM_1_T) ? A32.t() : A32;
    cv::Mat Beff = (flags & cv::GEMM_2_T) ? B32.t() : B32;

    // Dimension check
    if (Aeff.channels() != 1 || Beff.channels() != 1)
        return ExecutionResult::fail(
            "gpu_matrix_mul: matrices must be single-channel (2-D)");
    if (Aeff.cols != Beff.rows)
        return ExecutionResult::fail(
            "gpu_matrix_mul: inner dimensions mismatch: A.cols=" +
            std::to_string(Aeff.cols) + " != B.rows=" + std::to_string(Beff.rows));

#ifdef VISIONPIPE_OPENCL_ENABLED
    bool haveOCL = false;
    try { haveOCL = cv::ocl::useOpenCL() && cv::ocl::haveOpenCL(); } catch (...) {}

    // ===== Path 1: Native OCL tiled GEMM =====
    if (haveOCL) {
        cv::ocl::Program prog = getGemmProgram(ctx.verbose);
        if (_programOk) {
            try {
                const int M = Aeff.rows;
                const int N = Beff.cols;

                // Persistent GPU buffers: reuse across frames
                Aeff.copyTo(_AU);             // reuses buffer
                Beff.copyTo(_BU);             // reuses buffer
                _DU.create(M, N, CV_32F);     // no-op if same size

                cv::ocl::Kernel kernel("gpu_matrix_mul", prog);
                if (kernel.empty())
                    throw cv::Exception(cv::Error::StsError,
                        "gpu_matrix_mul kernel not found", __func__, __FILE__, __LINE__);

                int arg = 0;
                arg = kernel.set(arg, cv::ocl::KernelArg::ReadOnly(_AU));
                arg = kernel.set(arg, cv::ocl::KernelArg::ReadOnly(_BU));
                arg = kernel.set(arg, cv::ocl::KernelArg::WriteOnly(_DU));
                arg = kernel.set(arg, (float)alpha);

                // Round global size up to GEMM_TILE multiple
                const int TILE = 16;
                size_t lsz[2] = { (size_t)TILE, (size_t)TILE };
                size_t gsz[2] = {
                    (size_t)(((N + TILE - 1) / TILE) * TILE),
                    (size_t)(((M + TILE - 1) / TILE) * TILE)
                };

                // Non-blocking: copyTo below waits for completion
                bool ok = kernel.run(2, gsz, lsz, /*sync=*/false);
                if (!ok)
                    throw cv::Exception(cv::Error::StsError,
                        "kernel.run() failed", __func__, __FILE__, __LINE__);

                cv::Mat result;
                _DU.copyTo(result);

                // Add scaled C if requested
                if (hasC && beta != 0.0) {
                    if (C32.rows == result.rows && C32.cols == result.cols)
                        cv::addWeighted(result, 1.0, C32, beta, 0.0, result);
                }

                if (ctx.verbose)
                    std::cout << "[GPU-MATMUL] Native OCL tiled GEMM: "
                              << M << "x" << Aeff.cols << " * "
                              << Aeff.cols << "x" << N << std::endl;
                return ExecutionResult::ok(result);

            } catch (const cv::Exception& e) {
                if (ctx.verbose)
                    std::cout << "[GPU-MATMUL] Native kernel error: " << e.what()
                              << " — falling back to T-API" << std::endl;
            }
        }
    }

    // ===== Path 2: OpenCV T-API (cv::gemm + UMat) =====
    if (haveOCL) {
        try {
            // Reuse persistent buffers for A and B
            A32.copyTo(_AU);
            B32.copyTo(_BU);
            cv::UMat CU;
            if (hasC) C32.copyTo(CU);

            cv::gemm(_AU, _BU, alpha, hasC ? CU : cv::UMat(), beta, _DU, flags);

            cv::Mat result;
            _DU.copyTo(result);

            if (ctx.verbose)
                std::cout << "[GPU-MATMUL] OpenCV T-API gemm: "
                          << A32.rows << "x" << A32.cols << std::endl;
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            if (ctx.verbose)
                std::cout << "[GPU-MATMUL] T-API error: " << e.what()
                          << " — falling back to CPU" << std::endl;
        }
    }
#endif // VISIONPIPE_OPENCL_ENABLED

    // ===== Path 3: CPU fallback =====
    {
        try {
            cv::Mat result;
            cv::gemm(A32, B32, alpha, hasC ? C32 : cv::Mat(), beta, result, flags);

            if (ctx.verbose)
                std::cout << "[GPU-MATMUL] CPU fallback: "
                          << A32.rows << "x" << A32.cols << std::endl;
            return ExecutionResult::ok(result);
        } catch (const cv::Exception& e) {
            return ExecutionResult::fail(
                "gpu_matrix_mul: OpenCV error: " + std::string(e.what()));
        }
    }
}

} // namespace visionpipe

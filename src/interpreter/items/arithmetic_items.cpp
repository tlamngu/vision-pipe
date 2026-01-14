#include "interpreter/items/arithmetic_items.h"
#include "interpreter/items/feature_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <cmath>

namespace visionpipe {

void registerArithmeticItems(ItemRegistry& registry) {
    // Matrix operations
    registry.add<GemmItem>();
    registry.add<TransformItem>();
    // PerspectiveTransformItem is registered in registerFeatureItems
    registry.add<InvertItem>();
    registry.add<SolveItem>();
    registry.add<EigenItem>();
    registry.add<SVDItem>();
    
    // DFT operations
    registry.add<DFTItem>();
    registry.add<IDFTItem>();
    registry.add<MulSpectrumsItem>();
    registry.add<DCTItem>();
    registry.add<IDCTItem>();
    registry.add<MagnitudeItem>();
    registry.add<PhaseItem>();
    
    // Statistical operations
    registry.add<NormItem>();
    registry.add<NormalizeItem>();
    registry.add<ReduceItem>();
    registry.add<SumItem>();
    registry.add<CountNonZeroItem>();
    
    // Math operations
    registry.add<ExpItem>();
    registry.add<NaturalLogItem>();
    registry.add<PowItem>();
    registry.add<SqrtItem>();
    registry.add<CartToPolarItem>();
    registry.add<PolarToCartItem>();
    
    // Random operations
    registry.add<RandnItem>();
    registry.add<RanduItem>();
    registry.add<RandShuffleItem>();
    
    // Lookup operations
    registry.add<LUTItem>();
    // MinMaxLocItem is registered in registerFeatureItems
}

// ============================================================================
// GemmItem
// ============================================================================

GemmItem::GemmItem() {
    _functionName = "gemm";
    _description = "Generalized matrix multiplication";
    _category = "arithmetic";
    _params = {
        ParamDef::required("src2_cache", BaseType::STRING, "Second matrix cache ID"),
        ParamDef::required("alpha", BaseType::FLOAT, "Alpha multiplier"),
        ParamDef::optional("src3_cache", BaseType::STRING, "Third matrix cache ID (optional)", ""),
        ParamDef::optional("beta", BaseType::FLOAT, "Beta multiplier for src3", 0.0),
        ParamDef::optional("flags", BaseType::STRING, "Flags: none, at, bt, atbt", "none")
    };
    _example = "gemm(\"B\", 1.0, \"C\", 1.0, \"none\")";
    _returnType = "mat";
    _tags = {"matrix", "multiply", "gemm"};
}

ExecutionResult GemmItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string src2Cache = args[0].asString();
    double alpha = args[1].asNumber();
    std::string src3Cache = args.size() > 2 ? args[2].asString() : "";
    double beta = args.size() > 3 ? args[3].asNumber() : 0.0;
    std::string flagsStr = args.size() > 4 ? args[4].asString() : "none";
    
    cv::Mat src2Opt = ctx.cacheManager->get(src2Cache);
    if (src2Opt.empty()) {
        return ExecutionResult::fail("Matrix not found: " + src2Cache);
    }
    
    cv::Mat src3 = cv::Mat();
    if (!src3Cache.empty()) {
        cv::Mat src3Opt = ctx.cacheManager->get(src3Cache);
        if (!src3Opt.empty()) src3 = src3Opt;
    }
    
    int flags = 0;
    if (flagsStr.find("at") != std::string::npos) flags |= cv::GEMM_1_T;
    if (flagsStr.find("bt") != std::string::npos) flags |= cv::GEMM_2_T;
    if (flagsStr == "atbt") flags = cv::GEMM_1_T | cv::GEMM_2_T;
    
    cv::Mat result;
    cv::gemm(ctx.currentMat, src2Opt, alpha, src3, beta, result, flags);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// TransformItem
// ============================================================================

TransformItem::TransformItem() {
    _functionName = "transform";
    _description = "Applies matrix transformation to each pixel";
    _category = "arithmetic";
    _params = {
        ParamDef::required("matrix_cache", BaseType::STRING, "Transformation matrix cache ID")
    };
    _example = "transform(\"color_transform\")";
    _returnType = "mat";
    _tags = {"transform", "matrix"};
}

ExecutionResult TransformItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string matrixCache = args[0].asString();
    
    cv::Mat matrixOpt = ctx.cacheManager->get(matrixCache);
    if (matrixOpt.empty()) {
        return ExecutionResult::fail("Matrix not found: " + matrixCache);
    }
    
    cv::Mat result;
    cv::transform(ctx.currentMat, result, matrixOpt);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// InvertItem
// ============================================================================

InvertItem::InvertItem() {
    _functionName = "invert";
    _description = "Computes matrix inverse";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("method", BaseType::STRING, "Method: lu, svd, eig, cholesky", "lu")
    };
    _example = "invert(\"lu\")";
    _returnType = "mat";
    _tags = {"matrix", "inverse"};
}

ExecutionResult InvertItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string method = args.size() > 0 ? args[0].asString() : "lu";
    
    int flag = cv::DECOMP_LU;
    if (method == "svd") flag = cv::DECOMP_SVD;
    else if (method == "eig") flag = cv::DECOMP_EIG;
    else if (method == "cholesky") flag = cv::DECOMP_CHOLESKY;
    
    cv::Mat result;
    double det = cv::invert(ctx.currentMat, result, flag);
    
    if (det == 0) {
        return ExecutionResult::fail("Matrix is singular, cannot invert");
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SolveItem
// ============================================================================

SolveItem::SolveItem() {
    _functionName = "solve";
    _description = "Solves linear system Ax = b";
    _category = "arithmetic";
    _params = {
        ParamDef::required("b_cache", BaseType::STRING, "Right-hand side matrix cache ID"),
        ParamDef::optional("method", BaseType::STRING, "Method: lu, svd, eig, cholesky, qr, normal", "lu"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for solution", "solution")
    };
    _example = "solve(\"b\", \"lu\", \"x\")";
    _returnType = "mat";
    _tags = {"matrix", "solve", "linear"};
}

ExecutionResult SolveItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string bCache = args[0].asString();
    std::string method = args.size() > 1 ? args[1].asString() : "lu";
    std::string cacheId = args.size() > 2 ? args[2].asString() : "solution";
    
    cv::Mat bOpt = ctx.cacheManager->get(bCache);
    if (bOpt.empty()) {
        return ExecutionResult::fail("Matrix not found: " + bCache);
    }
    
    int flag = cv::DECOMP_LU;
    if (method == "svd") flag = cv::DECOMP_SVD;
    else if (method == "eig") flag = cv::DECOMP_EIG;
    else if (method == "cholesky") flag = cv::DECOMP_CHOLESKY;
    else if (method == "qr") flag = cv::DECOMP_QR;
    else if (method == "normal") flag = cv::DECOMP_NORMAL;
    
    cv::Mat solution;
    bool success = cv::solve(ctx.currentMat, bOpt, solution, flag);
    
    if (!success) {
        return ExecutionResult::fail("Failed to solve linear system");
    }
    
    ctx.cacheManager->set(cacheId, solution);
    
    return ExecutionResult::ok(solution);
}

// ============================================================================
// EigenItem
// ============================================================================

EigenItem::EigenItem() {
    _functionName = "eigen";
    _description = "Computes eigenvalues and eigenvectors of symmetric matrix";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache prefix for results", "eigen")
    };
    _example = "eigen(\"eigen\")";
    _returnType = "mat";
    _tags = {"matrix", "eigen", "eigenvalue"};
}

ExecutionResult EigenItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args.size() > 0 ? args[0].asString() : "eigen";
    
    cv::Mat eigenvalues, eigenvectors;
    bool success = cv::eigen(ctx.currentMat, eigenvalues, eigenvectors);
    
    if (!success) {
        return ExecutionResult::fail("Eigen decomposition failed");
    }
    
    ctx.cacheManager->set(prefix + "_values", eigenvalues);
    ctx.cacheManager->set(prefix + "_vectors", eigenvectors);
    
    return ExecutionResult::ok(eigenvalues);
}

// ============================================================================
// SVDItem
// ============================================================================

SVDItem::SVDItem() {
    _functionName = "svd";
    _description = "Singular Value Decomposition";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("flags", BaseType::STRING, "Flags: none, modify_a, no_uv, full_uv", "none"),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache prefix for results", "svd")
    };
    _example = "svd(\"none\", \"svd\")";
    _returnType = "mat";
    _tags = {"matrix", "svd", "decomposition"};
}

ExecutionResult SVDItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string flagsStr = args.size() > 0 ? args[0].asString() : "none";
    std::string prefix = args.size() > 1 ? args[1].asString() : "svd";
    
    int flags = 0;
    if (flagsStr.find("modify_a") != std::string::npos) flags |= cv::SVD::MODIFY_A;
    if (flagsStr.find("no_uv") != std::string::npos) flags |= cv::SVD::NO_UV;
    if (flagsStr.find("full_uv") != std::string::npos) flags |= cv::SVD::FULL_UV;
    
    cv::Mat w, u, vt;
    cv::SVD::compute(ctx.currentMat, w, u, vt, flags);
    
    ctx.cacheManager->set(prefix + "_w", w);
    ctx.cacheManager->set(prefix + "_u", u);
    ctx.cacheManager->set(prefix + "_vt", vt);
    
    return ExecutionResult::ok(w);
}

// ============================================================================
// DFTItem
// ============================================================================

DFTItem::DFTItem() {
    _functionName = "dft";
    _description = "Discrete Fourier Transform";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("flags", BaseType::STRING, "Flags: none, inverse, scale, rows, complex_output, real_output", "none"),
        ParamDef::optional("nonzero_rows", BaseType::INT, "Non-zero input rows (0 = all)", 0)
    };
    _example = "dft(\"complex_output\", 0)";
    _returnType = "mat";
    _tags = {"dft", "fourier", "frequency"};
}

ExecutionResult DFTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string flagsStr = args.size() > 0 ? args[0].asString() : "none";
    int nonzeroRows = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    
    int flags = 0;
    if (flagsStr.find("inverse") != std::string::npos) flags |= cv::DFT_INVERSE;
    if (flagsStr.find("scale") != std::string::npos) flags |= cv::DFT_SCALE;
    if (flagsStr.find("rows") != std::string::npos) flags |= cv::DFT_ROWS;
    if (flagsStr.find("complex_output") != std::string::npos) flags |= cv::DFT_COMPLEX_OUTPUT;
    if (flagsStr.find("real_output") != std::string::npos) flags |= cv::DFT_REAL_OUTPUT;
    
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F);
    
    cv::Mat result;
    cv::dft(floatMat, result, flags, nonzeroRows);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// IDFTItem
// ============================================================================

IDFTItem::IDFTItem() {
    _functionName = "idft";
    _description = "Inverse Discrete Fourier Transform";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("flags", BaseType::STRING, "Flags: none, scale, rows, real_output", "scale+real_output"),
        ParamDef::optional("nonzero_rows", BaseType::INT, "Non-zero input rows (0 = all)", 0)
    };
    _example = "idft(\"scale+real_output\", 0)";
    _returnType = "mat";
    _tags = {"dft", "fourier", "inverse"};
}

ExecutionResult IDFTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string flagsStr = args.size() > 0 ? args[0].asString() : "scale+real_output";
    int nonzeroRows = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    
    int flags = cv::DFT_INVERSE;
    if (flagsStr.find("scale") != std::string::npos) flags |= cv::DFT_SCALE;
    if (flagsStr.find("rows") != std::string::npos) flags |= cv::DFT_ROWS;
    if (flagsStr.find("real_output") != std::string::npos) flags |= cv::DFT_REAL_OUTPUT;
    
    cv::Mat result;
    cv::idft(ctx.currentMat, result, flags, nonzeroRows);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MulSpectrumsItem
// ============================================================================

MulSpectrumsItem::MulSpectrumsItem() {
    _functionName = "mul_spectrums";
    _description = "Per-element multiplication of two Fourier spectrums";
    _category = "arithmetic";
    _params = {
        ParamDef::required("other_cache", BaseType::STRING, "Second spectrum cache ID"),
        ParamDef::optional("flags", BaseType::STRING, "Flags: none, rows, conj", "none")
    };
    _example = "mul_spectrums(\"kernel_dft\", \"none\")";
    _returnType = "mat";
    _tags = {"dft", "multiply", "spectrum"};
}

ExecutionResult MulSpectrumsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string otherCache = args[0].asString();
    std::string flagsStr = args.size() > 1 ? args[1].asString() : "none";
    
    cv::Mat otherOpt = ctx.cacheManager->get(otherCache);
    if (otherOpt.empty()) {
        return ExecutionResult::fail("Spectrum not found: " + otherCache);
    }
    
    int flags = 0;
    if (flagsStr.find("rows") != std::string::npos) flags |= cv::DFT_ROWS;
    bool conjB = flagsStr.find("conj") != std::string::npos;
    
    cv::Mat result;
    cv::mulSpectrums(ctx.currentMat, otherOpt, result, flags, conjB);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DCTItem
// ============================================================================

DCTItem::DCTItem() {
    _functionName = "dct";
    _description = "Discrete Cosine Transform";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("flags", BaseType::STRING, "Flags: none, inverse, rows", "none")
    };
    _example = "dct(\"none\")";
    _returnType = "mat";
    _tags = {"dct", "cosine", "transform"};
}

ExecutionResult DCTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string flagsStr = args.size() > 0 ? args[0].asString() : "none";
    
    int flags = 0;
    if (flagsStr.find("inverse") != std::string::npos) flags |= cv::DCT_INVERSE;
    if (flagsStr.find("rows") != std::string::npos) flags |= cv::DCT_ROWS;
    
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F);
    
    cv::Mat result;
    cv::dct(floatMat, result, flags);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// IDCTItem
// ============================================================================

IDCTItem::IDCTItem() {
    _functionName = "idct";
    _description = "Inverse Discrete Cosine Transform";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("flags", BaseType::STRING, "Flags: none, rows", "none")
    };
    _example = "idct(\"none\")";
    _returnType = "mat";
    _tags = {"dct", "cosine", "inverse"};
}

ExecutionResult IDCTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string flagsStr = args.size() > 0 ? args[0].asString() : "none";
    
    int flags = cv::DCT_INVERSE;
    if (flagsStr.find("rows") != std::string::npos) flags |= cv::DCT_ROWS;
    
    cv::Mat result;
    cv::idct(ctx.currentMat, result, flags);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MagnitudeItem
// ============================================================================

MagnitudeItem::MagnitudeItem() {
    _functionName = "magnitude";
    _description = "Calculates magnitude of 2D vectors or complex values";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("y_cache", BaseType::STRING, "Y component cache (if not complex)", "")
    };
    _example = "magnitude(\"y\")";
    _returnType = "mat";
    _tags = {"magnitude", "complex", "vector"};
}

ExecutionResult MagnitudeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string yCache = args.size() > 0 ? args[0].asString() : "";
    
    cv::Mat result;
    
    if (yCache.empty()) {
        // Assume complex input
        std::vector<cv::Mat> planes;
        cv::split(ctx.currentMat, planes);
        if (planes.size() >= 2) {
            cv::magnitude(planes[0], planes[1], result);
        } else {
            result = ctx.currentMat.clone();
        }
    } else {
        cv::Mat yOpt = ctx.cacheManager->get(yCache);
        if (yOpt.empty()) {
            return ExecutionResult::fail("Y component not found: " + yCache);
        }
        cv::magnitude(ctx.currentMat, yOpt, result);
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// PhaseItem
// ============================================================================

PhaseItem::PhaseItem() {
    _functionName = "phase";
    _description = "Calculates phase angle of 2D vectors";
    _category = "arithmetic";
    _params = {
        ParamDef::required("y_cache", BaseType::STRING, "Y component cache"),
        ParamDef::optional("angle_in_degrees", BaseType::BOOL, "Return angle in degrees", false)
    };
    _example = "phase(\"y\", true)";
    _returnType = "mat";
    _tags = {"phase", "angle", "vector"};
}

ExecutionResult PhaseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string yCache = args[0].asString();
    bool angleInDegrees = args.size() > 1 ? args[1].asBool() : false;
    
    cv::Mat yOpt = ctx.cacheManager->get(yCache);
    if (yOpt.empty()) {
        return ExecutionResult::fail("Y component not found: " + yCache);
    }
    
    cv::Mat result;
    cv::phase(ctx.currentMat, yOpt, result, angleInDegrees);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// NormItem
// ============================================================================

NormItem::NormItem() {
    _functionName = "norm";
    _description = "Calculates matrix norm";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("norm_type", BaseType::STRING, "Type: l1, l2, linf, l2sqr", "l2"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", "")
    };
    _example = "norm(\"l2\")";
    _returnType = "mat";
    _tags = {"norm", "distance"};
}

ExecutionResult NormItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string normType = args.size() > 0 ? args[0].asString() : "l2";
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    int type = cv::NORM_L2;
    if (normType == "l1") type = cv::NORM_L1;
    else if (normType == "linf") type = cv::NORM_INF;
    else if (normType == "l2sqr") type = cv::NORM_L2SQR;
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    double norm = cv::norm(ctx.currentMat, type, mask);
    
    std::cout << "Norm (" << normType << "): " << norm << std::endl;
    ctx.variables["norm"] = norm;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// NormalizeItem
// ============================================================================

NormalizeItem::NormalizeItem() {
    _functionName = "normalize";
    _description = "Normalizes array values";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("alpha", BaseType::FLOAT, "Lower range boundary or norm value", 0.0),
        ParamDef::optional("beta", BaseType::FLOAT, "Upper range boundary", 255.0),
        ParamDef::optional("norm_type", BaseType::STRING, "Type: minmax, l1, l2, linf", "minmax"),
        ParamDef::optional("dtype", BaseType::STRING, "Output depth: -1, 8u, 32f", "-1"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", "")
    };
    _example = "normalize(0, 255, \"minmax\", \"8u\")";
    _returnType = "mat";
    _tags = {"normalize", "scale"};
}

ExecutionResult NormalizeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double alpha = args.size() > 0 ? args[0].asNumber() : 0.0;
    double beta = args.size() > 1 ? args[1].asNumber() : 255.0;
    std::string normTypeStr = args.size() > 2 ? args[2].asString() : "minmax";
    std::string dtypeStr = args.size() > 3 ? args[3].asString() : "-1";
    std::string maskCache = args.size() > 4 ? args[4].asString() : "";
    
    int normType = cv::NORM_MINMAX;
    if (normTypeStr == "l1") normType = cv::NORM_L1;
    else if (normTypeStr == "l2") normType = cv::NORM_L2;
    else if (normTypeStr == "linf") normType = cv::NORM_INF;
    
    int dtype = -1;
    if (dtypeStr == "8u") dtype = CV_8U;
    else if (dtypeStr == "32f") dtype = CV_32F;
    else if (dtypeStr == "64f") dtype = CV_64F;
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    cv::Mat result;
    cv::normalize(ctx.currentMat, result, alpha, beta, normType, dtype, mask);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ReduceItem
// ============================================================================

ReduceItem::ReduceItem() {
    _functionName = "reduce";
    _description = "Reduces matrix to vector by applying operation";
    _category = "arithmetic";
    _params = {
        ParamDef::required("dim", BaseType::INT, "Dimension to reduce: 0=rows, 1=cols"),
        ParamDef::optional("rtype", BaseType::STRING, "Operation: sum, avg, max, min", "sum"),
        ParamDef::optional("dtype", BaseType::STRING, "Output depth: -1, 32f, 64f", "-1")
    };
    _example = "reduce(0, \"sum\", \"32f\")";
    _returnType = "mat";
    _tags = {"reduce", "aggregate"};
}

ExecutionResult ReduceItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int dim = static_cast<int>(args[0].asNumber());
    std::string rtypeStr = args.size() > 1 ? args[1].asString() : "sum";
    std::string dtypeStr = args.size() > 2 ? args[2].asString() : "-1";
    
    int rtype = cv::REDUCE_SUM;
    if (rtypeStr == "avg") rtype = cv::REDUCE_AVG;
    else if (rtypeStr == "max") rtype = cv::REDUCE_MAX;
    else if (rtypeStr == "min") rtype = cv::REDUCE_MIN;
    
    int dtype = -1;
    if (dtypeStr == "32f") dtype = CV_32F;
    else if (dtypeStr == "64f") dtype = CV_64F;
    
    cv::Mat result;
    cv::reduce(ctx.currentMat, result, dim, rtype, dtype);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SumItem
// ============================================================================

SumItem::SumItem() {
    _functionName = "sum";
    _description = "Calculates sum of all elements";
    _category = "arithmetic";
    _params = {};
    _example = "sum()";
    _returnType = "mat";
    _tags = {"sum", "aggregate"};
}

ExecutionResult SumItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Scalar s = cv::sum(ctx.currentMat);
    
    std::cout << "Sum: [" << s[0] << ", " << s[1] << ", " << s[2] << ", " << s[3] << "]" << std::endl;
    
    ctx.variables["sum_0"] = s[0];
    ctx.variables["sum_1"] = s[1];
    ctx.variables["sum_2"] = s[2];
    ctx.variables["sum_3"] = s[3];
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CountNonZeroItem
// ============================================================================

CountNonZeroItem::CountNonZeroItem() {
    _functionName = "count_non_zero";
    _description = "Counts non-zero elements";
    _category = "arithmetic";
    _params = {};
    _example = "count_non_zero()";
    _returnType = "mat";
    _tags = {"count", "nonzero"};
}

ExecutionResult CountNonZeroItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat gray = ctx.currentMat;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }
    
    int count = cv::countNonZero(gray);
    
    std::cout << "Non-zero count: " << count << std::endl;
    ctx.variables["nonzero_count"] = static_cast<double>(count);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ExpItem
// ============================================================================

ExpItem::ExpItem() {
    _functionName = "exp";
    _description = "Calculates element-wise exponential";
    _category = "arithmetic";
    _params = {};
    _example = "exp()";
    _returnType = "mat";
    _tags = {"math", "exp"};
}

ExecutionResult ExpItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F);
    
    cv::Mat result;
    cv::exp(floatMat, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// NaturalLogItem
// ============================================================================

NaturalLogItem::NaturalLogItem() {
    _functionName = "ln";
    _description = "Calculates element-wise natural logarithm";
    _category = "arithmetic";
    _params = {};
    _example = "ln()";
    _returnType = "mat";
    _tags = {"math", "log"};
}

ExecutionResult NaturalLogItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F);
    floatMat = cv::max(floatMat, 1e-10);  // Avoid log(0)
    
    cv::Mat result;
    cv::log(floatMat, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// PowItem
// ============================================================================

PowItem::PowItem() {
    _functionName = "pow";
    _description = "Raises every element to a power";
    _category = "arithmetic";
    _params = {
        ParamDef::required("power", BaseType::FLOAT, "Exponent value")
    };
    _example = "pow(2.0)";
    _returnType = "mat";
    _tags = {"math", "power"};
}

ExecutionResult PowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double power = args[0].asNumber();
    
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F);
    
    cv::Mat result;
    cv::pow(floatMat, power, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SqrtItem
// ============================================================================

SqrtItem::SqrtItem() {
    _functionName = "sqrt";
    _description = "Calculates element-wise square root";
    _category = "arithmetic";
    _params = {};
    _example = "sqrt()";
    _returnType = "mat";
    _tags = {"math", "sqrt"};
}

ExecutionResult SqrtItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat floatMat;
    ctx.currentMat.convertTo(floatMat, CV_32F);
    floatMat = cv::max(floatMat, 0);  // Ensure non-negative
    
    cv::Mat result;
    cv::sqrt(floatMat, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// CartToPolarItem
// ============================================================================

CartToPolarItem::CartToPolarItem() {
    _functionName = "cart_to_polar";
    _description = "Converts Cartesian coordinates to polar";
    _category = "arithmetic";
    _params = {
        ParamDef::required("y_cache", BaseType::STRING, "Y component cache"),
        ParamDef::optional("angle_in_degrees", BaseType::BOOL, "Angle in degrees", false),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "polar")
    };
    _example = "cart_to_polar(\"y\", true, \"polar\")";
    _returnType = "mat";
    _tags = {"polar", "cartesian", "convert"};
}

ExecutionResult CartToPolarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string yCache = args[0].asString();
    bool angleInDegrees = args.size() > 1 ? args[1].asBool() : false;
    std::string prefix = args.size() > 2 ? args[2].asString() : "polar";
    
    cv::Mat yOpt = ctx.cacheManager->get(yCache);
    if (yOpt.empty()) {
        return ExecutionResult::fail("Y component not found: " + yCache);
    }
    
    cv::Mat magnitude, angle;
    cv::cartToPolar(ctx.currentMat, yOpt, magnitude, angle, angleInDegrees);
    
    ctx.cacheManager->set(prefix + "_mag", magnitude);
    ctx.cacheManager->set(prefix + "_angle", angle);
    
    return ExecutionResult::ok(magnitude);
}

// ============================================================================
// PolarToCartItem
// ============================================================================

PolarToCartItem::PolarToCartItem() {
    _functionName = "polar_to_cart";
    _description = "Converts polar coordinates to Cartesian";
    _category = "arithmetic";
    _params = {
        ParamDef::required("angle_cache", BaseType::STRING, "Angle component cache"),
        ParamDef::optional("angle_in_degrees", BaseType::BOOL, "Angle in degrees", false),
        ParamDef::optional("cache_prefix", BaseType::STRING, "Output cache prefix", "cart")
    };
    _example = "polar_to_cart(\"angle\", true, \"cart\")";
    _returnType = "mat";
    _tags = {"polar", "cartesian", "convert"};
}

ExecutionResult PolarToCartItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string angleCache = args[0].asString();
    bool angleInDegrees = args.size() > 1 ? args[1].asBool() : false;
    std::string prefix = args.size() > 2 ? args[2].asString() : "cart";
    
    cv::Mat angleOpt = ctx.cacheManager->get(angleCache);
    if (angleOpt.empty()) {
        return ExecutionResult::fail("Angle not found: " + angleCache);
    }
    
    cv::Mat x, y;
    cv::polarToCart(ctx.currentMat, angleOpt, x, y, angleInDegrees);
    
    ctx.cacheManager->set(prefix + "_x", x);
    ctx.cacheManager->set(prefix + "_y", y);
    
    return ExecutionResult::ok(x);
}

// ============================================================================
// RandnItem
// ============================================================================

RandnItem::RandnItem() {
    _functionName = "randn";
    _description = "Fills matrix with normally distributed random values";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("mean", BaseType::FLOAT, "Mean value", 0.0),
        ParamDef::optional("stddev", BaseType::FLOAT, "Standard deviation", 1.0)
    };
    _example = "randn(0, 50)";
    _returnType = "mat";
    _tags = {"random", "normal", "gaussian"};
}

ExecutionResult RandnItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double mean = args.size() > 0 ? args[0].asNumber() : 0.0;
    double stddev = args.size() > 1 ? args[1].asNumber() : 1.0;
    
    cv::Mat result = ctx.currentMat.clone();
    cv::randn(result, cv::Scalar::all(mean), cv::Scalar::all(stddev));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// RanduItem
// ============================================================================

RanduItem::RanduItem() {
    _functionName = "randu";
    _description = "Fills matrix with uniformly distributed random values";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("low", BaseType::FLOAT, "Lower bound", 0.0),
        ParamDef::optional("high", BaseType::FLOAT, "Upper bound", 255.0)
    };
    _example = "randu(0, 255)";
    _returnType = "mat";
    _tags = {"random", "uniform"};
}

ExecutionResult RanduItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double low = args.size() > 0 ? args[0].asNumber() : 0.0;
    double high = args.size() > 1 ? args[1].asNumber() : 255.0;
    
    cv::Mat result = ctx.currentMat.clone();
    cv::randu(result, cv::Scalar::all(low), cv::Scalar::all(high));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// RandShuffleItem
// ============================================================================

RandShuffleItem::RandShuffleItem() {
    _functionName = "rand_shuffle";
    _description = "Randomly shuffles matrix elements";
    _category = "arithmetic";
    _params = {
        ParamDef::optional("iter_factor", BaseType::FLOAT, "Iteration factor", 1.0)
    };
    _example = "rand_shuffle(1.0)";
    _returnType = "mat";
    _tags = {"random", "shuffle"};
}

ExecutionResult RandShuffleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double iterFactor = args.size() > 0 ? args[0].asNumber() : 1.0;
    
    cv::Mat result = ctx.currentMat.clone();
    cv::randShuffle(result, iterFactor);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// LUTItem
// ============================================================================

LUTItem::LUTItem() {
    _functionName = "lut";
    _description = "Applies lookup table transformation";
    _category = "arithmetic";
    _params = {
        ParamDef::required("lut_cache", BaseType::STRING, "Lookup table cache ID (256 entries)")
    };
    _example = "lut(\"gamma_table\")";
    _returnType = "mat";
    _tags = {"lut", "lookup", "transform"};
}

ExecutionResult LUTItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string lutCache = args[0].asString();
    
    cv::Mat lutOpt = ctx.cacheManager->get(lutCache);
    if (lutOpt.empty()) {
        return ExecutionResult::fail("LUT not found: " + lutCache);
    }
    
    cv::Mat result;
    cv::LUT(ctx.currentMat, lutOpt, result);
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

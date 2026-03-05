#ifndef VISIONPIPE_MATH_EVAL_ITEMS_H
#define VISIONPIPE_MATH_EVAL_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <cmath>

namespace visionpipe {

void registerMathEvalItems(ItemRegistry& registry);

// ============================================================================
// Math Evaluation Item
// ============================================================================

/**
 * @brief Evaluates mathematical expression and stores result in variable
 * 
 * Supports:
 * - Basic arithmetic: +, -, *, /, %, ^
 * - Trigonometry: sin, cos, tan, asin, acos, atan, atan2
 * - Math functions: sqrt, abs, pow, exp, ln, log10
 * - Rounding: floor, ceil, round
 * - Clamping: clamp(value, min, max), min(a, b), max(a, b)
 * - Constants: pi, e
 * - Variables: Uses values from context variables
 * 
 * Parameters:
 * - expression: Math expression string
 * - result_var: Variable name to store result (optional)
 * 
 * Examples:
 * - calc("sin(3.14159 / 2)", "result")
 * - calc("sqrt(x*x + y*y)", "distance")
 * - calc("clamp(threshold, 0, 255)", "safe_thresh")
 */
class CalcItem : public InterpreterItem {
public:
    CalcItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Trigonometric Functions
// ============================================================================

/**
 * @brief Calculates sine of variable value (in radians)
 * 
 * Parameters:
 * - input_var: Input variable name or literal value
 * - output_var: Output variable name
 * - in_degrees: Input is in degrees (default: false)
 */
class SinItem : public InterpreterItem {
public:
    SinItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class CosItem : public InterpreterItem {
public:
    CosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class TanItem : public InterpreterItem {
public:
    TanItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class AsinItem : public InterpreterItem {
public:
    AsinItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class AcosItem : public InterpreterItem {
public:
    AcosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class AtanItem : public InterpreterItem {
public:
    AtanItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class Atan2Item : public InterpreterItem {
public:
    Atan2Item();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Scalar Math Operations
// ============================================================================

/**
 * @brief Calculates absolute value
 */
class AbsScalarItem : public InterpreterItem {
public:
    AbsScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates square root
 */
class SqrtScalarItem : public InterpreterItem {
public:
    SqrtScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Raises value to power
 */
class PowScalarItem : public InterpreterItem {
public:
    PowScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates exponential
 */
class ExpScalarItem : public InterpreterItem {
public:
    ExpScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates natural logarithm
 */
class LnScalarItem : public InterpreterItem {
public:
    LnScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates base-10 logarithm
 */
class Log10ScalarItem : public InterpreterItem {
public:
    Log10ScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Rounds to nearest integer
 */
class RoundScalarItem : public InterpreterItem {
public:
    RoundScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Rounds down to integer
 */
class FloorScalarItem : public InterpreterItem {
public:
    FloorScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Rounds up to integer
 */
class CeilScalarItem : public InterpreterItem {
public:
    CeilScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Range and Comparison Functions
// ============================================================================

/**
 * @brief Clamps value to range [min, max]
 * 
 * Parameters:
 * - value: Value to clamp (variable name or literal)
 * - min: Minimum value
 * - max: Maximum value
 * - output_var: Output variable name
 */
class ClampItem : public InterpreterItem {
public:
    ClampItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Returns minimum of two values
 */
class MinScalarItem : public InterpreterItem {
public:
    MinScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Returns maximum of two values
 */
class MaxScalarItem : public InterpreterItem {
public:
    MaxScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Linear interpolation between two values
 * 
 * Parameters:
 * - start: Start value
 * - end: End value
 * - t: Interpolation factor (0-1)
 * - output_var: Output variable name
 */
class LerpScalarItem : public InterpreterItem {
public:
    LerpScalarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Variable Math Operations
// ============================================================================

/**
 * @brief Adds two variables and stores result
 * 
 * Parameters:
 * - var_a: First variable name or value
 * - var_b: Second variable name or value
 * - result_var: Result variable name
 */
class MathAddItem : public InterpreterItem {
public:
    MathAddItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class MathSubItem : public InterpreterItem {
public:
    MathSubItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class MathMulItem : public InterpreterItem {
public:
    MathMulItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class MathDivItem : public InterpreterItem {
public:
    MathDivItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

class MathModItem : public InterpreterItem {
public:
    MathModItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Random Number Generation
// ============================================================================

/**
 * @brief Generates random integer in range [min, max]
 */
class RandIntItem : public InterpreterItem {
public:
    RandIntItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Generates random float in range [min, max)
 */
class RandFloatItem : public InterpreterItem {
public:
    RandFloatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Constants
// ============================================================================

/**
 * @brief Sets variable to mathematical constant
 * 
 * Parameters:
 * - constant_name: "pi", "e", "phi" (golden ratio), "sqrt2"
 * - output_var: Variable name to store value
 */
class MathConstantItem : public InterpreterItem {
public:
    MathConstantItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Matrix Math Operations (OpenCV-accelerated, operate on MAT arguments)
//
// All matrix items accept cv::Mat values as arguments (not currentMat) and
// return the result mat both as the pipeline frame (outputMat) and as the
// call's scalar value so they work with assignment:
//
//   result = matrix_avg(mat_a, mat_b)
//   matrix_add(mat_a, mat_b)     // result flows as currentMat
// ============================================================================

/**
 * @brief Element-wise average of two matrices.
 *
 * Computes (A + B) / 2  using cv::addWeighted (SIMD-accelerated).
 * Both matrices must have the same size and type, or B is broadcast-compatible.
 *
 * Parameters
 * ----------
 * - mat_a  MAT   First matrix.
 * - mat_b  MAT   Second matrix.
 *
 * Returns  MAT   (A + B) × 0.5
 *
 * Example
 * -------
 *   result = matrix_avg(frame_a, frame_b)
 */
class MatrixAvgItem : public InterpreterItem {
public:
    MatrixAvgItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise addition of two matrices  (cv::add, SIMD-accelerated).
 *
 * Parameters
 * ----------
 * - mat_a  MAT  First matrix.
 * - mat_b  MAT  Second matrix (must match size & type, or be a scalar wrapped in a 1×1 mat).
 *
 * Returns  MAT  A + B
 */
class MatrixAddItem : public InterpreterItem {
public:
    MatrixAddItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise subtraction  A − B  (cv::subtract).
 */
class MatrixSubItem : public InterpreterItem {
public:
    MatrixSubItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise multiplication  A ⊙ B  (cv::multiply, not matrix product).
 *
 * For true matrix multiplication use matrix_gemm.
 */
class MatrixMulItem : public InterpreterItem {
public:
    MatrixMulItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise division  A ÷ B  (cv::divide).
 */
class MatrixDivItem : public InterpreterItem {
public:
    MatrixDivItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Multiply every element of the matrix by a scalar factor.
 *
 * Uses cv::multiply(mat, cv::Scalar(factor)) — SIMD-accelerated.
 *
 * Parameters
 * ----------
 * - mat     MAT    Input matrix.
 * - factor  FLOAT  Scale factor.
 *
 * Returns  MAT  mat × factor
 *
 * Example
 * -------
 *   half = matrix_scale(frame, 0.5)
 */
class MatrixScaleItem : public InterpreterItem {
public:
    MatrixScaleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise absolute difference  |A − B|  (cv::absdiff).
 *
 * Saturating subtraction in absolute value — useful for motion detection,
 * background subtraction, and comparison of stabilization outputs.
 */
class MatrixAbsDiffItem : public InterpreterItem {
public:
    MatrixAbsDiffItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise minimum  min(A, B)  (cv::min).
 */
class MatrixMinItem : public InterpreterItem {
public:
    MatrixMinItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise maximum  max(A, B)  (cv::max).
 */
class MatrixMaxItem : public InterpreterItem {
public:
    MatrixMaxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Normalize matrix values to [alpha, beta]  (cv::normalize).
 *
 * Parameters
 * ----------
 * - mat    MAT    Input matrix.
 * - alpha  FLOAT  Lower bound (default 0).
 * - beta   FLOAT  Upper bound (default 255).
 * - norm_type  STRING  "minmax" (default) | "l1" | "l2" | "inf".
 *
 * Returns  MAT  Normalized matrix (CV_32F intermediate, converted back to input depth).
 *
 * Example
 * -------
 *   norm = matrix_normalize(depth_map)
 *   norm = matrix_normalize(depth_map, 0, 1, "minmax")
 */
class MatrixNormalizeItem : public InterpreterItem {
public:
    MatrixNormalizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Weighted blend of two matrices  α·A + (1−α)·B  (cv::addWeighted).
 *
 * Parameters
 * ----------
 * - mat_a  MAT    First matrix.
 * - mat_b  MAT    Second matrix.
 * - alpha  FLOAT  Weight of mat_a ∈ [0, 1].  mat_b weight = 1 − alpha.
 *
 * Returns  MAT  α·A + (1−α)·B
 *
 * Example
 * -------
 *   blended = matrix_blend(current, stabilized, 0.7)
 */
class MatrixBlendItem : public InterpreterItem {
public:
    MatrixBlendItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Dot product of two single-channel vectors / matrices (returns scalar).
 *
 * Wraps cv::Mat::dot() which computes the sum of element-wise products.
 * Both matrices must have the same total element count.
 *
 * Returns  FLOAT  Σ(A ⊙ B)
 *
 * Example
 * -------
 *   d = matrix_dot(vec_a, vec_b)
 */
class MatrixDotItem : public InterpreterItem {
public:
    MatrixDotItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief General matrix multiplication  α·A·B + β·C  (cv::gemm / BLAS).
 *
 * Uses cv::gemm which maps to an optimized BLAS/LAPACK implementation
 * when available (e.g. OpenBLAS, MKL).
 *
 * Parameters
 * ----------
 * - mat_a  MAT    Left matrix.
 * - mat_b  MAT    Right matrix.
 * - alpha  FLOAT  Scale for A·B (default 1.0).
 * - mat_c  MAT    Addend matrix (optional — pass identity or omit).
 * - beta   FLOAT  Scale for C (default 0.0 → no addend).
 *
 * Returns  MAT  α·A·B + β·C
 *
 * Example
 * -------
 *   product = matrix_gemm(rot_mat, points)
 *   product = matrix_gemm(A, B, 1.0, C, 1.0)
 */
class MatrixGemmItem : public InterpreterItem {
public:
    MatrixGemmItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Transpose a matrix  Aᵀ  (cv::transpose).
 *
 * Parameters
 * ----------
 * - mat  MAT  Input matrix.
 *
 * Returns  MAT  Aᵀ
 */
class MatrixTransposeItem : public InterpreterItem {
public:
    MatrixTransposeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Invert a square matrix (cv::invert).
 *
 * Parameters
 * ----------
 * - mat     MAT     Square input matrix.
 * - method  STRING  "lu" (default) | "svd" | "cholesky" | "pseudo".
 *                   "pseudo" computes the Moore-Penrose pseudo-inverse
 *                   for non-square / singular matrices.
 *
 * Returns  MAT  A⁻¹  (or pseudo-inverse)
 *
 * Example
 * -------
 *   inv = matrix_invert(homography)
 *   inv = matrix_invert(cov_mat, "svd")
 */
class MatrixInvertItem : public InterpreterItem {
public:
    MatrixInvertItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_MATH_EVAL_ITEMS_H

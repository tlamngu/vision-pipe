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

} // namespace visionpipe

#endif // VISIONPIPE_MATH_EVAL_ITEMS_H

#include "interpreter/items/math_eval_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <random>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_E
#define M_E 2.71828182845904523536
#endif

namespace visionpipe {

void registerMathEvalItems(ItemRegistry& registry) {
    // Trigonometric functions
    registry.add<SinItem>();
    registry.add<CosItem>();
    registry.add<TanItem>();
    registry.add<AsinItem>();
    registry.add<AcosItem>();
    registry.add<AtanItem>();
    registry.add<Atan2Item>();
    
    // Scalar math operations
    registry.add<AbsScalarItem>();
    registry.add<SqrtScalarItem>();
    registry.add<PowScalarItem>();
    registry.add<ExpScalarItem>();
    registry.add<LnScalarItem>();
    registry.add<Log10ScalarItem>();
    registry.add<RoundScalarItem>();
    registry.add<FloorScalarItem>();
    registry.add<CeilScalarItem>();
    
    // Range and comparison
    registry.add<ClampItem>();
    registry.add<MinScalarItem>();
    registry.add<MaxScalarItem>();
    registry.add<LerpScalarItem>();
    
    // Variable math operations
    registry.add<MathAddItem>();
    registry.add<MathSubItem>();
    registry.add<MathMulItem>();
    registry.add<MathDivItem>();
    registry.add<MathModItem>();
    
    // Random generation
    registry.add<RandIntItem>();
    registry.add<RandFloatItem>();
    
    // Constants
    registry.add<MathConstantItem>();

    // Matrix math (OpenCV-accelerated)
    registry.add<MatrixAvgItem>();
    registry.add<MatrixAddItem>();
    registry.add<MatrixSubItem>();
    registry.add<MatrixMulItem>();
    registry.add<MatrixDivItem>();
    registry.add<MatrixScaleItem>();
    registry.add<MatrixAbsDiffItem>();
    registry.add<MatrixMinItem>();
    registry.add<MatrixMaxItem>();
    registry.add<MatrixNormalizeItem>();
    registry.add<MatrixBlendItem>();
    registry.add<MatrixDotItem>();
    registry.add<MatrixGemmItem>();
    registry.add<MatrixTransposeItem>();
    registry.add<MatrixInvertItem>();
}

// Helper function to get numeric value from argument
static double getNumericValue(const RuntimeValue& arg, ExecutionContext& ctx) {
    if (arg.isNumeric()) {
        return arg.asNumber();
    } else if (arg.isString()) {
        // Try to get from variable
        std::string varName = arg.asString();
        if (ctx.variables.find(varName) != ctx.variables.end()) {
            return ctx.variables[varName].asNumber();
        }
    }
    return 0.0;
}

// Helper to convert degrees to radians
static double degreesToRadians(double degrees) {
    return degrees * M_PI / 180.0;
}

// Helper to convert radians to degrees
static double radiansToDegrees(double radians) {
    return radians * 180.0 / M_PI;
}

// ============================================================================
// Trigonometric Functions
// ============================================================================

SinItem::SinItem() {
    _functionName = "sin";
    _description = "Calculates sine of value";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name"),
        ParamDef::optional("in_degrees", BaseType::BOOL, "Input in degrees", false)
    };
    _example = "sin(angle, \"result\", true)";
    _returnType = "mat";
    _tags = {"math", "trigonometry", "sin"};
}

ExecutionResult SinItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    bool inDegrees = args.size() > 2 ? args[2].asBool() : false;
    
    if (inDegrees) {
        value = degreesToRadians(value);
    }
    
    double result = std::sin(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

CosItem::CosItem() {
    _functionName = "cos";
    _description = "Calculates cosine of value";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name"),
        ParamDef::optional("in_degrees", BaseType::BOOL, "Input in degrees", false)
    };
    _example = "cos(angle, \"result\", true)";
    _returnType = "mat";
    _tags = {"math", "trigonometry", "cos"};
}

ExecutionResult CosItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    bool inDegrees = args.size() > 2 ? args[2].asBool() : false;
    
    if (inDegrees) {
        value = degreesToRadians(value);
    }
    
    double result = std::cos(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

TanItem::TanItem() {
    _functionName = "tan";
    _description = "Calculates tangent of value";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name"),
        ParamDef::optional("in_degrees", BaseType::BOOL, "Input in degrees", false)
    };
    _example = "tan(angle, \"result\", true)";
    _returnType = "mat";
    _tags = {"math", "trigonometry", "tan"};
}

ExecutionResult TanItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    bool inDegrees = args.size() > 2 ? args[2].asBool() : false;
    
    if (inDegrees) {
        value = degreesToRadians(value);
    }
    
    double result = std::tan(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

AsinItem::AsinItem() {
    _functionName = "asin";
    _description = "Calculates arc sine (inverse sine)";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value [-1, 1] or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name"),
        ParamDef::optional("in_degrees", BaseType::BOOL, "Output in degrees", false)
    };
    _example = "asin(0.5, \"result\", true)";
    _returnType = "mat";
    _tags = {"math", "trigonometry", "asin"};
}

ExecutionResult AsinItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    bool inDegrees = args.size() > 2 ? args[2].asBool() : false;
    
    if (value < -1.0 || value > 1.0) {
        return ExecutionResult::fail("asin: value must be in range [-1, 1]");
    }
    
    double result = std::asin(value);
    if (inDegrees) {
        result = radiansToDegrees(result);
    }
    
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

AcosItem::AcosItem() {
    _functionName = "acos";
    _description = "Calculates arc cosine (inverse cosine)";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value [-1, 1] or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name"),
        ParamDef::optional("in_degrees", BaseType::BOOL, "Output in degrees", false)
    };
    _example = "acos(0.5, \"result\", true)";
    _returnType = "mat";
    _tags = {"math", "trigonometry", "acos"};
}

ExecutionResult AcosItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    bool inDegrees = args.size() > 2 ? args[2].asBool() : false;
    
    if (value < -1.0 || value > 1.0) {
        return ExecutionResult::fail("acos: value must be in range [-1, 1]");
    }
    
    double result = std::acos(value);
    if (inDegrees) {
        result = radiansToDegrees(result);
    }
    
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

AtanItem::AtanItem() {
    _functionName = "atan";
    _description = "Calculates arc tangent (inverse tangent)";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name"),
        ParamDef::optional("in_degrees", BaseType::BOOL, "Output in degrees", false)
    };
    _example = "atan(1.0, \"result\", true)";
    _returnType = "mat";
    _tags = {"math", "trigonometry", "atan"};
}

ExecutionResult AtanItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    bool inDegrees = args.size() > 2 ? args[2].asBool() : false;
    
    double result = std::atan(value);
    if (inDegrees) {
        result = radiansToDegrees(result);
    }
    
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

Atan2Item::Atan2Item() {
    _functionName = "atan2";
    _description = "Calculates arc tangent of y/x (2-argument form)";
    _category = "math";
    _params = {
        ParamDef::required("y", BaseType::FLOAT, "Y value or variable name"),
        ParamDef::required("x", BaseType::FLOAT, "X value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name"),
        ParamDef::optional("in_degrees", BaseType::BOOL, "Output in degrees", false)
    };
    _example = "atan2(y, x, \"angle\", true)";
    _returnType = "mat";
    _tags = {"math", "trigonometry", "atan2", "angle"};
}

ExecutionResult Atan2Item::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double y = getNumericValue(args[0], ctx);
    double x = getNumericValue(args[1], ctx);
    std::string outputVar = args[2].asString();
    bool inDegrees = args.size() > 3 ? args[3].asBool() : false;
    
    double result = std::atan2(y, x);
    if (inDegrees) {
        result = radiansToDegrees(result);
    }
    
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Scalar Math Operations
// ============================================================================

AbsScalarItem::AbsScalarItem() {
    _functionName = "abs_scalar";
    _description = "Calculates absolute value of scalar";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "abs_scalar(-5.5, \"result\")";
    _returnType = "mat";
    _tags = {"math", "abs", "absolute"};
}

ExecutionResult AbsScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    double result = std::abs(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

SqrtScalarItem::SqrtScalarItem() {
    _functionName = "sqrt_scalar";
    _description = "Calculates square root of scalar";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "sqrt_scalar(16, \"result\")";
    _returnType = "mat";
    _tags = {"math", "sqrt", "square root"};
}

ExecutionResult SqrtScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    if (value < 0) {
        return ExecutionResult::fail("sqrt_scalar: value must be non-negative");
    }
    
    double result = std::sqrt(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

PowScalarItem::PowScalarItem() {
    _functionName = "pow_scalar";
    _description = "Raises scalar to power";
    _category = "math";
    _params = {
        ParamDef::required("base", BaseType::FLOAT, "Base value or variable name"),
        ParamDef::required("exponent", BaseType::FLOAT, "Exponent value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "pow_scalar(2, 8, \"result\")";
    _returnType = "mat";
    _tags = {"math", "pow", "power", "exponent"};
}

ExecutionResult PowScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double base = getNumericValue(args[0], ctx);
    double exponent = getNumericValue(args[1], ctx);
    std::string outputVar = args[2].asString();
    
    double result = std::pow(base, exponent);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

ExpScalarItem::ExpScalarItem() {
    _functionName = "exp_scalar";
    _description = "Calculates e^x";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "exp_scalar(2, \"result\")";
    _returnType = "mat";
    _tags = {"math", "exp", "exponential"};
}

ExecutionResult ExpScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    double result = std::exp(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

LnScalarItem::LnScalarItem() {
    _functionName = "ln_scalar";
    _description = "Calculates natural logarithm";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "ln_scalar(2.718, \"result\")";
    _returnType = "mat";
    _tags = {"math", "ln", "log", "logarithm"};
}

ExecutionResult LnScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    if (value <= 0) {
        return ExecutionResult::fail("ln_scalar: value must be positive");
    }
    
    double result = std::log(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

Log10ScalarItem::Log10ScalarItem() {
    _functionName = "log10_scalar";
    _description = "Calculates base-10 logarithm";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "log10_scalar(100, \"result\")";
    _returnType = "mat";
    _tags = {"math", "log10", "logarithm"};
}

ExecutionResult Log10ScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    if (value <= 0) {
        return ExecutionResult::fail("log10_scalar: value must be positive");
    }
    
    double result = std::log10(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

RoundScalarItem::RoundScalarItem() {
    _functionName = "round_scalar";
    _description = "Rounds to nearest integer";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "round_scalar(3.7, \"result\")";
    _returnType = "mat";
    _tags = {"math", "round", "rounding"};
}

ExecutionResult RoundScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    double result = std::round(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

FloorScalarItem::FloorScalarItem() {
    _functionName = "floor_scalar";
    _description = "Rounds down to integer";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "floor_scalar(3.7, \"result\")";
    _returnType = "mat";
    _tags = {"math", "floor", "rounding"};
}

ExecutionResult FloorScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    double result = std::floor(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

CeilScalarItem::CeilScalarItem() {
    _functionName = "ceil_scalar";
    _description = "Rounds up to integer";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Input value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "ceil_scalar(3.1, \"result\")";
    _returnType = "mat";
    _tags = {"math", "ceil", "ceiling", "rounding"};
}

ExecutionResult CeilScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string outputVar = args[1].asString();
    
    double result = std::ceil(value);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Range and Comparison Functions
// ============================================================================

ClampItem::ClampItem() {
    _functionName = "clamp";
    _description = "Clamps value to range [min, max]";
    _category = "math";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Value to clamp or variable name"),
        ParamDef::required("min", BaseType::FLOAT, "Minimum value"),
        ParamDef::required("max", BaseType::FLOAT, "Maximum value"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "clamp(value, 0, 255, \"clamped\")";
    _returnType = "mat";
    _tags = {"math", "clamp", "range", "limit"};
}

ExecutionResult ClampItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    double minVal = getNumericValue(args[1], ctx);
    double maxVal = getNumericValue(args[2], ctx);
    std::string outputVar = args[3].asString();
    
    double result = std::max(minVal, std::min(value, maxVal));
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

MinScalarItem::MinScalarItem() {
    _functionName = "min_scalar";
    _description = "Returns minimum of two values";
    _category = "math";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "First value or variable name"),
        ParamDef::required("b", BaseType::FLOAT, "Second value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "min_scalar(a, b, \"min_val\")";
    _returnType = "mat";
    _tags = {"math", "min", "minimum"};
}

ExecutionResult MinScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    double b = getNumericValue(args[1], ctx);
    std::string outputVar = args[2].asString();
    
    double result = std::min(a, b);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

MaxScalarItem::MaxScalarItem() {
    _functionName = "max_scalar";
    _description = "Returns maximum of two values";
    _category = "math";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "First value or variable name"),
        ParamDef::required("b", BaseType::FLOAT, "Second value or variable name"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "max_scalar(a, b, \"max_val\")";
    _returnType = "mat";
    _tags = {"math", "max", "maximum"};
}

ExecutionResult MaxScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    double b = getNumericValue(args[1], ctx);
    std::string outputVar = args[2].asString();
    
    double result = std::max(a, b);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

LerpScalarItem::LerpScalarItem() {
    _functionName = "lerp_scalar";
    _description = "Linear interpolation between two values";
    _category = "math";
    _params = {
        ParamDef::required("start", BaseType::FLOAT, "Start value or variable name"),
        ParamDef::required("end", BaseType::FLOAT, "End value or variable name"),
        ParamDef::required("t", BaseType::FLOAT, "Interpolation factor (0-1)"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "lerp_scalar(0, 100, 0.5, \"result\")";
    _returnType = "mat";
    _tags = {"math", "lerp", "interpolation", "blend"};
}

ExecutionResult LerpScalarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double start = getNumericValue(args[0], ctx);
    double end = getNumericValue(args[1], ctx);
    double t = getNumericValue(args[2], ctx);
    std::string outputVar = args[3].asString();
    
    double result = start + (end - start) * t;
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Variable Math Operations
// ============================================================================

MathAddItem::MathAddItem() {
    _functionName = "math_add";
    _description = "Adds two values and stores result";
    _category = "math";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "First value or variable name"),
        ParamDef::required("b", BaseType::FLOAT, "Second value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Result variable name")
    };
    _example = "math_add(var_a, var_b, \"sum\")";
    _returnType = "mat";
    _tags = {"math", "add", "addition", "sum"};
}

ExecutionResult MathAddItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    double b = getNumericValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    ctx.variables[resultVar] = RuntimeValue(a + b);
    
    return ExecutionResult::ok(ctx.currentMat);
}

MathSubItem::MathSubItem() {
    _functionName = "math_sub";
    _description = "Subtracts two values and stores result";
    _category = "math";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "First value or variable name"),
        ParamDef::required("b", BaseType::FLOAT, "Second value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Result variable name")
    };
    _example = "math_sub(var_a, var_b, \"diff\")";
    _returnType = "mat";
    _tags = {"math", "subtract", "subtraction", "difference"};
}

ExecutionResult MathSubItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    double b = getNumericValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    ctx.variables[resultVar] = RuntimeValue(a - b);
    
    return ExecutionResult::ok(ctx.currentMat);
}

MathMulItem::MathMulItem() {
    _functionName = "math_mul";
    _description = "Multiplies two values and stores result";
    _category = "math";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "First value or variable name"),
        ParamDef::required("b", BaseType::FLOAT, "Second value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Result variable name")
    };
    _example = "math_mul(var_a, var_b, \"product\")";
    _returnType = "mat";
    _tags = {"math", "multiply", "multiplication", "product"};
}

ExecutionResult MathMulItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    double b = getNumericValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    ctx.variables[resultVar] = RuntimeValue(a * b);
    
    return ExecutionResult::ok(ctx.currentMat);
}

MathDivItem::MathDivItem() {
    _functionName = "math_div";
    _description = "Divides two values and stores result";
    _category = "math";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "Numerator or variable name"),
        ParamDef::required("b", BaseType::FLOAT, "Denominator or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Result variable name")
    };
    _example = "math_div(var_a, var_b, \"quotient\")";
    _returnType = "mat";
    _tags = {"math", "divide", "division", "quotient"};
}

ExecutionResult MathDivItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    double b = getNumericValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    if (std::abs(b) < 1e-10) {
        return ExecutionResult::fail("math_div: division by zero");
    }
    
    ctx.variables[resultVar] = RuntimeValue(a / b);
    
    return ExecutionResult::ok(ctx.currentMat);
}

MathModItem::MathModItem() {
    _functionName = "math_mod";
    _description = "Calculates modulo and stores result";
    _category = "math";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "Value or variable name"),
        ParamDef::required("b", BaseType::FLOAT, "Modulus or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Result variable name")
    };
    _example = "math_mod(var_a, 10, \"remainder\")";
    _returnType = "mat";
    _tags = {"math", "modulo", "remainder", "mod"};
}

ExecutionResult MathModItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    double b = getNumericValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    if (std::abs(b) < 1e-10) {
        return ExecutionResult::fail("math_mod: modulus cannot be zero");
    }
    
    ctx.variables[resultVar] = RuntimeValue(std::fmod(a, b));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Random Number Generation
// ============================================================================

RandIntItem::RandIntItem() {
    _functionName = "rand_int";
    _description = "Generates random integer in range [min, max]";
    _category = "math";
    _params = {
        ParamDef::required("min", BaseType::INT, "Minimum value (inclusive)"),
        ParamDef::required("max", BaseType::INT, "Maximum value (inclusive)"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "rand_int(0, 100, \"random_val\")";
    _returnType = "mat";
    _tags = {"math", "random", "integer", "rand"};
}

ExecutionResult RandIntItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int minVal = static_cast<int>(getNumericValue(args[0], ctx));
    int maxVal = static_cast<int>(getNumericValue(args[1], ctx));
    std::string outputVar = args[2].asString();
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(minVal, maxVal);
    
    int result = dis(gen);
    ctx.variables[outputVar] = RuntimeValue(static_cast<int64_t>(result));
    
    return ExecutionResult::ok(ctx.currentMat);
}

RandFloatItem::RandFloatItem() {
    _functionName = "rand_float";
    _description = "Generates random float in range [min, max)";
    _category = "math";
    _params = {
        ParamDef::required("min", BaseType::FLOAT, "Minimum value (inclusive)"),
        ParamDef::required("max", BaseType::FLOAT, "Maximum value (exclusive)"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "rand_float(0.0, 1.0, \"random_val\")";
    _returnType = "mat";
    _tags = {"math", "random", "float", "rand"};
}

ExecutionResult RandFloatItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double minVal = getNumericValue(args[0], ctx);
    double maxVal = getNumericValue(args[1], ctx);
    std::string outputVar = args[2].asString();
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(minVal, maxVal);
    
    double result = dis(gen);
    ctx.variables[outputVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Constants
// ============================================================================

MathConstantItem::MathConstantItem() {
    _functionName = "math_const";
    _description = "Sets variable to mathematical constant";
    _category = "math";
    _params = {
        ParamDef::required("constant", BaseType::STRING, "Constant name: pi, e, phi, sqrt2"),
        ParamDef::required("output_var", BaseType::STRING, "Output variable name")
    };
    _example = "math_const(\"pi\", \"pi_val\")";
    _returnType = "mat";
    _tags = {"math", "constant", "pi", "e"};
}

ExecutionResult MathConstantItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string constant = args[0].asString();
    std::string outputVar = args[1].asString();
    
    double value = 0.0;
    
    if (constant == "pi") {
        value = M_PI;
    } else if (constant == "e") {
        value = M_E;
    } else if (constant == "phi") {
        value = (1.0 + std::sqrt(5.0)) / 2.0;  // Golden ratio
    } else if (constant == "sqrt2") {
        value = std::sqrt(2.0);
    } else {
        return ExecutionResult::fail("Unknown constant: " + constant);
    }
    
    ctx.variables[outputVar] = RuntimeValue(value);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Matrix Math Operations (OpenCV-accelerated)
// ============================================================================

// Helper: retrieve a cv::Mat from a RuntimeValue (MAT) arg.
// Returns empty mat if the arg is not a MAT.
static cv::Mat getMatArg(const RuntimeValue& arg) {
    if (arg.isMat()) return arg.asMat();
    return cv::Mat{};
}

// Helper: ensure two mats are compatible for element-wise ops.
// Converts types if needed so both share the same depth.
static bool ensureCompatible(cv::Mat& a, cv::Mat& b, std::string& err) {
    if (a.empty() || b.empty()) { err = "matrix argument is empty"; return false; }
    if (a.size() != b.size())   { err = "matrix size mismatch"; return false; }
    if (a.type() != b.type()) {
        // Convert to the higher-depth common type
        int depth = std::max(a.depth(), b.depth());
        if (depth < CV_32F) depth = CV_32F;  // promote integer types
        int type = CV_MAKETYPE(depth, std::max(a.channels(), b.channels()));
        if (a.type() != type) a.convertTo(a, type);
        if (b.type() != type) b.convertTo(b, type);
    }
    return true;
}

// ---------------------------------------------------------------------------
// matrix_avg
// ---------------------------------------------------------------------------

MatrixAvgItem::MatrixAvgItem() {
    _functionName = "matrix_avg";
    _description  = "Element-wise average of two matrices: (A + B) / 2. "
                    "Uses cv::addWeighted (SIMD-accelerated). "
                    "Both matrices must have the same size; types are auto-promoted if needed.";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "First matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Second matrix")
    };
    _example    = "result = matrix_avg(frame_a, frame_b)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "average", "blend", "mean"};
}

ExecutionResult MatrixAvgItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_avg: requires mat_a and mat_b");

    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_avg: " + err);

    cv::Mat result;
    cv::addWeighted(a, 0.5, b, 0.5, 0.0, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_add
// ---------------------------------------------------------------------------

MatrixAddItem::MatrixAddItem() {
    _functionName = "matrix_add";
    _description  = "Element-wise addition A + B (cv::add, SIMD-accelerated). "
                    "Both matrices must have the same size.";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "First matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Second matrix")
    };
    _example    = "sum = matrix_add(frame_a, frame_b)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "add", "addition"};
}

ExecutionResult MatrixAddItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_add: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_add: " + err);
    cv::Mat result;
    cv::add(a, b, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_sub
// ---------------------------------------------------------------------------

MatrixSubItem::MatrixSubItem() {
    _functionName = "matrix_sub";
    _description  = "Element-wise subtraction A - B (cv::subtract, SIMD-accelerated).";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "Minuend matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Subtrahend matrix")
    };
    _example    = "diff = matrix_sub(frame_a, frame_b)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "subtract", "subtraction", "difference"};
}

ExecutionResult MatrixSubItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_sub: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_sub: " + err);
    cv::Mat result;
    cv::subtract(a, b, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_mul (element-wise)
// ---------------------------------------------------------------------------

MatrixMulItem::MatrixMulItem() {
    _functionName = "matrix_mul";
    _description  = "Element-wise (Hadamard) multiplication A ⊙ B (cv::multiply). "
                    "For true matrix multiplication use matrix_gemm.";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "First matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Second matrix")
    };
    _example    = "product = matrix_mul(mask, frame)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "multiply", "hadamard", "element-wise"};
}

ExecutionResult MatrixMulItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_mul: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_mul: " + err);
    cv::Mat result;
    cv::multiply(a, b, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_div (element-wise)
// ---------------------------------------------------------------------------

MatrixDivItem::MatrixDivItem() {
    _functionName = "matrix_div";
    _description  = "Element-wise division A ÷ B (cv::divide). "
                    "Division by zero pixels produces 0 (OpenCV convention).";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "Numerator matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Denominator matrix")
    };
    _example    = "quotient = matrix_div(foreground, background)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "divide", "division"};
}

ExecutionResult MatrixDivItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_div: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_div: " + err);
    cv::Mat result;
    cv::divide(a, b, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_scale
// ---------------------------------------------------------------------------

MatrixScaleItem::MatrixScaleItem() {
    _functionName = "matrix_scale";
    _description  = "Multiply every element of the matrix by a scalar factor "
                    "(cv::multiply with Scalar — SIMD-accelerated). "
                    "Useful for halving frame brightness, normalizing depth maps, etc.";
    _category     = "math";
    _params = {
        ParamDef::required("mat",    BaseType::MAT,   "Input matrix"),
        ParamDef::required("factor", BaseType::FLOAT, "Scale factor")
    };
    _example    = "half = matrix_scale(frame, 0.5)\n"
                  "bright = matrix_scale(frame, 2.0)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "scale", "multiply", "scalar"};
}

ExecutionResult MatrixScaleItem::execute(const std::vector<RuntimeValue>& args,
                                         ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_scale: requires mat and factor");
    cv::Mat a = getMatArg(args[0]);
    if (a.empty())
        return ExecutionResult::fail("matrix_scale: mat argument is empty");
    double factor = args[1].asNumber();
    cv::Mat result;
    a.convertTo(result, a.depth() < CV_32F ? CV_32F : a.depth(), factor);
    // Convert back to original depth if it was integer
    if (a.depth() < CV_32F) {
        result.convertTo(result, a.type());
    }
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_abs_diff
// ---------------------------------------------------------------------------

MatrixAbsDiffItem::MatrixAbsDiffItem() {
    _functionName = "matrix_abs_diff";
    _description  = "Element-wise absolute difference |A - B| (cv::absdiff, SIMD-accelerated). "
                    "Useful for motion detection, frame differencing, and comparing "
                    "stabilization outputs.";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "First matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Second matrix")
    };
    _example    = "diff = matrix_abs_diff(current, previous)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "abs_diff", "difference", "motion"};
}

ExecutionResult MatrixAbsDiffItem::execute(const std::vector<RuntimeValue>& args,
                                           ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_abs_diff: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_abs_diff: " + err);
    cv::Mat result;
    cv::absdiff(a, b, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_min
// ---------------------------------------------------------------------------

MatrixMinItem::MatrixMinItem() {
    _functionName = "matrix_min";
    _description  = "Element-wise minimum min(A, B) (cv::min, SIMD-accelerated).";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "First matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Second matrix")
    };
    _example    = "darkest = matrix_min(frame_a, frame_b)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "min", "minimum"};
}

ExecutionResult MatrixMinItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_min: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_min: " + err);
    cv::Mat result;
    cv::min(a, b, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_max
// ---------------------------------------------------------------------------

MatrixMaxItem::MatrixMaxItem() {
    _functionName = "matrix_max";
    _description  = "Element-wise maximum max(A, B) (cv::max, SIMD-accelerated).";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "First matrix"),
        ParamDef::required("mat_b", BaseType::MAT, "Second matrix")
    };
    _example    = "brightest = matrix_max(frame_a, frame_b)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "max", "maximum"};
}

ExecutionResult MatrixMaxItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_max: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_max: " + err);
    cv::Mat result;
    cv::max(a, b, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_normalize
// ---------------------------------------------------------------------------

MatrixNormalizeItem::MatrixNormalizeItem() {
    _functionName = "matrix_normalize";
    _description  = "Normalize matrix values to [alpha, beta] range using cv::normalize. "
                    "norm_type selects the normalization method: "
                    "\"minmax\" (default) maps the min/max to [alpha, beta]; "
                    "\"l1\", \"l2\", \"inf\" normalise by the corresponding vector norm.";
    _category     = "math";
    _params = {
        ParamDef::required("mat",       BaseType::MAT,    "Input matrix"),
        ParamDef::optional("alpha",     BaseType::FLOAT,  "Lower bound / norm target (default 0)", 0.0),
        ParamDef::optional("beta",      BaseType::FLOAT,  "Upper bound (default 255, ignored for non-minmax norms)", 255.0),
        ParamDef::optional("norm_type", BaseType::STRING, "\"minmax\" | \"l1\" | \"l2\" | \"inf\" (default \"minmax\")", std::string("minmax"))
    };
    _example    = "norm = matrix_normalize(depth_map)\n"
                  "norm = matrix_normalize(feature_map, 0, 1, \"l2\")";
    _returnType = "mat";
    _tags       = {"math", "matrix", "normalize", "scale", "range"};
}

ExecutionResult MatrixNormalizeItem::execute(const std::vector<RuntimeValue>& args,
                                             ExecutionContext& ctx) {
    if (args.empty())
        return ExecutionResult::fail("matrix_normalize: requires mat argument");
    cv::Mat a = getMatArg(args[0]);
    if (a.empty())
        return ExecutionResult::fail("matrix_normalize: mat argument is empty");

    double alpha    = args.size() > 1 ? args[1].asNumber() : 0.0;
    double beta     = args.size() > 2 ? args[2].asNumber() : 255.0;
    std::string ntype = args.size() > 3 ? args[3].asString() : "minmax";

    int normType = cv::NORM_MINMAX;
    if      (ntype == "l1")  normType = cv::NORM_L1;
    else if (ntype == "l2")  normType = cv::NORM_L2;
    else if (ntype == "inf") normType = cv::NORM_INF;

    // Normalize always into float; convert back to original depth.
    int origDepth = a.depth();
    int origType  = a.type();
    cv::Mat f;
    a.convertTo(f, CV_32F);
    cv::Mat result;
    cv::normalize(f, result, alpha, beta, normType, CV_32F);
    // Restore original integer depth (if applicable)
    if (origDepth < CV_32F) {
        result.convertTo(result, origType);
    }
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_blend
// ---------------------------------------------------------------------------

MatrixBlendItem::MatrixBlendItem() {
    _functionName = "matrix_blend";
    _description  = "Weighted blend of two matrices: α·A + (1−α)·B (cv::addWeighted). "
                    "alpha=1 returns A unchanged; alpha=0 returns B unchanged.";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT,   "First matrix"),
        ParamDef::required("mat_b", BaseType::MAT,   "Second matrix"),
        ParamDef::required("alpha", BaseType::FLOAT, "Weight of mat_a ∈ [0, 1]")
    };
    _example    = "blended = matrix_blend(current, stabilized, 0.7)\n"
                  "// equal blend:\n"
                  "blended = matrix_blend(frame_a, frame_b, 0.5)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "blend", "lerp", "mix", "alpha"};
}

ExecutionResult MatrixBlendItem::execute(const std::vector<RuntimeValue>& args,
                                         ExecutionContext& ctx) {
    if (args.size() < 3)
        return ExecutionResult::fail("matrix_blend: requires mat_a, mat_b, and alpha");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    double alpha = std::max(0.0, std::min(args[2].asNumber(), 1.0));
    std::string err;
    if (!ensureCompatible(a, b, err))
        return ExecutionResult::fail("matrix_blend: " + err);
    cv::Mat result;
    cv::addWeighted(a, alpha, b, 1.0 - alpha, 0.0, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_dot
// ---------------------------------------------------------------------------

MatrixDotItem::MatrixDotItem() {
    _functionName = "matrix_dot";
    _description  = "Dot product of two matrices / vectors: Σ(A ⊙ B). "
                    "Both must have the same total element count. "
                    "Returns a scalar FLOAT value.";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a", BaseType::MAT, "First matrix / vector"),
        ParamDef::required("mat_b", BaseType::MAT, "Second matrix / vector")
    };
    _example    = "d = matrix_dot(vec_a, vec_b)";
    _returnType = "float";
    _tags       = {"math", "matrix", "dot", "inner product", "vector"};
}

ExecutionResult MatrixDotItem::execute(const std::vector<RuntimeValue>& args,
                                       ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_dot: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    if (a.empty() || b.empty())
        return ExecutionResult::fail("matrix_dot: matrix argument is empty");
    if (a.total() != b.total())
        return ExecutionResult::fail("matrix_dot: matrices must have the same total element count");
    // Flatten to single-row if needed, promote to float for precision
    cv::Mat af, bf;
    a.reshape(1, 1).convertTo(af, CV_64F);
    b.reshape(1, 1).convertTo(bf, CV_64F);
    double dot = af.dot(bf);
    return ExecutionResult::okWithMat(ctx.currentMat, RuntimeValue(dot));
}

// ---------------------------------------------------------------------------
// matrix_gemm
// ---------------------------------------------------------------------------

MatrixGemmItem::MatrixGemmItem() {
    _functionName = "matrix_gemm";
    _description  = "General matrix multiplication: α·A·B + β·C (cv::gemm). "
                    "Maps to an optimized BLAS implementation when available. "
                    "Inputs must be CV_32F or CV_64F (converted automatically).";
    _category     = "math";
    _params = {
        ParamDef::required("mat_a",  BaseType::MAT,   "Left matrix"),
        ParamDef::required("mat_b",  BaseType::MAT,   "Right matrix"),
        ParamDef::optional("alpha",  BaseType::FLOAT, "Scale for A·B (default 1.0)", 1.0),
        ParamDef::optional("mat_c",  BaseType::MAT,   "Addend matrix (optional, pass empty mat to skip)", cv::Mat()),
        ParamDef::optional("beta",   BaseType::FLOAT, "Scale for C (default 0.0)" , 0.0)
    };
    _example    = "product = matrix_gemm(rot_mat, points)\n"
                  "result  = matrix_gemm(A, B, 1.0, C, 1.0)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "gemm", "matmul", "linear algebra", "blas"};
}

ExecutionResult MatrixGemmItem::execute(const std::vector<RuntimeValue>& args,
                                        ExecutionContext& ctx) {
    if (args.size() < 2)
        return ExecutionResult::fail("matrix_gemm: requires mat_a and mat_b");
    cv::Mat a = getMatArg(args[0]);
    cv::Mat b = getMatArg(args[1]);
    if (a.empty() || b.empty())
        return ExecutionResult::fail("matrix_gemm: matrix argument is empty");

    double alpha = args.size() > 2 ? args[2].asNumber() : 1.0;
    cv::Mat c    = args.size() > 3 ? getMatArg(args[3]) : cv::Mat();
    double beta  = args.size() > 4 ? args[4].asNumber() : 0.0;

    // cv::gemm requires CV_32F or CV_64F
    int dtype = CV_64F;
    cv::Mat af, bf, cf, result;
    a.convertTo(af, dtype);
    b.convertTo(bf, dtype);
    if (!c.empty()) {
        c.convertTo(cf, dtype);
    }
    cv::gemm(af, bf, alpha, cf, beta, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_transpose
// ---------------------------------------------------------------------------

MatrixTransposeItem::MatrixTransposeItem() {
    _functionName = "matrix_transpose";
    _description  = "Transpose a matrix Aᵀ (cv::transpose, SIMD-accelerated).";
    _category     = "math";
    _params = {
        ParamDef::required("mat", BaseType::MAT, "Input matrix")
    };
    _example    = "t = matrix_transpose(homography)";
    _returnType = "mat";
    _tags       = {"math", "matrix", "transpose", "linear algebra"};
}

ExecutionResult MatrixTransposeItem::execute(const std::vector<RuntimeValue>& args,
                                             ExecutionContext& ctx) {
    if (args.empty())
        return ExecutionResult::fail("matrix_transpose: requires mat argument");
    cv::Mat a = getMatArg(args[0]);
    if (a.empty())
        return ExecutionResult::fail("matrix_transpose: mat argument is empty");
    cv::Mat result;
    cv::transpose(a, result);
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

// ---------------------------------------------------------------------------
// matrix_invert
// ---------------------------------------------------------------------------

MatrixInvertItem::MatrixInvertItem() {
    _functionName = "matrix_invert";
    _description  = "Invert a square matrix using cv::invert. "
                    "method: \"lu\" (LU decomposition, default) for regular matrices; "
                    "\"svd\" or \"pseudo\" for the Moore-Penrose pseudo-inverse "
                    "(handles singular / non-square matrices); "
                    "\"cholesky\" for symmetric positive-definite matrices.";
    _category     = "math";
    _params = {
        ParamDef::required("mat",    BaseType::MAT,    "Input matrix (square or any size for pseudo)"),
        ParamDef::optional("method", BaseType::STRING, "\"lu\" | \"svd\" | \"cholesky\" | \"pseudo\" (default \"lu\")",
                           std::string("lu"))
    };
    _example    = "inv = matrix_invert(homography)\n"
                  "inv = matrix_invert(cov_mat, \"svd\")";
    _returnType = "mat";
    _tags       = {"math", "matrix", "invert", "inverse", "linear algebra", "svd"};
}

ExecutionResult MatrixInvertItem::execute(const std::vector<RuntimeValue>& args,
                                          ExecutionContext& ctx) {
    if (args.empty())
        return ExecutionResult::fail("matrix_invert: requires mat argument");
    cv::Mat a = getMatArg(args[0]);
    if (a.empty())
        return ExecutionResult::fail("matrix_invert: mat argument is empty");

    std::string methodStr = args.size() > 1 ? args[1].asString() : "lu";
    int method = cv::DECOMP_LU;
    if      (methodStr == "svd" || methodStr == "pseudo") method = cv::DECOMP_SVD;
    else if (methodStr == "cholesky") method = cv::DECOMP_CHOLESKY;

    // cv::invert requires CV_32F or CV_64F
    cv::Mat f, result;
    a.convertTo(f, CV_64F);
    double det = cv::invert(f, result, method);
    if (det == 0.0 && methodStr == "lu")
        return ExecutionResult::fail("matrix_invert: matrix is singular (try method \"svd\" for pseudo-inverse)");
    return ExecutionResult::okWithMat(result, RuntimeValue(result));
}

} // namespace visionpipe

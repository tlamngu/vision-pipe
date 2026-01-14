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

} // namespace visionpipe

#include "interpreter/items/conditional_items.h"
#include "interpreter/cache_manager.h"
#include <opencv2/highgui.hpp>
#include <iostream>
#include <cmath>

namespace visionpipe {

void registerConditionalItems(ItemRegistry& registry) {
    // Comparison operations
    registry.add<ScalarCompareItem>();
    registry.add<ScalarInRangeItem>();
    
    // Logical operations
    registry.add<LogicalAndItem>();
    registry.add<LogicalOrItem>();
    registry.add<LogicalNotItem>();
    registry.add<LogicalXorItem>();
    
    // Conditional variable assignment
    registry.add<SelectItem>();
    registry.add<SetIfItem>();
    registry.add<ToggleVarItem>();
    
    // Conditional pipeline control
    registry.add<SkipIfFalseItem>();
    registry.add<SkipIfTrueItem>();
    
    // Value testing
    registry.add<IsDefinedItem>();
    registry.add<IsZeroItem>();
    registry.add<IsPositiveItem>();
    registry.add<IsNegativeItem>();
    registry.add<IsEvenItem>();
    registry.add<IsOddItem>();
    
    // Conditional Mat operations
    registry.add<CacheIfItem>();
    registry.add<UseIfItem>();
    registry.add<DisplayIfItem>();
    
    // Counter and trigger operations
    registry.add<TriggerEveryItem>();
    registry.add<TriggerOnRisingItem>();
    registry.add<TriggerOnFallingItem>();
    registry.add<TriggerOnChangeItem>();
}

// Helper function to get numeric value from argument
static double getNumericValue(const RuntimeValue& arg, ExecutionContext& ctx) {
    if (arg.isNumeric()) {
        return arg.asNumber();
    } else if (arg.isString()) {
        std::string varName = arg.asString();
        if (ctx.variables.find(varName) != ctx.variables.end()) {
            return ctx.variables[varName].asNumber();
        }
    }
    return 0.0;
}

// Helper function to get boolean value from argument
static bool getBoolValue(const RuntimeValue& arg, ExecutionContext& ctx) {
    if (arg.isBool()) {
        return arg.asBool();
    } else if (arg.isNumeric()) {
        return arg.asNumber() != 0.0;
    } else if (arg.isString()) {
        std::string varName = arg.asString();
        if (ctx.variables.find(varName) != ctx.variables.end()) {
            RuntimeValue& val = ctx.variables[varName];
            if (val.isBool()) return val.asBool();
            if (val.isNumeric()) return val.asNumber() != 0.0;
        }
    }
    return false;
}

// ============================================================================
// Comparison Operations
// ============================================================================

ScalarCompareItem::ScalarCompareItem() {
    _functionName = "scalar_compare";
    _description = "Compares two values and stores boolean result";
    _category = "conditional";
    _params = {
        ParamDef::required("a", BaseType::FLOAT, "First value or variable name"),
        ParamDef::required("op", BaseType::STRING, "Operator: ==, !=, <, <=, >, >="),
        ParamDef::required("b", BaseType::FLOAT, "Second value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "scalar_compare(threshold, \">\", 100, \"is_high\")";
    _returnType = "mat";
    _tags = {"conditional", "compare", "logic"};
}

ExecutionResult ScalarCompareItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double a = getNumericValue(args[0], ctx);
    std::string op = args[1].asString();
    double b = getNumericValue(args[2], ctx);
    std::string resultVar = args[3].asString();
    
    bool result = false;
    
    if (op == "==" || op == "eq") {
        result = std::abs(a - b) < 1e-10;
    } else if (op == "!=" || op == "ne") {
        result = std::abs(a - b) >= 1e-10;
    } else if (op == "<" || op == "lt") {
        result = a < b;
    } else if (op == "<=" || op == "le") {
        result = a <= b;
    } else if (op == ">" || op == "gt") {
        result = a > b;
    } else if (op == ">=" || op == "ge") {
        result = a >= b;
    } else {
        return ExecutionResult::fail("Unknown operator: " + op);
    }
    
    ctx.variables[resultVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

ScalarInRangeItem::ScalarInRangeItem() {
    _functionName = "scalar_in_range";
    _description = "Checks if value is within range [min, max]";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Value to check or variable name"),
        ParamDef::required("min", BaseType::FLOAT, "Minimum value"),
        ParamDef::required("max", BaseType::FLOAT, "Maximum value"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "scalar_in_range(threshold, 50, 200, \"is_valid\")";
    _returnType = "mat";
    _tags = {"conditional", "range", "check"};
}

ExecutionResult ScalarInRangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    double minVal = getNumericValue(args[1], ctx);
    double maxVal = getNumericValue(args[2], ctx);
    std::string resultVar = args[3].asString();
    
    bool result = (value >= minVal) && (value <= maxVal);
    ctx.variables[resultVar] = RuntimeValue(result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Logical Operations
// ============================================================================

LogicalAndItem::LogicalAndItem() {
    _functionName = "logical_and";
    _description = "Logical AND of two boolean values";
    _category = "conditional";
    _params = {
        ParamDef::required("a", BaseType::BOOL, "First boolean value or variable name"),
        ParamDef::required("b", BaseType::BOOL, "Second boolean value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "logical_and(cond_a, cond_b, \"both_true\")";
    _returnType = "mat";
    _tags = {"conditional", "logic", "and"};
}

ExecutionResult LogicalAndItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool a = getBoolValue(args[0], ctx);
    bool b = getBoolValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    ctx.variables[resultVar] = RuntimeValue(a && b);
    
    return ExecutionResult::ok(ctx.currentMat);
}

LogicalOrItem::LogicalOrItem() {
    _functionName = "logical_or";
    _description = "Logical OR of two boolean values";
    _category = "conditional";
    _params = {
        ParamDef::required("a", BaseType::BOOL, "First boolean value or variable name"),
        ParamDef::required("b", BaseType::BOOL, "Second boolean value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "logical_or(cond_a, cond_b, \"any_true\")";
    _returnType = "mat";
    _tags = {"conditional", "logic", "or"};
}

ExecutionResult LogicalOrItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool a = getBoolValue(args[0], ctx);
    bool b = getBoolValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    ctx.variables[resultVar] = RuntimeValue(a || b);
    
    return ExecutionResult::ok(ctx.currentMat);
}

LogicalNotItem::LogicalNotItem() {
    _functionName = "logical_not";
    _description = "Logical NOT of a boolean value";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::BOOL, "Boolean value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "logical_not(is_enabled, \"is_disabled\")";
    _returnType = "mat";
    _tags = {"conditional", "logic", "not"};
}

ExecutionResult LogicalNotItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool value = getBoolValue(args[0], ctx);
    std::string resultVar = args[1].asString();
    
    ctx.variables[resultVar] = RuntimeValue(!value);
    
    return ExecutionResult::ok(ctx.currentMat);
}

LogicalXorItem::LogicalXorItem() {
    _functionName = "logical_xor";
    _description = "Logical XOR of two boolean values";
    _category = "conditional";
    _params = {
        ParamDef::required("a", BaseType::BOOL, "First boolean value or variable name"),
        ParamDef::required("b", BaseType::BOOL, "Second boolean value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "logical_xor(cond_a, cond_b, \"one_true\")";
    _returnType = "mat";
    _tags = {"conditional", "logic", "xor"};
}

ExecutionResult LogicalXorItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool a = getBoolValue(args[0], ctx);
    bool b = getBoolValue(args[1], ctx);
    std::string resultVar = args[2].asString();
    
    ctx.variables[resultVar] = RuntimeValue(a != b);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Conditional Variable Assignment
// ============================================================================

SelectItem::SelectItem() {
    _functionName = "select";
    _description = "Sets variable to one of two values based on condition";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Boolean condition value or variable"),
        ParamDef::required("true_value", BaseType::ANY, "Value if condition is true"),
        ParamDef::required("false_value", BaseType::ANY, "Value if condition is false"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "select(is_valid, 255, 0, \"output_value\")";
    _returnType = "mat";
    _tags = {"conditional", "select", "ternary"};
}

ExecutionResult SelectItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = getBoolValue(args[0], ctx);
    std::string resultVar = args[3].asString();
    
    if (condition) {
        ctx.variables[resultVar] = args[1];
    } else {
        ctx.variables[resultVar] = args[2];
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

SetIfItem::SetIfItem() {
    _functionName = "set_if";
    _description = "Sets variable only if condition is true";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Boolean condition value or variable"),
        ParamDef::required("var_name", BaseType::STRING, "Variable name to set"),
        ParamDef::required("value", BaseType::ANY, "Value to set")
    };
    _example = "set_if(is_valid, \"threshold\", 128)";
    _returnType = "mat";
    _tags = {"conditional", "set", "variable"};
}

ExecutionResult SetIfItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = getBoolValue(args[0], ctx);
    std::string varName = args[1].asString();
    
    if (condition) {
        ctx.variables[varName] = args[2];
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

ToggleVarItem::ToggleVarItem() {
    _functionName = "toggle_var";
    _description = "Toggles a boolean variable";
    _category = "conditional";
    _params = {
        ParamDef::required("var_name", BaseType::STRING, "Boolean variable name to toggle")
    };
    _example = "toggle_var(\"is_enabled\")";
    _returnType = "mat";
    _tags = {"conditional", "toggle", "boolean"};
}

ExecutionResult ToggleVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string varName = args[0].asString();
    
    bool current = false;
    if (ctx.variables.find(varName) != ctx.variables.end()) {
        current = getBoolValue(ctx.variables[varName], ctx);
    }
    
    ctx.variables[varName] = RuntimeValue(!current);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Conditional Pipeline Control
// ============================================================================

SkipIfFalseItem::SkipIfFalseItem() {
    _functionName = "skip_if_false";
    _description = "Sets skip count if condition is false (used with pipeline interpreter)";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Boolean condition value or variable"),
        ParamDef::optional("count", BaseType::INT, "Number of items to skip", 1)
    };
    _example = "skip_if_false(is_enabled, 3)";
    _returnType = "mat";
    _tags = {"conditional", "skip", "control"};
}

ExecutionResult SkipIfFalseItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = getBoolValue(args[0], ctx);
    int count = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 1;
    
    if (!condition) {
        ctx.variables["_skip_count"] = RuntimeValue(static_cast<int64_t>(count));
    } else {
        ctx.variables["_skip_count"] = RuntimeValue(static_cast<int64_t>(0));
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

SkipIfTrueItem::SkipIfTrueItem() {
    _functionName = "skip_if_true";
    _description = "Sets skip count if condition is true (used with pipeline interpreter)";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Boolean condition value or variable"),
        ParamDef::optional("count", BaseType::INT, "Number of items to skip", 1)
    };
    _example = "skip_if_true(is_disabled, 3)";
    _returnType = "mat";
    _tags = {"conditional", "skip", "control"};
}

ExecutionResult SkipIfTrueItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = getBoolValue(args[0], ctx);
    int count = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 1;
    
    if (condition) {
        ctx.variables["_skip_count"] = RuntimeValue(static_cast<int64_t>(count));
    } else {
        ctx.variables["_skip_count"] = RuntimeValue(static_cast<int64_t>(0));
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Value Testing
// ============================================================================

IsDefinedItem::IsDefinedItem() {
    _functionName = "is_defined";
    _description = "Checks if variable exists";
    _category = "conditional";
    _params = {
        ParamDef::required("var_name", BaseType::STRING, "Variable name to check"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "is_defined(\"threshold\", \"has_threshold\")";
    _returnType = "mat";
    _tags = {"conditional", "check", "variable"};
}

ExecutionResult IsDefinedItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string varName = args[0].asString();
    std::string resultVar = args[1].asString();
    
    bool exists = ctx.variables.find(varName) != ctx.variables.end();
    ctx.variables[resultVar] = RuntimeValue(exists);
    
    return ExecutionResult::ok(ctx.currentMat);
}

IsZeroItem::IsZeroItem() {
    _functionName = "is_zero";
    _description = "Checks if value is zero";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "is_zero(counter, \"is_start\")";
    _returnType = "mat";
    _tags = {"conditional", "check", "zero"};
}

ExecutionResult IsZeroItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string resultVar = args[1].asString();
    
    ctx.variables[resultVar] = RuntimeValue(std::abs(value) < 1e-10);
    
    return ExecutionResult::ok(ctx.currentMat);
}

IsPositiveItem::IsPositiveItem() {
    _functionName = "is_positive";
    _description = "Checks if value is positive (> 0)";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "is_positive(diff, \"is_growing\")";
    _returnType = "mat";
    _tags = {"conditional", "check", "positive"};
}

ExecutionResult IsPositiveItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string resultVar = args[1].asString();
    
    ctx.variables[resultVar] = RuntimeValue(value > 0);
    
    return ExecutionResult::ok(ctx.currentMat);
}

IsNegativeItem::IsNegativeItem() {
    _functionName = "is_negative";
    _description = "Checks if value is negative (< 0)";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "is_negative(diff, \"is_shrinking\")";
    _returnType = "mat";
    _tags = {"conditional", "check", "negative"};
}

ExecutionResult IsNegativeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = getNumericValue(args[0], ctx);
    std::string resultVar = args[1].asString();
    
    ctx.variables[resultVar] = RuntimeValue(value < 0);
    
    return ExecutionResult::ok(ctx.currentMat);
}

IsEvenItem::IsEvenItem() {
    _functionName = "is_even";
    _description = "Checks if value is even integer";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::INT, "Value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "is_even(frame_count, \"is_even_frame\")";
    _returnType = "mat";
    _tags = {"conditional", "check", "even"};
}

ExecutionResult IsEvenItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int value = static_cast<int>(getNumericValue(args[0], ctx));
    std::string resultVar = args[1].asString();
    
    ctx.variables[resultVar] = RuntimeValue(value % 2 == 0);
    
    return ExecutionResult::ok(ctx.currentMat);
}

IsOddItem::IsOddItem() {
    _functionName = "is_odd";
    _description = "Checks if value is odd integer";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::INT, "Value or variable name"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for result")
    };
    _example = "is_odd(frame_count, \"is_odd_frame\")";
    _returnType = "mat";
    _tags = {"conditional", "check", "odd"};
}

ExecutionResult IsOddItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int value = static_cast<int>(getNumericValue(args[0], ctx));
    std::string resultVar = args[1].asString();
    
    ctx.variables[resultVar] = RuntimeValue(value % 2 != 0);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Conditional Mat Operations
// ============================================================================

CacheIfItem::CacheIfItem() {
    _functionName = "cache_if";
    _description = "Caches current Mat only if condition is true";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Boolean condition value or variable"),
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier")
    };
    _example = "cache_if(is_valid, \"good_frame\")";
    _returnType = "mat";
    _tags = {"conditional", "cache", "mat"};
}

ExecutionResult CacheIfItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = getBoolValue(args[0], ctx);
    std::string cacheId = args[1].asString();
    
    if (condition && ctx.cacheManager) {
        ctx.cacheManager->set(cacheId, ctx.currentMat.clone());
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

UseIfItem::UseIfItem() {
    _functionName = "use_if";
    _description = "Loads cached Mat only if condition is true";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Boolean condition value or variable"),
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier")
    };
    _example = "use_if(need_previous, \"previous_frame\")";
    _returnType = "mat";
    _tags = {"conditional", "cache", "load"};
}

ExecutionResult UseIfItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = getBoolValue(args[0], ctx);
    std::string cacheId = args[1].asString();
    
    if (condition && ctx.cacheManager) {
        cv::Mat cached = ctx.cacheManager->get(cacheId);
        if (!cached.empty()) {
            ctx.currentMat = cached.clone();
        }
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

DisplayIfItem::DisplayIfItem() {
    _functionName = "display_if";
    _description = "Displays Mat only if condition is true";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Boolean condition value or variable"),
        ParamDef::optional("window_name", BaseType::STRING, "Window name", "Output")
    };
    _example = "display_if(show_debug, \"Debug View\")";
    _returnType = "mat";
    _tags = {"conditional", "display", "window"};
}

ExecutionResult DisplayIfItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = getBoolValue(args[0], ctx);
    std::string windowName = args.size() > 1 ? args[1].asString() : "Output";
    
    if (condition && !ctx.currentMat.empty()) {
        cv::imshow(windowName, ctx.currentMat);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// Counter and Trigger Operations
// ============================================================================

TriggerEveryItem::TriggerEveryItem() {
    _functionName = "trigger_every";
    _description = "Triggers true every N increments of counter";
    _category = "conditional";
    _params = {
        ParamDef::required("counter_var", BaseType::STRING, "Counter variable name"),
        ParamDef::required("interval", BaseType::INT, "Interval for triggering"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for trigger result")
    };
    _example = "trigger_every(\"frame_count\", 10, \"every_10th\")";
    _returnType = "mat";
    _tags = {"conditional", "trigger", "interval"};
}

ExecutionResult TriggerEveryItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string counterVar = args[0].asString();
    int interval = static_cast<int>(args[1].asNumber());
    std::string resultVar = args[2].asString();
    
    int counter = 0;
    if (ctx.variables.find(counterVar) != ctx.variables.end()) {
        counter = static_cast<int>(ctx.variables[counterVar].asNumber());
    }
    
    bool triggered = (interval > 0) && (counter % interval == 0);
    ctx.variables[resultVar] = RuntimeValue(triggered);
    
    return ExecutionResult::ok(ctx.currentMat);
}

TriggerOnRisingItem::TriggerOnRisingItem() {
    _functionName = "trigger_on_rising";
    _description = "Triggers true once when condition changes from false to true";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Current condition value or variable"),
        ParamDef::required("state_var", BaseType::STRING, "Variable to track previous state"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for trigger result")
    };
    _example = "trigger_on_rising(button_pressed, \"_prev_button\", \"just_pressed\")";
    _returnType = "mat";
    _tags = {"conditional", "trigger", "rising", "edge"};
}

ExecutionResult TriggerOnRisingItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool current = getBoolValue(args[0], ctx);
    std::string stateVar = args[1].asString();
    std::string resultVar = args[2].asString();
    
    bool previous = false;
    if (ctx.variables.find(stateVar) != ctx.variables.end()) {
        previous = getBoolValue(ctx.variables[stateVar], ctx);
    }
    
    bool triggered = current && !previous;
    ctx.variables[resultVar] = RuntimeValue(triggered);
    ctx.variables[stateVar] = RuntimeValue(current);
    
    return ExecutionResult::ok(ctx.currentMat);
}

TriggerOnFallingItem::TriggerOnFallingItem() {
    _functionName = "trigger_on_falling";
    _description = "Triggers true once when condition changes from true to false";
    _category = "conditional";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Current condition value or variable"),
        ParamDef::required("state_var", BaseType::STRING, "Variable to track previous state"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for trigger result")
    };
    _example = "trigger_on_falling(button_pressed, \"_prev_button\", \"just_released\")";
    _returnType = "mat";
    _tags = {"conditional", "trigger", "falling", "edge"};
}

ExecutionResult TriggerOnFallingItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool current = getBoolValue(args[0], ctx);
    std::string stateVar = args[1].asString();
    std::string resultVar = args[2].asString();
    
    bool previous = false;
    if (ctx.variables.find(stateVar) != ctx.variables.end()) {
        previous = getBoolValue(ctx.variables[stateVar], ctx);
    }
    
    bool triggered = !current && previous;
    ctx.variables[resultVar] = RuntimeValue(triggered);
    ctx.variables[stateVar] = RuntimeValue(current);
    
    return ExecutionResult::ok(ctx.currentMat);
}

TriggerOnChangeItem::TriggerOnChangeItem() {
    _functionName = "trigger_on_change";
    _description = "Triggers true once when value changes";
    _category = "conditional";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Current value or variable"),
        ParamDef::required("state_var", BaseType::STRING, "Variable to track previous value"),
        ParamDef::required("result_var", BaseType::STRING, "Variable name for trigger result")
    };
    _example = "trigger_on_change(slider_value, \"_prev_slider\", \"slider_changed\")";
    _returnType = "mat";
    _tags = {"conditional", "trigger", "change"};
}

ExecutionResult TriggerOnChangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double current = getNumericValue(args[0], ctx);
    std::string stateVar = args[1].asString();
    std::string resultVar = args[2].asString();
    
    double previous = current;  // Default: no change on first call
    if (ctx.variables.find(stateVar) != ctx.variables.end()) {
        previous = ctx.variables[stateVar].asNumber();
    }
    
    bool triggered = std::abs(current - previous) > 1e-10;
    ctx.variables[resultVar] = RuntimeValue(triggered);
    ctx.variables[stateVar] = RuntimeValue(current);
    
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

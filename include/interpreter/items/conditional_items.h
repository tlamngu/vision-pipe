#ifndef VISIONPIPE_CONDITIONAL_ITEMS_H
#define VISIONPIPE_CONDITIONAL_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerConditionalItems(ItemRegistry& registry);

// ============================================================================
// Comparison Operations
// ============================================================================

/**
 * @brief Compares two values and stores boolean result
 * 
 * Parameters:
 * - a: First value or variable name
 * - op: Operator string ("==", "!=", "<", "<=", ">", ">=")
 * - b: Second value or variable name
 * - result_var: Variable name to store boolean result
 */
class ScalarCompareItem : public InterpreterItem {
public:
    ScalarCompareItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if value is within range [min, max]
 * 
 * Parameters:
 * - value: Value to check or variable name
 * - min: Minimum value
 * - max: Maximum value
 * - result_var: Variable name to store boolean result
 */
class ScalarInRangeItem : public InterpreterItem {
public:
    ScalarInRangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Logical Operations
// ============================================================================

/**
 * @brief Logical AND of two boolean values
 * 
 * Parameters:
 * - a: First boolean value or variable name
 * - b: Second boolean value or variable name
 * - result_var: Variable name to store result
 */
class LogicalAndItem : public InterpreterItem {
public:
    LogicalAndItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Logical OR of two boolean values
 */
class LogicalOrItem : public InterpreterItem {
public:
    LogicalOrItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Logical NOT of a boolean value
 */
class LogicalNotItem : public InterpreterItem {
public:
    LogicalNotItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Logical XOR of two boolean values
 */
class LogicalXorItem : public InterpreterItem {
public:
    LogicalXorItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Conditional Variable Assignment
// ============================================================================

/**
 * @brief Sets variable to one of two values based on condition
 * 
 * Parameters:
 * - condition: Boolean condition value or variable name
 * - true_value: Value if condition is true
 * - false_value: Value if condition is false
 * - result_var: Variable name to store result
 */
class SelectItem : public InterpreterItem {
public:
    SelectItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets variable only if condition is true
 * 
 * Parameters:
 * - condition: Boolean condition value or variable name
 * - var_name: Variable name to set
 * - value: Value to set
 */
class SetIfItem : public InterpreterItem {
public:
    SetIfItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Toggles a boolean variable
 * 
 * Parameters:
 * - var_name: Boolean variable name to toggle
 */
class ToggleVarItem : public InterpreterItem {
public:
    ToggleVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Conditional Pipeline Control
// ============================================================================

/**
 * @brief Skips next N items if condition is false
 * 
 * Parameters:
 * - condition: Boolean condition value or variable name
 * - count: Number of items to skip if false (default: 1)
 */
class SkipIfFalseItem : public InterpreterItem {
public:
    SkipIfFalseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Skips next N items if condition is true
 */
class SkipIfTrueItem : public InterpreterItem {
public:
    SkipIfTrueItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Value Testing
// ============================================================================

/**
 * @brief Checks if variable exists
 * 
 * Parameters:
 * - var_name: Variable name to check
 * - result_var: Variable name to store boolean result
 */
class IsDefinedItem : public InterpreterItem {
public:
    IsDefinedItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if value is zero
 */
class IsZeroItem : public InterpreterItem {
public:
    IsZeroItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if value is positive (> 0)
 */
class IsPositiveItem : public InterpreterItem {
public:
    IsPositiveItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if value is negative (< 0)
 */
class IsNegativeItem : public InterpreterItem {
public:
    IsNegativeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if value is even integer
 */
class IsEvenItem : public InterpreterItem {
public:
    IsEvenItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if value is odd integer
 */
class IsOddItem : public InterpreterItem {
public:
    IsOddItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Conditional Mat Operations
// ============================================================================

/**
 * @brief Caches current Mat only if condition is true
 * 
 * Parameters:
 * - condition: Boolean condition value or variable name
 * - cache_id: Cache identifier
 */
class CacheIfItem : public InterpreterItem {
public:
    CacheIfItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Loads cached Mat only if condition is true
 * 
 * Parameters:
 * - condition: Boolean condition value or variable name
 * - cache_id: Cache identifier
 */
class UseIfItem : public InterpreterItem {
public:
    UseIfItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Displays Mat only if condition is true
 * 
 * Parameters:
 * - condition: Boolean condition value or variable name
 * - window_name: Window name (default: "Output")
 */
class DisplayIfItem : public InterpreterItem {
public:
    DisplayIfItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Counter and Trigger Operations
// ============================================================================

/**
 * @brief Triggers true every N frames
 * 
 * Parameters:
 * - counter_var: Counter variable name
 * - interval: Interval for triggering (every N counts)
 * - result_var: Variable name for trigger result
 */
class TriggerEveryItem : public InterpreterItem {
public:
    TriggerEveryItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Triggers true once when condition changes from false to true
 * 
 * Parameters:
 * - condition: Current condition value
 * - state_var: Variable to track previous state
 * - result_var: Variable name for trigger result
 */
class TriggerOnRisingItem : public InterpreterItem {
public:
    TriggerOnRisingItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Triggers true once when condition changes from true to false
 */
class TriggerOnFallingItem : public InterpreterItem {
public:
    TriggerOnFallingItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Triggers true once when value changes
 * 
 * Parameters:
 * - value: Current value or variable
 * - state_var: Variable to track previous value
 * - result_var: Variable name for trigger result
 */
class TriggerOnChangeItem : public InterpreterItem {
public:
    TriggerOnChangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_CONDITIONAL_ITEMS_H

#ifndef VISIONPIPE_CONTROL_ITEMS_H
#define VISIONPIPE_CONTROL_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerControlItems(ItemRegistry& registry);

// ============================================================================
// Cache Operations
// ============================================================================

/**
 * @brief Uses (loads) a cached Mat by cache ID
 * 
 * Parameters:
 * - cache_id: String cache identifier
 * 
 * Note: This is typically used with `use "cache_id"` syntax
 */
class UseItem : public InterpreterItem {
public:
    UseItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Caches current Mat with specified ID
 * 
 * Parameters:
 * - cache_id: String cache identifier
 * 
 * Note: This is typically used with `cache("id")` or `func() -> "id"` syntax
 */
class CacheItem : public InterpreterItem {
public:
    CacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Caches current Mat GLOBALLY with specified ID
 * 
 * Parameters:
 * - cache_id: String cache identifier
 * 
 * Note: This caches to global scope, accessible from all pipelines
 */
class GlobalCacheItem : public InterpreterItem {
public:
    GlobalCacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Promotes a local cache entry to global scope
 * 
 * Parameters:
 * - cache_id: String cache identifier to promote
 * 
 * Note: This is used with `global("cache_id")` syntax inside pipelines
 */
class PromoteToGlobalItem : public InterpreterItem {
public:
    PromoteToGlobalItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Clears cached Mat(s)
 * 
 * Parameters:
 * - cache_id: Specific cache ID to clear (optional)
 *             If omitted, clears all cached items
 */
class ClearCacheItem : public InterpreterItem {
public:
    ClearCacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Lists all cached item IDs
 * 
 * Returns: Array of cache IDs
 */
class ListCacheItem : public InterpreterItem {
public:
    ListCacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if cache ID exists
 * 
 * Parameters:
 * - cache_id: Cache ID to check
 * 
 * Returns: Boolean
 */
class HasCacheItem : public InterpreterItem {
public:
    HasCacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Copies cache from one ID to another
 * 
 * Parameters:
 * - from_id: Source cache ID
 * - to_id: Destination cache ID
 */
class CopyCacheItem : public InterpreterItem {
public:
    CopyCacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Loads a cached Mat and sets it as current
 * 
 * Parameters:
 * - cache_id: Cache ID to load
 */
class LoadCacheItem : public InterpreterItem {
public:
    LoadCacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Swaps two cached Mats
 * 
 * Parameters:
 * - id1: First cache ID
 * - id2: Second cache ID
 */
class SwapCacheItem : public InterpreterItem {
public:
    SwapCacheItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Flow Control
// ============================================================================

/**
 * @brief Pass-through, does nothing (placeholder in pipeline)
 */
class PassItem : public InterpreterItem {
public:
    PassItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sleeps for specified duration
 * 
 * Parameters:
 * - milliseconds: Sleep duration in milliseconds
 */
class SleepItem : public InterpreterItem {
public:
    SleepItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Stops pipeline execution
 * 
 * Parameters:
 * - message: Optional stop message
 */
class StopItem : public InterpreterItem {
public:
    StopItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Exits pipeline with specified code
 * 
 * Parameters:
 * - code: Exit code (default: 0)
 */
class ExitItem : public InterpreterItem {
public:
    ExitItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Skips to next iteration in loop
 */
class ContinueItem : public InterpreterItem {
public:
    ContinueItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Breaks out of current loop
 */
class BreakItem : public InterpreterItem {
public:
    BreakItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Conditional execution
 * 
 * Parameters:
 * - condition: Condition variable or expression
 */
class IfItem : public InterpreterItem {
public:
    IfItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Loop execution
 * 
 * Parameters:
 * - count: Number of iterations
 */
class LoopItem : public InterpreterItem {
public:
    LoopItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Returns from current function/block
 */
class ReturnItem : public InterpreterItem {
public:
    ReturnItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Variable Operations
// ============================================================================

/**
 * @brief Sets a variable value
 * 
 * Parameters:
 * - name: Variable name
 * - value: Value to set
 */
class SetVarItem : public InterpreterItem {
public:
    SetVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets a variable value
 * 
 * Parameters:
 * - name: Variable name
 * - default: Default value if not found (optional)
 */
class GetVarItem : public InterpreterItem {
public:
    GetVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Deletes a variable
 * 
 * Parameters:
 * - name: Variable name to delete
 */
class DeleteVarItem : public InterpreterItem {
public:
    DeleteVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Lists all defined variables
 * 
 * Returns: Array of variable names
 */
class ListVarsItem : public InterpreterItem {
public:
    ListVarsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if variable exists
 * 
 * Parameters:
 * - name: Variable name
 * 
 * Returns: Boolean
 */
class HasVarItem : public InterpreterItem {
public:
    HasVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Increments a variable by specified amount
 * 
 * Parameters:
 * - name: Variable name
 * - amount: Increment amount (default: 1.0)
 */
class IncrementVarItem : public InterpreterItem {
public:
    IncrementVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Decrements a variable by specified amount
 * 
 * Parameters:
 * - name: Variable name
 * - amount: Decrement amount (default: 1.0)
 */
class DecrementVarItem : public InterpreterItem {
public:
    DecrementVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Mat Operations
// ============================================================================

/**
 * @brief Creates a new empty Mat
 * 
 * Parameters:
 * - width: Image width (default: 640)
 * - height: Image height (default: 480)
 * - type: Mat type string (default: "CV_8UC3")
 * - value: Initial value or [B, G, R] (default: 0)
 */
class CreateMatItem : public InterpreterItem {
public:
    CreateMatItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates Mat filled with zeros
 * 
 * Parameters:
 * - width: Width
 * - height: Height
 * - type: Mat type (default: "CV_8UC3")
 */
class ZerosItem : public InterpreterItem {
public:
    ZerosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates Mat filled with ones
 * 
 * Parameters:
 * - width: Width
 * - height: Height
 * - type: Mat type (default: "CV_8UC1")
 */
class OnesItem : public InterpreterItem {
public:
    OnesItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates identity matrix
 * 
 * Parameters:
 * - size: Matrix size
 * - type: Mat type (default: "CV_64FC1")
 */
class EyeItem : public InterpreterItem {
public:
    EyeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Clones current Mat
 */
class CloneItem : public InterpreterItem {
public:
    CloneItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Copies Mat to destination with optional mask
 * 
 * Parameters:
 * - mask: Optional mask (uses cached or passed Mat)
 */
class CopyToItem : public InterpreterItem {
public:
    CopyToItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Converts Mat to different type/depth
 * 
 * Parameters:
 * - type: Target type (e.g., "CV_32FC3", "CV_8UC1")
 * - alpha: Optional scale factor
 * - beta: Optional delta to add
 */
class ConvertToItem : public InterpreterItem {
public:
    ConvertToItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets all Mat elements to specified value
 * 
 * Parameters:
 * - value: Value or [B, G, R] for color images
 */
class SetToItem : public InterpreterItem {
public:
    SetToItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Reshapes Mat without copying data
 * 
 * Parameters:
 * - cn: New number of channels
 * - rows: New number of rows (0 = auto)
 */
class ReshapeItem : public InterpreterItem {
public:
    ReshapeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Extracts region of interest
 * 
 * Parameters:
 * - x: X coordinate
 * - y: Y coordinate
 * - width: ROI width
 * - height: ROI height
 */
class ROIItem : public InterpreterItem {
public:
    ROIItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Arithmetic Operations
// ============================================================================

/**
 * @brief Adds two Mats or Mat and scalar
 * 
 * Parameters:
 * - value: Mat (from cache) or scalar value to add
 * - mask: Optional mask
 * - dtype: Optional output type
 */
class AddItem : public InterpreterItem {
public:
    AddItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Subtracts two Mats or Mat and scalar
 * 
 * Parameters:
 * - value: Mat or scalar to subtract
 * - mask: Optional mask
 * - dtype: Optional output type
 */
class SubtractItem : public InterpreterItem {
public:
    SubtractItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Multiplies two Mats element-wise
 * 
 * Parameters:
 * - value: Mat or scalar multiplier
 * - scale: Optional scale factor (default: 1)
 * - dtype: Optional output type
 */
class MultiplyItem : public InterpreterItem {
public:
    MultiplyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Divides two Mats element-wise
 * 
 * Parameters:
 * - value: Mat or scalar divisor
 * - scale: Optional scale factor (default: 1)
 * - dtype: Optional output type
 */
class DivideItem : public InterpreterItem {
public:
    DivideItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Blends two images with weights
 * 
 * Parameters:
 * - other: Second image (from cache)
 * - alpha: Weight of first image
 * - beta: Weight of second image
 * - gamma: Scalar added to sum (default: 0)
 * - dtype: Optional output type
 */
class AddWeightedItem : public InterpreterItem {
public:
    AddWeightedItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates absolute value
 */
class AbsItem : public InterpreterItem {
public:
    AbsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates absolute difference between two Mats
 * 
 * Parameters:
 * - other: Second Mat (from cache)
 */
class AbsDiffItem : public InterpreterItem {
public:
    AbsDiffItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Scales and adds two arrays: dst = scale*src1 + src2
 * 
 * Parameters:
 * - other: Second Mat (from cache)
 * - scale: Scale factor for first Mat
 */
class ScaleAddItem : public InterpreterItem {
public:
    ScaleAddItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Bitwise Operations
// ============================================================================

/**
 * @brief Bitwise AND
 * 
 * Parameters:
 * - other: Second Mat (from cache)
 * - mask: Optional mask
 */
class BitwiseAndItem : public InterpreterItem {
public:
    BitwiseAndItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Bitwise OR
 */
class BitwiseOrItem : public InterpreterItem {
public:
    BitwiseOrItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Bitwise XOR
 */
class BitwiseXorItem : public InterpreterItem {
public:
    BitwiseXorItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Bitwise NOT (inversion)
 */
class BitwiseNotItem : public InterpreterItem {
public:
    BitwiseNotItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Comparison Operations
// ============================================================================

/**
 * @brief Element-wise comparison
 * 
 * Parameters:
 * - other: Second Mat or scalar
 * - op: "eq", "ne", "lt", "le", "gt", "ge"
 */
class CompareItem : public InterpreterItem {
public:
    CompareItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise minimum
 * 
 * Parameters:
 * - other: Second Mat or scalar
 */
class MinItem : public InterpreterItem {
public:
    MinItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Element-wise maximum
 */
class MaxItem : public InterpreterItem {
public:
    MaxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Statistical Operations
// ============================================================================

/**
 * @brief Calculates mean value
 * 
 * Parameters:
 * - mask: Optional mask
 * 
 * Returns: [mean_ch0, mean_ch1, ...]
 */
class MeanItem : public InterpreterItem {
public:
    MeanItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates mean and standard deviation
 * 
 * Returns: [[mean_channels], [stddev_channels]]
 */
class MeanStdDevItem : public InterpreterItem {
public:
    MeanStdDevItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Finds non-zero element locations
 * 
 * Returns: Array of [x, y] coordinates
 */
class FindNonZeroItem : public InterpreterItem {
public:
    FindNonZeroItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sorts Mat elements
 * 
 * Parameters:
 * - flags: "ascending", "descending", "every_row", "every_column"
 */
class SortItem : public InterpreterItem {
public:
    SortItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_CONTROL_ITEMS_H

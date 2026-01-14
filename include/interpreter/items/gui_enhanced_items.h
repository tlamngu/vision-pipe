#ifndef VISIONPIPE_GUI_ENHANCED_ITEMS_H
#define VISIONPIPE_GUI_ENHANCED_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>
#include <functional>
#include <map>

namespace visionpipe {

void registerGuiEnhancedItems(ItemRegistry& registry);

// ============================================================================
// Trackbar-Variable Binding
// ============================================================================

/**
 * @brief Creates a trackbar bound to a variable - the variable auto-updates
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 * - var_name: Variable name to bind (will be updated when trackbar moves)
 * - min_val: Minimum value
 * - max_val: Maximum value
 * - initial: Initial value (optional, defaults to min_val)
 */
class TrackbarVarItem : public InterpreterItem {
public:
    TrackbarVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Syncs an existing trackbar to a variable (bidirectional)
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 * - var_name: Variable name to sync
 */
class SyncTrackbarVarItem : public InterpreterItem {
public:
    SyncTrackbarVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets trackbar value as expression-usable value and stores in variable
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 * - var_name: Variable name to store the value
 */
class TrackbarValueItem : public InterpreterItem {
public:
    TrackbarValueItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets trackbar position from a variable value
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 * - var_name: Variable name containing the value
 */
class SetTrackbarFromVarItem : public InterpreterItem {
public:
    SetTrackbarFromVarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Checkbox/Toggle UI Items
// ============================================================================

/**
 * @brief Creates a checkbox using a trackbar (0-1 range) bound to a boolean variable
 * 
 * Parameters:
 * - name: Checkbox name
 * - window: Window name
 * - var_name: Boolean variable name
 * - initial: Initial state (optional, defaults to false/0)
 */
class CheckboxItem : public InterpreterItem {
public:
    CheckboxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets checkbox state as boolean
 * 
 * Parameters:
 * - name: Checkbox name
 * - window: Window name
 * - var_name: Variable to store boolean result
 */
class GetCheckboxItem : public InterpreterItem {
public:
    GetCheckboxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Button Simulation Items
// ============================================================================

/**
 * @brief Creates a button-like trackbar that resets after being clicked
 * 
 * Parameters:
 * - name: Button name
 * - window: Window name
 * - clicked_var: Variable set to true when button is clicked
 */
class ButtonItem : public InterpreterItem {
public:
    ButtonItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Checks if a button was clicked and resets the state
 * 
 * Parameters:
 * - name: Button name
 * - window: Window name
 * - result_var: Variable to store click state
 */
class CheckButtonClickItem : public InterpreterItem {
public:
    CheckButtonClickItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Multi-Slider/Group Items
// ============================================================================

/**
 * @brief Creates an HSV range control panel (6 trackbars for min/max H, S, V)
 * 
 * Parameters:
 * - prefix: Variable prefix (creates prefix_h_min, prefix_h_max, etc.)
 * - window: Window name
 */
class HsvRangeControlItem : public InterpreterItem {
public:
    HsvRangeControlItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets HSV range values from control panel into variables
 * 
 * Parameters:
 * - prefix: Variable prefix
 * - window: Window name
 */
class GetHsvRangeItem : public InterpreterItem {
public:
    GetHsvRangeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Creates a color picker control panel (3 trackbars for B, G, R)
 * 
 * Parameters:
 * - prefix: Variable prefix (creates prefix_b, prefix_g, prefix_r)
 * - window: Window name
 * - initial_b, initial_g, initial_r: Initial values (optional)
 */
class ColorPickerControlItem : public InterpreterItem {
public:
    ColorPickerControlItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets color from picker into a Scalar variable
 * 
 * Parameters:
 * - prefix: Variable prefix
 * - window: Window name
 * - var_name: Variable to store Scalar result
 */
class GetColorFromPickerItem : public InterpreterItem {
public:
    GetColorFromPickerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Control Panel Layout Items
// ============================================================================

/**
 * @brief Creates a control panel window with common image processing controls
 * 
 * Parameters:
 * - name: Control panel window name
 * - preset: Preset type ("threshold", "blur", "edge", "morphology", "color")
 */
class ControlPanelItem : public InterpreterItem {
public:
    ControlPanelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Reads all control panel values into variables
 * 
 * Parameters:
 * - name: Control panel window name
 * - prefix: Variable prefix for all values
 */
class ReadControlPanelItem : public InterpreterItem {
public:
    ReadControlPanelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Display Enhancement Items
// ============================================================================

/**
 * @brief Displays an image with an overlay showing variable values
 * 
 * Parameters:
 * - window: Window name
 * - var_list: Comma-separated list of variable names to display
 */
class ImshowWithVarsItem : public InterpreterItem {
public:
    ImshowWithVarsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Updates window title with variable value
 * 
 * Parameters:
 * - window: Window name
 * - format: Title format string with {var} placeholders
 */
class UpdateWindowTitleItem : public InterpreterItem {
public:
    UpdateWindowTitleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_GUI_ENHANCED_ITEMS_H

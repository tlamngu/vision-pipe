#ifndef VISIONPIPE_DISPLAY_ITEMS_H
#define VISIONPIPE_DISPLAY_ITEMS_H

#include "interpreter/item_registry.h"
#include <opencv2/opencv.hpp>

namespace visionpipe {

void registerDisplayItems(ItemRegistry& registry);

// ============================================================================
// Window Management
// ============================================================================

/**
 * @brief Creates a named window
 * 
 * Parameters:
 * - name: Window name (default: "VisionPipe")
 * - flags: Window flags:
 *          "normal" - resizable window
 *          "autosize" - fixed size based on image
 *          "opengl" - OpenGL support
 *          "fullscreen" - fullscreen mode
 *          "freeratio" - allow aspect ratio change
 *          "keepratio" - maintain aspect ratio
 *          "gui_expanded" - enhanced GUI
 *          "gui_normal" - basic GUI
 */
class NamedWindowItem : public InterpreterItem {
public:
    NamedWindowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Destroys a named window
 * 
 * Parameters:
 * - name: Window name to destroy
 */
class DestroyWindowItem : public InterpreterItem {
public:
    DestroyWindowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Destroys all open windows
 */
class DestroyAllWindowsItem : public InterpreterItem {
public:
    DestroyAllWindowsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Moves window to specified position
 * 
 * Parameters:
 * - name: Window name
 * - x: X position
 * - y: Y position
 */
class MoveWindowItem : public InterpreterItem {
public:
    MoveWindowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Resizes window to specified size
 * 
 * Parameters:
 * - name: Window name
 * - width: Window width
 * - height: Window height
 */
class ResizeWindowItem : public InterpreterItem {
public:
    ResizeWindowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets window title
 * 
 * Parameters:
 * - name: Window name
 * - title: New window title
 */
class SetWindowTitleItem : public InterpreterItem {
public:
    SetWindowTitleItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets window property
 * 
 * Parameters:
 * - name: Window name
 * - prop: Property name: "fullscreen", "autosize", "aspect_ratio", 
 *         "opengl", "visible", "topmost"
 */
class GetWindowPropertyItem : public InterpreterItem {
public:
    GetWindowPropertyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets window property
 * 
 * Parameters:
 * - name: Window name
 * - prop: Property name
 * - value: Property value
 */
class SetWindowPropertyItem : public InterpreterItem {
public:
    SetWindowPropertyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Image Display
// ============================================================================

/**
 * @brief Displays image in a window
 * 
 * Parameters:
 * - name: Window name (default: "VisionPipe")
 */
class ImShowItem : public InterpreterItem {
public:
    ImShowItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Waits for a key event
 * 
 * Parameters:
 * - delay: Delay in milliseconds (0 = infinite wait)
 * 
 * Returns: ASCII code of pressed key (-1 if no key pressed)
 */
class WaitKeyItem : public InterpreterItem {
public:
    WaitKeyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Polls for UI events without blocking
 * 
 * Returns: ASCII code of pressed key or -1
 */
class PollKeyItem : public InterpreterItem {
public:
    PollKeyItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Trackbars & UI
// ============================================================================

/**
 * @brief Creates a trackbar
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 * - value: Initial value
 * - max: Maximum value
 * - variable: Variable name to store value (optional)
 */
class CreateTrackbarItem : public InterpreterItem {
public:
    CreateTrackbarItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets trackbar position
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 */
class GetTrackbarPosItem : public InterpreterItem {
public:
    GetTrackbarPosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets trackbar position
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 * - value: New position value
 */
class SetTrackbarPosItem : public InterpreterItem {
public:
    SetTrackbarPosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets trackbar min/max values
 * 
 * Parameters:
 * - name: Trackbar name
 * - window: Window name
 * - min: Minimum value (default: 0)
 * - max: Maximum value
 */
class SetTrackbarMinMaxItem : public InterpreterItem {
public:
    SetTrackbarMinMaxItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Mouse Events
// ============================================================================

/**
 * @brief Registers mouse callback (interactive mode)
 * 
 * Parameters:
 * - window: Window name
 * - callback_var: Variable name to store mouse event data
 * 
 * Stores: [event_type, x, y, flags] in callback_var
 */
class SetMouseCallbackItem : public InterpreterItem {
public:
    SetMouseCallbackItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets last mouse position
 * 
 * Parameters:
 * - window: Window name
 * 
 * Returns: [x, y]
 */
class GetMousePosItem : public InterpreterItem {
public:
    GetMousePosItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Debug & Logging
// ============================================================================

/**
 * @brief Prints message to console
 * 
 * Parameters:
 * - message: Message to print (supports variables with {var_name})
 */
class PrintItem : public InterpreterItem {
public:
    PrintItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Prints debug information about current image
 * 
 * Shows: size, channels, depth, min/max values, mean, std dev
 */
class DebugItem : public InterpreterItem {
public:
    DebugItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Logs message with timestamp and level
 * 
 * Parameters:
 * - message: Log message
 * - level: "debug", "info", "warn", "error" (default: "info")
 */
class LogItem : public InterpreterItem {
public:
    LogItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Asserts a condition and stops on failure
 * 
 * Parameters:
 * - condition: Boolean condition
 * - message: Error message if assertion fails
 */
class AssertItem : public InterpreterItem {
public:
    AssertItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Timing & Performance
// ============================================================================

/**
 * @brief Gets current timestamp in milliseconds
 * 
 * Returns: timestamp as double
 */
class GetTickItem : public InterpreterItem {
public:
    GetTickItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Calculates elapsed time from a previous tick
 * 
 * Parameters:
 * - start_tick: Starting tick value
 * 
 * Returns: elapsed time in milliseconds
 */
class ElapsedTimeItem : public InterpreterItem {
public:
    ElapsedTimeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Starts a named timer
 * 
 * Parameters:
 * - name: Timer name (default: "default")
 */
class StartTimerItem : public InterpreterItem {
public:
    StartTimerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Stops timer and returns elapsed time
 * 
 * Parameters:
 * - name: Timer name (default: "default")
 * - print: Print result to console (default: false)
 * 
 * Returns: elapsed time in milliseconds
 */
class StopTimerItem : public InterpreterItem {
public:
    StopTimerItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Measures FPS (frames per second)
 * 
 * Parameters:
 * - window_size: Number of frames to average (default: 30)
 * 
 * Returns: current FPS estimate
 */
class FPSItem : public InterpreterItem {
public:
    FPSItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

// ============================================================================
// Image Info
// ============================================================================

/**
 * @brief Gets image width
 * 
 * Returns: width in pixels
 */
class GetWidthItem : public InterpreterItem {
public:
    GetWidthItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets image height
 * 
 * Returns: height in pixels
 */
class GetHeightItem : public InterpreterItem {
public:
    GetHeightItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets image size
 * 
 * Returns: [width, height]
 */
class GetSizeItem : public InterpreterItem {
public:
    GetSizeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets number of channels
 * 
 * Returns: number of channels
 */
class GetChannelsItem : public InterpreterItem {
public:
    GetChannelsItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets image depth type
 * 
 * Returns: depth string (e.g., "CV_8U", "CV_32F")
 */
class GetDepthItem : public InterpreterItem {
public:
    GetDepthItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets image type
 * 
 * Returns: type string (e.g., "CV_8UC3", "CV_32FC1")
 */
class GetTypeItem : public InterpreterItem {
public:
    GetTypeItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Gets pixel value at position
 * 
 * Parameters:
 * - x: X coordinate
 * - y: Y coordinate
 * 
 * Returns: pixel value (single value for grayscale, array for multi-channel)
 */
class GetPixelItem : public InterpreterItem {
public:
    GetPixelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

/**
 * @brief Sets pixel value at position
 * 
 * Parameters:
 * - x: X coordinate
 * - y: Y coordinate
 * - value: New pixel value
 */
class SetPixelItem : public InterpreterItem {
public:
    SetPixelItem();
    ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override;
};

} // namespace visionpipe

#endif // VISIONPIPE_DISPLAY_ITEMS_H

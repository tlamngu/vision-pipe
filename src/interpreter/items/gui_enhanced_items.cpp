#include "interpreter/items/gui_enhanced_items.h"
#include "interpreter/cache_manager.h"
#include <opencv2/highgui.hpp>
#include <iostream>
#include <sstream>
#include <map>
#include <memory>
#include <mutex>

namespace visionpipe {

// ============================================================================
// Trackbar-Variable Binding System
// ============================================================================

// Global storage for trackbar-variable bindings
struct TrackbarBinding {
    std::string varName;
    ExecutionContext* ctx;
    int currentValue;
};

static std::mutex g_trackbarMutex;
static std::map<std::string, std::shared_ptr<TrackbarBinding>> g_trackbarBindings;

static std::string makeTrackbarKey(const std::string& name, const std::string& window) {
    return window + "::" + name;
}

// Trackbar callback for variable-bound trackbars
static void trackbarVarCallback(int value, void* userdata) {
    auto* binding = static_cast<TrackbarBinding*>(userdata);
    if (binding && binding->ctx) {
        std::lock_guard<std::mutex> lock(g_trackbarMutex);
        binding->currentValue = value;
        binding->ctx->variables[binding->varName] = RuntimeValue(static_cast<double>(value));
    }
}

void registerGuiEnhancedItems(ItemRegistry& registry) {
    // Trackbar-variable binding
    registry.add<TrackbarVarItem>();
    registry.add<SyncTrackbarVarItem>();
    registry.add<TrackbarValueItem>();
    registry.add<SetTrackbarFromVarItem>();
    
    // Checkbox/toggle
    registry.add<CheckboxItem>();
    registry.add<GetCheckboxItem>();
    
    // Button simulation
    registry.add<ButtonItem>();
    registry.add<CheckButtonClickItem>();
    
    // Multi-slider groups
    registry.add<HsvRangeControlItem>();
    registry.add<GetHsvRangeItem>();
    registry.add<ColorPickerControlItem>();
    registry.add<GetColorFromPickerItem>();
    
    // Control panel
    registry.add<ControlPanelItem>();
    registry.add<ReadControlPanelItem>();
    
    // Display enhancement
    registry.add<ImshowWithVarsItem>();
    registry.add<UpdateWindowTitleItem>();
}

// Helper: Get numeric value from argument (variable or literal)
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

// ============================================================================
// TrackbarVarItem - Creates trackbar bound to a variable
// ============================================================================

TrackbarVarItem::TrackbarVarItem() {
    _functionName = "trackbar_var";
    _description = "Creates a trackbar bound to a variable that auto-updates";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Trackbar name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("var_name", BaseType::STRING, "Variable name to bind"),
        ParamDef::required("min_val", BaseType::INT, "Minimum value"),
        ParamDef::required("max_val", BaseType::INT, "Maximum value"),
        ParamDef::optional("initial", BaseType::INT, "Initial value", 0)
    };
    _example = "trackbar_var(\"Threshold\", \"Control\", \"thresh\", 0, 255, 128)";
    _returnType = "mat";
    _tags = {"gui", "trackbar", "variable", "binding"};
}

ExecutionResult TrackbarVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    std::string varName = args[2].asString();
    int minVal = static_cast<int>(args[3].asNumber());
    int maxVal = static_cast<int>(args[4].asNumber());
    int initial = args.size() > 5 ? static_cast<int>(args[5].asNumber()) : minVal;
    
    // Create or get binding
    std::string key = makeTrackbarKey(name, window);
    {
        std::lock_guard<std::mutex> lock(g_trackbarMutex);
        auto binding = std::make_shared<TrackbarBinding>();
        binding->varName = varName;
        binding->ctx = &ctx;
        binding->currentValue = initial;
        g_trackbarBindings[key] = binding;
        
        // Initialize variable
        ctx.variables[varName] = RuntimeValue(static_cast<double>(initial));
        
        // Create the trackbar with callback
        cv::createTrackbar(name, window, nullptr, maxVal - minVal, trackbarVarCallback, binding.get());
        cv::setTrackbarPos(name, window, initial - minVal);
        cv::setTrackbarMin(name, window, minVal);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SyncTrackbarVarItem - Syncs existing trackbar with variable
// ============================================================================

SyncTrackbarVarItem::SyncTrackbarVarItem() {
    _functionName = "sync_trackbar_var";
    _description = "Syncs an existing trackbar with a variable bidirectionally";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Trackbar name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("var_name", BaseType::STRING, "Variable name to sync")
    };
    _example = "sync_trackbar_var(\"Threshold\", \"Control\", \"thresh\")";
    _returnType = "mat";
    _tags = {"gui", "trackbar", "variable", "sync"};
}

ExecutionResult SyncTrackbarVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    std::string varName = args[2].asString();
    
    // Get current trackbar position
    int pos = cv::getTrackbarPos(name, window);
    
    // Check if variable exists and differs
    if (ctx.variables.find(varName) != ctx.variables.end()) {
        int varVal = static_cast<int>(ctx.variables[varName].asNumber());
        if (varVal != pos) {
            // Variable was changed externally, update trackbar
            cv::setTrackbarPos(name, window, varVal);
            pos = varVal;
        }
    }
    
    // Update variable with current trackbar value
    ctx.variables[varName] = RuntimeValue(static_cast<double>(pos));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// TrackbarValueItem - Gets trackbar value into variable
// ============================================================================

TrackbarValueItem::TrackbarValueItem() {
    _functionName = "trackbar_value";
    _description = "Gets trackbar value and stores in a variable";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Trackbar name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::optional("var_name", BaseType::STRING, "Return value into this variable", "")
    };
    _example = "trackbar_value(\"Threshold\", \"Control\", \"thresh\")";
    _returnType = "mat";
    _tags = {"gui", "trackbar", "value"};
}

ExecutionResult TrackbarValueItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    
    int pos = cv::getTrackbarPos(name, window);
    RuntimeValue val(static_cast<double>(pos));
    
    if (args.size() > 2 && !args[2].asString().empty()) {
        ctx.variables[args[2].asString()] = val;
    }
    
    return ExecutionResult::okWithMat(ctx.currentMat, val);
}

// ============================================================================
// SetTrackbarFromVarItem - Sets trackbar from variable
// ============================================================================

SetTrackbarFromVarItem::SetTrackbarFromVarItem() {
    _functionName = "set_trackbar_from_var";
    _description = "Sets trackbar position from a variable value";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Trackbar name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("var_name", BaseType::STRING, "Variable name containing value")
    };
    _example = "set_trackbar_from_var(\"Threshold\", \"Control\", \"thresh\")";
    _returnType = "mat";
    _tags = {"gui", "trackbar", "variable"};
}

ExecutionResult SetTrackbarFromVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    std::string varName = args[2].asString();
    
    if (ctx.variables.find(varName) == ctx.variables.end()) {
        return ExecutionResult::fail("Variable not found: " + varName);
    }
    
    int value = static_cast<int>(ctx.variables[varName].asNumber());
    cv::setTrackbarPos(name, window, value);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CheckboxItem - Creates a checkbox (0-1 trackbar) bound to boolean
// ============================================================================

CheckboxItem::CheckboxItem() {
    _functionName = "checkbox";
    _description = "Creates a checkbox using a trackbar bound to a boolean variable";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Checkbox name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("var_name", BaseType::STRING, "Boolean variable name"),
        ParamDef::optional("initial", BaseType::BOOL, "Initial state", false)
    };
    _example = "checkbox(\"Enable Filter\", \"Control\", \"filter_enabled\", true)";
    _returnType = "mat";
    _tags = {"gui", "checkbox", "boolean", "toggle"};
}

ExecutionResult CheckboxItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    std::string varName = args[2].asString();
    bool initial = args.size() > 3 ? args[3].asBool() : false;
    
    // Create binding
    std::string key = makeTrackbarKey(name, window);
    {
        std::lock_guard<std::mutex> lock(g_trackbarMutex);
        auto binding = std::make_shared<TrackbarBinding>();
        binding->varName = varName;
        binding->ctx = &ctx;
        binding->currentValue = initial ? 1 : 0;
        g_trackbarBindings[key] = binding;
        
        // Initialize variable as boolean
        ctx.variables[varName] = RuntimeValue(initial);
        
        // Create 0-1 trackbar
        cv::createTrackbar(name, window, nullptr, 1, 
            [](int value, void* userdata) {
                auto* binding = static_cast<TrackbarBinding*>(userdata);
                if (binding && binding->ctx) {
                    binding->currentValue = value;
                    binding->ctx->variables[binding->varName] = RuntimeValue(value != 0);
                }
            }, binding.get());
        cv::setTrackbarPos(name, window, initial ? 1 : 0);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetCheckboxItem - Gets checkbox state as boolean
// ============================================================================

GetCheckboxItem::GetCheckboxItem() {
    _functionName = "get_checkbox";
    _description = "Gets checkbox state as boolean into a variable";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Checkbox name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::optional("var_name", BaseType::STRING, "Return value into this variable", "")
    };
    _example = "get_checkbox(\"Enable Filter\", \"Control\", \"is_enabled\")";
    _returnType = "mat";
    _tags = {"gui", "checkbox", "boolean"};
}

ExecutionResult GetCheckboxItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    
    int pos = cv::getTrackbarPos(name, window);
    RuntimeValue val(pos != 0);
    
    if (args.size() > 2 && !args[2].asString().empty()) {
        ctx.variables[args[2].asString()] = val;
    }
    
    return ExecutionResult::okWithMat(ctx.currentMat, val);
}

// ============================================================================
// ButtonItem - Creates a button-like trackbar
// ============================================================================

ButtonItem::ButtonItem() {
    _functionName = "button";
    _description = "Creates a button-like trackbar that triggers when clicked";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Button name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("clicked_var", BaseType::STRING, "Variable set when clicked")
    };
    _example = "button(\"Apply\", \"Control\", \"apply_clicked\")";
    _returnType = "mat";
    _tags = {"gui", "button", "trigger"};
}

ExecutionResult ButtonItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    std::string varName = args[2].asString();
    
    // Initialize variable to false
    ctx.variables[varName] = RuntimeValue(false);
    
    std::string key = makeTrackbarKey(name, window);
    {
        std::lock_guard<std::mutex> lock(g_trackbarMutex);
        auto binding = std::make_shared<TrackbarBinding>();
        binding->varName = varName;
        binding->ctx = &ctx;
        binding->currentValue = 0;
        g_trackbarBindings[key] = binding;
        
        // Create 0-1 trackbar that acts as button
        cv::createTrackbar(name, window, nullptr, 1,
            [](int value, void* userdata) {
                auto* binding = static_cast<TrackbarBinding*>(userdata);
                if (binding && binding->ctx && value == 1) {
                    binding->ctx->variables[binding->varName] = RuntimeValue(true);
                }
            }, binding.get());
        cv::setTrackbarPos(name, window, 0);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CheckButtonClickItem - Checks and resets button click
// ============================================================================

CheckButtonClickItem::CheckButtonClickItem() {
    _functionName = "check_button_click";
    _description = "Checks if button was clicked and resets the state";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Button name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::optional("var_name", BaseType::STRING, "Return value into this variable", "")
    };
    _example = "check_button_click(\"Apply\", \"Control\", \"was_clicked\")";
    _returnType = "mat";
    _tags = {"gui", "button", "check"};
}

ExecutionResult CheckButtonClickItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    
    int pos = cv::getTrackbarPos(name, window);
    RuntimeValue val(pos != 0);
    
    if (args.size() > 2 && !args[2].asString().empty()) {
        ctx.variables[args[2].asString()] = val;
    }
    
    // Reset button
    if (pos != 0) {
        cv::setTrackbarPos(name, window, 0);
    }
    
    return ExecutionResult::okWithMat(ctx.currentMat, val);
}

// ============================================================================
// HsvRangeControlItem - Creates HSV range sliders
// ============================================================================

HsvRangeControlItem::HsvRangeControlItem() {
    _functionName = "hsv_range_control";
    _description = "Creates 6 trackbars for HSV min/max range selection";
    _category = "gui";
    _params = {
        ParamDef::required("prefix", BaseType::STRING, "Variable prefix for h_min, h_max, etc."),
        ParamDef::required("window", BaseType::STRING, "Window name")
    };
    _example = "hsv_range_control(\"hsv\", \"HSV Control\")";
    _returnType = "mat";
    _tags = {"gui", "hsv", "color", "range"};
}

ExecutionResult HsvRangeControlItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args[0].asString();
    std::string window = args[1].asString();
    
    // Create trackbars for H, S, V min and max
    cv::createTrackbar("H Min", window, nullptr, 179);
    cv::createTrackbar("H Max", window, nullptr, 179);
    cv::createTrackbar("S Min", window, nullptr, 255);
    cv::createTrackbar("S Max", window, nullptr, 255);
    cv::createTrackbar("V Min", window, nullptr, 255);
    cv::createTrackbar("V Max", window, nullptr, 255);
    
    // Set defaults
    cv::setTrackbarPos("H Min", window, 0);
    cv::setTrackbarPos("H Max", window, 179);
    cv::setTrackbarPos("S Min", window, 0);
    cv::setTrackbarPos("S Max", window, 255);
    cv::setTrackbarPos("V Min", window, 0);
    cv::setTrackbarPos("V Max", window, 255);
    
    // Initialize variables
    ctx.variables[prefix + "_h_min"] = RuntimeValue(0.0);
    ctx.variables[prefix + "_h_max"] = RuntimeValue(179.0);
    ctx.variables[prefix + "_s_min"] = RuntimeValue(0.0);
    ctx.variables[prefix + "_s_max"] = RuntimeValue(255.0);
    ctx.variables[prefix + "_v_min"] = RuntimeValue(0.0);
    ctx.variables[prefix + "_v_max"] = RuntimeValue(255.0);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetHsvRangeItem - Gets HSV range values
// ============================================================================

GetHsvRangeItem::GetHsvRangeItem() {
    _functionName = "get_hsv_range";
    _description = "Gets HSV range values from control into variables";
    _category = "gui";
    _params = {
        ParamDef::required("prefix", BaseType::STRING, "Variable prefix"),
        ParamDef::required("window", BaseType::STRING, "Window name")
    };
    _example = "get_hsv_range(\"hsv\", \"HSV Control\")";
    _returnType = "mat";
    _tags = {"gui", "hsv", "color", "range"};
}

ExecutionResult GetHsvRangeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args[0].asString();
    std::string window = args[1].asString();
    
    ctx.variables[prefix + "_h_min"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("H Min", window)));
    ctx.variables[prefix + "_h_max"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("H Max", window)));
    ctx.variables[prefix + "_s_min"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("S Min", window)));
    ctx.variables[prefix + "_s_max"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("S Max", window)));
    ctx.variables[prefix + "_v_min"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("V Min", window)));
    ctx.variables[prefix + "_v_max"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("V Max", window)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ColorPickerControlItem - Creates BGR color picker
// ============================================================================

ColorPickerControlItem::ColorPickerControlItem() {
    _functionName = "color_picker_control";
    _description = "Creates 3 trackbars for BGR color selection";
    _category = "gui";
    _params = {
        ParamDef::required("prefix", BaseType::STRING, "Variable prefix for _b, _g, _r"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::optional("initial_b", BaseType::INT, "Initial blue value", 0),
        ParamDef::optional("initial_g", BaseType::INT, "Initial green value", 0),
        ParamDef::optional("initial_r", BaseType::INT, "Initial red value", 0)
    };
    _example = "color_picker_control(\"color\", \"Color Picker\", 255, 0, 0)";
    _returnType = "mat";
    _tags = {"gui", "color", "picker"};
}

ExecutionResult ColorPickerControlItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args[0].asString();
    std::string window = args[1].asString();
    int initB = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 0;
    int initG = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;
    int initR = args.size() > 4 ? static_cast<int>(args[4].asNumber()) : 0;
    
    cv::createTrackbar("Blue", window, nullptr, 255);
    cv::createTrackbar("Green", window, nullptr, 255);
    cv::createTrackbar("Red", window, nullptr, 255);
    
    cv::setTrackbarPos("Blue", window, initB);
    cv::setTrackbarPos("Green", window, initG);
    cv::setTrackbarPos("Red", window, initR);
    
    ctx.variables[prefix + "_b"] = RuntimeValue(static_cast<double>(initB));
    ctx.variables[prefix + "_g"] = RuntimeValue(static_cast<double>(initG));
    ctx.variables[prefix + "_r"] = RuntimeValue(static_cast<double>(initR));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetColorFromPickerItem - Gets color as Scalar
// ============================================================================

GetColorFromPickerItem::GetColorFromPickerItem() {
    _functionName = "get_color_from_picker";
    _description = "Gets BGR values from color picker into individual variables";
    _category = "gui";
    _params = {
        ParamDef::required("prefix", BaseType::STRING, "Variable prefix"),
        ParamDef::required("window", BaseType::STRING, "Window name")
    };
    _example = "get_color_from_picker(\"color\", \"Color Picker\")";
    _returnType = "mat";
    _tags = {"gui", "color", "picker"};
}

ExecutionResult GetColorFromPickerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args[0].asString();
    std::string window = args[1].asString();
    
    ctx.variables[prefix + "_b"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Blue", window)));
    ctx.variables[prefix + "_g"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Green", window)));
    ctx.variables[prefix + "_r"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Red", window)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ControlPanelItem - Creates preset control panels
// ============================================================================

ControlPanelItem::ControlPanelItem() {
    _functionName = "control_panel";
    _description = "Creates a control panel window with common image processing controls";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Control panel window name"),
        ParamDef::required("preset", BaseType::STRING, "Preset: threshold, blur, edge, morphology, color")
    };
    _example = "control_panel(\"Controls\", \"threshold\")";
    _returnType = "mat";
    _tags = {"gui", "control", "panel", "preset"};
}

ExecutionResult ControlPanelItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string preset = args[1].asString();
    
    cv::namedWindow(name, cv::WINDOW_AUTOSIZE);
    
    if (preset == "threshold") {
        cv::createTrackbar("Threshold", name, nullptr, 255);
        cv::createTrackbar("Max Value", name, nullptr, 255);
        cv::createTrackbar("Type", name, nullptr, 4);  // Binary, Binary Inv, Trunc, ToZero, ToZero Inv
        cv::setTrackbarPos("Threshold", name, 128);
        cv::setTrackbarPos("Max Value", name, 255);
        cv::setTrackbarPos("Type", name, 0);
    } else if (preset == "blur") {
        cv::createTrackbar("Kernel Size", name, nullptr, 31);
        cv::createTrackbar("Sigma", name, nullptr, 100);
        cv::setTrackbarPos("Kernel Size", name, 5);
        cv::setTrackbarPos("Sigma", name, 10);
    } else if (preset == "edge") {
        cv::createTrackbar("Low Threshold", name, nullptr, 255);
        cv::createTrackbar("High Threshold", name, nullptr, 255);
        cv::createTrackbar("Aperture", name, nullptr, 7);
        cv::setTrackbarPos("Low Threshold", name, 50);
        cv::setTrackbarPos("High Threshold", name, 150);
        cv::setTrackbarPos("Aperture", name, 3);
    } else if (preset == "morphology") {
        cv::createTrackbar("Kernel Size", name, nullptr, 21);
        cv::createTrackbar("Operation", name, nullptr, 6);  // Erode, Dilate, Open, Close, Gradient, TopHat, BlackHat
        cv::createTrackbar("Iterations", name, nullptr, 10);
        cv::setTrackbarPos("Kernel Size", name, 3);
        cv::setTrackbarPos("Operation", name, 0);
        cv::setTrackbarPos("Iterations", name, 1);
    } else if (preset == "color") {
        cv::createTrackbar("H Min", name, nullptr, 179);
        cv::createTrackbar("H Max", name, nullptr, 179);
        cv::createTrackbar("S Min", name, nullptr, 255);
        cv::createTrackbar("S Max", name, nullptr, 255);
        cv::createTrackbar("V Min", name, nullptr, 255);
        cv::createTrackbar("V Max", name, nullptr, 255);
        cv::setTrackbarPos("H Min", name, 0);
        cv::setTrackbarPos("H Max", name, 179);
        cv::setTrackbarPos("S Min", name, 50);
        cv::setTrackbarPos("S Max", name, 255);
        cv::setTrackbarPos("V Min", name, 50);
        cv::setTrackbarPos("V Max", name, 255);
    } else {
        return ExecutionResult::fail("Unknown preset: " + preset + ". Use: threshold, blur, edge, morphology, color");
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ReadControlPanelItem - Reads control panel values
// ============================================================================

ReadControlPanelItem::ReadControlPanelItem() {
    _functionName = "read_control_panel";
    _description = "Reads all control panel values into variables with prefix";
    _category = "gui";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Control panel window name"),
        ParamDef::required("prefix", BaseType::STRING, "Variable prefix"),
        ParamDef::required("preset", BaseType::STRING, "Preset type used when creating")
    };
    _example = "read_control_panel(\"Controls\", \"ctrl\", \"threshold\")";
    _returnType = "mat";
    _tags = {"gui", "control", "panel", "read"};
}

ExecutionResult ReadControlPanelItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string prefix = args[1].asString();
    std::string preset = args[2].asString();
    
    if (preset == "threshold") {
        ctx.variables[prefix + "_threshold"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Threshold", name)));
        ctx.variables[prefix + "_max_value"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Max Value", name)));
        ctx.variables[prefix + "_type"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Type", name)));
    } else if (preset == "blur") {
        int ksize = cv::getTrackbarPos("Kernel Size", name);
        if (ksize % 2 == 0) ksize++;  // Ensure odd
        if (ksize < 1) ksize = 1;
        ctx.variables[prefix + "_ksize"] = RuntimeValue(static_cast<double>(ksize));
        ctx.variables[prefix + "_sigma"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Sigma", name)) / 10.0);
    } else if (preset == "edge") {
        ctx.variables[prefix + "_low"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Low Threshold", name)));
        ctx.variables[prefix + "_high"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("High Threshold", name)));
        int aperture = cv::getTrackbarPos("Aperture", name);
        if (aperture < 3) aperture = 3;
        if (aperture % 2 == 0) aperture++;
        if (aperture > 7) aperture = 7;
        ctx.variables[prefix + "_aperture"] = RuntimeValue(static_cast<double>(aperture));
    } else if (preset == "morphology") {
        int ksize = cv::getTrackbarPos("Kernel Size", name);
        if (ksize < 1) ksize = 1;
        if (ksize % 2 == 0) ksize++;
        ctx.variables[prefix + "_ksize"] = RuntimeValue(static_cast<double>(ksize));
        ctx.variables[prefix + "_operation"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("Operation", name)));
        int iters = cv::getTrackbarPos("Iterations", name);
        if (iters < 1) iters = 1;
        ctx.variables[prefix + "_iterations"] = RuntimeValue(static_cast<double>(iters));
    } else if (preset == "color") {
        ctx.variables[prefix + "_h_min"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("H Min", name)));
        ctx.variables[prefix + "_h_max"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("H Max", name)));
        ctx.variables[prefix + "_s_min"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("S Min", name)));
        ctx.variables[prefix + "_s_max"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("S Max", name)));
        ctx.variables[prefix + "_v_min"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("V Min", name)));
        ctx.variables[prefix + "_v_max"] = RuntimeValue(static_cast<double>(cv::getTrackbarPos("V Max", name)));
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ImshowWithVarsItem - Displays image with variable overlay
// ============================================================================

ImshowWithVarsItem::ImshowWithVarsItem() {
    _functionName = "imshow_with_vars";
    _description = "Displays image with overlay showing variable values";
    _category = "gui";
    _params = {
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("var_list", BaseType::STRING, "Comma-separated list of variable names")
    };
    _example = "imshow_with_vars(\"Debug\", \"threshold,blur_size,enabled\")";
    _returnType = "mat";
    _tags = {"gui", "display", "debug", "overlay"};
}

ExecutionResult ImshowWithVarsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string window = args[0].asString();
    std::string varList = args[1].asString();
    
    if (ctx.currentMat.empty()) {
        return ExecutionResult::fail("No image to display");
    }
    
    // Clone image for overlay
    cv::Mat display = ctx.currentMat.clone();
    if (display.channels() == 1) {
        cv::cvtColor(display, display, cv::COLOR_GRAY2BGR);
    }
    
    // Parse variable list
    std::vector<std::string> vars;
    std::stringstream ss(varList);
    std::string var;
    while (std::getline(ss, var, ',')) {
        // Trim whitespace
        var.erase(0, var.find_first_not_of(" \t"));
        var.erase(var.find_last_not_of(" \t") + 1);
        if (!var.empty()) {
            vars.push_back(var);
        }
    }
    
    // Draw variable values
    int y = 20;
    for (const auto& varName : vars) {
        std::string text = varName + ": ";
        if (ctx.variables.find(varName) != ctx.variables.end()) {
            const RuntimeValue& val = ctx.variables[varName];
            if (val.isBool()) {
                text += val.asBool() ? "true" : "false";
            } else if (val.isNumeric()) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << val.asNumber();
                text += oss.str();
            } else if (val.isString()) {
                text += val.asString();
            } else {
                text += "<unknown>";
            }
        } else {
            text += "<undefined>";
        }
        
        // Draw with shadow for visibility
        cv::putText(display, text, cv::Point(11, y + 1), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 2);
        cv::putText(display, text, cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        y += 20;
    }
    
    cv::imshow(window, display);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// UpdateWindowTitleItem - Updates window title with variable
// ============================================================================

UpdateWindowTitleItem::UpdateWindowTitleItem() {
    _functionName = "update_window_title";
    _description = "Updates window title with variable values using {var} placeholders";
    _category = "gui";
    _params = {
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("format", BaseType::STRING, "Title format with {var} placeholders")
    };
    _example = "update_window_title(\"Output\", \"Threshold: {threshold} - FPS: {fps}\")";
    _returnType = "mat";
    _tags = {"gui", "window", "title"};
}

ExecutionResult UpdateWindowTitleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string window = args[0].asString();
    std::string format = args[1].asString();
    
    // Replace {var} placeholders
    std::string result = format;
    size_t pos = 0;
    while ((pos = result.find('{', pos)) != std::string::npos) {
        size_t end = result.find('}', pos);
        if (end == std::string::npos) break;
        
        std::string varName = result.substr(pos + 1, end - pos - 1);
        std::string replacement = "<undefined>";
        
        if (ctx.variables.find(varName) != ctx.variables.end()) {
            const RuntimeValue& val = ctx.variables[varName];
            if (val.isBool()) {
                replacement = val.asBool() ? "true" : "false";
            } else if (val.isNumeric()) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << val.asNumber();
                replacement = oss.str();
            } else if (val.isString()) {
                replacement = val.asString();
            }
        }
        
        result.replace(pos, end - pos + 1, replacement);
        pos += replacement.length();
    }
    
    cv::setWindowTitle(window, result);
    
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

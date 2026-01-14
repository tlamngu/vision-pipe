#include "interpreter/items/display_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <chrono>

namespace visionpipe {

void registerDisplayItems(ItemRegistry& registry) {
    registry.add<NamedWindowItem>();
    registry.add<DestroyWindowItem>();
    registry.add<DestroyAllWindowsItem>();
    registry.add<MoveWindowItem>();
    registry.add<ResizeWindowItem>();
    registry.add<SetWindowTitleItem>();
    registry.add<ImShowItem>();
    registry.add<WaitKeyItem>();
    registry.add<PollKeyItem>();
    registry.add<CreateTrackbarItem>();
    registry.add<GetTrackbarPosItem>();
    registry.add<SetTrackbarPosItem>();
    registry.add<PrintItem>();
    registry.add<DebugItem>();
    registry.add<LogItem>();
    registry.add<AssertItem>();
    registry.add<GetTickItem>();
    registry.add<ElapsedTimeItem>();
    registry.add<StartTimerItem>();
    registry.add<StopTimerItem>();
    registry.add<FPSItem>();
    registry.add<GetWidthItem>();
    registry.add<GetHeightItem>();
    registry.add<GetSizeItem>();
    registry.add<GetChannelsItem>();
    registry.add<GetDepthItem>();
    registry.add<GetTypeItem>();
}

// ============================================================================
// NamedWindowItem
// ============================================================================

NamedWindowItem::NamedWindowItem() {
    _functionName = "named_window";
    _description = "Creates a named window";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Window name"),
        ParamDef::optional("flags", BaseType::STRING, "Flags: normal, autosize, fullscreen, freeratio, keepratio", "autosize")
    };
    _example = "named_window(\"Output\", \"normal\")";
    _returnType = "mat";
    _tags = {"window", "display", "gui"};
}

ExecutionResult NamedWindowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string flags = args.size() > 1 ? args[1].asString() : "autosize";
    
    int windowFlags = cv::WINDOW_AUTOSIZE;
    if (flags == "normal") windowFlags = cv::WINDOW_NORMAL;
    else if (flags == "fullscreen") windowFlags = cv::WINDOW_FULLSCREEN;
    else if (flags == "freeratio") windowFlags = cv::WINDOW_FREERATIO;
    else if (flags == "keepratio") windowFlags = cv::WINDOW_KEEPRATIO;
    
    cv::namedWindow(name, windowFlags);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DestroyWindowItem
// ============================================================================

DestroyWindowItem::DestroyWindowItem() {
    _functionName = "destroy_window";
    _description = "Destroys specified window";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Window name")
    };
    _example = "destroy_window(\"Output\")";
    _returnType = "mat";
    _tags = {"window", "destroy", "gui"};
}

ExecutionResult DestroyWindowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    cv::destroyWindow(name);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DestroyAllWindowsItem
// ============================================================================

DestroyAllWindowsItem::DestroyAllWindowsItem() {
    _functionName = "destroy_all_windows";
    _description = "Destroys all HighGUI windows";
    _category = "display";
    _params = {};
    _example = "destroy_all_windows()";
    _returnType = "mat";
    _tags = {"window", "destroy", "all", "gui"};
}

ExecutionResult DestroyAllWindowsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::destroyAllWindows();
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// MoveWindowItem
// ============================================================================

MoveWindowItem::MoveWindowItem() {
    _functionName = "move_window";
    _description = "Moves window to specified position";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Window name"),
        ParamDef::required("x", BaseType::INT, "X position"),
        ParamDef::required("y", BaseType::INT, "Y position")
    };
    _example = "move_window(\"Output\", 100, 100)";
    _returnType = "mat";
    _tags = {"window", "move", "position", "gui"};
}

ExecutionResult MoveWindowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    int x = static_cast<int>(args[1].asNumber());
    int y = static_cast<int>(args[2].asNumber());
    cv::moveWindow(name, x, y);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ResizeWindowItem
// ============================================================================

ResizeWindowItem::ResizeWindowItem() {
    _functionName = "resize_window";
    _description = "Resizes window to specified size";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Window name"),
        ParamDef::required("width", BaseType::INT, "Width"),
        ParamDef::required("height", BaseType::INT, "Height")
    };
    _example = "resize_window(\"Output\", 640, 480)";
    _returnType = "mat";
    _tags = {"window", "resize", "size", "gui"};
}

ExecutionResult ResizeWindowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    int width = static_cast<int>(args[1].asNumber());
    int height = static_cast<int>(args[2].asNumber());
    cv::resizeWindow(name, width, height);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetWindowTitleItem
// ============================================================================

SetWindowTitleItem::SetWindowTitleItem() {
    _functionName = "set_window_title";
    _description = "Sets window title";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Window name"),
        ParamDef::required("title", BaseType::STRING, "New title")
    };
    _example = "set_window_title(\"Output\", \"Processed Image\")";
    _returnType = "mat";
    _tags = {"window", "title", "gui"};
}

ExecutionResult SetWindowTitleItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string title = args[1].asString();
    cv::setWindowTitle(name, title);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ImShowItem
// ============================================================================

ImShowItem::ImShowItem() {
    _functionName = "imshow";
    _description = "Displays image in a window";
    _category = "display";
    _params = {
        ParamDef::optional("name", BaseType::STRING, "Window name", "Output")
    };
    _example = "imshow(\"Result\")";
    _returnType = "mat";
    _tags = {"display", "show", "window"};
}

ExecutionResult ImShowItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args.size() > 0 ? args[0].asString() : "Output";
    cv::imshow(name, ctx.currentMat);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// WaitKeyItem
// ============================================================================

WaitKeyItem::WaitKeyItem() {
    _functionName = "wait_key";
    _description = "Waits for a key press and returns the key code";
    _category = "display";
    _params = {
        ParamDef::optional("delay", BaseType::INT, "Delay in ms (0=infinite)", 0)
    };
    _example = "key = wait_key(30)";
    _returnType = "int";
    _tags = {"input", "key", "wait"};
}

ExecutionResult WaitKeyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int delay = args.size() > 0 ? static_cast<int>(args[0].asNumber()) : 0;
    int key = cv::waitKey(delay);
    // Store in cache for backward compatibility
    ctx.cacheManager->set("last_key", cv::Mat(1, 1, CV_32S, cv::Scalar(key)));
    // Return both the current mat and the key value
    return ExecutionResult::okWithMat(ctx.currentMat, RuntimeValue(static_cast<int64_t>(key)));
}

// ============================================================================
// PollKeyItem
// ============================================================================

PollKeyItem::PollKeyItem() {
    _functionName = "poll_key";
    _description = "Polls for key without blocking and returns the key code";
    _category = "display";
    _params = {};
    _example = "key = poll_key()";
    _returnType = "int";
    _tags = {"input", "key", "poll"};
}

ExecutionResult PollKeyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    int key = cv::pollKey();
    ctx.cacheManager->set("last_key", cv::Mat(1, 1, CV_32S, cv::Scalar(key)));
    return ExecutionResult::okWithMat(ctx.currentMat, RuntimeValue(static_cast<int64_t>(key)));
}

// ============================================================================
// CreateTrackbarItem
// ============================================================================

CreateTrackbarItem::CreateTrackbarItem() {
    _functionName = "create_trackbar";
    _description = "Creates a trackbar and attaches it to window";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Trackbar name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("max_value", BaseType::INT, "Maximum value"),
        ParamDef::optional("initial", BaseType::INT, "Initial value", 0)
    };
    _example = "create_trackbar(\"Threshold\", \"Output\", 255, 128)";
    _returnType = "mat";
    _tags = {"trackbar", "slider", "gui"};
}

ExecutionResult CreateTrackbarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    int maxVal = static_cast<int>(args[2].asNumber());
    int initial = args.size() > 3 ? static_cast<int>(args[3].asNumber()) : 0;
    
    cv::createTrackbar(name, window, nullptr, maxVal);
    cv::setTrackbarPos(name, window, initial);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetTrackbarPosItem
// ============================================================================

GetTrackbarPosItem::GetTrackbarPosItem() {
    _functionName = "get_trackbar_pos";
    _description = "Gets current trackbar position";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Trackbar name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for value", "trackbar_val")
    };
    _example = "get_trackbar_pos(\"Threshold\", \"Output\", \"thresh\")";
    _returnType = "mat";
    _tags = {"trackbar", "value", "get"};
}

ExecutionResult GetTrackbarPosItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    std::string cacheId = args.size() > 2 ? args[2].asString() : "trackbar_val";
    
    int pos = cv::getTrackbarPos(name, window);
    ctx.cacheManager->set(cacheId, cv::Mat(1, 1, CV_32S, cv::Scalar(pos)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetTrackbarPosItem
// ============================================================================

SetTrackbarPosItem::SetTrackbarPosItem() {
    _functionName = "set_trackbar_pos";
    _description = "Sets trackbar position";
    _category = "display";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Trackbar name"),
        ParamDef::required("window", BaseType::STRING, "Window name"),
        ParamDef::required("pos", BaseType::INT, "Position value")
    };
    _example = "set_trackbar_pos(\"Threshold\", \"Output\", 128)";
    _returnType = "mat";
    _tags = {"trackbar", "value", "set"};
}

ExecutionResult SetTrackbarPosItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    std::string window = args[1].asString();
    int pos = static_cast<int>(args[2].asNumber());
    
    cv::setTrackbarPos(name, window, pos);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// PrintItem
// ============================================================================

PrintItem::PrintItem() {
    _functionName = "print";
    _description = "Prints message to console";
    _category = "display";
    _params = {
        ParamDef::required("message", BaseType::STRING, "Message to print")
    };
    _example = "print(\"Processing complete\")";
    _returnType = "mat";
    _tags = {"print", "console", "output"};
}

ExecutionResult PrintItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string message = args[0].asString();
    std::cout << message << std::endl;
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DebugItem
// ============================================================================

DebugItem::DebugItem() {
    _functionName = "debug";
    _description = "Prints debug information about current Mat";
    _category = "display";
    _params = {
        ParamDef::optional("label", BaseType::STRING, "Debug label", "Mat")
    };
    _example = "debug(\"After blur\")";
    _returnType = "mat";
    _tags = {"debug", "info", "inspect"};
}

ExecutionResult DebugItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string label = args.size() > 0 ? args[0].asString() : "Mat";
    
    std::cout << "[DEBUG] " << label << ": "
              << ctx.currentMat.cols << "x" << ctx.currentMat.rows
              << " ch=" << ctx.currentMat.channels()
              << " depth=" << ctx.currentMat.depth()
              << " type=" << ctx.currentMat.type()
              << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LogItem
// ============================================================================

LogItem::LogItem() {
    _functionName = "log";
    _description = "Logs message with level";
    _category = "display";
    _params = {
        ParamDef::required("level", BaseType::STRING, "Level: info, warn, error, debug"),
        ParamDef::required("message", BaseType::STRING, "Log message")
    };
    _example = "log(\"info\", \"Pipeline started\")";
    _returnType = "mat";
    _tags = {"log", "console", "debug"};
}

ExecutionResult LogItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string level = args[0].asString();
    std::string message = args[1].asString();
    
    std::string prefix = "[INFO]";
    if (level == "warn") prefix = "[WARN]";
    else if (level == "error") prefix = "[ERROR]";
    else if (level == "debug") prefix = "[DEBUG]";
    
    std::cout << prefix << " " << message << std::endl;
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// AssertItem
// ============================================================================

AssertItem::AssertItem() {
    _functionName = "assert";
    _description = "Asserts condition and stops if false";
    _category = "display";
    _params = {
        ParamDef::required("condition", BaseType::BOOL, "Condition to check"),
        ParamDef::optional("message", BaseType::STRING, "Error message", "Assertion failed")
    };
    _example = "assert(true, \"Mat should not be empty\")";
    _returnType = "mat";
    _tags = {"assert", "check", "validation"};
}

ExecutionResult AssertItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool condition = args[0].asBool();
    std::string message = args.size() > 1 ? args[1].asString() : "Assertion failed";
    
    if (!condition) {
        return ExecutionResult::fail(message);
    }
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetTickItem
// ============================================================================

GetTickItem::GetTickItem() {
    _functionName = "get_tick";
    _description = "Gets current tick count for timing";
    _category = "display";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for tick", "tick")
    };
    _example = "get_tick(\"start_tick\")";
    _returnType = "mat";
    _tags = {"tick", "timing", "performance"};
}

ExecutionResult GetTickItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "tick";
    
    int64 tick = cv::getTickCount();
    ctx.cacheManager->set(cacheId, cv::Mat(1, 1, CV_64F, cv::Scalar(static_cast<double>(tick))));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ElapsedTimeItem
// ============================================================================

ElapsedTimeItem::ElapsedTimeItem() {
    _functionName = "elapsed_time";
    _description = "Calculates elapsed time from cached tick";
    _category = "display";
    _params = {
        ParamDef::required("tick_cache", BaseType::STRING, "Cache ID of start tick"),
        ParamDef::optional("unit", BaseType::STRING, "Unit: ms, s, us", "ms"),
        ParamDef::optional("print", BaseType::BOOL, "Print result", true)
    };
    _example = "elapsed_time(\"start_tick\", \"ms\", true)";
    _returnType = "mat";
    _tags = {"elapsed", "timing", "performance"};
}

ExecutionResult ElapsedTimeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string tickCache = args[0].asString();
    std::string unit = args.size() > 1 ? args[1].asString() : "ms";
    bool print = args.size() > 2 ? args[2].asBool() : true;
    
    cv::Mat tickOpt = ctx.cacheManager->get(tickCache);
    if (tickOpt.empty()) {
        return ExecutionResult::fail("Tick not found in cache: " + tickCache);
    }
    
    double startTick = tickOpt.at<double>(0, 0);
    double currentTick = static_cast<double>(cv::getTickCount());
    double elapsed = (currentTick - startTick) / cv::getTickFrequency();
    
    if (unit == "ms") elapsed *= 1000;
    else if (unit == "us") elapsed *= 1000000;
    
    if (print) {
        std::cout << "Elapsed: " << elapsed << " " << unit << std::endl;
    }
    
    ctx.cacheManager->set("elapsed_" + tickCache, cv::Mat(1, 1, CV_64F, cv::Scalar(elapsed)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// StartTimerItem
// ============================================================================

StartTimerItem::StartTimerItem() {
    _functionName = "start_timer";
    _description = "Starts a named timer";
    _category = "display";
    _params = {
        ParamDef::optional("name", BaseType::STRING, "Timer name", "default")
    };
    _example = "start_timer(\"processing\")";
    _returnType = "mat";
    _tags = {"timer", "start", "performance"};
}

ExecutionResult StartTimerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args.size() > 0 ? args[0].asString() : "default";
    
    auto now = std::chrono::high_resolution_clock::now();
    auto ticks = now.time_since_epoch().count();
    ctx.cacheManager->set("timer_" + name, cv::Mat(1, 1, CV_64F, cv::Scalar(static_cast<double>(ticks))));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// StopTimerItem
// ============================================================================

StopTimerItem::StopTimerItem() {
    _functionName = "stop_timer";
    _description = "Stops timer and reports elapsed time";
    _category = "display";
    _params = {
        ParamDef::optional("name", BaseType::STRING, "Timer name", "default"),
        ParamDef::optional("print", BaseType::BOOL, "Print result", true)
    };
    _example = "stop_timer(\"processing\", true)";
    _returnType = "mat";
    _tags = {"timer", "stop", "performance"};
}

ExecutionResult StopTimerItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args.size() > 0 ? args[0].asString() : "default";
    bool print = args.size() > 1 ? args[1].asBool() : true;
    
    cv::Mat timerOpt = ctx.cacheManager->get("timer_" + name);
    if (timerOpt.empty()) {
        return ExecutionResult::fail("Timer not found: " + name);
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    double startTicks = timerOpt.at<double>(0, 0);
    double nowTicks = static_cast<double>(now.time_since_epoch().count());
    
    double elapsedNs = nowTicks - startTicks;
    double elapsedMs = elapsedNs / 1e6;
    
    if (print) {
        std::cout << "Timer [" << name << "]: " << elapsedMs << " ms" << std::endl;
    }
    
    ctx.cacheManager->set("elapsed_" + name, cv::Mat(1, 1, CV_64F, cv::Scalar(elapsedMs)));
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// FPSItem
// ============================================================================

FPSItem::FPSItem() {
    _functionName = "fps";
    _description = "Calculates and displays FPS";
    _category = "display";
    _params = {
        ParamDef::optional("draw", BaseType::BOOL, "Draw FPS on image", true),
        ParamDef::optional("x", BaseType::INT, "Text X position", 10),
        ParamDef::optional("y", BaseType::INT, "Text Y position", 30)
    };
    _example = "fps(true, 10, 30)";
    _returnType = "mat";
    _tags = {"fps", "performance", "display"};
}

ExecutionResult FPSItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    bool draw = args.size() > 0 ? args[0].asBool() : true;
    int x = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 10;
    int y = args.size() > 2 ? static_cast<int>(args[2].asNumber()) : 30;
    
    static int64 prevTick = 0;
    static double fps = 0;
    
    int64 currentTick = cv::getTickCount();
    if (prevTick > 0) {
        double elapsed = (currentTick - prevTick) / cv::getTickFrequency();
        fps = 0.9 * fps + 0.1 * (1.0 / elapsed);  // Smoothed FPS
    }
    prevTick = currentTick;
    
    cv::Mat result = ctx.currentMat.clone();
    
    if (draw) {
        if (result.channels() == 1) {
            cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
        }
        
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps));
        cv::putText(result, fpsText, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
    }
    
    ctx.cacheManager->set("fps", cv::Mat(1, 1, CV_64F, cv::Scalar(fps)));
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// GetWidthItem
// ============================================================================

GetWidthItem::GetWidthItem() {
    _functionName = "get_width";
    _description = "Gets image width";
    _category = "display";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "width")
    };
    _example = "get_width(\"w\")";
    _returnType = "mat";
    _tags = {"width", "size", "info"};
}

ExecutionResult GetWidthItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "width";
    ctx.cacheManager->set(cacheId, cv::Mat(1, 1, CV_32S, cv::Scalar(ctx.currentMat.cols)));
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetHeightItem
// ============================================================================

GetHeightItem::GetHeightItem() {
    _functionName = "get_height";
    _description = "Gets image height";
    _category = "display";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "height")
    };
    _example = "get_height(\"h\")";
    _returnType = "mat";
    _tags = {"height", "size", "info"};
}

ExecutionResult GetHeightItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "height";
    ctx.cacheManager->set(cacheId, cv::Mat(1, 1, CV_32S, cv::Scalar(ctx.currentMat.rows)));
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetSizeItem
// ============================================================================

GetSizeItem::GetSizeItem() {
    _functionName = "get_size";
    _description = "Gets image size (width, height)";
    _category = "display";
    _params = {
        ParamDef::optional("cache_prefix", BaseType::STRING, "Cache prefix", "size")
    };
    _example = "get_size(\"img\")";
    _returnType = "mat";
    _tags = {"size", "dimensions", "info"};
}

ExecutionResult GetSizeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string prefix = args.size() > 0 ? args[0].asString() : "size";
    ctx.cacheManager->set(prefix + "_width", cv::Mat(1, 1, CV_32S, cv::Scalar(ctx.currentMat.cols)));
    ctx.cacheManager->set(prefix + "_height", cv::Mat(1, 1, CV_32S, cv::Scalar(ctx.currentMat.rows)));
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetChannelsItem
// ============================================================================

GetChannelsItem::GetChannelsItem() {
    _functionName = "get_channels";
    _description = "Gets number of image channels";
    _category = "display";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "channels")
    };
    _example = "get_channels(\"ch\")";
    _returnType = "mat";
    _tags = {"channels", "info"};
}

ExecutionResult GetChannelsItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "channels";
    ctx.cacheManager->set(cacheId, cv::Mat(1, 1, CV_32S, cv::Scalar(ctx.currentMat.channels())));
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetDepthItem
// ============================================================================

GetDepthItem::GetDepthItem() {
    _functionName = "get_depth";
    _description = "Gets image depth (CV_8U, CV_32F, etc)";
    _category = "display";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "depth")
    };
    _example = "get_depth(\"d\")";
    _returnType = "mat";
    _tags = {"depth", "type", "info"};
}

ExecutionResult GetDepthItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "depth";
    ctx.cacheManager->set(cacheId, cv::Mat(1, 1, CV_32S, cv::Scalar(ctx.currentMat.depth())));
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetTypeItem
// ============================================================================

GetTypeItem::GetTypeItem() {
    _functionName = "get_type";
    _description = "Gets image type (CV_8UC3, etc)";
    _category = "display";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID", "type")
    };
    _example = "get_type(\"t\")";
    _returnType = "mat";
    _tags = {"type", "info"};
}

ExecutionResult GetTypeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "type";
    ctx.cacheManager->set(cacheId, cv::Mat(1, 1, CV_32S, cv::Scalar(ctx.currentMat.type())));
    return ExecutionResult::ok(ctx.currentMat);
}

} // namespace visionpipe

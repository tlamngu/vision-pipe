#include "interpreter/items/control_items.h"
#include "interpreter/items/transform_items.h"
#include "interpreter/cache_manager.h"
#include <iostream>
#include <sstream>
#include <cmath>

namespace visionpipe {

void registerControlItems(ItemRegistry& registry) {
    // Cache operations
    registry.add<CacheItem>();
    registry.add<GlobalCacheItem>();
    registry.add<PromoteToGlobalItem>();
    registry.add<LoadCacheItem>();
    registry.add<ClearCacheItem>();
    registry.add<ListCacheItem>();
    registry.add<CopyCacheItem>();
    registry.add<SwapCacheItem>();
    
    // Variable operations
    registry.add<SetVarItem>();
    registry.add<GetVarItem>();
    registry.add<IncrementVarItem>();
    registry.add<DecrementVarItem>();
    
    // Control flow
    registry.add<IfItem>();
    registry.add<LoopItem>();
    registry.add<BreakItem>();
    registry.add<ContinueItem>();
    registry.add<ReturnItem>();
    
    // Mat operations
    registry.add<CloneItem>();
    registry.add<CopyToItem>();
    registry.add<SetToItem>();
    registry.add<ConvertToItem>();
    registry.add<ReshapeItem>();
    registry.add<TransposeItem>();
    registry.add<ROIItem>();
    registry.add<CreateMatItem>();
    registry.add<ZerosItem>();
    registry.add<OnesItem>();
    registry.add<EyeItem>();
    
    // Arithmetic operations
    registry.add<AddItem>();
    registry.add<SubtractItem>();
    registry.add<MultiplyItem>();
    registry.add<DivideItem>();
    registry.add<AbsDiffItem>();
    registry.add<AddWeightedItem>();
    registry.add<ScaleAddItem>();
    
    // Bitwise operations
    registry.add<BitwiseAndItem>();
    registry.add<BitwiseOrItem>();
    registry.add<BitwiseXorItem>();
    registry.add<BitwiseNotItem>();
}

// ============================================================================
// CacheItem
// ============================================================================

CacheItem::CacheItem() {
    _functionName = "cache";
    _description = "Stores current image in cache with given ID";
    _category = "control";
    _params = {
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier")
    };
    _example = "cache(\"my_image\")";
    _returnType = "mat";
    _tags = {"cache", "store", "memory"};
}

ExecutionResult CacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    ctx.cacheManager->set(cacheId, ctx.currentMat.clone());
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GlobalCacheItem
// ============================================================================

GlobalCacheItem::GlobalCacheItem() {
    _functionName = "global_cache";
    _description = "Stores current image in GLOBAL cache with given ID";
    _category = "control";
    _params = {
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier")
    };
    _example = "global_cache(\"my_image\")";
    _returnType = "mat";
    _tags = {"cache", "store", "memory", "global"};
}

ExecutionResult GlobalCacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    ctx.cacheManager->set(cacheId, ctx.currentMat.clone(), true);  // true = global
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// PromoteToGlobalItem
// ============================================================================

PromoteToGlobalItem::PromoteToGlobalItem() {
    _functionName = "promote_global";
    _description = "Promotes a local cache entry to global scope";
    _category = "control";
    _params = {
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier to promote")
    };
    _example = "promote_global(\"my_image\")";
    _returnType = "mat";
    _tags = {"cache", "global", "promote"};
}

ExecutionResult PromoteToGlobalItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    cv::Mat mat = ctx.cacheManager->get(cacheId);
    if (mat.empty()) {
        return ExecutionResult::fail("Cache not found: " + cacheId);
    }
    ctx.cacheManager->set(cacheId, mat.clone(), true);  // Promote to global
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LoadCacheItem
// ============================================================================

LoadCacheItem::LoadCacheItem() {
    _functionName = "load_cache";
    _description = "Loads image from cache to current mat";
    _category = "control";
    _params = {
        ParamDef::required("cache_id", BaseType::STRING, "Cache identifier to load")
    };
    _example = "load_cache(\"my_image\")";
    _returnType = "mat";
    _tags = {"cache", "load", "memory"};
}

ExecutionResult LoadCacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args[0].asString();
    cv::Mat cached = ctx.cacheManager->get(cacheId);
    if (cached.empty()) {
        return ExecutionResult::fail("Cache not found: " + cacheId);
    }
    return ExecutionResult::ok(cached.clone());
}

// ============================================================================
// ClearCacheItem
// ============================================================================

ClearCacheItem::ClearCacheItem() {
    _functionName = "clear_cache";
    _description = "Clears specified cache entry or all cache";
    _category = "control";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID to clear (empty = all)", "")
    };
    _example = "clear_cache(\"my_image\")";
    _returnType = "mat";
    _tags = {"cache", "clear", "memory"};
}

ExecutionResult ClearCacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string cacheId = args.size() > 0 ? args[0].asString() : "";
    if (cacheId.empty()) {
        ctx.cacheManager->clearGlobal();
    } else {
        ctx.cacheManager->removeGlobal(cacheId);
    }
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ListCacheItem
// ============================================================================

ListCacheItem::ListCacheItem() {
    _functionName = "list_cache";
    _description = "Lists all cache entries with sizes";
    _category = "control";
    _params = {};
    _example = "list_cache()";
    _returnType = "mat";
    _tags = {"cache", "list", "debug"};
}

ExecutionResult ListCacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::cout << "=== Cache Contents ===" << std::endl;
    auto keys = ctx.cacheManager->getGlobalIds();
    for (const auto& key : keys) {
        cv::Mat mat = ctx.cacheManager->get(key);
        if (!mat.empty()) {
            std::cout << "  " << key << ": " << mat.cols << "x" << mat.rows 
                      << " (" << mat.channels() << "ch, type=" << mat.type() << ")" << std::endl;
        }
    }
    std::cout << "======================" << std::endl;
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CopyCacheItem
// ============================================================================

CopyCacheItem::CopyCacheItem() {
    _functionName = "copy_cache";
    _description = "Copies one cache entry to another";
    _category = "control";
    _params = {
        ParamDef::required("source", BaseType::STRING, "Source cache ID"),
        ParamDef::required("dest", BaseType::STRING, "Destination cache ID")
    };
    _example = "copy_cache(\"src\", \"dst\")";
    _returnType = "mat";
    _tags = {"cache", "copy"};
}

ExecutionResult CopyCacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string src = args[0].asString();
    std::string dst = args[1].asString();
    
    cv::Mat cached = ctx.cacheManager->get(src);
    if (cached.empty()) {
        return ExecutionResult::fail("Source cache not found: " + src);
    }
    
    ctx.cacheManager->set(dst, cached.clone());
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SwapCacheItem
// ============================================================================

SwapCacheItem::SwapCacheItem() {
    _functionName = "swap_cache";
    _description = "Swaps two cache entries";
    _category = "control";
    _params = {
        ParamDef::required("cache1", BaseType::STRING, "First cache ID"),
        ParamDef::required("cache2", BaseType::STRING, "Second cache ID")
    };
    _example = "swap_cache(\"img1\", \"img2\")";
    _returnType = "mat";
    _tags = {"cache", "swap"};
}

ExecutionResult SwapCacheItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string id1 = args[0].asString();
    std::string id2 = args[1].asString();
    
    cv::Mat mat1 = ctx.cacheManager->get(id1);
    cv::Mat mat2 = ctx.cacheManager->get(id2);
    
    if (mat1.empty()) return ExecutionResult::fail("Cache not found: " + id1);
    if (mat2.empty()) return ExecutionResult::fail("Cache not found: " + id2);
    
    cv::Mat temp = mat1.clone();
    ctx.cacheManager->set(id1, mat2.clone());
    ctx.cacheManager->set(id2, temp);
    
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetVarItem
// ============================================================================

SetVarItem::SetVarItem() {
    _functionName = "set_var";
    _description = "Sets a variable in the execution context";
    _category = "control";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Variable name"),
        ParamDef::required("value", BaseType::FLOAT, "Variable value")
    };
    _example = "set_var(\"threshold\", 127)";
    _returnType = "mat";
    _tags = {"variable", "set"};
}

ExecutionResult SetVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    double value = args[1].asNumber();
    ctx.variables[name] = value;
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// GetVarItem
// ============================================================================

GetVarItem::GetVarItem() {
    _functionName = "get_var";
    _description = "Gets a variable value and prints it";
    _category = "control";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Variable name")
    };
    _example = "get_var(\"threshold\")";
    _returnType = "mat";
    _tags = {"variable", "get"};
}

ExecutionResult GetVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    auto it = ctx.variables.find(name);
    if (it != ctx.variables.end()) {
        std::cout << name << " = " << it->second.asNumber() << std::endl;
    } else {
        std::cout << name << " is not defined" << std::endl;
    }
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// IncrementVarItem
// ============================================================================

IncrementVarItem::IncrementVarItem() {
    _functionName = "increment_var";
    _description = "Increments a variable by specified amount";
    _category = "control";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Variable name"),
        ParamDef::optional("amount", BaseType::FLOAT, "Increment amount", 1.0)
    };
    _example = "increment_var(\"counter\", 1)";
    _returnType = "mat";
    _tags = {"variable", "increment"};
}

ExecutionResult IncrementVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    double amount = args.size() > 1 ? args[1].asNumber() : 1.0;
    double currentValue = ctx.variables.count(name) ? ctx.variables[name].asNumber() : 0.0;
    ctx.variables[name] = RuntimeValue(currentValue + amount);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// DecrementVarItem
// ============================================================================

DecrementVarItem::DecrementVarItem() {
    _functionName = "decrement_var";
    _description = "Decrements a variable by specified amount";
    _category = "control";
    _params = {
        ParamDef::required("name", BaseType::STRING, "Variable name"),
        ParamDef::optional("amount", BaseType::FLOAT, "Decrement amount", 1.0)
    };
    _example = "decrement_var(\"counter\", 1)";
    _returnType = "mat";
    _tags = {"variable", "decrement"};
}

ExecutionResult DecrementVarItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string name = args[0].asString();
    double amount = args.size() > 1 ? args[1].asNumber() : 1.0;
    double currentValue = ctx.variables.count(name) ? ctx.variables[name].asNumber() : 0.0;
    ctx.variables[name] = RuntimeValue(currentValue - amount);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// IfItem
// ============================================================================

IfItem::IfItem() {
    _functionName = "if";
    _description = "Conditional execution (requires interpreter support)";
    _category = "control";
    _params = {
        ParamDef::required("condition", BaseType::STRING, "Condition expression")
    };
    _example = "if(\"$counter > 5\")";
    _returnType = "mat";
    _tags = {"control", "conditional"};
}

ExecutionResult IfItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Placeholder - actual implementation in interpreter
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// LoopItem
// ============================================================================

LoopItem::LoopItem() {
    _functionName = "loop";
    _description = "Loop execution (requires interpreter support)";
    _category = "control";
    _params = {
        ParamDef::required("count", BaseType::INT, "Number of iterations")
    };
    _example = "loop(10)";
    _returnType = "mat";
    _tags = {"control", "loop"};
}

ExecutionResult LoopItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Placeholder - actual implementation in interpreter
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// BreakItem
// ============================================================================

BreakItem::BreakItem() {
    _functionName = "break";
    _description = "Breaks out of current loop (exec_loop, while)";
    _category = "control";
    _params = {};
    _example = "break()";
    _returnType = "void";
    _tags = {"control", "break", "loop"};
}

ExecutionResult BreakItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    (void)args;
    ctx.shouldBreak = true;
    return ExecutionResult::breakLoop();
}

// ============================================================================
// ContinueItem
// ============================================================================

ContinueItem::ContinueItem() {
    _functionName = "continue";
    _description = "Continues to next loop iteration";
    _category = "control";
    _params = {};
    _example = "continue()";
    _returnType = "mat";
    _tags = {"control", "continue"};
}

ExecutionResult ContinueItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Placeholder - actual implementation in interpreter
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// ReturnItem
// ============================================================================

ReturnItem::ReturnItem() {
    _functionName = "return";
    _description = "Returns from pipeline execution";
    _category = "control";
    _params = {};
    _example = "return()";
    _returnType = "mat";
    _tags = {"control", "return"};
}

ExecutionResult ReturnItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    // Placeholder - actual implementation in interpreter
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// CloneItem
// ============================================================================

CloneItem::CloneItem() {
    _functionName = "clone";
    _description = "Creates a deep copy of current mat";
    _category = "control";
    _params = {
        ParamDef::optional("cache_id", BaseType::STRING, "Cache ID for clone", "")
    };
    _example = "clone(\"backup\")";
    _returnType = "mat";
    _tags = {"mat", "clone", "copy"};
}

ExecutionResult CloneItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    cv::Mat cloned = ctx.currentMat.clone();
    if (args.size() > 0 && !args[0].asString().empty()) {
        ctx.cacheManager->set(args[0].asString(), cloned);
    }
    return ExecutionResult::ok(cloned);
}

// ============================================================================
// CopyToItem
// ============================================================================

CopyToItem::CopyToItem() {
    _functionName = "copy_to";
    _description = "Copies current mat to destination with optional mask";
    _category = "control";
    _params = {
        ParamDef::required("dest_cache", BaseType::STRING, "Destination cache ID"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask cache ID", "")
    };
    _example = "copy_to(\"dest\", \"mask\")";
    _returnType = "mat";
    _tags = {"mat", "copy"};
}

ExecutionResult CopyToItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string destCache = args[0].asString();
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    cv::Mat dest = ctx.currentMat.clone();
    
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) {
            cv::Mat result;
            ctx.currentMat.copyTo(result, maskOpt);
            dest = result;
        }
    }
    
    ctx.cacheManager->set(destCache, dest);
    return ExecutionResult::ok(ctx.currentMat);
}

// ============================================================================
// SetToItem
// ============================================================================

SetToItem::SetToItem() {
    _functionName = "set_to";
    _description = "Sets mat pixels to specified value";
    _category = "control";
    _params = {
        ParamDef::required("value", BaseType::FLOAT, "Scalar value to set"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask cache ID", "")
    };
    _example = "set_to(0, \"mask\")";
    _returnType = "mat";
    _tags = {"mat", "set"};
}

ExecutionResult SetToItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double value = args[0].asNumber();
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    cv::Mat result = ctx.currentMat.clone();
    
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) {
            result.setTo(cv::Scalar::all(value), maskOpt);
        }
    } else {
        result.setTo(cv::Scalar::all(value));
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ConvertToItem
// ============================================================================

ConvertToItem::ConvertToItem() {
    _functionName = "convert_to";
    _description = "Converts mat to different depth with optional scaling";
    _category = "control";
    _params = {
        ParamDef::required("depth", BaseType::STRING, "Target depth: 8u, 8s, 16u, 16s, 32s, 32f, 64f"),
        ParamDef::optional("alpha", BaseType::FLOAT, "Scale factor", 1.0),
        ParamDef::optional("beta", BaseType::FLOAT, "Offset", 0.0)
    };
    _example = "convert_to(\"32f\", 1.0/255.0, 0)";
    _returnType = "mat";
    _tags = {"mat", "convert", "depth"};
}

ExecutionResult ConvertToItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string depthStr = args[0].asString();
    double alpha = args.size() > 1 ? args[1].asNumber() : 1.0;
    double beta = args.size() > 2 ? args[2].asNumber() : 0.0;
    
    int depth = CV_8U;
    if (depthStr == "8s") depth = CV_8S;
    else if (depthStr == "16u") depth = CV_16U;
    else if (depthStr == "16s") depth = CV_16S;
    else if (depthStr == "32s") depth = CV_32S;
    else if (depthStr == "32f") depth = CV_32F;
    else if (depthStr == "64f") depth = CV_64F;
    
    cv::Mat result;
    ctx.currentMat.convertTo(result, depth, alpha, beta);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ReshapeItem
// ============================================================================

ReshapeItem::ReshapeItem() {
    _functionName = "reshape";
    _description = "Changes mat shape without copying data";
    _category = "control";
    _params = {
        ParamDef::required("channels", BaseType::INT, "New number of channels"),
        ParamDef::optional("rows", BaseType::INT, "New number of rows (0 = auto)", 0)
    };
    _example = "reshape(1, 0)";
    _returnType = "mat";
    _tags = {"mat", "reshape"};
}

ExecutionResult ReshapeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int cn = static_cast<int>(args[0].asNumber());
    int rows = args.size() > 1 ? static_cast<int>(args[1].asNumber()) : 0;
    
    cv::Mat result = ctx.currentMat.reshape(cn, rows);
    return ExecutionResult::ok(result);
}

// ============================================================================
// ROIItem
// ============================================================================

ROIItem::ROIItem() {
    _functionName = "roi";
    _description = "Extracts region of interest from current mat";
    _category = "control";
    _params = {
        ParamDef::required("x", BaseType::INT, "X coordinate"),
        ParamDef::required("y", BaseType::INT, "Y coordinate"),
        ParamDef::required("width", BaseType::INT, "Width"),
        ParamDef::required("height", BaseType::INT, "Height")
    };
    _example = "roi(100, 100, 200, 200)";
    _returnType = "mat";
    _tags = {"mat", "roi", "crop"};
}

ExecutionResult ROIItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int x = static_cast<int>(args[0].asNumber());
    int y = static_cast<int>(args[1].asNumber());
    int w = static_cast<int>(args[2].asNumber());
    int h = static_cast<int>(args[3].asNumber());
    
    cv::Rect roi(x, y, w, h);
    roi = roi & cv::Rect(0, 0, ctx.currentMat.cols, ctx.currentMat.rows);
    
    return ExecutionResult::ok(ctx.currentMat(roi).clone());
}

// ============================================================================
// CreateMatItem
// ============================================================================

CreateMatItem::CreateMatItem() {
    _functionName = "create_mat";
    _description = "Creates a new mat with specified dimensions and type";
    _category = "control";
    _params = {
        ParamDef::required("rows", BaseType::INT, "Number of rows"),
        ParamDef::required("cols", BaseType::INT, "Number of columns"),
        ParamDef::optional("type", BaseType::STRING, "Type: 8uc1, 8uc3, 32fc1, etc.", "8uc3"),
        ParamDef::optional("value", BaseType::FLOAT, "Initial value", 0.0)
    };
    _example = "create_mat(480, 640, \"8uc3\", 0)";
    _returnType = "mat";
    _tags = {"mat", "create"};
}

ExecutionResult CreateMatItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int rows = static_cast<int>(args[0].asNumber());
    int cols = static_cast<int>(args[1].asNumber());
    std::string typeStr = args.size() > 2 ? args[2].asString() : "8uc3";
    double value = args.size() > 3 ? args[3].asNumber() : 0.0;
    
    int type = CV_8UC3;
    if (typeStr == "8uc1") type = CV_8UC1;
    else if (typeStr == "8uc4") type = CV_8UC4;
    else if (typeStr == "16uc1") type = CV_16UC1;
    else if (typeStr == "16sc1") type = CV_16SC1;
    else if (typeStr == "32fc1") type = CV_32FC1;
    else if (typeStr == "32fc3") type = CV_32FC3;
    else if (typeStr == "64fc1") type = CV_64FC1;
    
    cv::Mat result(rows, cols, type, cv::Scalar::all(value));
    return ExecutionResult::ok(result);
}

// ============================================================================
// ZerosItem
// ============================================================================

ZerosItem::ZerosItem() {
    _functionName = "zeros";
    _description = "Creates a zero-filled mat";
    _category = "control";
    _params = {
        ParamDef::required("rows", BaseType::INT, "Number of rows"),
        ParamDef::required("cols", BaseType::INT, "Number of columns"),
        ParamDef::optional("type", BaseType::STRING, "Type: 8uc1, 8uc3, 32fc1, etc.", "8uc3")
    };
    _example = "zeros(480, 640, \"8uc3\")";
    _returnType = "mat";
    _tags = {"mat", "zeros", "create"};
}

ExecutionResult ZerosItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int rows = static_cast<int>(args[0].asNumber());
    int cols = static_cast<int>(args[1].asNumber());
    std::string typeStr = args.size() > 2 ? args[2].asString() : "8uc3";
    
    int type = CV_8UC3;
    if (typeStr == "8uc1") type = CV_8UC1;
    else if (typeStr == "8uc4") type = CV_8UC4;
    else if (typeStr == "32fc1") type = CV_32FC1;
    else if (typeStr == "32fc3") type = CV_32FC3;
    else if (typeStr == "64fc1") type = CV_64FC1;
    
    return ExecutionResult::ok(cv::Mat::zeros(rows, cols, type));
}

// ============================================================================
// OnesItem
// ============================================================================

OnesItem::OnesItem() {
    _functionName = "ones";
    _description = "Creates a mat filled with ones";
    _category = "control";
    _params = {
        ParamDef::required("rows", BaseType::INT, "Number of rows"),
        ParamDef::required("cols", BaseType::INT, "Number of columns"),
        ParamDef::optional("type", BaseType::STRING, "Type: 8uc1, 8uc3, 32fc1, etc.", "32fc1")
    };
    _example = "ones(480, 640, \"32fc1\")";
    _returnType = "mat";
    _tags = {"mat", "ones", "create"};
}

ExecutionResult OnesItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int rows = static_cast<int>(args[0].asNumber());
    int cols = static_cast<int>(args[1].asNumber());
    std::string typeStr = args.size() > 2 ? args[2].asString() : "32fc1";
    
    int type = CV_32FC1;
    if (typeStr == "8uc1") type = CV_8UC1;
    else if (typeStr == "8uc3") type = CV_8UC3;
    else if (typeStr == "64fc1") type = CV_64FC1;
    
    return ExecutionResult::ok(cv::Mat::ones(rows, cols, type));
}

// ============================================================================
// EyeItem
// ============================================================================

EyeItem::EyeItem() {
    _functionName = "eye";
    _description = "Creates an identity matrix";
    _category = "control";
    _params = {
        ParamDef::required("size", BaseType::INT, "Matrix size (square)"),
        ParamDef::optional("type", BaseType::STRING, "Type: 32fc1, 64fc1", "32fc1")
    };
    _example = "eye(3, \"64fc1\")";
    _returnType = "mat";
    _tags = {"mat", "identity", "create"};
}

ExecutionResult EyeItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    int size = static_cast<int>(args[0].asNumber());
    std::string typeStr = args.size() > 1 ? args[1].asString() : "32fc1";
    
    int type = CV_32FC1;
    if (typeStr == "64fc1") type = CV_64FC1;
    
    return ExecutionResult::ok(cv::Mat::eye(size, size, type));
}

// ============================================================================
// AddItem
// ============================================================================

AddItem::AddItem() {
    _functionName = "add";
    _description = "Adds current mat with another mat or scalar";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID or scalar value"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", ""),
        ParamDef::optional("dtype", BaseType::STRING, "Output depth", "-1")
    };
    _example = "add(\"other_img\")";
    _returnType = "mat";
    _tags = {"arithmetic", "add"};
}

ExecutionResult AddItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    cv::Mat result;
    cv::Mat operandMat = ctx.cacheManager->get(operand);
    if (!operandMat.empty()) {
        cv::add(ctx.currentMat, operandMat, result, mask);
    } else {
        try {
            double scalar = std::stod(operand);
            cv::add(ctx.currentMat, cv::Scalar::all(scalar), result, mask);
        } catch (...) {
            return ExecutionResult::fail("Invalid operand: " + operand);
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// SubtractItem
// ============================================================================

SubtractItem::SubtractItem() {
    _functionName = "subtract";
    _description = "Subtracts another mat or scalar from current mat";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID or scalar value"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", "")
    };
    _example = "subtract(\"background\")";
    _returnType = "mat";
    _tags = {"arithmetic", "subtract"};
}

ExecutionResult SubtractItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    cv::Mat result;
    cv::Mat operandMat = ctx.cacheManager->get(operand);
    if (!operandMat.empty()) {
        cv::subtract(ctx.currentMat, operandMat, result, mask);
    } else {
        try {
            double scalar = std::stod(operand);
            cv::subtract(ctx.currentMat, cv::Scalar::all(scalar), result, mask);
        } catch (...) {
            return ExecutionResult::fail("Invalid operand: " + operand);
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// MultiplyItem
// ============================================================================

MultiplyItem::MultiplyItem() {
    _functionName = "multiply";
    _description = "Element-wise multiplication";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID or scalar value"),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0)
    };
    _example = "multiply(\"mask\", 1.0)";
    _returnType = "mat";
    _tags = {"arithmetic", "multiply"};
}

ExecutionResult MultiplyItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    double scale = args.size() > 1 ? args[1].asNumber() : 1.0;
    
    cv::Mat result;
    cv::Mat operandMat = ctx.cacheManager->get(operand);
    if (!operandMat.empty()) {
        cv::multiply(ctx.currentMat, operandMat, result, scale);
    } else {
        try {
            double scalar = std::stod(operand);
            result = ctx.currentMat * scalar * scale;
        } catch (...) {
            return ExecutionResult::fail("Invalid operand: " + operand);
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// DivideItem
// ============================================================================

DivideItem::DivideItem() {
    _functionName = "divide";
    _description = "Element-wise division";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID or scalar value"),
        ParamDef::optional("scale", BaseType::FLOAT, "Scale factor", 1.0)
    };
    _example = "divide(\"normalizer\", 1.0)";
    _returnType = "mat";
    _tags = {"arithmetic", "divide"};
}

ExecutionResult DivideItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    double scale = args.size() > 1 ? args[1].asNumber() : 1.0;
    
    cv::Mat result;
    cv::Mat operandMat = ctx.cacheManager->get(operand);
    if (!operandMat.empty()) {
        cv::divide(ctx.currentMat, operandMat, result, scale);
    } else {
        try {
            double scalar = std::stod(operand);
            if (scalar == 0) {
                return ExecutionResult::fail("Division by zero");
            }
            result = ctx.currentMat / scalar * scale;
        } catch (...) {
            return ExecutionResult::fail("Invalid operand: " + operand);
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// AbsDiffItem
// ============================================================================

AbsDiffItem::AbsDiffItem() {
    _functionName = "absdiff";
    _description = "Calculates absolute difference between mats";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID or scalar value")
    };
    _example = "absdiff(\"background\")";
    _returnType = "mat";
    _tags = {"arithmetic", "absdiff"};
}

ExecutionResult AbsDiffItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    
    cv::Mat result;
    cv::Mat operandMat = ctx.cacheManager->get(operand);
    if (!operandMat.empty()) {
        cv::absdiff(ctx.currentMat, operandMat, result);
    } else {
        try {
            double scalar = std::stod(operand);
            cv::absdiff(ctx.currentMat, cv::Scalar::all(scalar), result);
        } catch (...) {
            return ExecutionResult::fail("Invalid operand: " + operand);
        }
    }
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// AddWeightedItem
// ============================================================================

AddWeightedItem::AddWeightedItem() {
    _functionName = "add_weighted";
    _description = "Weighted sum of two mats";
    _category = "control";
    _params = {
        ParamDef::required("alpha", BaseType::FLOAT, "Weight for current mat"),
        ParamDef::required("other_cache", BaseType::STRING, "Second image cache ID"),
        ParamDef::required("beta", BaseType::FLOAT, "Weight for second mat"),
        ParamDef::optional("gamma", BaseType::FLOAT, "Added scalar", 0.0)
    };
    _example = "add_weighted(0.7, \"overlay\", 0.3, 0)";
    _returnType = "mat";
    _tags = {"arithmetic", "blend", "weighted"};
}

ExecutionResult AddWeightedItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double alpha = args[0].asNumber();
    std::string otherCache = args[1].asString();
    double beta = args[2].asNumber();
    double gamma = args.size() > 3 ? args[3].asNumber() : 0.0;
    
    cv::Mat otherOpt = ctx.cacheManager->get(otherCache);
    if (otherOpt.empty()) {
        return ExecutionResult::fail("Image not found: " + otherCache);
    }
    
    cv::Mat result;
    cv::addWeighted(ctx.currentMat, alpha, otherOpt, beta, gamma, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// ScaleAddItem
// ============================================================================

ScaleAddItem::ScaleAddItem() {
    _functionName = "scale_add";
    _description = "Scaled addition: dst = scale*src1 + src2";
    _category = "control";
    _params = {
        ParamDef::required("scale", BaseType::FLOAT, "Scale for current mat"),
        ParamDef::required("other_cache", BaseType::STRING, "Second image cache ID")
    };
    _example = "scale_add(0.5, \"other\")";
    _returnType = "mat";
    _tags = {"arithmetic", "scale"};
}

ExecutionResult ScaleAddItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    double scale = args[0].asNumber();
    std::string otherCache = args[1].asString();
    
    cv::Mat otherOpt = ctx.cacheManager->get(otherCache);
    if (otherOpt.empty()) {
        return ExecutionResult::fail("Image not found: " + otherCache);
    }
    
    cv::Mat result;
    cv::scaleAdd(ctx.currentMat, scale, otherOpt, result);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BitwiseAndItem
// ============================================================================

BitwiseAndItem::BitwiseAndItem() {
    _functionName = "bitwise_and";
    _description = "Bitwise AND operation";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID for second operand"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", "")
    };
    _example = "bitwise_and(\"mask\")";
    _returnType = "mat";
    _tags = {"bitwise", "and"};
}

ExecutionResult BitwiseAndItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    cv::Mat operandOpt = ctx.cacheManager->get(operand);
    if (operandOpt.empty()) {
        return ExecutionResult::fail("Operand not found: " + operand);
    }
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    cv::Mat result;
    cv::bitwise_and(ctx.currentMat, operandOpt, result, mask);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BitwiseOrItem
// ============================================================================

BitwiseOrItem::BitwiseOrItem() {
    _functionName = "bitwise_or";
    _description = "Bitwise OR operation";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID for second operand"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", "")
    };
    _example = "bitwise_or(\"mask\")";
    _returnType = "mat";
    _tags = {"bitwise", "or"};
}

ExecutionResult BitwiseOrItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    cv::Mat operandOpt = ctx.cacheManager->get(operand);
    if (operandOpt.empty()) {
        return ExecutionResult::fail("Operand not found: " + operand);
    }
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    cv::Mat result;
    cv::bitwise_or(ctx.currentMat, operandOpt, result, mask);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BitwiseXorItem
// ============================================================================

BitwiseXorItem::BitwiseXorItem() {
    _functionName = "bitwise_xor";
    _description = "Bitwise XOR operation";
    _category = "control";
    _params = {
        ParamDef::required("operand", BaseType::STRING, "Cache ID for second operand"),
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", "")
    };
    _example = "bitwise_xor(\"mask\")";
    _returnType = "mat";
    _tags = {"bitwise", "xor"};
}

ExecutionResult BitwiseXorItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string operand = args[0].asString();
    std::string maskCache = args.size() > 1 ? args[1].asString() : "";
    
    cv::Mat operandOpt = ctx.cacheManager->get(operand);
    if (operandOpt.empty()) {
        return ExecutionResult::fail("Operand not found: " + operand);
    }
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    cv::Mat result;
    cv::bitwise_xor(ctx.currentMat, operandOpt, result, mask);
    
    return ExecutionResult::ok(result);
}

// ============================================================================
// BitwiseNotItem
// ============================================================================

BitwiseNotItem::BitwiseNotItem() {
    _functionName = "bitwise_not";
    _description = "Bitwise NOT (inversion) operation";
    _category = "control";
    _params = {
        ParamDef::optional("mask_cache", BaseType::STRING, "Optional mask", "")
    };
    _example = "bitwise_not()";
    _returnType = "mat";
    _tags = {"bitwise", "not", "invert"};
}

ExecutionResult BitwiseNotItem::execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) {
    std::string maskCache = args.size() > 0 ? args[0].asString() : "";
    
    cv::Mat mask;
    if (!maskCache.empty()) {
        cv::Mat maskOpt = ctx.cacheManager->get(maskCache);
        if (!maskOpt.empty()) mask = maskOpt;
    }
    
    cv::Mat result;
    cv::bitwise_not(ctx.currentMat, result, mask);
    
    return ExecutionResult::ok(result);
}

} // namespace visionpipe

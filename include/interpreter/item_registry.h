#ifndef VISIONPIPE_ITEM_REGISTRY_H
#define VISIONPIPE_ITEM_REGISTRY_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>
#include <variant>
#include <opencv2/core/mat.hpp>

// Forward declarations for tensor types
namespace visionpipe {
class Vector;
class Matrix;
}

namespace visionpipe {

// Forward declarations
class Pipeline;
class PipelineItem;
class CacheManager;

/**
 * @brief Base type enumeration for interpreter values
 */
enum class BaseType {
    VOID,
    INT,
    FLOAT,
    STRING,
    BOOL,
    MAT,           // cv::Mat
    VECTOR,        // visionpipe::Vector (1D numeric array)
    MATRIX,        // visionpipe::Matrix (2D numeric array)
    ARRAY,
    CACHE_REF,
    ANY            // Accepts any type
};

std::string baseTypeToString(BaseType type);
BaseType stringToBaseType(const std::string& str);

/**
 * @brief Represents a runtime value in the interpreter
 */
struct RuntimeValue {
    using Value = std::variant<
        std::monostate,
        int64_t,
        double,
        std::string,
        bool,
        cv::Mat,
        std::vector<RuntimeValue>,
        std::shared_ptr<void>  // Used for Vector/Matrix (type-erased storage)
    >;
    
    Value value;
    BaseType type;
    
    RuntimeValue() : type(BaseType::VOID) {}
    RuntimeValue(int64_t v) : value(v), type(BaseType::INT) {}
    RuntimeValue(int v) : value(static_cast<int64_t>(v)), type(BaseType::INT) {}
    RuntimeValue(double v) : value(v), type(BaseType::FLOAT) {}
    RuntimeValue(const std::string& v) : value(v), type(BaseType::STRING) {}
    RuntimeValue(const char* v) : value(std::string(v)), type(BaseType::STRING) {}
    RuntimeValue(bool v) : value(v), type(BaseType::BOOL) {}
    RuntimeValue(const cv::Mat& v) : value(v), type(BaseType::MAT) {}
    RuntimeValue(std::vector<RuntimeValue> v) : value(std::move(v)), type(BaseType::ARRAY) {}
    
    // Vector/Matrix constructors (defined in cpp file)
    static RuntimeValue fromVector(const Vector& v);
    static RuntimeValue fromMatrix(const Matrix& m);
    
    // Type checking
    bool isVoid() const { return type == BaseType::VOID; }
    bool isInt() const { return type == BaseType::INT; }
    bool isFloat() const { return type == BaseType::FLOAT; }
    bool isNumeric() const { return type == BaseType::INT || type == BaseType::FLOAT; }
    bool isString() const { return type == BaseType::STRING; }
    bool isBool() const { return type == BaseType::BOOL; }
    bool isMat() const { return type == BaseType::MAT; }
    bool isVector() const { return type == BaseType::VECTOR; }
    bool isMatrix() const { return type == BaseType::MATRIX; }
    bool isTensor() const { return isVector() || isMatrix() || isMat(); }
    bool isArray() const { return type == BaseType::ARRAY; }
    
    // Value extraction
    int64_t asInt() const;
    double asFloat() const;
    double asNumber() const;  // Works for both int and float
    std::string asString() const;
    bool asBool() const;
    cv::Mat asMat() const;
    const std::vector<RuntimeValue>& asArray() const;
    Vector asVector() const;    // Get as Vector
    Matrix asMatrix() const;    // Get as Matrix
    
    // Conversion
    RuntimeValue toInt() const;
    RuntimeValue toFloat() const;
    RuntimeValue toString() const;
    RuntimeValue toBool() const;
    RuntimeValue toVector() const;   // Convert to Vector
    RuntimeValue toMatrix() const;   // Convert to Matrix
    
    std::string typeString() const { return baseTypeToString(type); }
    std::string debugString() const;
};

/**
 * @brief Parameter definition for an interpreter item
 */
struct ParamDef {
    std::string name;
    BaseType type;
    std::string description;
    bool isOptional = false;
    std::optional<RuntimeValue> defaultValue;
    
    ParamDef(const std::string& n, BaseType t, const std::string& desc)
        : name(n), type(t), description(desc) {}
    
    ParamDef(const std::string& n, BaseType t, const std::string& desc, RuntimeValue defaultVal)
        : name(n), type(t), description(desc), isOptional(true), defaultValue(defaultVal) {}
    
    static ParamDef required(const std::string& name, BaseType type, const std::string& desc) {
        return ParamDef(name, type, desc);
    }
    
    static ParamDef optional(const std::string& name, BaseType type, const std::string& desc, RuntimeValue defaultVal) {
        return ParamDef(name, type, desc, defaultVal);
    }
};

/**
 * @brief Execution context passed to interpreter items
 */
struct ExecutionContext {
    cv::Mat currentMat;                     // Current frame/Mat being processed
    CacheManager* cacheManager = nullptr;   // Access to cache
    Pipeline* currentPipeline = nullptr;    // Current pipeline being executed
    std::unordered_map<std::string, RuntimeValue> variables;  // Local variables
    
    bool verbose = false;                   // Enable verbose DNN/performance output
    bool shouldBreak = false;
    bool shouldContinue = false;
    bool shouldReturn = false;
    RuntimeValue returnValue;
    
    void reset() {
        shouldBreak = false;
        shouldContinue = false;
        shouldReturn = false;
        returnValue = RuntimeValue();
    }
};

/**
 * @brief Result of executing an interpreter item
 */
struct ExecutionResult {
    bool success = true;
    cv::Mat outputMat;
    std::optional<std::string> error;
    std::optional<std::string> cacheOutputId;
    std::optional<RuntimeValue> scalarValue;  // For items that return non-Mat values (e.g., wait_key returns int)
    bool shouldBreak = false;                  // Set to true to break out of exec_loop
    
    static ExecutionResult ok(const cv::Mat& mat = cv::Mat()) {
        ExecutionResult r;
        r.outputMat = mat;
        return r;
    }
    
    static ExecutionResult ok(const RuntimeValue& value) {
        ExecutionResult r;
        r.scalarValue = value;
        return r;
    }
    
    static ExecutionResult okWithMat(const cv::Mat& mat, const RuntimeValue& value) {
        ExecutionResult r;
        r.outputMat = mat;
        r.scalarValue = value;
        return r;
    }
    
    static ExecutionResult fail(const std::string& error) {
        ExecutionResult r;
        r.success = false;
        r.error = error;
        return r;
    }
    
    static ExecutionResult breakLoop() {
        ExecutionResult r;
        r.shouldBreak = true;
        return r;
    }
    
    ExecutionResult& withCache(const std::string& cacheId) {
        cacheOutputId = cacheId;
        return *this;
    }
    
    ExecutionResult& withValue(const RuntimeValue& value) {
        scalarValue = value;
        return *this;
    }
};

/**
 * @brief Base class for all interpreter items
 * 
 * This is the core abstraction that makes the interpreter modular.
 * Each function in the VSP language is represented by an InterpreterItem.
 * 
 * Example:
 * ```
 * class VideoCaptureItem : public InterpreterItem {
 * public:
 *     VideoCaptureItem() {
 *         _functionName = "video_cap";
 *         _description = "Captures video from a source";
 *         _params = {ParamDef::required("input_id", BaseType::STRING, "Source identifier")};
 *         _example = "video_cap(0)";
 *     }
 *     
 *     ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) override {
 *         // Implementation
 *     }
 * };
 * ```
 */
class InterpreterItem {
public:
    virtual ~InterpreterItem() = default;
    
    // Metadata
    const std::string& functionName() const { return _functionName; }
    const std::string& description() const { return _description; }
    const std::string& category() const { return _category; }
    const std::vector<ParamDef>& params() const { return _params; }
    const std::string& example() const { return _example; }
    const std::string& returnType() const { return _returnType; }
    const std::vector<std::string>& tags() const { return _tags; }
    
    /**
     * @brief Execute the item with given arguments
     * @param args Evaluated arguments
     * @param ctx Execution context
     * @return Execution result
     */
    virtual ExecutionResult execute(const std::vector<RuntimeValue>& args, ExecutionContext& ctx) = 0;
    
    /**
     * @brief Validate arguments before execution
     * @param args Arguments to validate
     * @return Error message if invalid, nullopt if valid
     */
    virtual std::optional<std::string> validateArgs(const std::vector<RuntimeValue>& args) const;
    
    /**
     * @brief Create a corresponding PipelineItem (for optimization)
     * @param args Arguments for the item
     * @return Shared pointer to PipelineItem, or nullptr if not applicable
     */
    virtual std::shared_ptr<PipelineItem> createPipelineItem(const std::vector<RuntimeValue>& args) const {
        return nullptr;
    }
    
    /**
     * @brief Check if this item modifies the current Mat
     */
    virtual bool modifiesMat() const { return true; }
    
    /**
     * @brief Check if this item is pure (no side effects)
     */
    virtual bool isPure() const { return false; }
    
    /**
     * @brief Generate documentation for this item
     */
    std::string generateDocumentation() const;
    
    /**
     * @brief Generate JSON schema for this item
     */
    std::string generateJsonSchema() const;
    
protected:
    std::string _functionName;
    std::string _description;
    std::string _category = "general";
    std::vector<ParamDef> _params;
    std::string _example;
    std::string _returnType = "mat";
    std::vector<std::string> _tags;
};

/**
 * @brief Registry for interpreter items
 * 
 * Provides a central registry for all available functions in the interpreter.
 * Items can be registered and looked up by name.
 */
class ItemRegistry {
public:
    using ItemFactory = std::function<std::shared_ptr<InterpreterItem>()>;
    
    ItemRegistry();
    ~ItemRegistry() = default;
    
    /**
     * @brief Register an item by providing a factory function
     */
    void registerItem(const std::string& name, ItemFactory factory);
    
    /**
     * @brief Register an item instance directly
     */
    void registerItem(std::shared_ptr<InterpreterItem> item);
    
    /**
     * @brief Register an item using its type (requires default constructor)
     */
    template<typename T>
    void registerItem() {
        auto item = std::make_shared<T>();
        registerItem(item);
    }
    
    /**
     * @brief Add an item (convenience method, same as registerItem)
     */
    void add(std::shared_ptr<InterpreterItem> item) {
        registerItem(item);
    }
    
    /**
     * @brief Add an item using its type
     */
    template<typename T>
    void add() {
        registerItem<T>();
    }
    
    /**
     * @brief Get an item by name
     * @return Shared pointer to item, or nullptr if not found
     */
    std::shared_ptr<InterpreterItem> getItem(const std::string& name) const;
    
    /**
     * @brief Check if an item exists
     */
    bool hasItem(const std::string& name) const;
    
    /**
     * @brief Get all registered item names
     */
    std::vector<std::string> getItemNames() const;
    
    /**
     * @brief Get all items in a category
     */
    std::vector<std::shared_ptr<InterpreterItem>> getItemsByCategory(const std::string& category) const;
    
    /**
     * @brief Get all unique categories
     */
    std::vector<std::string> getCategories() const;
    
    /**
     * @brief Get items matching tags
     */
    std::vector<std::shared_ptr<InterpreterItem>> getItemsByTag(const std::string& tag) const;
    
    /**
     * @brief Generate documentation for all items
     */
    std::string generateDocumentation() const;
    
    /**
     * @brief Generate JSON schema for all items
     */
    std::string generateJsonSchema() const;
    
    /**
     * @brief Clear all registered items
     */
    void clear();
    
    /**
     * @brief Get the singleton instance
     */
    static ItemRegistry& instance();
    
private:
    std::unordered_map<std::string, std::shared_ptr<InterpreterItem>> _items;
    std::unordered_map<std::string, ItemFactory> _factories;
};

} // namespace visionpipe

#endif // VISIONPIPE_ITEM_REGISTRY_H

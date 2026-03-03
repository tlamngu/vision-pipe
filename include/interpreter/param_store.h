#ifndef VISIONPIPE_PARAM_STORE_H
#define VISIONPIPE_PARAM_STORE_H

/**
 * @file param_store.h
 * @brief Thread-safe runtime parameter store for VisionPipe
 *
 * The ParameterStore holds named, typed parameters that can be updated at
 * runtime from external sources (TCP client, libvisionpipe, Python) while a
 * pipeline is executing.
 *
 * VSP syntax:
 * @code
 *   # Declare at top-level (before exec_loop)
 *   params [
 *     brightness: int = 50,
 *     gain: float = 1.0,
 *     mode: string = "auto"
 *   ]
 *
 *   # Reference inline with @ prefix
 *   pipeline process
 *     adjust_brightness(@brightness)
 *     set_gain(@gain)
 *   end
 *
 *   # Event handler – runs whenever 'brightness' changes
 *   on_params @brightness
 *     log("Brightness changed to", @brightness)
 *     adjust_brightness(@brightness)
 *   end
 *
 *   exec_loop process
 * @endcode
 */

#include <string>
#include <variant>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <optional>
#include <stdexcept>

namespace visionpipe {

// ============================================================================
// ParamValue – typed parameter value
// ============================================================================

/**
 * @brief Supported parameter types
 */
enum class ParamType {
    INT,
    FLOAT,
    STRING,
    BOOL,
    UNKNOWN
};

std::string paramTypeToString(ParamType t);
ParamType   paramTypeFromString(const std::string& s);

/**
 * @brief Parameter value – holds int / float / string / bool
 */
struct ParamValue {
    using Storage = std::variant<std::monostate, int64_t, double, std::string, bool>;

    Storage      storage;
    ParamType    type = ParamType::UNKNOWN;

    // Constructors
    ParamValue() = default;
    explicit ParamValue(int64_t v)          : storage(v),               type(ParamType::INT)    {}
    explicit ParamValue(int v)              : storage((int64_t)v),       type(ParamType::INT)    {}
    explicit ParamValue(double v)           : storage(v),               type(ParamType::FLOAT)  {}
    explicit ParamValue(const std::string& v) : storage(v),             type(ParamType::STRING) {}
    explicit ParamValue(std::string&& v)    : storage(std::move(v)),    type(ParamType::STRING) {}
    explicit ParamValue(bool v)             : storage(v),               type(ParamType::BOOL)   {}

    bool        isNull()   const { return std::holds_alternative<std::monostate>(storage); }
    bool        isInt()    const { return type == ParamType::INT;    }
    bool        isFloat()  const { return type == ParamType::FLOAT;  }
    bool        isString() const { return type == ParamType::STRING; }
    bool        isBool()   const { return type == ParamType::BOOL;   }

    int64_t     asInt()    const;
    double      asFloat()  const;
    std::string asString() const;
    bool        asBool()   const;

    /** Serialize for wire protocol (JSON-compatible string) */
    std::string toWireString() const;

    /** Parse from wire protocol string, given type hint */
    static ParamValue fromWireString(const std::string& s, ParamType hint = ParamType::UNKNOWN);

    bool operator==(const ParamValue& o) const { return storage == o.storage; }
    bool operator!=(const ParamValue& o) const { return !(*this == o); }
};

// ============================================================================
// ParamDescriptor – metadata about a declared parameter
// ============================================================================

struct ParamDescriptor {
    std::string  name;
    ParamType    type     = ParamType::UNKNOWN;
    ParamValue   defaultValue;
    bool         hasDefault = false;

    ParamDescriptor() = default;
    ParamDescriptor(std::string n, ParamType t, ParamValue def)
        : name(std::move(n)), type(t), defaultValue(std::move(def)), hasDefault(true) {}
};

// ============================================================================
// ParamChangeEvent – delivered to change listeners
// ============================================================================

struct ParamChangeEvent {
    std::string  name;
    ParamValue   oldValue;
    ParamValue   newValue;
};

using ParamChangeCallback = std::function<void(const ParamChangeEvent&)>;

// ============================================================================
// ParameterStore
// ============================================================================

/**
 * @brief Thread-safe runtime parameter store
 *
 * Shared (via shared_ptr) between the Runtime, Interpreter, and child
 * workers so that @param references always read the most recent value
 * regardless of which thread set it.
 *
 * Change listeners are called on the thread that calls set(), so they
 * must be short / non-blocking.  Use an on_params pipeline in VSP for
 * heavy work – the interpreter schedules those on the loop thread.
 */
class ParameterStore {
public:
    ParameterStore() = default;

    // =========================================================================
    // Schema registration (called by interpreter on params [] declaration)
    // =========================================================================

    /** Declare a parameter with type and optional default */
    void declare(const std::string& name, ParamType type,
                 const ParamValue& defaultValue);

    /** Check whether a parameter was declared via params [] */
    bool isDeclared(const std::string& name) const;

    /** Get all declared descriptors */
    std::vector<ParamDescriptor> descriptors() const;

    // =========================================================================
    // Read / write
    // =========================================================================

    /**
     * @brief Set a parameter value.
     * @param name   Parameter name
     * @param value  New value
     * @throws std::invalid_argument if name was declared with a different type
     *         and strict mode is on (currently always coerces silently).
     */
    void set(const std::string& name, ParamValue value);

    /**
     * @brief Set from a raw string (wire-protocol form).
     *        Uses the declared type hint (if any) for parsing.
     */
    void setFromString(const std::string& name, const std::string& raw);

    /**
     * @brief Get a parameter value.
     * @return Empty ParamValue if not set.
     */
    ParamValue get(const std::string& name) const;

    /** Check whether a parameter has been set (or has a default) */
    bool has(const std::string& name) const;

    /**
     * @brief List all parameters as name -> value map.
     */
    std::unordered_map<std::string, ParamValue> list() const;

    // =========================================================================
    // Change listeners
    // =========================================================================

    /**
     * @brief Subscribe to changes on a specific parameter.
     * @return Subscription ID (pass to unsubscribe())
     */
    uint64_t subscribe(const std::string& name, ParamChangeCallback cb);

    /**
     * @brief Subscribe to ALL parameter changes.
     */
    uint64_t subscribeAll(ParamChangeCallback cb);

    /**
     * @brief Remove a subscription.
     */
    void unsubscribe(uint64_t id);

    // =========================================================================
    // Serialization helpers (for TCP protocol)
    // =========================================================================

    /** Serialize all params as a JSON object string */
    std::string toJson() const;

    /** Serialize a single param as JSON */
    std::string paramToJson(const std::string& name) const;

private:
    mutable std::shared_mutex _mutex;

    std::unordered_map<std::string, ParamDescriptor> _descriptors; // declared schema
    std::unordered_map<std::string, ParamValue>       _values;     // current values

    // Listeners
    struct Listener {
        uint64_t          id;
        std::string       name;  // empty = all-params listener
        ParamChangeCallback cb;
    };
    std::vector<Listener> _listeners;
    uint64_t              _nextListenerId = 1;
    mutable std::mutex    _listenerMutex;

    void notifyListeners(const ParamChangeEvent& evt);
};

} // namespace visionpipe

#endif // VISIONPIPE_PARAM_STORE_H

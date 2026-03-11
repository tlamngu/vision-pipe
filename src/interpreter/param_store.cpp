#include "interpreter/param_store.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>

namespace visionpipe {

// ============================================================================
// ParamType helpers
// ============================================================================

std::string paramTypeToString(ParamType t) {
    switch (t) {
        case ParamType::INT:    return "int";
        case ParamType::FLOAT:  return "float";
        case ParamType::STRING: return "string";
        case ParamType::BOOL:   return "bool";
        default:                return "unknown";
    }
}

ParamType paramTypeFromString(const std::string& s) {
    if (s == "int"   || s == "integer") return ParamType::INT;
    if (s == "float" || s == "double")  return ParamType::FLOAT;
    if (s == "string"|| s == "str")     return ParamType::STRING;
    if (s == "bool"  || s == "boolean") return ParamType::BOOL;
    return ParamType::UNKNOWN;
}

// ============================================================================
// ParamValue
// ============================================================================

int64_t ParamValue::asInt() const {
    if (std::holds_alternative<int64_t>(storage)) return std::get<int64_t>(storage);
    if (std::holds_alternative<double>(storage))  return static_cast<int64_t>(std::get<double>(storage));
    if (std::holds_alternative<bool>(storage))    return std::get<bool>(storage) ? 1 : 0;
    if (std::holds_alternative<std::string>(storage)) {
        try { return std::stoll(std::get<std::string>(storage)); } catch (...) {}
    }
    return 0;
}

double ParamValue::asFloat() const {
    if (std::holds_alternative<double>(storage))  return std::get<double>(storage);
    if (std::holds_alternative<int64_t>(storage)) return static_cast<double>(std::get<int64_t>(storage));
    if (std::holds_alternative<bool>(storage))    return std::get<bool>(storage) ? 1.0 : 0.0;
    if (std::holds_alternative<std::string>(storage)) {
        try { return std::stod(std::get<std::string>(storage)); } catch (...) {}
    }
    return 0.0;
}

std::string ParamValue::asString() const {
    if (std::holds_alternative<std::string>(storage)) return std::get<std::string>(storage);
    if (std::holds_alternative<int64_t>(storage))  return std::to_string(std::get<int64_t>(storage));
    if (std::holds_alternative<double>(storage))   return std::to_string(std::get<double>(storage));
    if (std::holds_alternative<bool>(storage))     return std::get<bool>(storage) ? "true" : "false";
    return "";
}

bool ParamValue::asBool() const {
    if (std::holds_alternative<bool>(storage))    return std::get<bool>(storage);
    if (std::holds_alternative<int64_t>(storage)) return std::get<int64_t>(storage) != 0;
    if (std::holds_alternative<double>(storage))  return std::get<double>(storage) != 0.0;
    if (std::holds_alternative<std::string>(storage)) {
        const auto& s = std::get<std::string>(storage);
        return s == "true" || s == "1" || s == "yes";
    }
    return false;
}

std::string ParamValue::toWireString() const {
    if (std::holds_alternative<std::monostate>(storage)) return "null";
    if (std::holds_alternative<std::string>(storage)) {
        // JSON-escape the string
        const auto& s = std::get<std::string>(storage);
        std::string out = "\"";
        for (char c : s) {
            if (c == '"')  out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else           out += c;
        }
        out += "\"";
        return out;
    }
    if (std::holds_alternative<bool>(storage))    return std::get<bool>(storage) ? "true" : "false";
    if (std::holds_alternative<int64_t>(storage)) return std::to_string(std::get<int64_t>(storage));
    if (std::holds_alternative<double>(storage)) {
        std::ostringstream ss;
        ss << std::get<double>(storage);
        return ss.str();
    }
    return "null";
}

ParamValue ParamValue::fromWireString(const std::string& s, ParamType hint) {
    if (s == "null") return ParamValue{};

    // Explicit type conversion based on hint
    if (hint == ParamType::BOOL) {
        bool v = (s == "true" || s == "1" || s == "yes");
        return ParamValue(v);
    }
    if (hint == ParamType::INT) {
        try { return ParamValue((int64_t)std::stoll(s)); } catch (...) {}
        return ParamValue((int64_t)0);
    }
    if (hint == ParamType::FLOAT) {
        try { return ParamValue(std::stod(s)); } catch (...) {}
        return ParamValue(0.0);
    }
    if (hint == ParamType::STRING) {
        // Strip surrounding quotes if present
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return ParamValue(s.substr(1, s.size() - 2));
        }
        return ParamValue(s);
    }

    // Auto-detect
    if (s == "true")  return ParamValue(true);
    if (s == "false") return ParamValue(false);

    // Try int
    try {
        size_t pos;
        int64_t iv = std::stoll(s, &pos);
        if (pos == s.size()) return ParamValue(iv);
    } catch (...) {}

    // Try float
    try {
        size_t pos;
        double dv = std::stod(s, &pos);
        if (pos == s.size()) return ParamValue(dv);
    } catch (...) {}

    // Strip quotes for strings from wire
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return ParamValue(s.substr(1, s.size() - 2));
    }

    return ParamValue(s);  // treat as string
}

// ============================================================================
// ParameterStore
// ============================================================================

void ParameterStore::declare(const std::string& name, ParamType type,
                              const ParamValue& defaultValue) {
    std::unique_lock lock(_mutex);
    ParamDescriptor desc;
    desc.name         = name;
    desc.type         = type;
    desc.defaultValue = defaultValue;
    desc.hasDefault   = true;
    _descriptors[name] = desc;

    // Set default if not yet set
    if (_values.find(name) == _values.end()) {
        _values[name] = defaultValue;
    } else {
        // Value already exists (e.g. from --param override set before the
        // params [] block was parsed).  Coerce it to the declared type so
        // that e.g. "60" (string from CLI) becomes 60 (int) when the script
        // declares brightness:int.
        ParamValue& existing = _values[name];
        if (existing.type != type) {
            switch (type) {
                case ParamType::INT:    existing = ParamValue(existing.asInt());    break;
                case ParamType::FLOAT:  existing = ParamValue(existing.asFloat());  break;
                case ParamType::STRING: existing = ParamValue(existing.asString()); break;
                case ParamType::BOOL:   existing = ParamValue(existing.asBool());   break;
                default: break;
            }
        }
    }

    // Bump the generation counter so that any interpreter's param cache is
    // invalidated and reloaded on the next @param access.  Without this,
    // refreshParamCache() sees gen==0 == _paramCacheGen==0 and skips the
    // load, leaving the cache empty and every @param returning void.
    uint64_t newGen = _gen.fetch_add(1, std::memory_order_release) + 1;

    if (_verbose) {
        std::cerr << "[PARAM DEBUG] declare: name='" << name
                  << "' type=" << paramTypeToString(type)
                  << " default=" << defaultValue.toWireString()
                  << " gen=" << newGen << "\n";
    }
}

bool ParameterStore::isDeclared(const std::string& name) const {
    std::shared_lock lock(_mutex);
    return _descriptors.count(name) > 0;
}

std::vector<ParamDescriptor> ParameterStore::descriptors() const {
    std::shared_lock lock(_mutex);
    std::vector<ParamDescriptor> result;
    result.reserve(_descriptors.size());
    for (const auto& [_, d] : _descriptors) {
        result.push_back(d);
    }
    return result;
}

void ParameterStore::set(const std::string& name, ParamValue value) {
    ParamChangeEvent evt;
    evt.name = name;

    {
        std::unique_lock lock(_mutex);

        // Coerce type if descriptor says so
        auto dit = _descriptors.find(name);
        if (dit != _descriptors.end()) {
            switch (dit->second.type) {
                case ParamType::INT:
                    value = ParamValue(value.asInt());
                    break;
                case ParamType::FLOAT:
                    value = ParamValue(value.asFloat());
                    break;
                case ParamType::STRING:
                    value = ParamValue(value.asString());
                    break;
                case ParamType::BOOL:
                    value = ParamValue(value.asBool());
                    break;
                default:
                    break;
            }
        }

        auto it = _values.find(name);
        if (it != _values.end()) {
            evt.oldValue = it->second;
            it->second   = value;
        } else {
            _values[name] = value;
        }
        evt.newValue = value;
    }

    uint64_t newGen = _gen.fetch_add(1, std::memory_order_release) + 1;
    if (_verbose) {
        std::cerr << "[PARAM DEBUG] set: name='" << evt.name
                  << "' value=" << evt.newValue.toWireString()
                  << " gen=" << newGen << "\n";
    }
    notifyListeners(evt);
}

void ParameterStore::setFromString(const std::string& name, const std::string& raw) {
    ParamType hint = ParamType::UNKNOWN;
    {
        std::shared_lock lock(_mutex);
        auto dit = _descriptors.find(name);
        if (dit != _descriptors.end()) hint = dit->second.type;
    }
    set(name, ParamValue::fromWireString(raw, hint));
}

ParamValue ParameterStore::get(const std::string& name) const {
    std::shared_lock lock(_mutex);
    auto it = _values.find(name);
    if (it != _values.end()) return it->second;
    return ParamValue{};
}

bool ParameterStore::has(const std::string& name) const {
    std::shared_lock lock(_mutex);
    return _values.count(name) > 0;
}

std::unordered_map<std::string, ParamValue> ParameterStore::list() const {
    std::shared_lock lock(_mutex);
    return _values;
}

uint64_t ParameterStore::subscribe(const std::string& name, ParamChangeCallback cb) {
    std::lock_guard<std::mutex> lock(_listenerMutex);
    uint64_t id = _nextListenerId++;
    _listeners.push_back({id, name, std::move(cb)});
    return id;
}

uint64_t ParameterStore::subscribeAll(ParamChangeCallback cb) {
    return subscribe("", std::move(cb));
}

void ParameterStore::unsubscribe(uint64_t id) {
    std::lock_guard<std::mutex> lock(_listenerMutex);
    _listeners.erase(
        std::remove_if(_listeners.begin(), _listeners.end(),
                       [id](const Listener& l){ return l.id == id; }),
        _listeners.end());
}

void ParameterStore::notifyListeners(const ParamChangeEvent& evt) {
    // Fast path: skip lock + copy when no listeners are registered
    {
        std::lock_guard<std::mutex> lock(_listenerMutex);
        if (_listeners.empty()) return;
    }
    std::vector<Listener> snapshot;
    {
        std::lock_guard<std::mutex> lock(_listenerMutex);
        snapshot = _listeners;
    }
    for (const auto& listener : snapshot) {
        if (listener.name.empty() || listener.name == evt.name) {
            listener.cb(evt);
        }
    }
}

std::string ParameterStore::toJson() const {
    auto params = list();
    std::ostringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& [name, val] : params) {
        if (!first) ss << ",";
        ss << "\"" << name << "\":" << val.toWireString();
        first = false;
    }
    ss << "}";
    return ss.str();
}

std::string ParameterStore::paramToJson(const std::string& name) const {
    ParamValue v = get(name);
    std::ostringstream ss;
    ss << "{\"name\":\"" << name << "\",\"value\":" << v.toWireString() << "}";
    return ss.str();
}

} // namespace visionpipe

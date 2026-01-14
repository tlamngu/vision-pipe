#include "interpreter/item_registry.h"
#include "interpreter/tensor_types.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace visionpipe {

// ============================================================================
// BaseType utilities
// ============================================================================

std::string baseTypeToString(BaseType type) {
    switch (type) {
        case BaseType::VOID: return "void";
        case BaseType::INT: return "int";
        case BaseType::FLOAT: return "float";
        case BaseType::STRING: return "string";
        case BaseType::BOOL: return "bool";
        case BaseType::MAT: return "mat";
        case BaseType::VECTOR: return "vector";
        case BaseType::MATRIX: return "matrix";
        case BaseType::ARRAY: return "array";
        case BaseType::CACHE_REF: return "cache_ref";
        case BaseType::ANY: return "any";
    }
    return "unknown";
}

BaseType stringToBaseType(const std::string& str) {
    if (str == "void") return BaseType::VOID;
    if (str == "int") return BaseType::INT;
    if (str == "float") return BaseType::FLOAT;
    if (str == "string") return BaseType::STRING;
    if (str == "bool") return BaseType::BOOL;
    if (str == "mat") return BaseType::MAT;
    if (str == "vector") return BaseType::VECTOR;
    if (str == "matrix") return BaseType::MATRIX;
    if (str == "array") return BaseType::ARRAY;
    if (str == "cache_ref") return BaseType::CACHE_REF;
    if (str == "any") return BaseType::ANY;
    return BaseType::ANY;
}

// ============================================================================
// RuntimeValue
// ============================================================================

int64_t RuntimeValue::asInt() const {
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    }
    if (std::holds_alternative<double>(value)) {
        return static_cast<int64_t>(std::get<double>(value));
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to int");
}

double RuntimeValue::asFloat() const {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    }
    if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to float");
}

double RuntimeValue::asNumber() const {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    }
    if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to number");
}

std::string RuntimeValue::asString() const {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    // Convert other types to string
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to string");
}

bool RuntimeValue::asBool() const {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    }
    // Truthiness for other types
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value) != 0;
    }
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value) != 0.0;
    }
    if (std::holds_alternative<std::string>(value)) {
        return !std::get<std::string>(value).empty();
    }
    if (std::holds_alternative<cv::Mat>(value)) {
        return !std::get<cv::Mat>(value).empty();
    }
    return false;
}

cv::Mat RuntimeValue::asMat() const {
    if (std::holds_alternative<cv::Mat>(value)) {
        return std::get<cv::Mat>(value);
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to mat");
}

const std::vector<RuntimeValue>& RuntimeValue::asArray() const {
    if (std::holds_alternative<std::vector<RuntimeValue>>(value)) {
        return std::get<std::vector<RuntimeValue>>(value);
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to array");
}

RuntimeValue RuntimeValue::toInt() const {
    return RuntimeValue(asInt());
}

RuntimeValue RuntimeValue::toFloat() const {
    return RuntimeValue(asFloat());
}

RuntimeValue RuntimeValue::toString() const {
    return RuntimeValue(asString());
}

RuntimeValue RuntimeValue::toBool() const {
    return RuntimeValue(asBool());
}

// ============================================================================
// Vector/Matrix support
// ============================================================================

RuntimeValue RuntimeValue::fromVector(const Vector& v) {
    RuntimeValue rv;
    rv.type = BaseType::VECTOR;
    rv.value = std::make_shared<Vector>(v);
    return rv;
}

RuntimeValue RuntimeValue::fromMatrix(const Matrix& m) {
    RuntimeValue rv;
    rv.type = BaseType::MATRIX;
    rv.value = std::make_shared<Matrix>(m);
    return rv;
}

Vector RuntimeValue::asVector() const {
    if (type == BaseType::VECTOR) {
        if (std::holds_alternative<std::shared_ptr<void>>(value)) {
            auto ptr = std::get<std::shared_ptr<void>>(value);
            return *std::static_pointer_cast<Vector>(ptr);
        }
    }
    // Try to convert from other types
    if (type == BaseType::ARRAY) {
        const auto& arr = asArray();
        std::vector<double> values;
        values.reserve(arr.size());
        for (const auto& elem : arr) {
            if (elem.isNumeric()) {
                values.push_back(elem.asNumber());
            }
        }
        return Vector(values);
    }
    if (type == BaseType::MAT) {
        return Vector(asMat());
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to vector");
}

Matrix RuntimeValue::asMatrix() const {
    if (type == BaseType::MATRIX) {
        if (std::holds_alternative<std::shared_ptr<void>>(value)) {
            auto ptr = std::get<std::shared_ptr<void>>(value);
            return *std::static_pointer_cast<Matrix>(ptr);
        }
    }
    // Try to convert from Mat
    if (type == BaseType::MAT) {
        return Matrix(asMat());
    }
    // Try to convert from 2D array
    if (type == BaseType::ARRAY) {
        const auto& arr = asArray();
        if (!arr.empty() && arr[0].isArray()) {
            std::vector<std::vector<double>> values;
            for (const auto& row : arr) {
                const auto& rowArr = row.asArray();
                std::vector<double> rowData;
                for (const auto& elem : rowArr) {
                    if (elem.isNumeric()) {
                        rowData.push_back(elem.asNumber());
                    }
                }
                values.push_back(rowData);
            }
            return Matrix(values);
        }
    }
    throw std::runtime_error("Cannot convert " + typeString() + " to matrix");
}

RuntimeValue RuntimeValue::toVector() const {
    return fromVector(asVector());
}

RuntimeValue RuntimeValue::toMatrix() const {
    return fromMatrix(asMatrix());
}

std::string RuntimeValue::debugString() const {
    std::ostringstream oss;
    oss << "RuntimeValue(" << typeString() << ": ";
    
    if (std::holds_alternative<std::monostate>(value)) {
        oss << "void";
    } else if (std::holds_alternative<int64_t>(value)) {
        oss << std::get<int64_t>(value);
    } else if (std::holds_alternative<double>(value)) {
        oss << std::get<double>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        oss << "\"" << std::get<std::string>(value) << "\"";
    } else if (std::holds_alternative<bool>(value)) {
        oss << (std::get<bool>(value) ? "true" : "false");
    } else if (std::holds_alternative<cv::Mat>(value)) {
        const auto& mat = std::get<cv::Mat>(value);
        oss << "Mat(" << mat.cols << "x" << mat.rows << ", type=" << mat.type() << ")";
    } else if (std::holds_alternative<std::vector<RuntimeValue>>(value)) {
        const auto& arr = std::get<std::vector<RuntimeValue>>(value);
        oss << "Array[" << arr.size() << "]";
    } else if (std::holds_alternative<std::shared_ptr<void>>(value)) {
        if (type == BaseType::VECTOR) {
            auto ptr = std::static_pointer_cast<Vector>(std::get<std::shared_ptr<void>>(value));
            oss << ptr->toString();
        } else if (type == BaseType::MATRIX) {
            auto ptr = std::static_pointer_cast<Matrix>(std::get<std::shared_ptr<void>>(value));
            oss << ptr->toString();
        } else {
            oss << "shared_ptr";
        }
    }
    
    oss << ")";
    return oss.str();
}

// ============================================================================
// InterpreterItem
// ============================================================================

std::optional<std::string> InterpreterItem::validateArgs(const std::vector<RuntimeValue>& args) const {
    size_t requiredCount = 0;
    for (const auto& param : _params) {
        if (!param.isOptional) {
            ++requiredCount;
        }
    }
    
    if (args.size() < requiredCount) {
        return "Expected at least " + std::to_string(requiredCount) + 
               " arguments, got " + std::to_string(args.size());
    }
    
    if (args.size() > _params.size()) {
        return "Too many arguments: expected at most " + std::to_string(_params.size()) + 
               ", got " + std::to_string(args.size());
    }
    
    // Type checking
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& param = _params[i];
        const auto& arg = args[i];
        
        if (param.type != BaseType::ANY) {
            bool typeMatch = false;
            switch (param.type) {
                case BaseType::INT:
                    typeMatch = arg.isInt() || arg.isFloat();
                    break;
                case BaseType::FLOAT:
                    typeMatch = arg.isNumeric();
                    break;
                case BaseType::STRING:
                    typeMatch = arg.isString();
                    break;
                case BaseType::BOOL:
                    typeMatch = arg.isBool();
                    break;
                case BaseType::MAT:
                    typeMatch = arg.isMat();
                    break;
                case BaseType::ARRAY:
                    typeMatch = arg.isArray();
                    break;
                default:
                    typeMatch = true;
            }
            
            if (!typeMatch) {
                return "Argument '" + param.name + "' expected " + 
                       baseTypeToString(param.type) + ", got " + arg.typeString();
            }
        }
    }
    
    return std::nullopt;
}

std::string InterpreterItem::generateDocumentation() const {
    std::ostringstream oss;
    
    oss << "## " << _functionName << "\n\n";
    oss << _description << "\n\n";
    
    oss << "**Category:** " << _category << "\n\n";
    
    if (!_params.empty()) {
        oss << "### Parameters\n\n";
        oss << "| Name | Type | Required | Description |\n";
        oss << "|------|------|----------|-------------|\n";
        
        for (const auto& param : _params) {
            oss << "| " << param.name 
                << " | " << baseTypeToString(param.type)
                << " | " << (param.isOptional ? "No" : "Yes")
                << " | " << param.description;
            if (param.defaultValue.has_value()) {
                oss << " (default: " << param.defaultValue->debugString() << ")";
            }
            oss << " |\n";
        }
        oss << "\n";
    }
    
    oss << "**Returns:** " << _returnType << "\n\n";
    
    if (!_example.empty()) {
        oss << "### Example\n\n```vsp\n" << _example << "\n```\n\n";
    }
    
    if (!_tags.empty()) {
        oss << "**Tags:** ";
        for (size_t i = 0; i < _tags.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << _tags[i];
        }
        oss << "\n";
    }
    
    return oss.str();
}

std::string InterpreterItem::generateJsonSchema() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"name\": \"" << _functionName << "\",\n";
    oss << "  \"description\": \"" << _description << "\",\n";
    oss << "  \"category\": \"" << _category << "\",\n";
    oss << "  \"returnType\": \"" << _returnType << "\",\n";
    oss << "  \"parameters\": [\n";
    
    for (size_t i = 0; i < _params.size(); ++i) {
        const auto& param = _params[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << param.name << "\",\n";
        oss << "      \"type\": \"" << baseTypeToString(param.type) << "\",\n";
        oss << "      \"required\": " << (param.isOptional ? "false" : "true") << ",\n";
        oss << "      \"description\": \"" << param.description << "\"";
        if (param.defaultValue.has_value()) {
            oss << ",\n      \"default\": \"" << param.defaultValue->asString() << "\"";
        }
        oss << "\n    }";
        if (i < _params.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "  ],\n";
    oss << "  \"example\": \"" << _example << "\",\n";
    oss << "  \"tags\": [";
    for (size_t i = 0; i < _tags.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << _tags[i] << "\"";
    }
    oss << "]\n";
    oss << "}";
    
    return oss.str();
}

// ============================================================================
// ItemRegistry
// ============================================================================

ItemRegistry::ItemRegistry() {}

void ItemRegistry::registerItem(const std::string& name, ItemFactory factory) {
    _factories[name] = factory;
    // Create instance immediately for lookup
    _items[name] = factory();
}

void ItemRegistry::registerItem(std::shared_ptr<InterpreterItem> item) {
    if (item) {
        _items[item->functionName()] = item;
    }
}

std::shared_ptr<InterpreterItem> ItemRegistry::getItem(const std::string& name) const {
    auto it = _items.find(name);
    if (it != _items.end()) {
        return it->second;
    }
    
    // Try factory
    auto factoryIt = _factories.find(name);
    if (factoryIt != _factories.end()) {
        return factoryIt->second();
    }
    
    return nullptr;
}

bool ItemRegistry::hasItem(const std::string& name) const {
    return _items.count(name) > 0 || _factories.count(name) > 0;
}

std::vector<std::string> ItemRegistry::getItemNames() const {
    std::vector<std::string> names;
    names.reserve(_items.size());
    
    for (const auto& [name, _] : _items) {
        names.push_back(name);
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::shared_ptr<InterpreterItem>> ItemRegistry::getItemsByCategory(const std::string& category) const {
    std::vector<std::shared_ptr<InterpreterItem>> result;
    
    for (const auto& [_, item] : _items) {
        if (item->category() == category) {
            result.push_back(item);
        }
    }
    
    return result;
}

std::vector<std::string> ItemRegistry::getCategories() const {
    std::vector<std::string> categories;
    
    for (const auto& [_, item] : _items) {
        const auto& cat = item->category();
        if (std::find(categories.begin(), categories.end(), cat) == categories.end()) {
            categories.push_back(cat);
        }
    }
    
    std::sort(categories.begin(), categories.end());
    return categories;
}

std::vector<std::shared_ptr<InterpreterItem>> ItemRegistry::getItemsByTag(const std::string& tag) const {
    std::vector<std::shared_ptr<InterpreterItem>> result;
    
    for (const auto& [_, item] : _items) {
        const auto& tags = item->tags();
        if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
            result.push_back(item);
        }
    }
    
    return result;
}

std::string ItemRegistry::generateDocumentation() const {
    std::ostringstream oss;
    
    oss << "# VisionPipe Script Reference\n\n";
    oss << "This document lists all available functions in the VisionPipe interpreter.\n\n";
    
    // Group by category
    auto categories = getCategories();
    
    for (const auto& category : categories) {
        oss << "# " << category << "\n\n";
        
        auto items = getItemsByCategory(category);
        std::sort(items.begin(), items.end(), 
            [](const auto& a, const auto& b) { return a->functionName() < b->functionName(); });
        
        for (const auto& item : items) {
            oss << item->generateDocumentation() << "\n---\n\n";
        }
    }
    
    return oss.str();
}

std::string ItemRegistry::generateJsonSchema() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"version\": \"1.0.0\",\n";
    oss << "  \"items\": [\n";
    
    auto names = getItemNames();
    for (size_t i = 0; i < names.size(); ++i) {
        auto item = getItem(names[i]);
        if (item) {
            oss << "    " << item->generateJsonSchema();
            if (i < names.size() - 1) oss << ",";
            oss << "\n";
        }
    }
    
    oss << "  ]\n";
    oss << "}";
    
    return oss.str();
}

void ItemRegistry::clear() {
    _items.clear();
    _factories.clear();
}

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry registry;
    return registry;
}

} // namespace visionpipe

#include "interpreter/ast.h"
#include <sstream>
#include <algorithm>
#include <functional>

namespace visionpipe {

// ============================================================================
// ValueType utilities
// ============================================================================

std::string valueTypeToString(ValueType type) {
    switch (type) {
        case ValueType::VOID: return "void";
        case ValueType::INTEGER: return "int";
        case ValueType::FLOAT: return "float";
        case ValueType::STRING: return "string";
        case ValueType::BOOLEAN: return "bool";
        case ValueType::MAT: return "mat";
        case ValueType::ARRAY: return "array";
        case ValueType::OBJECT: return "object";
        case ValueType::CACHE_REF: return "cache_ref";
        case ValueType::UNKNOWN: return "unknown";
    }
    return "unknown";
}

// ============================================================================
// Expression toString implementations
// ============================================================================

std::string LiteralExpr::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "Literal(";
    
    if (std::holds_alternative<double>(value)) {
        oss << std::get<double>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        oss << "\"" << std::get<std::string>(value) << "\"";
    } else if (std::holds_alternative<bool>(value)) {
        oss << (std::get<bool>(value) ? "true" : "false");
    }
    
    oss << ")";
    return oss.str();
}

std::string IdentifierExpr::toString(int ind) const {
    return indent(ind) + "Identifier(" + name + ")";
}

std::string FunctionCallExpr::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "FunctionCall(" << functionName;
    
    if (!arguments.empty()) {
        oss << ", args=[";
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << arguments[i]->toString(0);
        }
        oss << "]";
    }
    
    if (cacheOutput.has_value()) {
        oss << " -> \"" << cacheOutput->cacheId << "\"";
        if (cacheOutput->isGlobal) {
            oss << " [global]";
        }
    }
    
    oss << ")";
    return oss.str();
}

std::string BinaryExpr::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "BinaryExpr(\n";
    oss << left->toString(ind + 1) << "\n";
    oss << indent(ind + 1) << Token::typeToString(op) << "\n";
    oss << right->toString(ind + 1) << "\n";
    oss << indent(ind) << ")";
    return oss.str();
}

std::string UnaryExpr::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "UnaryExpr(" << Token::typeToString(op) << ", ";
    oss << operand->toString(0) << ")";
    return oss.str();
}

std::string CacheAccessExpr::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "CacheAccess(\"" << cacheId << "\"";
    if (isGlobal) {
        oss << " [global]";
    }
    oss << ")";
    return oss.str();
}

std::string ArrayExpr::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "Array([";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << elements[i]->toString(0);
    }
    oss << "])";
    return oss.str();
}

std::string CacheLoadExpr::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind);
    if (isDynamic) {
        oss << cacheId;
    } else {
        oss << "\"" << cacheId << "\"";
    }
    if (isGlobal) oss << " global";
    oss << " -> " << targetCall->toString(0);
    return oss.str();
}

// ============================================================================
// Statement toString implementations
// ============================================================================

std::string ExpressionStmt::toString(int ind) const {
    return indent(ind) + "ExprStmt(" + expression->toString(0) + ")";
}

std::string ExecSeqStmt::toString(int ind) const {
    return indent(ind) + "exec_seq " + pipelineRef->toString(0);
}

std::string ExecMultiStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "exec_multi [\n";
    for (const auto& ref : pipelineRefs) {
        oss << ref->toString(ind + 1) << "\n";
    }
    oss << indent(ind) << "]";
    return oss.str();
}

std::string ExecLoopStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "exec_loop " << pipelineRef->toString(0);
    if (condition.has_value()) {
        oss << " while " << (*condition)->toString(0);
    }
    return oss.str();
}

std::string UseStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "use(\"" << cacheId << "\")";
    if (isGlobal) {
        oss << " [global]";
    }
    return oss.str();
}

std::string CacheStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "cache(\"" << cacheId << "\", " << value->toString(0) << ")";
    if (isGlobal) {
        oss << " [global]";
    }
    return oss.str();
}

std::string IfStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "if " << condition->toString(0) << " {\n";
    for (const auto& stmt : thenBranch) {
        oss << stmt->toString(ind + 1) << "\n";
    }
    if (!elseBranch.empty()) {
        oss << indent(ind) << "} else {\n";
        for (const auto& stmt : elseBranch) {
            oss << stmt->toString(ind + 1) << "\n";
        }
    }
    oss << indent(ind) << "}";
    return oss.str();
}

std::string WhileStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "while " << condition->toString(0) << " {\n";
    for (const auto& stmt : body) {
        oss << stmt->toString(ind + 1) << "\n";
    }
    oss << indent(ind) << "}";
    return oss.str();
}

std::string ReturnStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "return";
    if (value.has_value()) {
        oss << " " << (*value)->toString(0);
    }
    return oss.str();
}

std::string BreakStmt::toString(int ind) const {
    return indent(ind) + "break";
}

std::string ContinueStmt::toString(int ind) const {
    return indent(ind) + "continue";
}

std::string ImportStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "import \"" << path << "\"";
    if (alias.has_value()) {
        oss << " as " << *alias;
    }
    return oss.str();
}

std::string GlobalStmt::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "global " << cacheId;
    if (initialValue.has_value()) {
        oss << " = " << (*initialValue)->toString(0);
    }
    return oss.str();
}

// ============================================================================
// Declaration toString implementations
// ============================================================================

std::string PipelineDecl::toString(int ind) const {
    std::ostringstream oss;
    
    if (!docComment.empty()) {
        oss << indent(ind) << "## " << docComment << "\n";
    }
    
    oss << indent(ind) << "pipeline " << name;
    
    if (!parameters.empty()) {
        oss << "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) oss << ", ";
            const auto& param = parameters[i];
            if (param.isLetBinding) oss << "let ";
            if (param.isGlobal) oss << "global ";
            oss << param.name;
            if (param.typeHint.has_value()) {
                oss << ": " << valueTypeToString(*param.typeHint);
            }
        }
        oss << ")";
    }
    
    if (cacheOutput.has_value()) {
        oss << " -> \"" << cacheOutput->cacheId << "\"";
    }
    
    oss << "\n";
    
    for (const auto& stmt : body) {
        oss << stmt->toString(ind + 1) << "\n";
    }
    
    oss << indent(ind) << "end";
    return oss.str();
}

std::string ConfigDecl::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "config " << name << "\n";
    for (const auto& [key, value] : settings) {
        oss << indent(ind + 1) << key << " = " << value->toString(0) << "\n";
    }
    oss << indent(ind) << "end";
    return oss.str();
}

std::string Program::toString(int ind) const {
    std::ostringstream oss;
    oss << indent(ind) << "Program(" << sourceFile << ") {\n";
    
    if (!imports.empty()) {
        oss << indent(ind + 1) << "// Imports\n";
        for (const auto& import : imports) {
            oss << import->toString(ind + 1) << "\n";
        }
    }
    
    if (!globals.empty()) {
        oss << indent(ind + 1) << "// Globals\n";
        for (const auto& global : globals) {
            oss << global->toString(ind + 1) << "\n";
        }
    }
    
    if (!configs.empty()) {
        oss << indent(ind + 1) << "// Configs\n";
        for (const auto& config : configs) {
            oss << config->toString(ind + 1) << "\n";
        }
    }
    
    if (!pipelines.empty()) {
        oss << indent(ind + 1) << "// Pipelines\n";
        for (const auto& pipeline : pipelines) {
            oss << pipeline->toString(ind + 1) << "\n\n";
        }
    }
    
    if (!topLevelStatements.empty()) {
        oss << indent(ind + 1) << "// Top-level statements\n";
        for (const auto& stmt : topLevelStatements) {
            oss << stmt->toString(ind + 1) << "\n";
        }
    }
    
    oss << indent(ind) << "}";
    return oss.str();
}

std::shared_ptr<PipelineDecl> Program::findPipeline(const std::string& name) const {
    for (const auto& pipeline : pipelines) {
        if (pipeline->name == name) {
            return pipeline;
        }
    }
    return nullptr;
}

std::vector<std::string> Program::getPipelineNames() const {
    std::vector<std::string> names;
    names.reserve(pipelines.size());
    for (const auto& pipeline : pipelines) {
        names.push_back(pipeline->name);
    }
    return names;
}

std::vector<Program::CacheDependency> Program::analyzeCacheDependencies() const {
    std::unordered_map<std::string, CacheDependency> deps;
    
    // Helper to extract cache references from expressions
    std::function<void(const std::shared_ptr<Expression>&, const std::string&, bool)> 
        analyzeExpr = [&](const std::shared_ptr<Expression>& expr, 
                          const std::string& currentPipeline, 
                          bool isWrite) {
        if (!expr) return;
        
        switch (expr->nodeType) {
            case ASTNodeType::FUNCTION_CALL_EXPR: {
                auto* call = static_cast<FunctionCallExpr*>(expr.get());
                
                // Check cache output
                if (call->cacheOutput.has_value()) {
                    const auto& cacheId = call->cacheOutput->cacheId;
                    auto& dep = deps[cacheId];
                    dep.cacheId = cacheId;
                    dep.isGlobal = call->cacheOutput->isGlobal;
                    dep.producerPipeline = currentPipeline;
                }
                
                // Analyze arguments
                for (const auto& arg : call->arguments) {
                    analyzeExpr(arg, currentPipeline, false);
                }
                break;
            }
            case ASTNodeType::CACHE_ACCESS_EXPR: {
                auto* access = static_cast<CacheAccessExpr*>(expr.get());
                auto& dep = deps[access->cacheId];
                dep.cacheId = access->cacheId;
                dep.isGlobal = access->isGlobal;
                if (std::find(dep.consumerPipelines.begin(), 
                             dep.consumerPipelines.end(), 
                             currentPipeline) == dep.consumerPipelines.end()) {
                    dep.consumerPipelines.push_back(currentPipeline);
                }
                break;
            }
            default:
                break;
        }
    };
    
    // Analyze all pipelines
    for (const auto& pipeline : pipelines) {
        // Pipeline-level cache output
        if (pipeline->cacheOutput.has_value()) {
            const auto& cacheId = pipeline->cacheOutput->cacheId;
            auto& dep = deps[cacheId];
            dep.cacheId = cacheId;
            dep.isGlobal = pipeline->cacheOutput->isGlobal;
            dep.producerPipeline = pipeline->name;
        }
        
        // Analyze statements
        for (const auto& stmt : pipeline->body) {
            switch (stmt->nodeType) {
                case ASTNodeType::EXPRESSION_STMT: {
                    auto* exprStmt = static_cast<ExpressionStmt*>(stmt.get());
                    analyzeExpr(exprStmt->expression, pipeline->name, false);
                    break;
                }
                case ASTNodeType::USE_STMT: {
                    auto* useStmt = static_cast<UseStmt*>(stmt.get());
                    auto& dep = deps[useStmt->cacheId];
                    dep.cacheId = useStmt->cacheId;
                    dep.isGlobal = useStmt->isGlobal;
                    if (std::find(dep.consumerPipelines.begin(), 
                                 dep.consumerPipelines.end(), 
                                 pipeline->name) == dep.consumerPipelines.end()) {
                        dep.consumerPipelines.push_back(pipeline->name);
                    }
                    break;
                }
                case ASTNodeType::CACHE_STMT: {
                    auto* cacheStmt = static_cast<CacheStmt*>(stmt.get());
                    auto& dep = deps[cacheStmt->cacheId];
                    dep.cacheId = cacheStmt->cacheId;
                    dep.isGlobal = cacheStmt->isGlobal;
                    dep.producerPipeline = pipeline->name;
                    break;
                }
                default:
                    break;
            }
        }
    }
    
    // Convert to vector
    std::vector<CacheDependency> result;
    result.reserve(deps.size());
    for (auto& [_, dep] : deps) {
        result.push_back(std::move(dep));
    }
    return result;
}

} // namespace visionpipe

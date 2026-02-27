#include "interpreter/interpreter.h"
#include "interpreter/parser.h"
#include "pipeline/pipeline.h"
#include "pipeline/pipeline_threaded_group.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

namespace visionpipe {

// ============================================================================
// Constructor / Destructor
// ============================================================================

Interpreter::Interpreter() : Interpreter(InterpreterConfig{}) {}

Interpreter::Interpreter(InterpreterConfig config) 
    : _config(std::move(config)) {
    // Initialize with global scope
    _scopes.push_back(Scope{});
    
    // Initialize execution context
    _context.cacheManager = &_cacheManager;
    _context.verbose = _config.verbose;
}

Interpreter::~Interpreter() = default;

void Interpreter::setConfig(const InterpreterConfig& config) {
    _config = config;
    _context.verbose = _config.verbose;
}

// ============================================================================
// Item registration
// ============================================================================

void Interpreter::add(std::shared_ptr<InterpreterItem> item) {
    _registry.add(item);
}

// ============================================================================
// Program execution
// ============================================================================

void Interpreter::executeFile(const std::string& filename) {
    try {
        auto program = parseFile(filename);
        execute(program);
    } catch (const std::exception& e) {
        _hasError = true;
        _lastError = std::string("Error loading file: ") + e.what();
        throw;
    }
}

void Interpreter::execute(std::shared_ptr<Program> program) {
    if (!program) {
        throw std::runtime_error("Cannot execute null program");
    }
    
    _loadedPrograms.push_back(program);
    executeProgram(program);
}

RuntimeValue Interpreter::evaluate(std::shared_ptr<Expression> expr) {
    if (!expr) {
        return RuntimeValue();
    }
    return evalExpression(expr.get());
}

void Interpreter::executeStatement(std::shared_ptr<Statement> stmt) {
    if (!stmt) return;
    
    switch (stmt->nodeType) {
        case ASTNodeType::EXPRESSION_STMT:
            execExpressionStmt(static_cast<ExpressionStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_SEQ_STMT:
            execExecSeq(static_cast<ExecSeqStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_MULTI_STMT:
            execExecMulti(static_cast<ExecMultiStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_LOOP_STMT:
            execExecLoop(static_cast<ExecLoopStmt*>(stmt.get()));
            break;
        case ASTNodeType::USE_STMT:
            execUse(static_cast<UseStmt*>(stmt.get()));
            break;
        case ASTNodeType::CACHE_STMT:
            execCache(static_cast<CacheStmt*>(stmt.get()));
            break;
        case ASTNodeType::GLOBAL_STMT:
            execGlobalPromote(static_cast<GlobalStmt*>(stmt.get()));
            break;
        case ASTNodeType::IF_STMT:
            execIf(static_cast<IfStmt*>(stmt.get()));
            break;
        case ASTNodeType::WHILE_STMT:
            execWhile(static_cast<WhileStmt*>(stmt.get()));
            break;
        case ASTNodeType::RETURN_STMT:
            execReturn(static_cast<ReturnStmt*>(stmt.get()));
            break;
        case ASTNodeType::BREAK_STMT:
            execBreak(static_cast<BreakStmt*>(stmt.get()));
            break;
        case ASTNodeType::CONTINUE_STMT:
            execContinue(static_cast<ContinueStmt*>(stmt.get()));
            break;
        default:
            throw std::runtime_error("Unknown statement type");
    }
}

// ============================================================================
// Pipeline management
// ============================================================================

std::shared_ptr<PipelineDecl> Interpreter::getPipeline(const std::string& name) const {
    auto it = _pipelines.find(name);
    if (it != _pipelines.end()) {
        return it->second;
    }
    return nullptr;
}

bool Interpreter::hasPipeline(const std::string& name) const {
    return _pipelines.count(name) > 0;
}

std::vector<std::string> Interpreter::getPipelineNames() const {
    std::vector<std::string> names;
    names.reserve(_pipelines.size());
    for (const auto& [name, _] : _pipelines) {
        names.push_back(name);
    }
    return names;
}

cv::Mat Interpreter::executePipeline(const std::string& name, 
                                      const std::vector<RuntimeValue>& args,
                                      const cv::Mat& input) {
    auto pipeline = getPipeline(name);
    if (!pipeline) {
        throw std::runtime_error("Pipeline not found: " + name);
    }
    
    return executePipelineDecl(pipeline.get(), args, input);
}

// ============================================================================
// Variable management
// ============================================================================

void Interpreter::setVariable(const std::string& name, const RuntimeValue& value) {
    if (_scopes.empty()) {
        _scopes.push_back(Scope{});
    }
    _scopes.front().variables[name] = value;  // Global scope
}

RuntimeValue Interpreter::getVariable(const std::string& name) const {
    // Search from innermost to outermost scope
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
        auto varIt = it->variables.find(name);
        if (varIt != it->variables.end()) {
            return varIt->second;
        }
    }
    return RuntimeValue();
}

bool Interpreter::hasVariable(const std::string& name) const {
    for (const auto& scope : _scopes) {
        if (scope.variables.count(name) > 0) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// State management
// ============================================================================

void Interpreter::reset() {
    _cacheManager.clearGlobal();
    _pipelines.clear();
    _loadedPrograms.clear();
    _scopes.clear();
    _scopes.push_back(Scope{});
    _loopRunning = false;
    _stopRequested = false;
    _hasError = false;
    _lastError.clear();
    _recursionDepth = 0;
    _framesProcessed = 0;
    _context.reset();
}

// ============================================================================
// Internal execution methods
// ============================================================================

void Interpreter::executeProgram(std::shared_ptr<Program> program) {
    // Load imports first
    loadImports(program);
    
    // Register all pipelines
    registerPipelines(program);
    
    // Execute top-level statements
    executeTopLevel(program);
}

void Interpreter::loadImports(std::shared_ptr<Program> program) {
    for (const auto& import : program->imports) {
        std::string path = import->path;
        
        // Resolve relative path
        if (path[0] != '/' && path[1] != ':') {
            path = _config.workingDirectory + "/" + path;
        }
        
        // Load imported file
        try {
            auto importedProgram = parseFile(path);
            
            // Recursively process imports
            loadImports(importedProgram);
            registerPipelines(importedProgram);
            
            _loadedPrograms.push_back(importedProgram);
        } catch (const std::exception& e) {
            reportError("Failed to import '" + import->path + "': " + e.what(), 
                       import->location);
        }
    }
}

void Interpreter::registerPipelines(std::shared_ptr<Program> program) {
    for (const auto& pipeline : program->pipelines) {
        _pipelines[pipeline->name] = pipeline;
    }
    
    // Register global variables
    for (const auto& global : program->globals) {
        RuntimeValue value;
        if (global->initialValue.has_value()) {
            value = evaluate(*global->initialValue);
        }
        setVariable(global->cacheId, value);
    }
}

void Interpreter::executeTopLevel(std::shared_ptr<Program> program) {
    for (const auto& stmt : program->topLevelStatements) {
        executeStatement(stmt);
        
        if (_context.shouldReturn || _stopRequested) {
            break;
        }
    }
}

// ============================================================================
// Statement execution
// ============================================================================

void Interpreter::execExpressionStmt(ExpressionStmt* stmt) {
    RuntimeValue result = evalExpression(stmt->expression.get());
    
    // If result is a Mat, update context
    if (result.isMat()) {
        _context.currentMat = result.asMat();
    }
}

void Interpreter::execExecSeq(ExecSeqStmt* stmt) {
    // Evaluate pipeline reference
    if (auto* call = dynamic_cast<FunctionCallExpr*>(stmt->pipelineRef.get())) {
        // It's a pipeline call with arguments
        std::vector<RuntimeValue> args;
        for (const auto& arg : call->arguments) {
            args.push_back(evalExpression(arg.get()));
        }
        executePipeline(call->functionName, args, _context.currentMat);
    } else if (auto* ident = dynamic_cast<IdentifierExpr*>(stmt->pipelineRef.get())) {
        // It's a simple pipeline reference
        executePipeline(ident->name, {}, _context.currentMat);
    }
}

void Interpreter::execExecMulti(ExecMultiStmt* stmt) {
    // Execute pipelines in parallel using threads
    std::vector<std::thread> threads;
    std::vector<cv::Mat> results(stmt->pipelineRefs.size());
    
    for (size_t i = 0; i < stmt->pipelineRefs.size(); ++i) {
        auto& ref = stmt->pipelineRefs[i];
        
        threads.emplace_back([this, &ref, &results, i]() {
            try {
                if (auto* call = dynamic_cast<FunctionCallExpr*>(ref.get())) {
                    std::vector<RuntimeValue> args;
                    for (const auto& arg : call->arguments) {
                        args.push_back(evalExpression(arg.get()));
                    }
                    results[i] = executePipeline(call->functionName, args, _context.currentMat);
                } else if (auto* ident = dynamic_cast<IdentifierExpr*>(ref.get())) {
                    results[i] = executePipeline(ident->name, {}, _context.currentMat);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in parallel pipeline: " << e.what() << std::endl;
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void Interpreter::execExecLoop(ExecLoopStmt* stmt) {
    _loopRunning = true;
    
    while (!_stopRequested) {
        // Check condition if present
        if (stmt->condition.has_value()) {
            RuntimeValue condResult = evalExpression(stmt->condition->get());
            if (!condResult.asBool()) {
                break;
            }
        }
        
        // Execute pipeline
        if (auto* call = dynamic_cast<FunctionCallExpr*>(stmt->pipelineRef.get())) {
            std::vector<RuntimeValue> args;
            for (const auto& arg : call->arguments) {
                args.push_back(evalExpression(arg.get()));
            }
            executePipeline(call->functionName, args, _context.currentMat);
        } else if (auto* ident = dynamic_cast<IdentifierExpr*>(stmt->pipelineRef.get())) {
            executePipeline(ident->name, {}, _context.currentMat);
        }
        
        // Increment frame counter after each loop iteration (only if enabled)
        if (_config.fpsCounting) {
            ++_framesProcessed;
        }
        
        if (_context.shouldBreak) {
            _context.shouldBreak = false;
            break;
        }
        
        if (_context.shouldReturn) {
            break;
        }
    }
    
    _loopRunning = false;
}

void Interpreter::execUse(UseStmt* stmt) {
    cv::Mat mat;
    
    if (stmt->isGlobal) {
        mat = _cacheManager.getGlobal(stmt->cacheId);
    } else {
        mat = _cacheManager.get(stmt->cacheId);
    }
    
    if (mat.empty() && _config.strictMode) {
        reportError("Cache entry not found: " + stmt->cacheId, stmt->location);
    }
    
    _context.currentMat = mat;
    
    // Handle optional cache output: use("a") -> "b"
    if (stmt->cacheOutput.has_value()) {
        const auto& output = stmt->cacheOutput.value();
        _cacheManager.set(output.cacheId, mat.clone(), output.isGlobal);
    }
}

void Interpreter::execCache(CacheStmt* stmt) {
    // If no value expression provided, cache the current mat
    if (!stmt->value) {
        _cacheManager.set(stmt->cacheId, _context.currentMat.clone(), stmt->isGlobal);
        return;
    }
    
    RuntimeValue value = evalExpression(stmt->value.get());
    
    if (value.isMat()) {
        _cacheManager.set(stmt->cacheId, value.asMat(), stmt->isGlobal);
    } else {
        // Store as variable instead
        if (stmt->isGlobal) {
            setVariable(stmt->cacheId, value);
        } else {
            setLocalVariable(stmt->cacheId, value);
        }
    }
}

void Interpreter::execGlobalPromote(GlobalStmt* stmt) {
    // Promote a local cache entry to global
    cv::Mat mat = _cacheManager.get(stmt->cacheId);
    if (!mat.empty()) {
        _cacheManager.set(stmt->cacheId, mat.clone(), true);  // true = global
    } else if (_config.strictMode) {
        reportError("Cache entry not found for global promotion: " + stmt->cacheId, stmt->location);
    }
}

void Interpreter::execIf(IfStmt* stmt) {
    RuntimeValue condition = evalExpression(stmt->condition.get());
    
    if (condition.asBool()) {
        for (const auto& s : stmt->thenBranch) {
            executeStatement(s);
            if (_context.shouldBreak || _context.shouldContinue || _context.shouldReturn) {
                break;
            }
        }
    } else if (!stmt->elseBranch.empty()) {
        for (const auto& s : stmt->elseBranch) {
            executeStatement(s);
            if (_context.shouldBreak || _context.shouldContinue || _context.shouldReturn) {
                break;
            }
        }
    }
}

void Interpreter::execWhile(WhileStmt* stmt) {
    while (evalExpression(stmt->condition.get()).asBool()) {
        for (const auto& s : stmt->body) {
            executeStatement(s);
            
            if (_context.shouldBreak) {
                _context.shouldBreak = false;
                return;
            }
            
            if (_context.shouldContinue) {
                _context.shouldContinue = false;
                break;
            }
            
            if (_context.shouldReturn) {
                return;
            }
        }
    }
}

void Interpreter::execReturn(ReturnStmt* stmt) {
    if (stmt->value.has_value()) {
        _context.returnValue = evalExpression(stmt->value->get());
    }
    _context.shouldReturn = true;
}

void Interpreter::execBreak(BreakStmt* stmt) {
    (void)stmt;
    _context.shouldBreak = true;
}

void Interpreter::execContinue(ContinueStmt* stmt) {
    (void)stmt;
    _context.shouldContinue = true;
}

// ============================================================================
// Expression evaluation
// ============================================================================

RuntimeValue Interpreter::evalExpression(Expression* expr) {
    if (!expr) {
        return RuntimeValue();
    }
    
    switch (expr->nodeType) {
        case ASTNodeType::LITERAL_EXPR:
            return evalLiteral(static_cast<LiteralExpr*>(expr));
        case ASTNodeType::IDENTIFIER_EXPR:
            return evalIdentifier(static_cast<IdentifierExpr*>(expr));
        case ASTNodeType::FUNCTION_CALL_EXPR:
            return evalFunctionCall(static_cast<FunctionCallExpr*>(expr));
        case ASTNodeType::BINARY_EXPR:
            return evalBinary(static_cast<BinaryExpr*>(expr));
        case ASTNodeType::UNARY_EXPR:
            return evalUnary(static_cast<UnaryExpr*>(expr));
        case ASTNodeType::CACHE_ACCESS_EXPR:
            return evalCacheAccess(static_cast<CacheAccessExpr*>(expr));
        case ASTNodeType::CACHE_LOAD_EXPR:
            return evalCacheLoad(static_cast<CacheLoadExpr*>(expr));
        case ASTNodeType::ARRAY_EXPR:
            return evalArray(static_cast<ArrayExpr*>(expr));
        default:
            throw std::runtime_error("Unknown expression type");
    }
}

RuntimeValue Interpreter::evalLiteral(LiteralExpr* expr) {
    if (std::holds_alternative<double>(expr->value)) {
        return RuntimeValue(std::get<double>(expr->value));
    } else if (std::holds_alternative<std::string>(expr->value)) {
        return RuntimeValue(std::get<std::string>(expr->value));
    } else if (std::holds_alternative<bool>(expr->value)) {
        return RuntimeValue(std::get<bool>(expr->value));
    }
    return RuntimeValue();
}

RuntimeValue Interpreter::evalIdentifier(IdentifierExpr* expr) {
    // First check local variables (from scopes)
    if (hasLocalVariable(expr->name)) {
        return getLocalVariable(expr->name);
    }
    
    // Then check global variables (from scopes)
    if (hasVariable(expr->name)) {
        return getVariable(expr->name);
    }
    
    // Check context variables (set by items like trackbar_value)
    auto it = _context.variables.find(expr->name);
    if (it != _context.variables.end()) {
        return it->second;
    }
    
    // NO FALLBACK: Report error if variable is not found
    reportError("Variable not found: " + expr->name, expr->location);
    return RuntimeValue();
}

RuntimeValue Interpreter::evalFunctionCall(FunctionCallExpr* expr) {
    // Check if it's a pipeline call
    if (hasPipeline(expr->functionName)) {
        std::vector<RuntimeValue> args;
        for (const auto& arg : expr->arguments) {
            args.push_back(evalExpression(arg.get()));
        }
        
        cv::Mat result = executePipeline(expr->functionName, args, _context.currentMat);
        
        // Handle cache output
        if (expr->cacheOutput.has_value()) {
            _cacheManager.set(expr->cacheOutput->cacheId, result, expr->cacheOutput->isGlobal);
        }
        
        return RuntimeValue(result);
    }
    
    // Check if it's a registered item
    auto item = _registry.getItem(expr->functionName);
    if (item) {
        // Evaluate arguments
        std::vector<RuntimeValue> args;
        for (const auto& arg : expr->arguments) {
            args.push_back(evalExpression(arg.get()));
        }
        
        // Validate arguments
        auto validationError = item->validateArgs(args);
        if (validationError.has_value()) {
            reportError(*validationError, expr->location);
            return RuntimeValue();
        }
        
        // Execute item
        ExecutionResult result = item->execute(args, _context);
        
        if (!result.success) {
            reportError(result.error.value_or("Unknown error"), expr->location);
            return RuntimeValue();
        }
        
        // Handle break signal from item
        if (result.shouldBreak) {
            _context.shouldBreak = true;
        }
        
        // Handle cache output
        if (expr->cacheOutput.has_value()) {
            _cacheManager.set(expr->cacheOutput->cacheId, result.outputMat, 
                            expr->cacheOutput->isGlobal);
        }
        
        // Update context mat
        if (!result.outputMat.empty()) {
            _context.currentMat = result.outputMat;
        }
        
        // Return scalar value if available, otherwise return Mat
        if (result.scalarValue.has_value()) {
            return result.scalarValue.value();
        }
        
        return RuntimeValue(result.outputMat);
    }
    
    reportError("Unknown function: " + expr->functionName, expr->location);
    return RuntimeValue();
}

RuntimeValue Interpreter::evalBinary(BinaryExpr* expr) {
    // Special handling for assignment to avoid evaluating left side as r-value
    if (expr->op == TokenType::OP_ASSIGN) {
        RuntimeValue right = evalExpression(expr->right.get());
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr->left.get())) {
            setLocalVariable(ident->name, right);
            return right;
        }
        reportError("Left side of assignment must be an identifier", expr->location);
        return RuntimeValue();
    }

    RuntimeValue left = evalExpression(expr->left.get());
    RuntimeValue right = evalExpression(expr->right.get());
    
    switch (expr->op) {
        // Arithmetic
        case TokenType::OP_PLUS:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() + right.asNumber());
            }
            if (left.isString() || right.isString()) {
                return RuntimeValue(left.asString() + right.asString());
            }
            break;
            
        case TokenType::OP_MINUS:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() - right.asNumber());
            }
            break;
            
        case TokenType::OP_MULTIPLY:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() * right.asNumber());
            }
            break;
            
        case TokenType::OP_DIVIDE:
            if (left.isNumeric() && right.isNumeric()) {
                double r = right.asNumber();
                if (r == 0) {
                    reportError("Division by zero", expr->location);
                    return RuntimeValue();
                }
                return RuntimeValue(left.asNumber() / r);
            }
            break;
            
        case TokenType::OP_MODULO:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(static_cast<int64_t>(left.asNumber()) % 
                                   static_cast<int64_t>(right.asNumber()));
            }
            break;
        
        // Comparison
        case TokenType::OP_EQ:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() == right.asNumber());
            }
            if (left.isString() && right.isString()) {
                return RuntimeValue(left.asString() == right.asString());
            }
            if (left.isBool() && right.isBool()) {
                return RuntimeValue(left.asBool() == right.asBool());
            }
            break;
            
        case TokenType::OP_NE:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() != right.asNumber());
            }
            if (left.isString() && right.isString()) {
                return RuntimeValue(left.asString() != right.asString());
            }
            break;
            
        case TokenType::OP_LT:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() < right.asNumber());
            }
            break;
            
        case TokenType::OP_LE:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() <= right.asNumber());
            }
            break;
            
        case TokenType::OP_GT:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() > right.asNumber());
            }
            break;
            
        case TokenType::OP_GE:
            if (left.isNumeric() && right.isNumeric()) {
                return RuntimeValue(left.asNumber() >= right.asNumber());
            }
            break;
        
        // Logical
        case TokenType::OP_AND:
            return RuntimeValue(left.asBool() && right.asBool());
            
        case TokenType::OP_OR:
            return RuntimeValue(left.asBool() || right.asBool());
            
        default:
            break;
    }
    
    reportError("Invalid binary operation", expr->location);
    return RuntimeValue();
}

RuntimeValue Interpreter::evalUnary(UnaryExpr* expr) {
    RuntimeValue operand = evalExpression(expr->operand.get());
    
    switch (expr->op) {
        case TokenType::OP_MINUS:
            if (operand.isNumeric()) {
                return RuntimeValue(-operand.asNumber());
            }
            break;
            
        case TokenType::OP_NOT:
            return RuntimeValue(!operand.asBool());
            
        default:
            break;
    }
    
    reportError("Invalid unary operation", expr->location);
    return RuntimeValue();
}

RuntimeValue Interpreter::evalCacheAccess(CacheAccessExpr* expr) {
    cv::Mat mat;
    
    if (expr->isGlobal) {
        mat = _cacheManager.getGlobal(expr->cacheId);
    } else {
        mat = _cacheManager.get(expr->cacheId);
    }
    
    return RuntimeValue(mat);
}

RuntimeValue Interpreter::evalCacheLoad(CacheLoadExpr* expr) {
    // Resolve cache ID (may be dynamic from variable)
    std::string cacheId = expr->cacheId;
    if (expr->isDynamic) {
        // Look up the variable to get the actual cache ID
        RuntimeValue varValue = getLocalVariable(cacheId);
        if (varValue.isVoid()) {
            varValue = getVariable(cacheId);
        }
        if (varValue.isString()) {
            cacheId = varValue.asString();
        }
    }
    
    // Load the cached Mat
    cv::Mat cachedMat;
    if (expr->isGlobal) {
        cachedMat = _cacheManager.getGlobal(cacheId);
    } else {
        cachedMat = _cacheManager.get(cacheId);
    }
    
    if (cachedMat.empty()) {
        throw std::runtime_error("Cache miss: '" + cacheId + "' not found");
    }
    
    // Save the current Mat and set the cached Mat as current
    cv::Mat previousMat = _context.currentMat;
    _context.currentMat = cachedMat;
    
    // Execute the target function call
    RuntimeValue result = evalFunctionCall(expr->targetCall.get());
    
    // The target function may have modified currentMat, that's expected
    // Return the result
    return result;
}

RuntimeValue Interpreter::evalArray(ArrayExpr* expr) {
    std::vector<RuntimeValue> elements;
    elements.reserve(expr->elements.size());
    
    for (const auto& elem : expr->elements) {
        elements.push_back(evalExpression(elem.get()));
    }
    
    return RuntimeValue(std::move(elements));
}

// ============================================================================
// Pipeline execution
// ============================================================================

cv::Mat Interpreter::executePipelineDecl(PipelineDecl* pipeline, 
                                          const std::vector<RuntimeValue>& args,
                                          const cv::Mat& input) {
    checkRecursionLimit();
    ++_recursionDepth;
    
    // Create new scope for pipeline execution
    pushScope();
    _cacheManager.pushScope();
    
    // Bind parameters
    for (size_t i = 0; i < pipeline->parameters.size(); ++i) {
        const auto& param = pipeline->parameters[i];
        RuntimeValue value;
        
        if (i < args.size()) {
            value = args[i];
        } else if (param.defaultValue.has_value()) {
            value = evaluate(*param.defaultValue);
        }
        
        if (param.isGlobal) {
            setVariable(param.name, value);
        } else {
            setLocalVariable(param.name, value);
        }
    }
    
    // Set input mat
    _context.currentMat = input;
    
    // Execute pipeline body
    for (const auto& stmt : pipeline->body) {
        executeStatement(stmt);
        
        if (_context.shouldReturn) {
            _context.shouldReturn = false;
            break;
        }
        
        if (_context.shouldBreak || _context.shouldContinue) {
            break;
        }
    }
    
    cv::Mat result = _context.currentMat;
    
    // Handle pipeline-level cache output
    if (pipeline->cacheOutput.has_value()) {
        _cacheManager.set(pipeline->cacheOutput->cacheId, result, 
                         pipeline->cacheOutput->isGlobal);
    }
    
    // Clean up scope
    _cacheManager.popScope();
    popScope();
    --_recursionDepth;
    
    return result;
}

// ============================================================================
// Scope management
// ============================================================================

void Interpreter::pushScope() {
    _scopes.push_back(Scope{});
}

void Interpreter::popScope() {
    if (_scopes.size() > 1) {
        _scopes.pop_back();
    }
}

void Interpreter::setLocalVariable(const std::string& name, const RuntimeValue& value) {
    if (_scopes.empty()) {
        _scopes.push_back(Scope{});
    }
    _scopes.back().variables[name] = value;
}

RuntimeValue Interpreter::getLocalVariable(const std::string& name) const {
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
        auto varIt = it->variables.find(name);
        if (varIt != it->variables.end()) {
            return varIt->second;
        }
    }
    return RuntimeValue();
}

bool Interpreter::hasLocalVariable(const std::string& name) const {
    for (const auto& scope : _scopes) {
        if (scope.variables.count(name) > 0) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Utility
// ============================================================================

void Interpreter::reportError(const std::string& message, const SourceLocation& loc) {
    _hasError = true;
    _lastError = loc.toString() + ": " + message;
    
    if (_config.strictMode) {
        throw std::runtime_error(_lastError);
    } else {
        std::cerr << "Error: " << _lastError << std::endl;
    }
}

void Interpreter::checkRecursionLimit() {
    if (_recursionDepth >= _config.maxRecursionDepth) {
        throw std::runtime_error("Maximum recursion depth exceeded");
    }
}

} // namespace visionpipe

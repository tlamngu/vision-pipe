#ifndef VISIONPIPE_INTERPRETER_H
#define VISIONPIPE_INTERPRETER_H

#include "interpreter/ast.h"
#include "interpreter/item_registry.h"
#include "interpreter/cache_manager.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <stack>

namespace visionpipe {

// Forward declarations
class Pipeline;
class PipelineThreadedGroup;

/**
 * @brief Interpreter configuration
 */
struct InterpreterConfig {
    bool debugMode = false;
    bool strictMode = false;
    bool verbose = false;  // Enable verbose DNN/performance output
    std::string workingDirectory = ".";
    size_t maxRecursionDepth = 100;
    bool enableOptimization = true;
    bool fpsCounting = false;  // Enable frame counting (disabled by default for long-running pipelines)
};

/**
 * @brief Interpreter for VisionPipe Script
 * 
 * The interpreter executes parsed AST from .vsp files.
 * It manages:
 * - Pipeline definitions and execution
 * - Function calls to registered items
 * - Variable bindings and scope
 * - Cache management
 */
class Interpreter {
public:
    Interpreter();
    explicit Interpreter(InterpreterConfig config);
    ~Interpreter();
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /**
     * @brief Set interpreter configuration
     */
    void setConfig(const InterpreterConfig& config);
    
    /**
     * @brief Get current configuration
     */
    const InterpreterConfig& config() const { return _config; }
    
    // =========================================================================
    // Item registration
    // =========================================================================
    
    /**
     * @brief Add an interpreter item
     */
    void add(std::shared_ptr<InterpreterItem> item);
    
    /**
     * @brief Add an item using its type
     */
    template<typename T>
    void add() {
        _registry.add<T>();
    }
    
    /**
     * @brief Get the item registry
     */
    ItemRegistry& registry() { return _registry; }
    const ItemRegistry& registry() const { return _registry; }
    
    // =========================================================================
    // Program execution
    // =========================================================================
    
    /**
     * @brief Load and execute a .vsp file
     * @param filename Path to the .vsp file
     */
    void executeFile(const std::string& filename);
    
    /**
     * @brief Execute a parsed program
     */
    void execute(std::shared_ptr<Program> program);
    
    /**
     * @brief Execute a single expression and return result
     */
    RuntimeValue evaluate(std::shared_ptr<Expression> expr);
    
    /**
     * @brief Execute a statement
     */
    void executeStatement(std::shared_ptr<Statement> stmt);
    
    // =========================================================================
    // Pipeline management
    // =========================================================================
    
    /**
     * @brief Get a defined pipeline by name
     */
    std::shared_ptr<PipelineDecl> getPipeline(const std::string& name) const;
    
    /**
     * @brief Check if a pipeline exists
     */
    bool hasPipeline(const std::string& name) const;
    
    /**
     * @brief Get all pipeline names
     */
    std::vector<std::string> getPipelineNames() const;
    
    /**
     * @brief Execute a pipeline by name with arguments
     */
    cv::Mat executePipeline(const std::string& name, 
                            const std::vector<RuntimeValue>& args = {},
                            const cv::Mat& input = cv::Mat());
    
    // =========================================================================
    // Cache access
    // =========================================================================
    
    /**
     * @brief Get the cache manager
     */
    CacheManager& cache() { return _cacheManager; }
    const CacheManager& cache() const { return _cacheManager; }
    
    // =========================================================================
    // Variable management
    // =========================================================================
    
    /**
     * @brief Set a global variable
     */
    void setVariable(const std::string& name, const RuntimeValue& value);
    
    /**
     * @brief Get a variable
     */
    RuntimeValue getVariable(const std::string& name) const;
    
    /**
     * @brief Check if a variable exists
     */
    bool hasVariable(const std::string& name) const;
    
    // =========================================================================
    // State management
    // =========================================================================
    
    /**
     * @brief Reset the interpreter state (clears cache, variables, etc.)
     */
    void reset();
    
    /**
     * @brief Check if the main loop should continue
     */
    bool shouldContinueLoop() const { return _loopRunning && !_stopRequested; }
    
    /**
     * @brief Request stop of the main loop
     */
    void requestStop() { _stopRequested = true; }
    
    /**
     * @brief Check if there was an error
     */
    bool hasError() const { return _hasError; }
    
    /**
     * @brief Get last error message
     */
    const std::string& lastError() const { return _lastError; }
    
    /**
     * @brief Get number of frames processed in exec_loop
     */
    uint64_t framesProcessed() const { return _framesProcessed; }
    
private:
    InterpreterConfig _config;
    ItemRegistry _registry;
    CacheManager _cacheManager;
    ExecutionContext _context;
    
    // Loaded programs and pipelines
    std::vector<std::shared_ptr<Program>> _loadedPrograms;
    std::unordered_map<std::string, std::shared_ptr<PipelineDecl>> _pipelines;
    
    // Variable scope stack
    struct Scope {
        std::unordered_map<std::string, RuntimeValue> variables;
    };
    std::vector<Scope> _scopes;
    
    // Execution state
    bool _loopRunning = false;
    bool _stopRequested = false;
    bool _hasError = false;
    std::string _lastError;
    size_t _recursionDepth = 0;
    uint64_t _framesProcessed = 0;
    
    // =========================================================================
    // Internal execution methods
    // =========================================================================
    
    void executeProgram(std::shared_ptr<Program> program);
    void loadImports(std::shared_ptr<Program> program);
    void registerPipelines(std::shared_ptr<Program> program);
    void executeTopLevel(std::shared_ptr<Program> program);
    
    // Statement execution
    void execExpressionStmt(ExpressionStmt* stmt);
    void execExecSeq(ExecSeqStmt* stmt);
    void execExecMulti(ExecMultiStmt* stmt);
    void execExecLoop(ExecLoopStmt* stmt);
    void execUse(UseStmt* stmt);
    void execCache(CacheStmt* stmt);
    void execGlobalPromote(GlobalStmt* stmt);
    void execIf(IfStmt* stmt);
    void execWhile(WhileStmt* stmt);
    void execReturn(ReturnStmt* stmt);
    void execBreak(BreakStmt* stmt);
    void execContinue(ContinueStmt* stmt);
    
    // Expression evaluation
    RuntimeValue evalExpression(Expression* expr);
    RuntimeValue evalLiteral(LiteralExpr* expr);
    RuntimeValue evalIdentifier(IdentifierExpr* expr);
    RuntimeValue evalFunctionCall(FunctionCallExpr* expr);
    RuntimeValue evalBinary(BinaryExpr* expr);
    RuntimeValue evalUnary(UnaryExpr* expr);
    RuntimeValue evalCacheAccess(CacheAccessExpr* expr);
    RuntimeValue evalCacheLoad(CacheLoadExpr* expr);
    RuntimeValue evalArray(ArrayExpr* expr);
    
    // Pipeline execution
    cv::Mat executePipelineDecl(PipelineDecl* pipeline, 
                                const std::vector<RuntimeValue>& args,
                                const cv::Mat& input);
    
    // Scope management
    void pushScope();
    void popScope();
    void setLocalVariable(const std::string& name, const RuntimeValue& value);
    RuntimeValue getLocalVariable(const std::string& name) const;
    bool hasLocalVariable(const std::string& name) const;
    
    // Utility
    void reportError(const std::string& message, const SourceLocation& loc);
    void checkRecursionLimit();
};

} // namespace visionpipe

#endif // VISIONPIPE_INTERPRETER_H

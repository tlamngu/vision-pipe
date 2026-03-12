#ifndef VISIONPIPE_AST_H
#define VISIONPIPE_AST_H

#include "interpreter/lexer.h"
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include <unordered_map>

namespace visionpipe {

// Forward declarations
struct ASTNode;
struct Expression;
struct Statement;
struct PipelineDecl;
struct FunctionCall;

// ============================================================================
// AST Value Types
// ============================================================================

/**
 * @brief Base type for VSP values
 */
enum class ValueType {
    VOID,
    INTEGER,
    FLOAT,
    STRING,
    BOOLEAN,
    MAT,        // OpenCV Mat
    ARRAY,
    OBJECT,
    CACHE_REF,  // Reference to a cached value
    UNKNOWN
};

std::string valueTypeToString(ValueType type);

/**
 * @brief Parameter declaration for pipeline functions
 */
struct ParameterDecl {
    std::string name;
    bool isLetBinding;      // declared with 'let'
    bool isGlobal;          // declared with 'global'
    std::optional<ValueType> typeHint;
    std::optional<std::shared_ptr<Expression>> defaultValue;
    SourceLocation location;
    
    ParameterDecl() : isLetBinding(false), isGlobal(false) {}
};

/**
 * @brief Cache output specification (-> "cache_id" or -> variable)
 */
struct CacheOutput {
    std::string cacheId;
    bool isGlobal;
    bool isDynamic;   // true if cacheId comes from a variable
    SourceLocation location;
    
    CacheOutput() : isGlobal(false), isDynamic(false) {}
};

// ============================================================================
// Base AST Node
// ============================================================================

enum class ASTNodeType {
    // Expressions
    LITERAL_EXPR,
    IDENTIFIER_EXPR,
    FUNCTION_CALL_EXPR,
    BINARY_EXPR,
    UNARY_EXPR,
    CACHE_ACCESS_EXPR,
    CACHE_LOAD_EXPR,    // "cache_id" -> item() pattern
    ARRAY_EXPR,
    PARAM_REF_EXPR,     // @param_name – live runtime parameter reference
    
    // Statements
    EXPRESSION_STMT,
    EXEC_SEQ_STMT,
    EXEC_MULTI_STMT,
    EXEC_LOOP_STMT,
    EXEC_INTERVAL_STMT,
    NO_INTERVAL_STMT,
    EXEC_INTERVAL_MULTI_STMT,
    EXEC_RT_SEQ_STMT,
    EXEC_RT_MULTI_STMT,
    EXEC_NASYNC_STMT,
    EXEC_FORK_STMT,
    DEBUG_START_STMT,
    DEBUG_END_STMT,
    USE_STMT,
    CACHE_STMT,
    IF_STMT,
    WHILE_STMT,
    RETURN_STMT,
    BREAK_STMT,
    CONTINUE_STMT,
    IMPORT_STMT,
    GLOBAL_STMT,
    PARAM_DECL_STMT,     // params [ name:type = default, ... ]
    ON_PARAMS_STMT,      // on_params @name ... end
    
    // Declarations
    PIPELINE_DECL,
    CONFIG_DECL,
    
    // Root
    PROGRAM
};

/**
 * @brief Base class for all AST nodes
 */
struct ASTNode {
    ASTNodeType nodeType;
    SourceLocation location;
    
    ASTNode(ASTNodeType type) : nodeType(type) {}
    virtual ~ASTNode() = default;
    
    virtual std::string toString(int indent = 0) const = 0;
    
protected:
    std::string indent(int level) const {
        return std::string(level * 2, ' ');
    }
};

// ============================================================================
// Expressions
// ============================================================================

struct Expression : ASTNode {
    Expression(ASTNodeType type) : ASTNode(type) {}
};

/**
 * @brief Literal value (number, string, boolean)
 */
struct LiteralExpr : Expression {
    std::variant<double, std::string, bool> value;
    ValueType valueType;
    
    LiteralExpr() : Expression(ASTNodeType::LITERAL_EXPR), valueType(ValueType::UNKNOWN) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Identifier reference
 */
struct IdentifierExpr : Expression {
    std::string name;
    
    IdentifierExpr() : Expression(ASTNodeType::IDENTIFIER_EXPR) {}
    explicit IdentifierExpr(const std::string& n) 
        : Expression(ASTNodeType::IDENTIFIER_EXPR), name(n) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Function call expression (e.g., video_cap(0))
 */
struct FunctionCallExpr : Expression {
    std::string functionName;
    std::vector<std::shared_ptr<Expression>> arguments;
    // Named (keyword) arguments: func(param=value)
    // These are ordered as written; resolved to positional in the interpreter.
    std::vector<std::pair<std::string, std::shared_ptr<Expression>>> namedArguments;
    std::optional<CacheOutput> cacheOutput;  // -> "cache_id"
    
    FunctionCallExpr() : Expression(ASTNodeType::FUNCTION_CALL_EXPR) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Binary expression (a + b, a == b, etc.)
 */
struct BinaryExpr : Expression {
    std::shared_ptr<Expression> left;
    TokenType op;
    std::shared_ptr<Expression> right;
    
    BinaryExpr() : Expression(ASTNodeType::BINARY_EXPR) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Unary expression (!a, -a)
 */
struct UnaryExpr : Expression {
    TokenType op;
    std::shared_ptr<Expression> operand;
    
    UnaryExpr() : Expression(ASTNodeType::UNARY_EXPR) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Cache access expression using use()
 */
struct CacheAccessExpr : Expression {
    std::string cacheId;
    bool isGlobal;
    
    CacheAccessExpr() : Expression(ASTNodeType::CACHE_ACCESS_EXPR), isGlobal(false) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Array literal [a, b, c]
 */
struct ArrayExpr : Expression {
    std::vector<std::shared_ptr<Expression>> elements;
    
    ArrayExpr() : Expression(ASTNodeType::ARRAY_EXPR) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Runtime parameter reference (@param_name)
 *
 * Reads the current value from the runtime ParameterStore.  Unlike a
 * variable, this is a live read – every evaluation fetches the latest value.
 */
struct ParamRefExpr : Expression {
    std::string paramName;  ///< Name without the '@' prefix

    ParamRefExpr() : Expression(ASTNodeType::PARAM_REF_EXPR) {}
    explicit ParamRefExpr(const std::string& n)
        : Expression(ASTNodeType::PARAM_REF_EXPR), paramName(n) {}

    std::string toString(int ind = 0) const override;
};

/**
 * @brief Cache load expression: "cache_id" -> item() or cache_var -> item()
 * 
 * Loads cached Mat and pipes it as input to the function call.
 * Different from cache output (item() -> "cache_id") where the result is cached.
 */
struct CacheLoadExpr : Expression {
    std::string cacheId;               // Cache identifier
    bool isDynamic = false;            // true if cacheId is from a variable
    bool isGlobal = false;             // Whether to load from global cache
    std::shared_ptr<FunctionCallExpr> targetCall;  // The function to call with cached Mat
    
    CacheLoadExpr() : Expression(ASTNodeType::CACHE_LOAD_EXPR) {}
    
    std::string toString(int ind = 0) const override;
};

// ============================================================================
// Statements
// ============================================================================

struct Statement : ASTNode {
    Statement(ASTNodeType type) : ASTNode(type) {}
};

/**
 * @brief Expression statement (expression at statement level)
 */
struct ExpressionStmt : Statement {
    std::shared_ptr<Expression> expression;
    
    ExpressionStmt() : Statement(ASTNodeType::EXPRESSION_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_seq statement - execute pipeline sequentially
 */
struct ExecSeqStmt : Statement {
    std::shared_ptr<Expression> pipelineRef;  // Can be identifier or function call
    
    ExecSeqStmt() : Statement(ASTNodeType::EXEC_SEQ_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_multi statement - execute pipelines in parallel
 */
struct ExecMultiStmt : Statement {
    std::vector<std::shared_ptr<Expression>> pipelineRefs;
    
    ExecMultiStmt() : Statement(ASTNodeType::EXEC_MULTI_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_loop statement - execute pipeline in loop
 */
struct ExecLoopStmt : Statement {
    std::shared_ptr<Expression> pipelineRef;
    std::optional<std::shared_ptr<Expression>> condition;  // Optional break condition
    
    ExecLoopStmt() : Statement(ASTNodeType::EXEC_LOOP_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_interval statement - execute pipeline repeatedly on a timer
 *
 * exec_interval <pipeline> <time_ms>
 * Schedules the pipeline to execute every time_ms milliseconds in a
 * background thread.  The interval continues until no_interval is called.
 */
struct ExecIntervalStmt : Statement {
    std::shared_ptr<Expression> pipelineRef;
    std::shared_ptr<Expression> intervalMs;  // float: interval in milliseconds

    ExecIntervalStmt() : Statement(ASTNodeType::EXEC_INTERVAL_STMT) {}

    std::string toString(int ind = 0) const override;
};

/**
 * @brief no_interval statement - stop a recurring pipeline interval
 *
 * no_interval <pipeline>
 */
struct NoIntervalStmt : Statement {
    std::shared_ptr<Expression> pipelineRef;

    NoIntervalStmt() : Statement(ASTNodeType::NO_INTERVAL_STMT) {}

    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_interval_multi statement - run multiple pipelines in parallel on a timer
 *
 * exec_interval_multi [pipeline1, pipeline2, ...] <time_ms>
 */
struct ExecIntervalMultiStmt : Statement {
    std::vector<std::shared_ptr<Expression>> pipelineRefs;
    std::shared_ptr<Expression> intervalMs;

    ExecIntervalMultiStmt() : Statement(ASTNodeType::EXEC_INTERVAL_MULTI_STMT) {}

    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_rt_seq statement - real-time sequential execution with timeout
 *
 * exec_rt_seq <pipeline> <timeout_ms>
 * Executes the pipeline.  If execution exceeds timeout_ms the pipeline is
 * considered late; a warning is emitted and the call returns without its result.
 */
struct ExecRtSeqStmt : Statement {
    std::shared_ptr<Expression> pipelineRef;
    std::shared_ptr<Expression> timeoutMs;

    ExecRtSeqStmt() : Statement(ASTNodeType::EXEC_RT_SEQ_STMT) {}

    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_rt_multi statement - real-time parallel execution with shared timeout
 *
 * exec_rt_multi [pipeline1, pipeline2, ...] <timeout_ms>
 */
struct ExecRtMultiStmt : Statement {
    std::vector<std::shared_ptr<Expression>> pipelineRefs;
    std::shared_ptr<Expression> timeoutMs;

    ExecRtMultiStmt() : Statement(ASTNodeType::EXEC_RT_MULTI_STMT) {}

    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_nasync statement – fire-and-forget async execution
 *
 * Two forms:
 *   exec_nasync <pipeline>          – run named pipeline in background thread
 *   exec_nasync start ... end       – run inline block in background thread
 *
 * In both cases the calling thread continues immediately and the current
 * Mat is UNCHANGED (bypasses through to the next item).  The background
 * thread receives a deep-clone of the Mat and fully private interpreter
 * state; the only shared resource is the global cache (thread-safe).
 */
struct ExecNasyncStmt : Statement {
    /// Named pipeline mode: non-null when a pipeline reference is given.
    std::shared_ptr<Expression> pipelineRef;
    /// Inline block mode: non-empty when exec_nasync start ... end is used.
    std::vector<std::shared_ptr<Statement>> body;

    bool isInlineBlock() const { return static_cast<bool>(body.size()); }

    ExecNasyncStmt() : Statement(ASTNodeType::EXEC_NASYNC_STMT) {}
    std::string toString(int ind = 0) const override;
};

/**
 * @brief exec_fork statement – fork a child process running a pipeline in a loop
 *
 * exec_fork <pipeline>
 * Forks a child process that continuously executes the named pipeline.
 * The parent continues immediately.  Communication between parent and
 * child happens exclusively through POSIX shared memory
 * (shm_write / shm_read items).
 */
struct ExecForkStmt : Statement {
    std::shared_ptr<Expression> pipelineRef;

    ExecForkStmt() : Statement(ASTNodeType::EXEC_FORK_STMT) {}
    std::string toString(int ind = 0) const override {
        return std::string(ind, ' ') + "exec_fork <pipeline>";
    }
};

/**
 * @brief debug_start statement - begin a verbose debug logging block
 */
struct DebugStartStmt : Statement {
    DebugStartStmt() : Statement(ASTNodeType::DEBUG_START_STMT) {}
    std::string toString(int ind = 0) const override;
};

/**
 * @brief debug_end statement - end a verbose debug logging block
 */
struct DebugEndStmt : Statement {
    DebugEndStmt() : Statement(ASTNodeType::DEBUG_END_STMT) {}
    std::string toString(int ind = 0) const override;
};

/**
 * @brief use statement - load from cache
 */
struct UseStmt : Statement {
    std::string cacheId;
    bool isGlobal;
    std::optional<CacheOutput> cacheOutput;  // Optional: use("a") -> "b"
    
    UseStmt() : Statement(ASTNodeType::USE_STMT), isGlobal(false) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief cache statement - explicit cache operation
 */
struct CacheStmt : Statement {
    std::string cacheId;
    std::shared_ptr<Expression> value;
    bool isGlobal;
    
    CacheStmt() : Statement(ASTNodeType::CACHE_STMT), isGlobal(false) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief if statement
 */
struct IfStmt : Statement {
    std::shared_ptr<Expression> condition;
    std::vector<std::shared_ptr<Statement>> thenBranch;
    std::vector<std::shared_ptr<Statement>> elseBranch;
    
    IfStmt() : Statement(ASTNodeType::IF_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief while statement
 */
struct WhileStmt : Statement {
    std::shared_ptr<Expression> condition;
    std::vector<std::shared_ptr<Statement>> body;
    
    WhileStmt() : Statement(ASTNodeType::WHILE_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief return statement
 */
struct ReturnStmt : Statement {
    std::optional<std::shared_ptr<Expression>> value;
    
    ReturnStmt() : Statement(ASTNodeType::RETURN_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief break statement
 */
struct BreakStmt : Statement {
    BreakStmt() : Statement(ASTNodeType::BREAK_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief continue statement
 */
struct ContinueStmt : Statement {
    ContinueStmt() : Statement(ASTNodeType::CONTINUE_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief import statement
 */
struct ImportStmt : Statement {
    std::string path;
    std::optional<std::string> alias;
    
    ImportStmt() : Statement(ASTNodeType::IMPORT_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief global cache declaration
 */
struct GlobalStmt : Statement {
    std::string cacheId;
    std::optional<std::shared_ptr<Expression>> initialValue;
    
    GlobalStmt() : Statement(ASTNodeType::GLOBAL_STMT) {}
    
    std::string toString(int ind = 0) const override;
};

// ============================================================================
// Runtime parameter declarations
// ============================================================================

/**
 * @brief One entry in a `params [...]` declaration
 */
struct ParamEntry {
    std::string                              name;
    std::string                              typeName;   ///< "int", "float", "string", "bool"
    std::optional<std::shared_ptr<Expression>> defaultValue;
    SourceLocation                           location;
};

/**
 * @brief params [ brightness:int=50, gain:float=1.0, ... ]
 *
 * Top-level statement that declares parameters readable from the TCP server.
 */
struct ParamDeclStmt : Statement {
    std::vector<ParamEntry> entries;

    ParamDeclStmt() : Statement(ASTNodeType::PARAM_DECL_STMT) {}
    std::string toString(int ind = 0) const override;
};

/**
 * @brief on_params @name\n  ...body...  end
 *
 * Registers a body of statements to execute whenever the named parameter
 * (or '*' for any parameter) changes.  Runs on the pipeline loop thread.
 */
struct OnParamsStmt : Statement {
    std::string                              paramName;  ///< param name (without @), or "*"
    std::vector<std::shared_ptr<Statement>>  body;

    OnParamsStmt() : Statement(ASTNodeType::ON_PARAMS_STMT) {}
    std::string toString(int ind = 0) const override;
};

// ============================================================================
// Declarations
// ============================================================================

/**
 * @brief Pipeline declaration
 * 
 * pipeline name(params) -> "cache_id"
 *     statements
 * end
 */
struct PipelineDecl : ASTNode {
    std::string name;
    std::vector<ParameterDecl> parameters;
    std::optional<CacheOutput> cacheOutput;  // Pipeline-level cache output
    std::vector<std::shared_ptr<Statement>> body;
    std::string docComment;  // Documentation from ## comments
    
    PipelineDecl() : ASTNode(ASTNodeType::PIPELINE_DECL) {}
    
    std::string toString(int ind = 0) const override;
};

/**
 * @brief Config block declaration
 * 
 * config name
 *     key = value
 * end
 */
struct ConfigDecl : ASTNode {
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<Expression>> settings;
    
    ConfigDecl() : ASTNode(ASTNodeType::CONFIG_DECL) {}
    
    std::string toString(int ind = 0) const override;
};

// ============================================================================
// Program Root
// ============================================================================

/**
 * @brief Root node of the AST representing the entire program
 */
struct Program : ASTNode {
    std::vector<std::shared_ptr<ImportStmt>> imports;
    std::vector<std::shared_ptr<GlobalStmt>> globals;
    std::vector<std::shared_ptr<ConfigDecl>> configs;
    std::vector<std::shared_ptr<PipelineDecl>> pipelines;
    std::vector<std::shared_ptr<ParamDeclStmt>> paramDecls;    ///< params [...] declarations
    std::vector<std::shared_ptr<OnParamsStmt>>  onParamsHandlers; ///< on_params @x ... end
    std::vector<std::shared_ptr<Statement>> topLevelStatements;  // exec_loop etc.
    
    std::string sourceFile;
    
    Program() : ASTNode(ASTNodeType::PROGRAM) {}
    
    std::string toString(int ind = 0) const override;
    
    /**
     * @brief Find a pipeline by name
     */
    std::shared_ptr<PipelineDecl> findPipeline(const std::string& name) const;
    
    /**
     * @brief Get all pipeline names
     */
    std::vector<std::string> getPipelineNames() const;
    
    /**
     * @brief Analyze cache dependencies between pipelines
     */
    struct CacheDependency {
        std::string cacheId;
        bool isGlobal;
        std::string producerPipeline;  // Pipeline that writes to this cache
        std::vector<std::string> consumerPipelines;  // Pipelines that read from this cache
    };
    
    std::vector<CacheDependency> analyzeCacheDependencies() const;
};

// ============================================================================
// AST Visitor Pattern
// ============================================================================

/**
 * @brief Visitor interface for traversing AST
 */
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    // Expressions
    virtual void visit(LiteralExpr& node) = 0;
    virtual void visit(IdentifierExpr& node) = 0;
    virtual void visit(FunctionCallExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(CacheAccessExpr& node) = 0;
    virtual void visit(ArrayExpr& node) = 0;
    
    // Statements
    virtual void visit(ExpressionStmt& node) = 0;
    virtual void visit(ExecSeqStmt& node) = 0;
    virtual void visit(ExecMultiStmt& node) = 0;
    virtual void visit(ExecLoopStmt& node) = 0;
    virtual void visit(ExecIntervalStmt& node) = 0;
    virtual void visit(NoIntervalStmt& node) = 0;
    virtual void visit(ExecIntervalMultiStmt& node) = 0;
    virtual void visit(ExecRtSeqStmt& node) = 0;
    virtual void visit(ExecRtMultiStmt& node) = 0;
    virtual void visit(ExecNasyncStmt& node) = 0;
    virtual void visit(ExecForkStmt& node) = 0;
    virtual void visit(DebugStartStmt& node) = 0;
    virtual void visit(DebugEndStmt& node) = 0;
    virtual void visit(UseStmt& node) = 0;
    virtual void visit(CacheStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(BreakStmt& node) = 0;
    virtual void visit(ContinueStmt& node) = 0;
    virtual void visit(ImportStmt& node) = 0;
    virtual void visit(GlobalStmt& node) = 0;
    virtual void visit(ParamDeclStmt& node) = 0;
    virtual void visit(OnParamsStmt& node) = 0;
    virtual void visit(ParamRefExpr& node) = 0;
    
    // Declarations
    virtual void visit(PipelineDecl& node) = 0;
    virtual void visit(ConfigDecl& node) = 0;
    virtual void visit(Program& node) = 0;
};

} // namespace visionpipe

#endif // VISIONPIPE_AST_H

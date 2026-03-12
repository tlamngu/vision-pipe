#ifndef VISIONPIPE_INTERPRETER_H
#define VISIONPIPE_INTERPRETER_H

#include "interpreter/ast.h"
#include "interpreter/item_registry.h"
#include "interpreter/cache_manager.h"
#include "interpreter/param_store.h"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <functional>
#include <stack>
#include <queue>
#include <condition_variable>
#include <climits>
#include <sys/types.h>  // pid_t

namespace visionpipe {

// Forward declarations
class Pipeline;
class PipelineThreadedGroup;
struct ShmArena;  // defined in utils/shm_zero_copy.h

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

    // ── Debug throughput mode ────────────────────────────────────────────────
    // When enabled, every executePipelineDecl() call is timed.  A background
    // thread prints a summary table at the given interval.
    bool   throughputMode           = false;
    double throughputPrintIntervalSec = 1.0;  ///< How often to refresh the console report

    // ── Latency mode ────────────────────────────────────────────────────────
    // When enabled, tracks per-pipeline latency histograms and prints a
    // latency percentile table (p50 / p95 / p99 / max) alongside (or instead
    // of) the throughput table.  Works for both in-process pipelines and
    // exec_fork child processes (via the shared ShmArena).
    bool   latencyMode              = false;
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
    // Runtime parameter store
    // =========================================================================

    /**
     * @brief Attach a shared ParameterStore.
     *        Must be called before execute() to have effect.
     */
    void setParamStore(std::shared_ptr<ParameterStore> store);

    /**
     * @brief Get the current ParameterStore (may be null).
     */
    std::shared_ptr<ParameterStore> paramStore() const { return _paramStore; }
    
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
    void requestStop();
    
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

    // Runtime parameter store (shared across main + workers)
    std::shared_ptr<ParameterStore> _paramStore;

    // Pending on_params handler bodies to execute on loop thread
    struct ParamHandlerBody {
        std::vector<std::shared_ptr<Statement>> statements;
    };
    std::queue<ParamHandlerBody>  _pendingParamHandlers;
    std::mutex                    _pendingParamMutex;
    std::atomic<bool>             _hasPendingParams{false};  ///< dirty-flag to skip mutex when empty

    // Local param value cache — avoids shared_mutex on every @param read
    uint64_t _paramCacheGen = 0;  ///< last store generation we synced from
    std::unordered_map<std::string, ParamValue> _paramCache;  ///< snapshot
    void refreshParamCache();  ///< re-reads store if generation changed

    // Registered on_params handlers (paramName -> body)
    struct OnParamsHandler {
        std::string                              paramName;  // "*" = all
        std::vector<std::shared_ptr<Statement>>  body;
        uint64_t                                 subscriptionId = 0;
    };
    std::vector<OnParamsHandler> _onParamsHandlers;
    
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
    std::atomic<bool> _stopRequested{false};
    bool _hasError = false;
    std::string _lastError;
    size_t _recursionDepth = 0;
    uint64_t _framesProcessed = 0;

    // =========================================================================
    // exec_multi worker pool
    //
    // Child Interpreter objects are expensive to construct each frame (they
    // copy the full _pipelines and _registry maps).  We keep one pool per
    // exec_multi usage, identified by the number of parallel pipelines.  If
    // the topology stays constant (the common case), workers are initialised
    // once and reused every frame; only their context / local state is reset
    // between iterations.
    //
    // Workers run as persistent threads that sleep on a condition variable
    // between frames, eliminating per-frame thread creation/join overhead.
    // =========================================================================
    struct MultiWorkerSync {
        std::mutex               mtx;
        std::condition_variable  startCv;   ///< main → worker: wake up and run
        std::condition_variable  doneCv;    ///< worker → main: finished this frame
        bool                     hasWork{false};
        bool                     workDone{false};
        bool                     shutdown{false};
        std::string              error;
        std::string              invName;
        std::vector<RuntimeValue> invArgs;
    };
    struct MultiWorker {
        std::unique_ptr<Interpreter> interp;
        std::string pipelineName;   // which pipeline this slot runs
        std::shared_ptr<MultiWorkerSync> sync;
        std::thread thread;         // persistent worker thread
    };
    std::vector<MultiWorker> _multiWorkers;
    // Sentinel: pipeline names at last init; used to detect topology changes.
    std::vector<std::string> _multiWorkerTopology;

    // =========================================================================
    // exec_interval worker pool
    //
    // Each exec_interval spawns a background thread that loops, sleeping for
    // the given interval, then executing the named pipeline.  no_interval
    // sets the stop flag to terminate the thread.
    // =========================================================================
    struct IntervalWorker {
        std::thread workerThread;
        std::atomic<bool> stop{false};
        // Set to true by the worker thread just before it exits.  The
        // destructor uses this to implement a timed join (avoids blocking
        // forever if the pipeline inside the worker is stuck on I/O).
        std::atomic<bool> done{false};
        std::unique_ptr<Interpreter> interp;  // dedicated child interpreter
    };
    // Key: pipeline name (or first pipeline name for multi)
    std::unordered_map<std::string, std::shared_ptr<IntervalWorker>> _intervalWorkers;
    std::mutex _intervalMutex;  // protect _intervalWorkers map

    // =========================================================================
    // exec_nasync persistent worker pool
    //
    // Instead of spawning a detached thread + constructing a full Interpreter
    // on every exec_nasync call, we maintain persistent worker threads that
    // sleep between frames.  If the worker is still busy from the previous
    // frame, the new invocation is skipped (preserving fire-and-forget
    // semantics).
    // =========================================================================
    struct NasyncWorkerSync {
        std::mutex              mtx;
        std::condition_variable cv;
        bool                    hasWork{false};
        bool                    busy{false};
        bool                    shutdown{false};
        std::string             pipelineName;
        std::vector<RuntimeValue> args;
        cv::Mat                 inputMat;
    };
    struct NasyncWorker {
        std::unique_ptr<Interpreter>    interp;
        std::shared_ptr<NasyncWorkerSync> sync;
        std::thread                     thread;
    };
    std::unordered_map<std::string, std::shared_ptr<NasyncWorker>> _nasyncWorkers;

    // =========================================================================
    // exec_fork child process tracking
    //
    // Each exec_fork call forks a child process that runs a pipeline in an
    // infinite loop, communicating via shared memory.  The parent records
    // the child PID so it can:
    //   (a) send SIGTERM on shutdown
    //   (b) waitpid to reap zombies
    // =========================================================================
    struct ForkChild {
        pid_t       pid;
        std::string pipelineName;
    };
    std::vector<ForkChild> _forkChildren;
    bool _hasForkChildren = false;  ///< Parent: true after any exec_fork call
    ShmArena* _shmArena = nullptr;  ///< Anonymous mmap arena for fork IPC

    /// Send SIGTERM to all fork children and waitpid.
    void shutdownForkChildren();

    // =========================================================================
    // Debug dump flag
    // =========================================================================
    bool _debugDump = false;  // set by debug_start, cleared by debug_end

    // =========================================================================
    // Debug throughput tracking
    //
    // All Interpreter instances that share the same root execution (parent +
    // exec_multi workers + exec_interval children) share a single
    // ThroughputTable via a std::shared_ptr.  This means per-pipeline stats
    // aggregate across all threads automatically.
    //
    // Hot path (recordThroughput) is lock-free: only atomic fetch_add /
    // compare_exchange operations.  The mutex inside ThroughputTable is held
    // only during initial entry insertion (once per pipeline name per run).
    // =========================================================================
    struct ThroughputTable {
        // ── 8-bucket log2 latency histogram ──────────────────────────────
        // Buckets (pipeline execution time):
        //   [0] < 1 ms   [1] 1-2 ms   [2] 2-4 ms   [3] 4-8 ms
        //   [4] 8-16 ms  [5] 16-32 ms [6] 32-64 ms  [7] >= 64 ms
        static constexpr int kBuckets = 8;

        struct Entry {
            std::atomic<uint64_t> callCount{0};
            std::atomic<uint64_t> totalNs{0};
            std::atomic<uint64_t> minNs{UINT64_MAX};
            std::atomic<uint64_t> maxNs{0};
            uint64_t snapCount{0};           // only touched by the printer thread
            std::atomic<uint32_t> latBuckets[kBuckets];  // histogram
            Entry() { for (auto& b : latBuckets) b.store(0); }
        };
        std::unordered_map<std::string, std::unique_ptr<Entry>> entries;
        std::mutex mutex;  ///< protects entries (insertions only)

        std::thread            printerThread;
        std::atomic<bool>      stop{false};
        std::condition_variable cv;
        std::mutex             cvMtx;

        // Printer display flags (set before startPrinter())
        bool latencyMode{false};

        // Fork-child throughput (read from ShmArena by the printer thread).
        // Atomic so the main thread can set it after fork() without a data
        // race against the already-running printer thread.
        std::atomic<ShmArena*> forkArena{nullptr};
        std::unordered_map<std::string, uint64_t> forkSnapCounts;

        void record(const std::string& name, uint64_t durationNs);
        void startPrinter(double intervalSec);
        void stopPrinter();
        ~ThroughputTable() { stopPrinter(); }
    };
    std::shared_ptr<ThroughputTable> _throughputTable;

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
    void execExecInterval(ExecIntervalStmt* stmt);
    void execNoInterval(NoIntervalStmt* stmt);
    void execExecIntervalMulti(ExecIntervalMultiStmt* stmt);
    void execExecRtSeq(ExecRtSeqStmt* stmt);
    void execExecRtMulti(ExecRtMultiStmt* stmt);
    void execExecNasync(ExecNasyncStmt* stmt);
    void execExecFork(ExecForkStmt* stmt);
    void execDebugStart(DebugStartStmt* stmt);
    void execDebugEnd(DebugEndStmt* stmt);
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
    RuntimeValue evalParamRef(ParamRefExpr* expr);
    
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

    // exec_multi helpers
    void initMultiWorkers(const std::vector<std::string>& names,
                          std::shared_ptr<GlobalCacheData> sharedGlobal);
    void resetWorkerState(Interpreter& worker, const cv::Mat& inputMat,
                          std::shared_ptr<GlobalCacheData> sharedGlobal);
    void shutdownMultiWorkers();
    void shutdownNasyncWorkers();
};

} // namespace visionpipe

#endif // VISIONPIPE_INTERPRETER_H

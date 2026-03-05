#include "interpreter/interpreter.h"
#include "interpreter/parser.h"
#include "interpreter/param_store.h"
#include "pipeline/pipeline.h"
#include "pipeline/pipeline_threaded_group.h"
#include "utils/shm_zero_copy.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <future>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace visionpipe {

// File-static flag for fork-child signal handling.  POSIX signal handlers
// cannot capture member variables, so we use a static atomic that the child
// process resets/checks in its tight loop.
static std::atomic<bool> s_forkChildStopped{false};

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

Interpreter::~Interpreter() {
    // Stop interval worker threads first (they may reference pipelines/registry).
    {
        std::lock_guard<std::mutex> lk(_intervalMutex);
        for (auto& [name, worker] : _intervalWorkers) {
            worker->stop.store(true);
        }
        for (auto& [name, worker] : _intervalWorkers) {
            if (worker->workerThread.joinable()) {
                worker->workerThread.join();
            }
        }
        _intervalWorkers.clear();
    }

    // Stop persistent exec_multi worker threads.
    shutdownMultiWorkers();

    // Stop persistent exec_nasync worker threads.
    shutdownNasyncWorkers();

    // Stop fork children (sends SIGTERM, waits, cleans up arena).
    shutdownForkChildren();

    // Destroy the shared-memory arena (munmap).
    if (_shmArena) {
        shmArenaDestroy(_shmArena);
        _shmArena = nullptr;
    }

    // Stop the throughput printer if this interpreter owns the last reference.
    _throughputTable.reset();
}

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
    
    // Create and start the throughput table for the root execution.
    if (_config.throughputMode && !_throughputTable) {
        _throughputTable = std::make_shared<ThroughputTable>();
        _throughputTable->startPrinter(_config.throughputPrintIntervalSec);
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
        case ASTNodeType::EXEC_INTERVAL_STMT:
            execExecInterval(static_cast<ExecIntervalStmt*>(stmt.get()));
            break;
        case ASTNodeType::NO_INTERVAL_STMT:
            execNoInterval(static_cast<NoIntervalStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_INTERVAL_MULTI_STMT:
            execExecIntervalMulti(static_cast<ExecIntervalMultiStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_RT_SEQ_STMT:
            execExecRtSeq(static_cast<ExecRtSeqStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_RT_MULTI_STMT:
            execExecRtMulti(static_cast<ExecRtMultiStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_NASYNC_STMT:
            execExecNasync(static_cast<ExecNasyncStmt*>(stmt.get()));
            break;
        case ASTNodeType::EXEC_FORK_STMT:
            execExecFork(static_cast<ExecForkStmt*>(stmt.get()));
            break;
        case ASTNodeType::DEBUG_START_STMT:
            execDebugStart(static_cast<DebugStartStmt*>(stmt.get()));
            break;
        case ASTNodeType::DEBUG_END_STMT:
            execDebugEnd(static_cast<DebugEndStmt*>(stmt.get()));
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
    // Invalidate cached workers; they reference stale _pipelines/_registry.
    _multiWorkers.clear();
    _multiWorkerTopology.clear();
    // Stop all interval workers.
    {
        std::lock_guard<std::mutex> lk(_intervalMutex);
        for (auto& [name, worker] : _intervalWorkers) {
            worker->stop.store(true);
        }
        for (auto& [name, worker] : _intervalWorkers) {
            if (worker->workerThread.joinable()) {
                worker->workerThread.join();
            }
        }
        _intervalWorkers.clear();
    }
    // Unsubscribe on_params handlers from the store
    if (_paramStore) {
        for (const auto& h : _onParamsHandlers) {
            _paramStore->unsubscribe(h.subscriptionId);
        }
    }
    _onParamsHandlers.clear();
    {
        std::lock_guard<std::mutex> lk(_pendingParamMutex);
        while (!_pendingParamHandlers.empty()) _pendingParamHandlers.pop();
        _hasPendingParams.store(false, std::memory_order_release);
    }
    _paramCache.clear();
    _paramCacheGen = 0;
    _debugDump = false;
}

// ============================================================================
// Runtime parameter store
// ============================================================================

void Interpreter::setParamStore(std::shared_ptr<ParameterStore> store) {
    _paramStore = std::move(store);
    _paramCacheGen = 0;
    _paramCache.clear();
}

void Interpreter::refreshParamCache() {
    if (!_paramStore) return;
    uint64_t currentGen = _paramStore->gen();
    if (currentGen == _paramCacheGen) return;  // no changes since last snapshot
    _paramCache = _paramStore->list();         // one lock, full snapshot
    _paramCacheGen = currentGen;
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

    // Register runtime parameter declarations
    if (_paramStore) {
        for (const auto& paramDecl : program->paramDecls) {
            for (const auto& entry : paramDecl->entries) {
                ParamType ptype = paramTypeFromString(entry.typeName);
                ParamValue defVal;
                if (entry.defaultValue.has_value()) {
                    RuntimeValue rv = evaluate(*entry.defaultValue);
                    switch (ptype) {
                        case ParamType::INT:    defVal = ParamValue(rv.asInt()); break;
                        case ParamType::FLOAT:  defVal = ParamValue(rv.asNumber()); break;
                        case ParamType::BOOL:   defVal = ParamValue(rv.asBool()); break;
                        default:                defVal = ParamValue(rv.asString()); break;
                    }
                }
                _paramStore->declare(entry.name, ptype, defVal);
            }
        }

        // Register on_params handlers (subscribe them to the store)
        for (const auto& handler : program->onParamsHandlers) {
            OnParamsHandler h;
            h.paramName = handler->paramName;
            h.body      = handler->body;

            auto body   = handler->body;
            auto pname  = handler->paramName;
            auto* self  = this;

            h.subscriptionId = (pname == "*")
                ? _paramStore->subscribeAll([self, body](const ParamChangeEvent&) {
                      std::lock_guard<std::mutex> lk(self->_pendingParamMutex);
                      self->_pendingParamHandlers.push({body});
                      self->_hasPendingParams.store(true, std::memory_order_release);
                  })
                : _paramStore->subscribe(pname, [self, body](const ParamChangeEvent&) {
                      std::lock_guard<std::mutex> lk(self->_pendingParamMutex);
                      self->_pendingParamHandlers.push({body});
                      self->_hasPendingParams.store(true, std::memory_order_release);
                  });

            _onParamsHandlers.push_back(std::move(h));
        }
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

void Interpreter::shutdownMultiWorkers() {
    for (auto& w : _multiWorkers) {
        if (w.sync) {
            {
                std::lock_guard<std::mutex> lk(w.sync->mtx);
                w.sync->shutdown = true;
            }
            w.sync->startCv.notify_one();
        }
        if (w.thread.joinable()) w.thread.join();
    }
    _multiWorkers.clear();
    _multiWorkerTopology.clear();
}

void Interpreter::shutdownNasyncWorkers() {
    for (auto& [name, nw] : _nasyncWorkers) {
        if (nw && nw->sync) {
            {
                std::lock_guard<std::mutex> lk(nw->sync->mtx);
                nw->sync->shutdown = true;
            }
            nw->sync->cv.notify_one();
        }
        if (nw && nw->thread.joinable()) nw->thread.join();
    }
    _nasyncWorkers.clear();
}

void Interpreter::initMultiWorkers(const std::vector<std::string>& names,
                                    std::shared_ptr<GlobalCacheData> sharedGlobal) {
    // Shut down any existing persistent worker threads.
    shutdownMultiWorkers();

    _multiWorkers.reserve(names.size());
    _multiWorkerTopology = names;

    // Phase 1: Create worker interpreters.
    for (const auto& name : names) {
        MultiWorker w;
        w.pipelineName = name;
        w.sync = std::make_shared<MultiWorkerSync>();
        w.interp = std::make_unique<Interpreter>(_config);

        // Share read-only AST nodes and item instances with the parent.
        // Both maps hold shared_ptrs so this is a shallow pointer copy — the
        // underlying PipelineDecl / InterpreterItem objects are shared, not
        // duplicated.  They are effectively immutable after the program is
        // parsed, so no locking is required.
        w.interp->_pipelines = _pipelines;
        w.interp->_registry  = _registry;
        if (_throughputTable) w.interp->_throughputTable = _throughputTable;

        // Share param store so workers can read @param references.
        if (_paramStore) {
            w.interp->_paramStore = _paramStore;
        }

        // Wire the shared global cache.
        w.interp->_cacheManager.replaceGlobalData(sharedGlobal);
        w.interp->_context.cacheManager = &w.interp->_cacheManager;

        // Propagate fork-child awareness so arena reads work in workers.
        w.interp->_hasForkChildren = _hasForkChildren;
        w.interp->_cacheManager.setHasForkChildren(_hasForkChildren);
        w.interp->_cacheManager.setShmArena(_shmArena);

        // Seed the worker's global scope from the parent so that top-level
        // 'global' variable declarations (e.g. movement_matrix) and their
        // current values are visible inside worker pipelines.
        w.interp->_scopes.clear();
        w.interp->_scopes.push_back(_scopes.empty() ? Scope{} : _scopes.front());

        _multiWorkers.push_back(std::move(w));
    }

    // Phase 2: Launch persistent threads.  Vector is fully populated so
    // element addresses are stable and we capture the shared_ptr<sync> +
    // raw Interpreter* which remain valid for the worker's lifetime.
    for (size_t i = 0; i < _multiWorkers.size(); ++i) {
        auto  syncPtr  = _multiWorkers[i].sync;
        auto* interpRaw = _multiWorkers[i].interp.get();

        _multiWorkers[i].thread = std::thread([syncPtr, interpRaw]() {
            while (true) {
                // Wait for work or shutdown signal.
                std::unique_lock<std::mutex> lk(syncPtr->mtx);
                syncPtr->startCv.wait(lk, [&] { return syncPtr->hasWork || syncPtr->shutdown; });
                if (syncPtr->shutdown) break;
                syncPtr->hasWork = false;
                // Snapshot invocation data under lock.
                std::string name  = syncPtr->invName;
                std::vector<RuntimeValue> args = syncPtr->invArgs;
                lk.unlock();

                // Execute the pipeline.
                try {
                    interpRaw->executePipeline(name, args,
                                               interpRaw->_context.currentMat);
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> errLk(syncPtr->mtx);
                    syncPtr->error = e.what();
                }

                // Signal completion.
                {
                    std::lock_guard<std::mutex> doneLk(syncPtr->mtx);
                    syncPtr->workDone = true;
                }
                syncPtr->doneCv.notify_one();
            }
        });
    }
}

void Interpreter::resetWorkerState(Interpreter& worker, const cv::Mat& inputMat,
                                    std::shared_ptr<GlobalCacheData> sharedGlobal) {
    // Re-sync global cache pointer (callers may have swapped it between runs,
    // e.g. if the parent's global store was rebuilt).
    worker._cacheManager.replaceGlobalData(sharedGlobal);
    worker._context.cacheManager = &worker._cacheManager;

    // Seed the worker's global scope from the parent interpreter so that
    // top-level 'global' declarations and their latest values (e.g. the
    // movement_matrix written by calculate_stabilization on the previous
    // frame) are visible inside worker pipelines on each new frame.
    // Local (non-global) variables are NOT inherited — only _scopes.front().
    worker._scopes.clear();
    worker._scopes.push_back(_scopes.empty() ? Scope{} : _scopes.front());

    // Reset local cache scope stack.
    worker._cacheManager.resetLocalScopes();

    // Clear execution control flags.
    worker._context.reset();
    worker._context.currentMat = inputMat;  // caller supplies a per-worker copy

    // Reset verbose/debug flags so debug_start in frame N does not bleed into
    // frame N+1 when the same worker Interpreter is reused across frames.
    worker._context.verbose   = worker._config.verbose;
    worker._context.debugDump = false;
    worker._debugDump         = false;

    worker._recursionDepth = 0;
    worker._hasError = false;
    worker._lastError.clear();
}

void Interpreter::execExecMulti(ExecMultiStmt* stmt) {
    // -------------------------------------------------------------------------
    // Step 1: Resolve pipeline names and evaluate all arguments on the MAIN
    //         thread, before signaling any worker threads.
    // -------------------------------------------------------------------------
    struct Invocation {
        std::string name;
        std::vector<RuntimeValue> args;
    };
    std::vector<Invocation> invocations;
    invocations.reserve(stmt->pipelineRefs.size());

    for (auto& ref : stmt->pipelineRefs) {
        Invocation inv;
        if (auto* call = dynamic_cast<FunctionCallExpr*>(ref.get())) {
            inv.name = call->functionName;
            for (const auto& arg : call->arguments) {
                inv.args.push_back(evalExpression(arg.get()));
            }
        } else if (auto* ident = dynamic_cast<IdentifierExpr*>(ref.get())) {
            inv.name = ident->name;
        } else {
            continue;
        }
        invocations.push_back(std::move(inv));
    }

    if (invocations.empty()) return;

    // -------------------------------------------------------------------------
    // Step 2: Build the topology key and (re)create workers if needed.
    //
    // Workers are reused every frame — only their per-frame state is reset.
    // Rebuilding only happens on first call or if pipeline topology changed.
    // -------------------------------------------------------------------------
    auto sharedGlobal = _cacheManager.getGlobalData();

    // Collect names for topology comparison.
    std::vector<std::string> names;
    names.reserve(invocations.size());
    for (const auto& inv : invocations) names.push_back(inv.name);

    if (_multiWorkerTopology != names) {
        // First call or topology change — allocate fresh workers + threads.
        initMultiWorkers(names, sharedGlobal);
    } else {
        // Likely same global store, but replaceGlobalData is cheap.
        for (auto& w : _multiWorkers) {
            w.interp->_cacheManager.replaceGlobalData(sharedGlobal);
            w.interp->_context.cacheManager = &w.interp->_cacheManager;
        }
    }

    // -------------------------------------------------------------------------
    // Step 3: Prepare per-worker input mats and signal persistent threads.
    //
    // Each worker gets its own copy of currentMat so threads never share a
    // buffer.  For pipelines that start with video_cap (which replaces
    // currentMat entirely), an empty Mat is sufficient.  The global-cache
    // store is the only truly shared resource and is protected by
    // shared_mutex internally.
    // -------------------------------------------------------------------------
    cv::Mat inputMat = _context.currentMat;  // shallow copy — workers that need isolation clone themselves

    for (size_t i = 0; i < invocations.size(); ++i) {
        // Give each worker its own shallow copy of the current Mat.
        // Workers that replace it (video_cap) never touch the old buffer.
        cv::Mat workerInput = inputMat;

        resetWorkerState(*_multiWorkers[i].interp, workerInput, sharedGlobal);

        auto& sync = *_multiWorkers[i].sync;
        {
            std::lock_guard<std::mutex> lk(sync.mtx);
            sync.invName  = invocations[i].name;
            sync.invArgs  = invocations[i].args;
            sync.workDone = false;
            sync.error.clear();
            sync.hasWork  = true;
        }
        sync.startCv.notify_one();
    }

    // -------------------------------------------------------------------------
    // Step 4: Wait for all workers to finish.
    // -------------------------------------------------------------------------
    for (size_t i = 0; i < invocations.size(); ++i) {
        auto& sync = *_multiWorkers[i].sync;
        std::unique_lock<std::mutex> lk(sync.mtx);
        sync.doneCv.wait(lk, [&sync] { return sync.workDone; });
    }

    // Merge each worker's global scope back into the parent so that global
    // variable assignments written inside pipelines (e.g.
    //   global movement_matrix = matrix_avg(...)  )
    // are visible in subsequent statements and in the next exec_multi frame.
    // Only non-void values are merged; void means "unchanged / not written".
    if (!_scopes.empty()) {
        for (auto& w : _multiWorkers) {
            if (!w.interp->_scopes.empty()) {
                for (auto& [name, val] : w.interp->_scopes.front().variables) {
                    if (!val.isVoid()) {
                        _scopes.front().variables[name] = val;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < invocations.size(); ++i) {
        auto& sync = *_multiWorkers[i].sync;
        std::lock_guard<std::mutex> lk(sync.mtx);
        if (!sync.error.empty()) {
            std::cerr << "[exec_multi] Error in pipeline '"
                      << invocations[i].name << "': " << sync.error << "\n";
        }
    }
}

void Interpreter::execExecLoop(ExecLoopStmt* stmt) {
    _loopRunning = true;
    
    while (!_stopRequested) {
        // Reset verbose/debug flags at each frame boundary so debug_start in a
        // previous iteration does not bleed through to the next frame.
        _context.verbose   = _config.verbose;
        _context.debugDump = false;
        _debugDump         = false;

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
        
        // Drain pending on_params handlers (run on runtime loop thread)
        // Fast path: skip mutex entirely when nothing was queued
        if (_hasPendingParams.load(std::memory_order_acquire)) {
            std::queue<ParamHandlerBody> pending;
            {
                std::lock_guard<std::mutex> lk(_pendingParamMutex);
                std::swap(pending, _pendingParamHandlers);
                _hasPendingParams.store(false, std::memory_order_release);
            }
            while (!pending.empty()) {
                auto& handler = pending.front();
                for (const auto& s : handler.statements) {
                    executeStatement(s);
                    if (_context.shouldBreak || _context.shouldReturn || _stopRequested) break;
                }
                pending.pop();
            }
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

// ============================================================================
// exec_interval / no_interval / exec_interval_multi
// ============================================================================

// Helper: extract pipeline name + args from an expression pointer.
static std::pair<std::string, std::vector<RuntimeValue>>
resolvePipelineRef(Expression* ref, Interpreter&) {
    if (auto* call = dynamic_cast<FunctionCallExpr*>(ref)) {
        // Arguments are evaluated on the calling thread before we spawn; we
        // cannot re-evaluate them inside the worker because the parent
        // interpreter state may have changed.  For simplicity we pass an empty
        // arg list here – interval pipelines that need args should use global
        // cache for communication.
        return {call->functionName, {}};
    }
    if (auto* ident = dynamic_cast<IdentifierExpr*>(ref)) {
        return {ident->name, {}};
    }
    return {"", {}};
}

void Interpreter::execExecInterval(ExecIntervalStmt* stmt) {
    auto [name, args] = resolvePipelineRef(stmt->pipelineRef.get(), *this);
    if (name.empty()) {
        reportError("exec_interval: could not resolve pipeline name", stmt->location);
        return;
    }

    RuntimeValue intervalVal = evalExpression(stmt->intervalMs.get());
    double intervalMs = intervalVal.asNumber();

    // Construct (or replace) the worker for this pipeline key.
    auto workerKey = name;

    // Stop any existing interval for the same key.
    {
        std::lock_guard<std::mutex> lk(_intervalMutex);
        auto it = _intervalWorkers.find(workerKey);
        if (it != _intervalWorkers.end()) {
            it->second->stop.store(true);
            if (it->second->workerThread.joinable()) it->second->workerThread.join();
            _intervalWorkers.erase(it);
        }
    }

    // Create child interpreter that shares pipelines and registry.
    auto worker = std::make_shared<IntervalWorker>();
    worker->interp = std::make_unique<Interpreter>(_config);
    worker->interp->_pipelines = _pipelines;
    worker->interp->_registry  = _registry;
    if (_throughputTable) worker->interp->_throughputTable = _throughputTable;
    if (_paramStore) worker->interp->_paramStore = _paramStore;
    auto sharedGlobal = _cacheManager.getGlobalData();
    worker->interp->_cacheManager.replaceGlobalData(sharedGlobal);
    worker->interp->_context.cacheManager = &worker->interp->_cacheManager;

    // Propagate fork-child awareness so arena reads work in interval threads.
    worker->interp->_hasForkChildren = _hasForkChildren;
    worker->interp->_cacheManager.setHasForkChildren(_hasForkChildren);
    worker->interp->_cacheManager.setShmArena(_shmArena);

    auto* workerPtr   = worker.get();
    auto* interpPtr   = worker->interp.get();
    std::string pname = name;

    worker->workerThread = std::thread([workerPtr, interpPtr, pname, args, intervalMs]() {
        using namespace std::chrono;
        while (!workerPtr->stop.load()) {
            auto next = steady_clock::now() + duration<double, std::milli>(intervalMs);

            try {
                interpPtr->_context.reset();
                interpPtr->_scopes.clear();
                interpPtr->_scopes.emplace_back();
                interpPtr->executePipeline(pname, args, interpPtr->_context.currentMat);
            } catch (const std::exception& e) {
                std::cerr << "[exec_interval] Error in pipeline '"
                          << pname << "': " << e.what() << "\n";
            }

            // Sleep until the next tick, but check stop frequently.
            while (!workerPtr->stop.load() && steady_clock::now() < next) {
                std::this_thread::sleep_for(milliseconds(1));
            }
        }
    });

    std::lock_guard<std::mutex> lk(_intervalMutex);
    _intervalWorkers[workerKey] = std::move(worker);
}

void Interpreter::execNoInterval(NoIntervalStmt* stmt) {
    std::string name = resolvePipelineRef(stmt->pipelineRef.get(), *this).first;
    if (name.empty()) {
        reportError("no_interval: could not resolve pipeline name", stmt->location);
        return;
    }

    std::lock_guard<std::mutex> lk(_intervalMutex);
    auto it = _intervalWorkers.find(name);
    if (it != _intervalWorkers.end()) {
        it->second->stop.store(true);
        if (it->second->workerThread.joinable()) it->second->workerThread.join();
        _intervalWorkers.erase(it);
    } else {
        std::cerr << "[no_interval] No active interval for pipeline '" << name << "'\n";
    }
}

void Interpreter::execExecIntervalMulti(ExecIntervalMultiStmt* stmt) {
    // Collect pipeline names.
    std::vector<std::string> names;
    for (auto& ref : stmt->pipelineRefs) {
        names.push_back(resolvePipelineRef(ref.get(), *this).first);
    }

    RuntimeValue intervalVal = evalExpression(stmt->intervalMs.get());
    double intervalMs = intervalVal.asNumber();

    // Use composite key = all names joined.
    std::string workerKey;
    for (const auto& n : names) workerKey += n + "|";

    {
        std::lock_guard<std::mutex> lk(_intervalMutex);
        auto it = _intervalWorkers.find(workerKey);
        if (it != _intervalWorkers.end()) {
            it->second->stop.store(true);
            if (it->second->workerThread.joinable()) it->second->workerThread.join();
            _intervalWorkers.erase(it);
        }
    }

    auto worker = std::make_shared<IntervalWorker>();
    // We give the multi-interval a single coordinator thread that launches
    // worker sub-threads on each tick (mirrors execExecMulti behaviour).
    auto sharedGlobal = _cacheManager.getGlobalData();

    // Snapshot pipelines / registry for child workers.
    auto pipelinesCopy = _pipelines;
    auto registryCopy  = _registry;
    auto thrTblCopy    = _throughputTable;

    auto* workerPtr = worker.get();

    worker->workerThread = std::thread([workerPtr, names, pipelinesCopy, registryCopy,
                                  sharedGlobal, intervalMs, thrTblCopy, cfg = _config]() mutable {
        using namespace std::chrono;
        while (!workerPtr->stop.load()) {
            auto next = steady_clock::now() + duration<double, std::milli>(intervalMs);

            // Launch one child interpreter per pipeline in parallel.
            std::vector<std::thread> threads;
            threads.reserve(names.size());
            for (const auto& pname : names) {
                threads.emplace_back([&pname, &pipelinesCopy, &registryCopy,
                                      &sharedGlobal, &thrTblCopy, &cfg]() {
                    Interpreter child(cfg);
                    child._pipelines = pipelinesCopy;
                    child._registry  = registryCopy;
                    if (thrTblCopy) child._throughputTable = thrTblCopy;
                    child._cacheManager.replaceGlobalData(sharedGlobal);
                    child._context.cacheManager = &child._cacheManager;
                    try {
                        child.executePipeline(pname, {}, {});
                    } catch (const std::exception& e) {
                        std::cerr << "[exec_interval_multi] Error in pipeline '"
                                  << pname << "': " << e.what() << "\n";
                    }
                });
            }
            for (auto& t : threads) if (t.joinable()) t.join();

            while (!workerPtr->stop.load() && steady_clock::now() < next) {
                std::this_thread::sleep_for(milliseconds(1));
            }
        }
    });

    std::lock_guard<std::mutex> lk(_intervalMutex);
    _intervalWorkers[workerKey] = std::move(worker);
}

// ============================================================================
// exec_rt_seq / exec_rt_multi  (real-time execution with deadline)
// ============================================================================

void Interpreter::execExecRtSeq(ExecRtSeqStmt* stmt) {
    // Resolve pipeline ref.
    std::string pname;
    std::vector<RuntimeValue> args;
    if (auto* call = dynamic_cast<FunctionCallExpr*>(stmt->pipelineRef.get())) {
        pname = call->functionName;
        for (const auto& arg : call->arguments) args.push_back(evalExpression(arg.get()));
    } else if (auto* ident = dynamic_cast<IdentifierExpr*>(stmt->pipelineRef.get())) {
        pname = ident->name;
    }

    if (pname.empty()) {
        reportError("exec_rt_seq: could not resolve pipeline name", stmt->location);
        return;
    }

    double timeoutMs = evalExpression(stmt->timeoutMs.get()).asNumber();

    // Capture shared global cache so the child can write to it.
    auto sharedGlobal = _cacheManager.getGlobalData();
    cv::Mat inputMat  = _context.currentMat.empty() ? cv::Mat() : _context.currentMat.clone();

    auto pipelines   = _pipelines;
    auto registry    = _registry;
    auto cfg         = _config;
    auto thrTbl      = _throughputTable;

    // Run in async; wait up to timeoutMs.
    auto fut = std::async(std::launch::async,
        [pname, args, inputMat, pipelines, registry, sharedGlobal, cfg, thrTbl]() mutable {
            Interpreter child(cfg);
            child._pipelines = std::move(pipelines);
            child._registry  = std::move(registry);
            if (thrTbl) child._throughputTable = thrTbl;
            child._cacheManager.replaceGlobalData(sharedGlobal);
            child._context.cacheManager = &child._cacheManager;
            child.executePipeline(pname, args, inputMat);
        });

    auto status = fut.wait_for(std::chrono::duration<double, std::milli>(timeoutMs));
    if (status == std::future_status::timeout) {
        std::cerr << "[exec_rt_seq] Deadline exceeded (" << timeoutMs
                  << " ms) for pipeline '" << pname << "'\n";
        // We cannot forcibly kill the thread; let it finish in the background.
        // The future keeps the thread alive until it completes.
        fut.wait();
    }
}

void Interpreter::execExecRtMulti(ExecRtMultiStmt* stmt) {
    struct Invocation { std::string name; std::vector<RuntimeValue> args; };
    std::vector<Invocation> invocations;
    for (auto& ref : stmt->pipelineRefs) {
        Invocation inv;
        if (auto* call = dynamic_cast<FunctionCallExpr*>(ref.get())) {
            inv.name = call->functionName;
            for (const auto& arg : call->arguments) inv.args.push_back(evalExpression(arg.get()));
        } else if (auto* ident = dynamic_cast<IdentifierExpr*>(ref.get())) {
            inv.name = ident->name;
        }
        if (!inv.name.empty()) invocations.push_back(std::move(inv));
    }

    if (invocations.empty()) return;

    double timeoutMs  = evalExpression(stmt->timeoutMs.get()).asNumber();
    auto sharedGlobal = _cacheManager.getGlobalData();
    cv::Mat inputMat  = _context.currentMat.empty() ? cv::Mat() : _context.currentMat.clone();
    auto pipelines    = _pipelines;
    auto registry     = _registry;
    auto cfg          = _config;
    auto thrTbl       = _throughputTable;

    auto fut = std::async(std::launch::async,
        [invocations, inputMat, pipelines, registry, sharedGlobal, cfg, thrTbl]() mutable {
            std::vector<std::thread> threads;
            threads.reserve(invocations.size());
            for (size_t i = 0; i < invocations.size(); ++i) {
                cv::Mat workerMat = (i == 0) ? inputMat : inputMat.clone();
                threads.emplace_back([&inv = invocations[i], workerMat, &pipelines,
                                      &registry, &sharedGlobal, &cfg, &thrTbl]() {
                    Interpreter child(cfg);
                    child._pipelines = pipelines;
                    child._registry  = registry;
                    if (thrTbl) child._throughputTable = thrTbl;
                    child._cacheManager.replaceGlobalData(sharedGlobal);
                    child._context.cacheManager = &child._cacheManager;
                    try {
                        child.executePipeline(inv.name, inv.args, workerMat);
                    } catch (const std::exception& e) {
                        std::cerr << "[exec_rt_multi] Error in pipeline '"
                                  << inv.name << "': " << e.what() << "\n";
                    }
                });
            }
            for (auto& t : threads) if (t.joinable()) t.join();
        });

    auto status = fut.wait_for(std::chrono::duration<double, std::milli>(timeoutMs));
    if (status == std::future_status::timeout) {
        std::cerr << "[exec_rt_multi] Deadline exceeded (" << timeoutMs
                  << " ms) for " << invocations.size() << " pipelines\n";
        fut.wait();
    }
}

// ============================================================================
// exec_nasync  (fire-and-forget with persistent worker threads)
//
// Semantics:
//   • The current Mat is shared (shallow copy) for the async thread.  Pipeline
//     items produce new buffers so the parent's Mat is unaffected.
//   • For named pipelines, a persistent worker thread is reused across frames.
//     If the worker is still busy from the previous frame, the new invocation
//     is silently skipped (preserving fire-and-forget semantics).
//   • For inline blocks, the original detached-thread behaviour is preserved
//     because inline blocks have no stable name to key the worker on.
//   • The calling thread's Mat is left unchanged (bypass behaviour).
//
// Forms:
//   exec_nasync pipeline_name        – run named pipeline (persistent worker)
//   exec_nasync start ... end        – run inline anonymous block (detached)
// ============================================================================

void Interpreter::execExecNasync(ExecNasyncStmt* stmt) {

    if (!stmt->isInlineBlock()) {
        // ── Named pipeline form ── persistent worker thread ──────────────────
        std::string pname;
        std::vector<RuntimeValue> args;

        if (auto* call = dynamic_cast<FunctionCallExpr*>(stmt->pipelineRef.get())) {
            pname = call->functionName;
            for (const auto& arg : call->arguments)
                args.push_back(evalExpression(arg.get()));
        } else if (auto* ident = dynamic_cast<IdentifierExpr*>(stmt->pipelineRef.get())) {
            pname = ident->name;
        }

        if (pname.empty()) {
            reportError("exec_nasync: could not resolve pipeline name", stmt->location);
            return;
        }

        // Shallow copy of the Mat — the async thread must not mutate it.
        cv::Mat asyncMat = _context.currentMat;

        // Find or create persistent worker for this pipeline name.
        auto it = _nasyncWorkers.find(pname);
        if (it == _nasyncWorkers.end()) {
            // First invocation: create the worker.
            auto nw   = std::make_shared<NasyncWorker>();
            nw->sync  = std::make_shared<NasyncWorkerSync>();
            nw->interp = std::make_unique<Interpreter>(_config);
            nw->interp->_pipelines = _pipelines;
            nw->interp->_registry  = _registry;
            if (_throughputTable) nw->interp->_throughputTable = _throughputTable;
            auto sharedGlobal = _cacheManager.getGlobalData();
            nw->interp->_cacheManager.replaceGlobalData(sharedGlobal);
            nw->interp->_context.cacheManager = &nw->interp->_cacheManager;
            if (_paramStore) nw->interp->_paramStore = _paramStore;

            // Propagate fork-child awareness so arena reads work.
            nw->interp->_hasForkChildren = _hasForkChildren;
            nw->interp->_cacheManager.setHasForkChildren(_hasForkChildren);
            nw->interp->_cacheManager.setShmArena(_shmArena);

            auto  syncPtr  = nw->sync;
            auto* interpRaw = nw->interp.get();

            nw->thread = std::thread([syncPtr, interpRaw]() {
                while (true) {
                    std::unique_lock<std::mutex> lk(syncPtr->mtx);
                    syncPtr->cv.wait(lk, [&] { return syncPtr->hasWork || syncPtr->shutdown; });
                    if (syncPtr->shutdown) break;
                    syncPtr->hasWork = false;
                    syncPtr->busy    = true;
                    std::string name    = syncPtr->pipelineName;
                    std::vector<RuntimeValue> a = syncPtr->args;
                    cv::Mat mat         = syncPtr->inputMat;
                    lk.unlock();

                    // Reset per-frame state.
                    interpRaw->_context.reset();
                    interpRaw->_context.currentMat     = mat;
                    interpRaw->_context.cacheManager   = &interpRaw->_cacheManager;
                    interpRaw->_context.verbose         = interpRaw->_config.verbose;
                    interpRaw->_context.debugDump       = false;
                    interpRaw->_cacheManager.resetLocalScopes();
                    interpRaw->_scopes.clear();
                    interpRaw->_scopes.emplace_back();
                    interpRaw->_recursionDepth = 0;

                    try {
                        interpRaw->executePipeline(name, a, mat);
                    } catch (const std::exception& e) {
                        std::cerr << "[exec_nasync] Error in pipeline '"
                                  << name << "': " << e.what() << "\n";
                    }

                    {
                        std::lock_guard<std::mutex> lk2(syncPtr->mtx);
                        syncPtr->busy = false;
                    }
                }
            });

            it = _nasyncWorkers.emplace(pname, std::move(nw)).first;
        }

        // Signal the persistent worker if it is idle.
        auto& sync = *it->second->sync;
        {
            std::lock_guard<std::mutex> lk(sync.mtx);
            if (sync.busy) {
                // Worker is still processing previous frame — skip.
                return;
            }
            // Refresh the shared global cache pointer (cheap swap).
            it->second->interp->_cacheManager.replaceGlobalData(
                _cacheManager.getGlobalData());
            if (_paramStore)
                it->second->interp->_paramStore = _paramStore;

            sync.pipelineName = pname;
            sync.args         = std::move(args);
            sync.inputMat     = asyncMat;
            sync.hasWork      = true;
        }
        sync.cv.notify_one();

    } else {
        // ── Inline block form ── detached thread (unchanged) ─────────────────
        // AST statement nodes are immutable shared_ptrs – safe to share across
        // threads without any locking.
        auto body = stmt->body;

        auto pipelines    = _pipelines;
        auto registry     = _registry;
        auto cfg          = _config;
        auto thrTbl       = _throughputTable;
        auto sharedGlobal = _cacheManager.getGlobalData();
        auto paramStore   = _paramStore;
        cv::Mat asyncMat  = _context.currentMat.empty()
                            ? cv::Mat()
                            : _context.currentMat.clone();

        std::thread([body = std::move(body), asyncMat,
                     pipelines = std::move(pipelines),
                     registry  = std::move(registry),
                     sharedGlobal, paramStore, cfg, thrTbl]() mutable {
            Interpreter child(cfg);
            child._pipelines = std::move(pipelines);
            child._registry  = std::move(registry);
            if (thrTbl) child._throughputTable = thrTbl;
            child._cacheManager.replaceGlobalData(sharedGlobal);
            child._context.cacheManager = &child._cacheManager;
            child._context.currentMat   = asyncMat;
            if (paramStore) child._paramStore = paramStore;
            child._scopes.emplace_back();  // global scope for the block
            try {
                for (const auto& s : body) {
                    child.executeStatement(s);
                    if (child._context.shouldBreak ||
                        child._context.shouldReturn ||
                        child._stopRequested.load(std::memory_order_relaxed)) break;
                }
            } catch (const std::exception& e) {
                std::cerr << "[exec_nasync] Error in inline block: " << e.what() << "\n";
            }
        }).detach();
    }

    // Main thread continues immediately with the original (unchanged) Mat.
}

// ============================================================================
// exec_fork  (child process via fork())
//
// Semantics:
//   • Forks a child process that runs the named pipeline in an infinite loop.
//   • The parent records the child PID and continues immediately.
//   • Communication between parent and child is via POSIX shared memory
//     (shm_write / shm_read items in the .vsp script).
//   • On SIGTERM (or shmFrameIsShutdown), the child exits its loop.
//   • The parent sends SIGTERM to all fork children on shutdown.
//
// IMPORTANT: exec_fork should be called BEFORE exec_loop to avoid fork()
//            in a multithreaded context.
// ============================================================================

void Interpreter::execExecFork(ExecForkStmt* stmt) {
    std::string pname;
    std::vector<RuntimeValue> args;

    if (auto* call = dynamic_cast<FunctionCallExpr*>(stmt->pipelineRef.get())) {
        pname = call->functionName;
        for (const auto& arg : call->arguments)
            args.push_back(evalExpression(arg.get()));
    } else if (auto* ident = dynamic_cast<IdentifierExpr*>(stmt->pipelineRef.get())) {
        pname = ident->name;
    }

    if (pname.empty()) {
        reportError("exec_fork: could not resolve pipeline name", stmt->location);
        return;
    }

    // Verify the pipeline exists before forking.
    if (!hasPipeline(pname)) {
        reportError("exec_fork: pipeline '" + pname + "' not found", stmt->location);
        return;
    }

    // On the very first exec_fork, create an anonymous mmap arena.
    // This must happen BEFORE fork() so children inherit the mapping.
    if (!_shmArena) {
        _shmArena = shmArenaCreate();  // 8 slots, 8 MB each (default)
        if (!_shmArena) {
            reportError("exec_fork: shmArenaCreate() failed", stmt->location);
            return;
        }
        _cacheManager.setShmArena(_shmArena);
    }

    pid_t pid = fork();

    if (pid < 0) {
        reportError("exec_fork: fork() failed: " + std::string(strerror(errno)),
                     stmt->location);
        return;
    }

    if (pid == 0) {
        // ── CHILD PROCESS ──────────────────────────────────────────────────
        // We are now in a forked child.  The only thread that exists is the
        // calling thread (POSIX guarantee).  Mutexes from threads that no
        // longer exist may be in an inconsistent state, so we avoid touching
        // them and reinitialise what we need.

        // Abandon the parent's ThroughputTable WITHOUT destroying it.
        // ~ThroughputTable() would try to join() the parent's printer thread,
        // which doesn't exist in the child (only the calling thread survives
        // fork).  join() on a dead pthread → ESRCH → exception from noexcept
        // destructor → std::terminate().  We intentionally leak the old table;
        // the OS reclaims all memory on _exit().
        if (_throughputTable) {
            new auto(std::move(_throughputTable));  // leak into heap
            // _throughputTable is now nullptr
        }
        // Don't create a child ThroughputTable — throughput is recorded
        // directly into the ShmArena (read by the parent's printer).

        // Clear parent's thread-based workers (they don't exist in the child).
        _multiWorkers.clear();
        _multiWorkerTopology.clear();
        _nasyncWorkers.clear();
        _intervalWorkers.clear();  // interval threads don't exist in child
        _forkChildren.clear();  // child doesn't own parent's children

        // Mark this interpreter as running inside a fork child so that
        // setGlobal() automatically writes to the shared-memory arena.
        _cacheManager.setForkChild(true);

        // Install a signal handler that sets the static stop flag.
        s_forkChildStopped.store(false);
        struct sigaction sa;
        sa.sa_handler = [](int) {
            s_forkChildStopped.store(true, std::memory_order_relaxed);
        };
        sa.sa_flags   = 0;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGINT,  &sa, nullptr);

        // Run the pipeline in a loop until signalled.
        _stopRequested.store(false);
        _loopRunning = true;
        bool firstIteration = true;
        using clock = std::chrono::steady_clock;

        while (!_stopRequested.load(std::memory_order_relaxed) &&
               !s_forkChildStopped.load(std::memory_order_relaxed)) {

            // Check the arena shutdown flag (set by parent).
            if (_shmArena && shmArenaIsShutdown(_shmArena)) {
                _stopRequested.store(true);
                break;
            }

            try {
                _context.reset();
                _cacheManager.resetLocalScopes();
                _scopes.clear();
                _scopes.emplace_back();

                auto t0 = clock::now();
                executePipeline(pname, args, cv::Mat());
                auto t1 = clock::now();

                // Record throughput into the arena (parent reads it).
                if (_shmArena) {
                    uint64_t ns = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
                    shmArenaRecordThroughput(_shmArena, pname, ns);
                }
            } catch (const std::exception& e) {
                std::cerr << "[exec_fork:" << pname << "] Error: " << e.what() << "\n";
                // On first iteration failure, exit (likely config error).
                if (firstIteration) break;
            }
            firstIteration = false;
        }

        _loopRunning = false;
        _shmArena = nullptr;  // don't destroy — parent owns it
        _exit(0);  // _exit to avoid running parent's atexit handlers
    }

    // ── PARENT PROCESS ─────────────────────────────────────────────────────
    _hasForkChildren = true;
    _cacheManager.setHasForkChildren(true);

    // Let the throughput printer read fork-child stats from the arena.
    if (_throughputTable && _shmArena) {
        _throughputTable->forkArena = _shmArena;
    }

    ForkChild child;
    child.pid          = pid;
    child.pipelineName = pname;
    _forkChildren.push_back(std::move(child));

    std::cerr << "[exec_fork] Forked child PID " << pid
              << " for pipeline '" << pname << "'\n";
}

void Interpreter::shutdownForkChildren() {
    if (_forkChildren.empty()) return;

    // Signal the arena shutdown flag — children poll this in their loop.
    if (_shmArena) {
        shmArenaSetShutdown(_shmArena);
    }

    // Send SIGTERM to all children.
    for (auto& c : _forkChildren) {
        if (c.pid > 0) {
            kill(c.pid, SIGTERM);
        }
    }

    // Wait for children to exit (timeout 3 seconds, then SIGKILL).
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    for (auto& c : _forkChildren) {
        if (c.pid <= 0) continue;
        while (std::chrono::steady_clock::now() < deadline) {
            int status;
            pid_t w = waitpid(c.pid, &status, WNOHANG);
            if (w == c.pid || w < 0) {
                c.pid = -1;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (c.pid > 0) {
            std::cerr << "[exec_fork] Force-killing child PID " << c.pid << "\n";
            kill(c.pid, SIGKILL);
            waitpid(c.pid, nullptr, 0);
            c.pid = -1;
        }
    }
    _forkChildren.clear();
}

// ============================================================================
// debug_start / debug_end
// ============================================================================

void Interpreter::execDebugStart(DebugStartStmt* /*stmt*/) {
    _debugDump = true;
    _context.debugDump = true;
    _context.verbose   = true;
    std::cerr << "[debug] debug_start – verbose logging enabled\n";
}

void Interpreter::execDebugEnd(DebugEndStmt* /*stmt*/) {
    _debugDump = false;
    _context.debugDump = false;
    _context.verbose   = _config.verbose;  // restore original verbose setting
    std::cerr << "[debug] debug_end – verbose logging disabled\n";
}

void Interpreter::execUse(UseStmt* stmt) {
    cv::Mat mat;
    
    if (stmt->isGlobal) {
        mat = _cacheManager.getGlobal(stmt->cacheId);
    } else {
        mat = _cacheManager.get(stmt->cacheId);
    }

    // When fork children are active and the frame hasn't arrived yet,
    // skip this pipeline iteration gracefully instead of passing an empty
    // mat to downstream items (resize, debayer, etc.) that would crash.
    if (mat.empty() && _hasForkChildren && stmt->isGlobal) {
        _context.currentMat = mat;
        _context.shouldReturn = true;
        return;
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
    if (stmt->initialValue.has_value()) {
        // global varname = expr  — evaluate and write to the interpreter's
        // global (front) scope so the value is visible across all subsequent
        // pipeline calls on this interpreter, and is picked up by the parent
        // interpreter's scope-merge after exec_multi completes.
        RuntimeValue value = evalExpression(stmt->initialValue->get());
        setVariable(stmt->cacheId, value);
        return;
    }

    // global "cache_id"  — promote a local cache entry to the global cache store.
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
        case ASTNodeType::PARAM_REF_EXPR:
            return evalParamRef(static_cast<ParamRefExpr*>(expr));
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
        // Resolve named args against pipeline parameter names
        if (!expr->namedArguments.empty()) {
            auto pipeline = getPipeline(expr->functionName);
            const auto& params = pipeline->parameters;
            // Extend args vector to accommodate all possibly-named params
            for (size_t i = args.size(); i < params.size(); ++i) {
                RuntimeValue def;
                if (params[i].defaultValue.has_value()) {
                    def = evaluate(*params[i].defaultValue);
                }
                args.push_back(def);
            }
            for (const auto& [paramName, valExpr] : expr->namedArguments) {
                RuntimeValue val = evalExpression(valExpr.get());
                bool found = false;
                for (size_t pi = 0; pi < params.size(); ++pi) {
                    if (params[pi].name == paramName) {
                        if (pi >= args.size()) args.resize(pi + 1);
                        args[pi] = std::move(val);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    reportError("Pipeline '" + expr->functionName +
                                "' has no parameter '" + paramName + "'",
                                expr->location);
                }
            }
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
        // Evaluate positional arguments
        std::vector<RuntimeValue> args;
        for (const auto& arg : expr->arguments) {
            args.push_back(evalExpression(arg.get()));
        }

        // ----------------------------------------------------------------
        // Resolve named (keyword) arguments
        //
        // func(p1, p2, key=val) is equivalent to:
        //   positional = [p1, p2]
        //   named      = {key: val}
        //
        // Algorithm:
        //  1) For each named arg, find its index i in item->params()
        //  2) If i >= args.size(), extend args with defaults up to i
        //     and insert the named value at position i
        //  3) Overwrite if position already filled (explicit named wins)
        // ----------------------------------------------------------------
        if (!expr->namedArguments.empty()) {
            const auto& paramDefs = item->params();
            // Pre-fill defaults for all parameters up to the max needed
            if (args.size() < paramDefs.size()) {
                for (size_t i = args.size(); i < paramDefs.size(); ++i) {
                    if (paramDefs[i].defaultValue.has_value()) {
                        args.push_back(*paramDefs[i].defaultValue);
                    } else {
                        args.push_back(RuntimeValue());  // placeholder
                    }
                }
            }

            for (const auto& [paramName, valExpr] : expr->namedArguments) {
                RuntimeValue val = evalExpression(valExpr.get());
                bool found = false;
                for (size_t pi = 0; pi < paramDefs.size(); ++pi) {
                    if (paramDefs[pi].name == paramName) {
                        if (pi >= args.size()) args.resize(pi + 1);
                        args[pi] = std::move(val);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    reportError("Function '" + expr->functionName +
                                "' has no parameter '" + paramName + "'",
                                expr->location);
                    return RuntimeValue();
                }
            }
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

RuntimeValue Interpreter::evalParamRef(ParamRefExpr* expr) {
    if (!_paramStore) {
        // No store attached – warn and return empty value
        reportError("@" + expr->paramName
                    + " referenced but no params [] block was declared",
                    expr->location);
        return RuntimeValue();
    }

    // Use local cache to avoid shared_mutex on every read
    refreshParamCache();
    auto it = _paramCache.find(expr->paramName);
    if (it == _paramCache.end() || it->second.isNull()) {
        return RuntimeValue();
    }
    const ParamValue& pv = it->second;
    switch (pv.type) {
        case ParamType::INT:    return RuntimeValue(static_cast<double>(pv.asInt()));
        case ParamType::FLOAT:  return RuntimeValue(pv.asFloat());
        case ParamType::STRING: return RuntimeValue(pv.asString());
        case ParamType::BOOL:   return RuntimeValue(pv.asBool());
        default:                return RuntimeValue(pv.asString());
    }
}

// ============================================================================
// Pipeline execution
// ============================================================================

cv::Mat Interpreter::executePipelineDecl(PipelineDecl* pipeline, 
                                          const std::vector<RuntimeValue>& args,
                                          const cv::Mat& input) {
    // ── Optional throughput timing (zero-cost when disabled) ────────────────────
    using clock = std::chrono::steady_clock;
    clock::time_point t0;
    const bool isTopLevel = (_recursionDepth == 0);
    if (_throughputTable) {
        t0 = clock::now();
    }
    checkRecursionLimit();
    ++_recursionDepth;
    // RAII guard: always decrement even if an exception flies out of the body.
    // Without this, a throwing pipeline would leave _recursionDepth stuck > 0
    // so subsequent calls on the same interpreter (exec_interval reuse) would
    // never satisfy the _recursionDepth == 0 recording gate.
    struct DepthGuard {
        size_t& depth;
        ~DepthGuard() { --depth; }
    } depthGuard{_recursionDepth};
    
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
    // _recursionDepth is decremented by DepthGuard RAII above.

    // Record throughput for the outermost call on this interpreter instance.
    // isTopLevel captures depth==0 BEFORE increment, so it is true only for
    // the first (non-nested) executePipelineDecl call on this interpreter.
    if (_throughputTable && isTopLevel) {
        uint64_t durationNs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                clock::now() - t0).count());
        _throughputTable->record(pipeline->name, durationNs);
    }
    
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

// ============================================================================
// Debug throughput helpers  (ThroughputTable methods)
// ============================================================================

void Interpreter::ThroughputTable::record(const std::string& name, uint64_t durationNs) {
    Entry* entry = nullptr;
    {
        std::unique_lock<std::mutex> lk(mutex);
        auto it = entries.find(name);
        if (it == entries.end()) {
            entries[name] = std::make_unique<Entry>();
            it = entries.find(name);
        }
        entry = it->second.get();
    }  // lock released – Entry pointer is stable (unique_ptr value, never moved)

    entry->callCount.fetch_add(1, std::memory_order_relaxed);
    entry->totalNs.fetch_add(durationNs, std::memory_order_relaxed);

    // Lock-free min update
    uint64_t curMin = entry->minNs.load(std::memory_order_relaxed);
    while (durationNs < curMin &&
           !entry->minNs.compare_exchange_weak(curMin, durationNs,
                                               std::memory_order_relaxed))
    { /* retry */ }

    // Lock-free max update
    uint64_t curMax = entry->maxNs.load(std::memory_order_relaxed);
    while (durationNs > curMax &&
           !entry->maxNs.compare_exchange_weak(curMax, durationNs,
                                               std::memory_order_relaxed))
    { /* retry */ }
}

void Interpreter::ThroughputTable::stopPrinter() {
    if (!printerThread.joinable()) return;
    stop.store(true);
    cv.notify_all();
    printerThread.join();
}

void Interpreter::ThroughputTable::startPrinter(double intervalSec) {
    stop.store(false);
    printerThread = std::thread([this, intervalSec]() {
        using namespace std::chrono;
        using clock = steady_clock;

        std::cout << "\n[Throughput] Debug throughput mode active"
                  << " (print interval: " << intervalSec << " s)\n" << std::flush;

        auto lastPrint = clock::now();

        while (!stop.load(std::memory_order_relaxed)) {
            {
                std::unique_lock<std::mutex> lk(cvMtx);
                cv.wait_for(lk, duration<double>(intervalSec),
                            [this]{ return stop.load(std::memory_order_relaxed); });
            }
            if (stop.load(std::memory_order_relaxed)) break;

            auto   now        = clock::now();
            double elapsedSec = duration<double>(now - lastPrint).count();
            lastPrint         = now;

            struct Row {
                std::string name;
                uint64_t total, windowCalls;
                double avgMs, minMs, maxMs, cps;
            };
            std::vector<Row> rows;

            {
                std::unique_lock<std::mutex> lk(mutex);
                for (auto& [pname, entry] : entries) {
                    uint64_t total = entry->callCount.load(std::memory_order_relaxed);
                    uint64_t ns    = entry->totalNs.load(std::memory_order_relaxed);
                    uint64_t minNs = entry->minNs.load(std::memory_order_relaxed);
                    uint64_t maxNs = entry->maxNs.load(std::memory_order_relaxed);

                    uint64_t win     = total - entry->snapCount;
                    entry->snapCount = total;

                    double avgMs = (total > 0) ? (static_cast<double>(ns) / 1e6 / total) : 0.0;
                    double cps   = (elapsedSec > 0) ? (static_cast<double>(win) / elapsedSec) : 0.0;
                    double minMs = (minNs == UINT64_MAX) ? 0.0 : (static_cast<double>(minNs) / 1e6);
                    double maxMs = static_cast<double>(maxNs) / 1e6;

                    rows.push_back({pname, total, win, avgMs, minMs, maxMs, cps});
                }
            }

            // ── Merge fork-child throughput from the shared arena ──────────
            if (forkArena && !shmArenaIsShutdown(forkArena)) {
                auto forkData = shmArenaReadThroughput(forkArena);
                for (const auto& fd : forkData) {
                    std::string rowName = "fork:" + fd.name;
                    uint64_t prevSnap = forkSnapCounts[rowName];
                    uint64_t win      = fd.callCount - prevSnap;
                    forkSnapCounts[rowName] = fd.callCount;

                    double avgMs = (fd.callCount > 0)
                        ? (static_cast<double>(fd.totalNs) / 1e6 / fd.callCount) : 0.0;
                    double cps   = (elapsedSec > 0)
                        ? (static_cast<double>(win) / elapsedSec) : 0.0;
                    double fMinMs = (fd.minNs == UINT64_MAX)
                        ? 0.0 : (static_cast<double>(fd.minNs) / 1e6);
                    double fMaxMs = static_cast<double>(fd.maxNs) / 1e6;

                    rows.push_back({rowName, fd.callCount, win, avgMs, fMinMs, fMaxMs, cps});
                }
            }

            if (rows.empty()) continue;

            // Slowest pipelines first (avg ms descending).
            std::sort(rows.begin(), rows.end(),
                [](const Row& a, const Row& b){ return a.avgMs > b.avgMs; });

            size_t maxNameLen = 8;  // "Pipeline"
            for (const auto& r : rows) maxNameLen = std::max(maxNameLen, r.name.size());

            const int W = static_cast<int>(maxNameLen) + 2;
            const int totalWidth = W + 8 + 10 + 10 + 10 + 10;
            std::ostringstream oss;
            oss << "\n[Throughput] " << std::string(totalWidth, '-') << '\n';
            oss << "[Throughput]  "
                << std::left  << std::setw(W)  << "Pipeline"
                << std::right << std::setw(8)  << "calls"
                << std::right << std::setw(10) << "calls/s"
                << std::right << std::setw(10) << "avg ms"
                << std::right << std::setw(10) << "min ms"
                << std::right << std::setw(10) << "max ms"
                << '\n';
            oss << "[Throughput]  " << std::string(totalWidth, '-') << '\n';

            for (const auto& r : rows) {
                oss << "[Throughput]  "
                    << std::left  << std::setw(W)  << r.name
                    << std::right << std::setw(8)  << r.total
                    << std::right << std::setw(10) << std::fixed << std::setprecision(1) << r.cps
                    << std::right << std::setw(10) << std::fixed << std::setprecision(2) << r.avgMs
                    << std::right << std::setw(10) << std::fixed << std::setprecision(2) << r.minMs
                    << std::right << std::setw(10) << std::fixed << std::setprecision(2) << r.maxMs
                    << '\n';
            }

            std::cout << oss.str() << std::flush;
        }
    });
}

} // namespace visionpipe
/**
 * VisionPipe CLI - Main entry point
 * 
 * A full-featured command-line interface for VisionPipe runtime.
 * 
 * Usage:
 *   visionpipe run <script.vsp> [--param key=value ...] [--verbose]
 *   visionpipe docs [--output <dir>] [--format html|md]
 *   visionpipe validate <script.vsp> [--verbose]
 *   visionpipe version
 *   visionpipe help
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <csignal>
#include <iomanip>
#include <chrono>

#include "interpreter/runtime.h"
#include "interpreter/lexer.h"
#include "interpreter/parser.h"

using namespace visionpipe;

// ============================================================================
// Global state
// ============================================================================

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_paused{false};

void signalHandler(int signum) {
    (void)signum;
    if (g_paused.load()) {
        // If paused, force quit
        std::cout << "\n[VisionPipe] Force quitting..." << std::endl;
        std::exit(1);
    }
    g_running.store(false);
    std::cout << "\n[VisionPipe] Stopping pipeline (press Ctrl+C again to force)..." << std::endl;
    g_paused.store(true);
}

// ============================================================================
// CLI Options
// ============================================================================

struct CLIOptions {
    std::string command;
    std::string scriptPath;
    std::map<std::string, std::string> params;
    std::string outputDir = ".";
    std::string outputFormat = "html";
    std::string pipelineName;  // Specific pipeline to run
    std::string executeItem;   // Item to execute
    bool verbose = false;
    bool quiet = false;
    bool help = false;
    bool showFps = true;
    bool enableDisplay = true;
    bool fpsCounting = false;  // Only count frames when explicitly enabled
    double targetFps = 0;  // 0 = unlimited
    int port = 8080;       // For docs server
};

// ============================================================================
// Usage and version
// ============================================================================

void printBanner() {
    std::cout << R"(
 _    ___      _             ____  _            
| |  / (_)____(_)___  ____  / __ \(_)___  ___   
| | / / / ___/ / __ \/ __ \/ /_/ / / __ \/ _ \  
| |/ / (__  ) / /_/ / / / / ____/ / /_/ /  __/  
|___/_/____/_/\____/_/ /_/_/   /_/ .___/\___/   
                                /_/             
)" << std::endl;

    // VISIONPIPE_VERSION should be provided by CMake (e.g. add_definitions(-DVISIONPIPE_VERSION=\"${PROJECT_VERSION}\"))
#ifndef VISIONPIPE_VERSION
#define VISIONPIPE_VERSION "v1.0.0"
#endif

    std::cout << "  standard edition " << VISIONPIPE_VERSION << std::endl;
}

void printUsage() {
    std::cout << R"(
VisionPipe - Vision Processing Pipeline Runtime
================================================

Usage:
  visionpipe <command> [options]

Commands:
  run <script.vsp>    Execute a VisionPipe script
  validate <script>   Validate script syntax without executing
  docs                Generate documentation for interpreter items
  serve               Start documentation web server
  version             Show version information
  help                Show this help message
  execute <item>      Execute a single interpreter item directly

Run Options:
  --param, -p key=value    Set pipeline parameter (repeatable)
  --pipeline, -P name      Run specific pipeline (default: first or 'main')
  --verbose, -v            Enable verbose output
  --quiet, -q              Suppress non-error output
  --no-display             Disable display windows
  --fps <rate>             Target frame rate (0 = unlimited)
  --fps-counting           Enable frame counting (disabled by default for long-running pipelines)

Docs Options:
  --output, -o <dir>       Output directory (default: current)
  --format, -f <fmt>       Output format: html, md, json (default: html)

Serve Options:
  --port <num>             Port for docs server (default: 8080)

Examples:
  visionpipe run stereo.vsp
  visionpipe run stereo.vsp --param input_left=0 --param input_right=1 --verbose
  visionpipe validate mypipeline.vsp
  visionpipe docs --output ./docs --format html
  visionpipe serve --port 3000
  visionpipe execute "libcam_list_controls()"

Environment Variables:
  VISIONPIPE_LOG_LEVEL     Set log level (debug, info, warn, error)
  VISIONPIPE_CACHE_DIR     Set cache directory for computed calibration data

)" << std::endl;
}

void printVersion() {
    std::cout << "VisionPipe v1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << std::endl;
    std::cout << "Core Features:" << std::endl;
    std::cout << "  + OpenCV image processing" << std::endl;
    std::cout << "  + Multi-threaded pipeline execution" << std::endl;
    std::cout << "  + VSP script interpreter" << std::endl;
    
#ifdef VISIONPIPE_FASTCV_ENABLED
    std::cout << "  + FastCV acceleration" << std::endl;
#endif
#ifdef VISIONPIPE_ICEORYX2_ENABLED
    std::cout << "  + Iceoryx2 IPC (publish_frame, subscribe_frame)" << std::endl;
#endif
#ifdef VISIONPIPE_CUDA_ENABLED
    std::cout << "  + CUDA GPU acceleration" << std::endl;
#endif
#ifdef VISIONPIPE_OPENCL_ENABLED
    std::cout << "  + OpenCL acceleration" << std::endl;
#endif
#ifdef LIBCAMERA_BACKEND
    std::cout << "  + LibCamera camera support" << std::endl;
#endif
}

// ============================================================================
// Argument parsing
// ============================================================================

CLIOptions parseArgs(int argc, char* argv[]) {
    CLIOptions opts;
    
    if (argc < 2) {
        opts.help = true;
        return opts;
    }
    
    opts.command = argv[1];
    
    if (opts.command == "execute" && argc > 2) {
        opts.executeItem = argv[2];
    }
    
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            opts.help = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg == "--no-display") {
            opts.enableDisplay = false;
        } else if ((arg == "--param" || arg == "-p") && i + 1 < argc) {
            std::string param = argv[++i];
            size_t eq = param.find('=');
            if (eq != std::string::npos) {
                opts.params[param.substr(0, eq)] = param.substr(eq + 1);
            } else {
                std::cerr << "Warning: Invalid param format (expected key=value): " << param << std::endl;
            }
        } else if ((arg == "--pipeline" || arg == "-P") && i + 1 < argc) {
            opts.pipelineName = argv[++i];
        } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            opts.outputDir = argv[++i];
        } else if ((arg == "--format" || arg == "-f") && i + 1 < argc) {
            opts.outputFormat = argv[++i];
        } else if (arg == "--fps" && i + 1 < argc) {
            opts.targetFps = std::stod(argv[++i]);
        } else if (arg == "--fps-counting") {
            opts.fpsCounting = true;
        } else if (arg == "--port" && i + 1 < argc) {
            opts.port = std::stoi(argv[++i]);
        } else if (arg[0] != '-' && opts.scriptPath.empty()) {
            opts.scriptPath = arg;
        } else if (arg[0] != '-') {
            std::cerr << "Warning: Ignoring extra argument: " << arg << std::endl;
        } else {
            std::cerr << "Warning: Unknown option: " << arg << std::endl;
        }
    }
    
    return opts;
}

// ============================================================================
// File utilities
// ============================================================================

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

void createDirectory(const std::string& path) {
#ifdef _WIN32
    system(("mkdir \"" + path + "\" 2>nul").c_str());
#else
    system(("mkdir -p \"" + path + "\"").c_str());
#endif
}

// ============================================================================
// Command: run
// ============================================================================

int cmdRun(const CLIOptions& opts) {
    if (opts.scriptPath.empty()) {
        std::cerr << "Error: No script file specified" << std::endl;
        std::cerr << "Usage: visionpipe run <script.vsp>" << std::endl;
        return 1;
    }
    
    if (!fileExists(opts.scriptPath)) {
        std::cerr << "Error: File not found: " << opts.scriptPath << std::endl;
        return 1;
    }
    
    if (!opts.quiet) {
        std::cout << "[VisionPipe] Loading: " << opts.scriptPath << std::endl;
    }
    
    try {
        // Configure runtime
        RuntimeConfig config;
        config.enableDisplay = opts.enableDisplay;
        config.enableLogging = opts.verbose;
        config.targetFps = opts.targetFps;
        config.interpreterConfig.fpsCounting = opts.fpsCounting;
        config.interpreterConfig.verbose = opts.verbose;
        
        // Create runtime
        Runtime runtime(config);
        runtime.loadBuiltins();
        
        // Set verbose parameters
        if (opts.verbose) {
            std::cout << "[VisionPipe] Parameters:" << std::endl;
            for (const auto& [key, value] : opts.params) {
                std::cout << "  " << key << " = " << value << std::endl;
            }
        }
        
        // Track statistics
        uint64_t frameCount = 0;
        auto startTime = std::chrono::high_resolution_clock::now();
        auto lastFpsTime = startTime;
        uint64_t lastFpsFrame = 0;
        double currentFps = 0;
        
        // Set callbacks
        runtime.onFrame([&](const cv::Mat& frame, uint64_t frameNum) {
            (void)frame;
            frameCount = frameNum;
            
            // Calculate FPS every second
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(now - lastFpsTime).count();
            if (elapsed >= 1.0) {
                currentFps = (frameNum - lastFpsFrame) / elapsed;
                lastFpsFrame = frameNum;
                lastFpsTime = now;
                
                if (opts.verbose && opts.showFps) {
                    std::cout << "\r[VisionPipe] Frame: " << frameNum 
                              << " | FPS: " << std::fixed << std::setprecision(1) << currentFps
                              << "     " << std::flush;
                }
            }
        });
        
        runtime.onError([&opts](const std::string& error) {
            std::cerr << "\n[ERROR] " << error << std::endl;
            if (opts.verbose) {
                // Could add stack trace here
            }
        });
        
        runtime.onStop([&opts]() {
            if (!opts.quiet) {
                std::cout << "\n[VisionPipe] Pipeline stopped" << std::endl;
            }
        });
        
        // Set up signal handler
        std::signal(SIGINT, signalHandler);
#ifndef _WIN32
        std::signal(SIGTERM, signalHandler);
#endif
        
        if (!opts.quiet) {
            std::cout << "[VisionPipe] Starting pipeline..." << std::endl;
        }
        
        // Run
        int result = runtime.run(opts.scriptPath);
        
        // Print statistics
        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration<double>(endTime - startTime).count();
        
        if (!opts.quiet) {
            std::cout << std::endl;
            std::cout << "[VisionPipe] " << (result == 0 ? "Completed" : "Failed") << std::endl;
            
            std::cout << "  Total time: " << std::fixed << std::setprecision(2) << totalDuration << "s" << std::endl;
            
            if (opts.fpsCounting) {
                auto stats = runtime.getStats();
                std::cout << "  Frames processed: " << stats.framesProcessed << std::endl;
                if (totalDuration > 0 && stats.framesProcessed > 0) {
                    std::cout << "  Average FPS: " << std::fixed << std::setprecision(1) 
                              << (stats.framesProcessed / totalDuration) << std::endl;
                }
            }
        }
        
        return result;
        
    } catch (const ParseError& e) {
        std::cerr << "[Parse Error] " << e.what() << std::endl;
        std::cerr << "  at " << e.location().toString() << std::endl;
        return 1;
    } catch (const LexerError& e) {
        std::cerr << "[Lexer Error] " << e.what() << std::endl;
        std::cerr << "  at " << e.location().toString() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }
}

// ============================================================================
// Command: validate
// ============================================================================

int cmdValidate(const CLIOptions& opts) {
    if (opts.scriptPath.empty()) {
        std::cerr << "Error: No script file specified" << std::endl;
        std::cerr << "Usage: visionpipe validate <script.vsp>" << std::endl;
        return 1;
    }
    
    if (!fileExists(opts.scriptPath)) {
        std::cerr << "Error: File not found: " << opts.scriptPath << std::endl;
        return 1;
    }
    
    std::cout << "[VisionPipe] Validating: " << opts.scriptPath << std::endl;
    
    try {
        std::string source = readFile(opts.scriptPath);
        
        // Lexical analysis
        std::cout << "  [1/3] Lexical analysis..." << std::flush;
        Lexer lexer(source, opts.scriptPath);
        std::vector<Token> tokens = lexer.tokenize();
        std::cout << " OK (" << tokens.size() << " tokens)" << std::endl;
        
        if (opts.verbose) {
            std::cout << "        Token types: ";
            std::map<TokenType, int> tokenCounts;
            for (const auto& tok : tokens) {
                tokenCounts[tok.type]++;
            }
            bool first = true;
            for (const auto& [type, count] : tokenCounts) {
                if (!first) std::cout << ", ";
                std::cout << Token::typeToString(type) << "(" << count << ")";
                first = false;
            }
            std::cout << std::endl;
        }
        
        // Parsing
        std::cout << "  [2/3] Parsing..." << std::flush;
        Parser parser(tokens, opts.scriptPath);
        auto program = parser.parse();
        
        if (parser.hasErrors()) {
            std::cout << " FAILED" << std::endl;
            std::cerr << "\nParse errors:" << std::endl;
            for (const auto& err : parser.errors()) {
                std::cerr << "  - " << err.what() << std::endl;
                std::cerr << "    at " << err.location().toString() << std::endl;
            }
            return 1;
        }
        
        std::cout << " OK (" << program->pipelines.size() << " pipelines)" << std::endl;
        
        // Semantic analysis
        std::cout << "  [3/3] Semantic analysis..." << std::flush;
        auto cacheDeps = program->analyzeCacheDependencies();
        std::cout << " OK" << std::endl;
        
        // Summary
        std::cout << std::endl;
        std::cout << "Summary:" << std::endl;
        std::cout << "  Pipelines: " << program->pipelines.size() << std::endl;
        
        for (const auto& pipeline : program->pipelines) {
            std::cout << "    - " << pipeline->name;
            if (!pipeline->parameters.empty()) {
                std::cout << "(";
                bool first = true;
                for (const auto& p : pipeline->parameters) {
                    if (!first) std::cout << ", ";
                    std::cout << p.name;
                    first = false;
                }
                std::cout << ")";
            }
            std::cout << " [" << pipeline->body.size() << " statements]" << std::endl;
        }
        
        if (!cacheDeps.empty()) {
            std::cout << "  Cache slots: " << cacheDeps.size() << std::endl;
            if (opts.verbose) {
                for (const auto& dep : cacheDeps) {
                    std::cout << "    - " << dep.cacheId;
                    if (dep.isGlobal) std::cout << " [global]";
                    if (!dep.producerPipeline.empty()) {
                        std::cout << " <- " << dep.producerPipeline;
                    }
                    if (!dep.consumerPipelines.empty()) {
                        std::cout << " -> {";
                        bool first = true;
                        for (const auto& c : dep.consumerPipelines) {
                            if (!first) std::cout << ", ";
                            std::cout << c;
                            first = false;
                        }
                        std::cout << "}";
                    }
                    std::cout << std::endl;
                }
            }
        }
        
        std::cout << std::endl;
        std::cout << "[OK] " << opts.scriptPath << " is valid" << std::endl;
        return 0;
        
    } catch (const LexerError& e) {
        std::cout << " FAILED" << std::endl;
        std::cerr << "\nLexer error: " << e.what() << std::endl;
        std::cerr << "  at " << e.location().toString() << std::endl;
        return 1;
    } catch (const ParseError& e) {
        std::cout << " FAILED" << std::endl;
        std::cerr << "\nParse error: " << e.what() << std::endl;
        std::cerr << "  at " << e.location().toString() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cout << " FAILED" << std::endl;
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }
}

// ============================================================================
// Command: docs (uses doc_gen module)
// ============================================================================

#include "doc_gen.h"

int cmdDocs(const CLIOptions& opts) {
    DocGenOptions docOpts;
    docOpts.outputDir = opts.outputDir;
    docOpts.outputFormat = opts.outputFormat;
    return generateDocs(docOpts);
}

// ============================================================================
// Command: execute
// ============================================================================

int cmdExecute(const CLIOptions& opts) {
    if (opts.executeItem.empty()) {
        std::cerr << "Error: No item specified for execution" << std::endl;
        std::cerr << "Usage: visionpipe execute \"item_name(args)\"" << std::endl;
        return 1;
    }

    std::stringstream ss;
    ss << "pipeline execute\n"
       << "  " << opts.executeItem << "\n"
       << "end\n"
       << "exec_seq execute\n";

    std::string source = ss.str();

    try {
        RuntimeConfig config;
        config.enableDisplay = opts.enableDisplay;
        config.enableLogging = opts.verbose;
        
        Runtime runtime(config);
        runtime.loadBuiltins();

        if (opts.verbose) {
            std::cout << "[VisionPipe] Executing item: " << opts.executeItem << std::endl;
        }

        return runtime.runSource(source, "command_line");
    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }
}

// ============================================================================
// Main entry point
// ============================================================================

#include "utils/camera_device_manager.h"

int main(int argc, char* argv[]) {
    CLIOptions opts = parseArgs(argc, argv);
    
    int result = 0;
    
    if (opts.help || opts.command == "help" || opts.command.empty()) {
        printBanner();
        printUsage();
    } else if (opts.command == "version" || opts.command == "--version") {
        printVersion();
    } else if (opts.command == "run") {
        result = cmdRun(opts);
    } else if (opts.command == "validate") {
        result = cmdValidate(opts);
    } else if (opts.command == "docs") {
        result = cmdDocs(opts);
    } else if (opts.command == "execute") {
        result = cmdExecute(opts);
    } else {
        std::cerr << "Unknown command: " << opts.command << std::endl;
        std::cerr << "Run 'visionpipe help' for usage information" << std::endl;
        result = 1;
    }
    
    // Clean shutdown
    CameraDeviceManager::instance().releaseAll();
    
    return result;
}

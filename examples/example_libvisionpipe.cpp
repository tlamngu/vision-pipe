/**
 * @file example_libvisionpipe.cpp
 * @brief Cross-platform example demonstrating libvisionpipe usage
 *
 * Supported platforms: Linux, macOS, Windows
 *
 * Build (standalone):
 *   # Linux/macOS:
 *   g++ -std=c++17 example_libvisionpipe.cpp -lvisionpipe $(pkg-config --cflags --libs opencv4) -o demo
 *
 *   # Or with CMake (see examples/CMakeLists.txt):
 *   mkdir build && cd build && cmake .. && make
 *
 * Run:
 *   ./demo [path_to_script.vsp]
 *
 * This requires the `visionpipe` CLI to be in PATH and a .vsp script
 * that uses frame_sink("output") to expose frames over shared memory.
 */

#include <libvisionpipe/libvisionpipe.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>

#if defined(_WIN32)
  #include <windows.h>
#endif

static std::atomic<bool> g_running{true};

// Cross-platform signal handling
void sigHandler(int) {
    g_running.store(false, std::memory_order_release);
}

#if defined(_WIN32)
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        g_running.store(false, std::memory_order_release);
        return TRUE;
    }
    return FALSE;
}
#endif

int main(int argc, char* argv[]) {
    // Register signal handlers (cross-platform)
    signal(SIGINT, sigHandler);
#if defined(_WIN32)
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    signal(SIGTERM, sigHandler);
#endif

    // Path to .vsp script (default: examples/libvisionpipe_demo.vsp)
    std::string scriptPath = "examples/libvisionpipe_demo.vsp";
    if (argc > 1) {
        scriptPath = argv[1];
    }

    // ========================================================================
    // 1. Create VisionPipe client
    // ========================================================================
    visionpipe::VisionPipe camera;

    // Optional: configure
    // camera.setExecutablePath("/usr/local/bin/visionpipe");
    // camera.setParam("input", "0");
    camera.setNoDisplay(true);

    // Check that visionpipe is available
    if (!camera.isAvailable()) {
        std::cerr << "Error: visionpipe executable not found in PATH.\n"
                  << "Make sure visionpipe is built and installed.\n";
        return 1;
    }

    std::cout << "VisionPipe: " << camera.version() << std::endl;

    // ========================================================================
    // 2. Run a .vsp script -> get a Session
    // ========================================================================
    auto session = camera.run(scriptPath);
    if (!session) {
        std::cerr << "Failed to start VisionPipe session.\n";
        return 1;
    }

    std::cout << "Session started: " << session->sessionId() << std::endl;

    // ========================================================================
    // 3. Register frame callbacks (event-driven mode)
    // ========================================================================
    uint64_t frameCount = 0;

    session->onFrame("processed", [&](const cv::Mat& frame, uint64_t seq) {
        frameCount++;

        // Example: display the frame
        cv::imshow("libvisionpipe - processed", frame);
        if (cv::waitKey(1) == 27) { // ESC to quit
            g_running.store(false);
        }

        if (frameCount % 100 == 0) {
            std::cout << "Received " << frameCount << " frames (seq=" << seq
                      << ", " << frame.cols << "x" << frame.rows << ")" << std::endl;
        }
    });

    // ========================================================================
    // 4. Main loop - dispatch frames
    // ========================================================================
    std::cout << "Receiving frames... Press ESC or Ctrl+C to stop.\n";

    while (g_running.load() && session->isRunning()) {
        session->spinOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // ========================================================================
    // Alternative: Polling mode (instead of callbacks)
    // ========================================================================
    /*
    cv::Mat frame;
    while (g_running.load() && session->isRunning()) {
        if (session->grabFrame("processed", frame)) {
            cv::imshow("Polled frame", frame);
            if (cv::waitKey(1) == 27) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    */

    // ========================================================================
    // Alternative: Run a script from a string (instead of a file)
    // ========================================================================
    /*
    auto session2 = camera.runString(R"(
        video_cap(0)
        resize(640, 480)
        frame_sink("output")
    )", "inline_demo");

    if (session2) {
        cv::Mat frame;
        while (g_running.load() && session2->isRunning()) {
            if (session2->grabFrame("output", frame)) {
                cv::imshow("runString demo", frame);
                if (cv::waitKey(1) == 27) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        session2->stop();
    }
    */

    // ========================================================================
    // 5. Cleanup
    // ========================================================================
    session->stop();
    std::cout << "Session ended. Total frames: " << frameCount << std::endl;

    return 0;
}

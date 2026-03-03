# libvisionpipe — Developer Reference

**Version**: 1.4.0 | **Platforms**: Linux · macOS · Windows

libvisionpipe is the C++ client SDK for VisionPipe. It spawns a VisionPipe process as a sidecar, manages its lifecycle, and delivers frames produced by `frame_sink()` items to your application via shared memory — all without copying pixel data through IPC buffers.

A Python binding layer (`pyvisionpipe`) wraps the same SDK and exposes frames as NumPy arrays.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Build Instructions](#2-build-instructions)
3. [C++ API Reference](#3-c-api-reference)
   - [VisionPipeConfig](#visionpipeconfig)
   - [SessionState](#sessionstate)
   - [Session](#session)
   - [VisionPipe](#visionpipe)
4. [C++ Usage Examples](#4-c-usage-examples)
5. [Python Bindings (pyvisionpipe)](#5-python-bindings-pyvisionpipe)
6. [frame_sink() — VSP Integration](#6-frame_sink--vsp-integration)
7. [Error Handling](#7-error-handling)
8. [Platform Notes](#8-platform-notes)

---

## 1. Architecture Overview

```
 Your Application
       │
       │  C++ API / Python API
       ▼
 ┌─────────────────┐
 │  VisionPipe     │  ← creates session, manages lifecycle
 │  Session        │  ← connects sink consumers, dispatches frames
 └────────┬────────┘
          │  fork/exec (POSIX) or CreateProcess (Win32)
          ▼
 ┌─────────────────┐
 │  visionpipe CLI │  ← runs the .vsp script
 │  (child process)│
 └────────┬────────┘
          │  POSIX shared memory (shm_open / mmap)
          ▼
 ┌─────────────────┐
 │  frame_sink()   │  ← VSP item: publishes frames to shm segment
 └─────────────────┘
```

**Key properties**:
- **Zero-copy delivery**: cv::Mat data lives in the shared memory segment; no memcpy on the consumer side (Linux/macOS).
- **Process isolation**: The VisionPipe pipeline runs in a separate process; a crash or hang in the script does not crash your application.
- **Named sinks**: Each `frame_sink("name")` in the .vsp script creates an independent shm channel. Multiple sinks per session are supported.
- **Dual access patterns**: `grabFrame()` (polling) or `onFrame()` + `spin()` (callback-driven). Both patterns can be mixed.

---

## 2. Build Instructions

### 2.1 Required CMake Flags

| Flag | Default | Effect |
|------|---------|--------|
| `VISIONPIPE_WITH_LIBVISIONPIPE` | `ON` | Build `libvisionpipe_client.so` / `.dll` |
| `LIB_VISIONPIPE_EXAMPLE` | `OFF` | Build the bundled C++ example (`libvisionpipe_example`) |
| `VISIONPIPE_WITH_PYTHON` | `OFF` | Build the `pyvisionpipe` Python extension module |

> `VISIONPIPE_WITH_PYTHON=ON` requires `VISIONPIPE_WITH_LIBVISIONPIPE=ON`.

### 2.2 Build Commands

```bash
# Minimal — only libvisionpipe shared library
cmake -B build -DVISIONPIPE_WITH_LIBVISIONPIPE=ON
cmake --build build --config Release

# With C++ example
cmake -B build \
  -DVISIONPIPE_WITH_LIBVISIONPIPE=ON \
  -DLIB_VISIONPIPE_EXAMPLE=ON
cmake --build build

# With Python bindings
cmake -B build \
  -DVISIONPIPE_WITH_LIBVISIONPIPE=ON \
  -DVISIONPIPE_WITH_PYTHON=ON
cmake --build build
```

### 2.3 CMake Integration (Downstream)

After `cmake --install build`:

```cmake
find_package(VisionPipe REQUIRED)
target_link_libraries(my_app PRIVATE visionpipe_client)
```

For in-tree use, add the build directory to `CMAKE_PREFIX_PATH`.

### 2.4 Python Installation

After building with `-DVISIONPIPE_WITH_PYTHON=ON`, the generated `.so` file is in `build/python/`. Either:

```bash
# Option A: add to PYTHONPATH
export PYTHONPATH=/path/to/build/python:$PYTHONPATH

# Option B: copy into site-packages
cp build/python/pyvisionpipe*.so "$(python3 -c 'import site; print(site.getsitepackages()[0])')"
```

The higher-level wrapper package at `python/visionpipe/` can be installed with:

```bash
pip install -e python/
```

---

## 3. C++ API Reference

Include path: `#include <libvisionpipe/libvisionpipe.h>`  
Namespace: `visionpipe`  
Link: `-lvisionpipe_client`

---

### VisionPipeConfig

```cpp
struct VisionPipeConfig {
    std::string executablePath;                   // default: "visionpipe" (or "visionpipe.exe")
    bool verbose        = false;                  // --verbose flag to subprocess
    bool noDisplay      = true;                   // --no-display flag (recommended for SDK use)
    std::map<std::string, std::string> params;    // --param key=value pairs
    std::string pipeline;                         // --pipeline name (for multi-pipeline scripts)
    int sinkTimeoutMs   = 10000;                  // ms to wait for frame_sink shm creation
};
```

All fields have sensible defaults. `noDisplay = true` is the recommended setting when running as a library (suppresses GUI windows in the child process).

---

### SessionState

```cpp
enum class SessionState {
    CREATED,    // Session object created, not yet started
    STARTING,   // Child process is being launched
    RUNNING,    // Child process active; sinks may be delivering frames
    STOPPING,   // stop() called; waiting for child to exit
    STOPPED,    // Child exited normally (exitCode == 0)
    ERROR       // Child exited with non-zero exit code
};
```

---

### Session

A `Session` object is created by `VisionPipe::run()` or `VisionPipe::runString()` and represents one running .vsp execution.

**Lifetime**: `Session` manages the child process. Destroying a `Session` object will call `stop()` if the process is still running.

```cpp
class Session {
public:
    // ── Frame Access ────────────────────────────────────────────────────────

    void onFrame(const std::string& sinkName, FrameCallback callback);
    bool grabFrame(const std::string& sinkName, cv::Mat& frame);

    // ── Event Loop ──────────────────────────────────────────────────────────

    void spin(int pollIntervalMs = 1);
    int  spinOnce();

    // ── Lifecycle ───────────────────────────────────────────────────────────

    void stop(int timeoutMs = 3000);
    bool waitForExit(int timeoutMs = 0);

    // ── State ───────────────────────────────────────────────────────────────

    bool         isRunning()  const;
    SessionState state()      const;
    int          exitCode()   const;  // -1 if still running
    const std::string& sessionId() const;
};
```

#### `onFrame(sinkName, callback)`

Registers a `FrameCallback` that is called by `spin()` / `spinOnce()` each time a new frame is available from the named sink.

```cpp
using FrameCallback = std::function<void(const cv::Mat& frame, uint64_t frameNumber)>;
```

- **`frame`**: read-only Mat backed by shared memory. Do not retain references beyond the callback.
- **`frameNumber`**: monotonically increasing counter reset to 0 when the session starts.
- Multiple callbacks may be registered per sink.

```cpp
session->onFrame("output", [](const cv::Mat& frame, uint64_t seq) {
    std::cout << "frame " << seq << " size=" << frame.size() << "\n";
});
```

#### `grabFrame(sinkName, frame)`

Non-blocking poll. Returns `true` only when a **new** frame (not previously seen) is available.

```cpp
cv::Mat frame;
if (session->grabFrame("output", frame)) {
    cv::imshow("live", frame);
}
```

#### `spin(pollIntervalMs)`

Blocking loop: calls `spinOnce()` in a tight loop, sleeping `pollIntervalMs` between iterations. Returns when the session ends or `stop()` is called from another thread.

#### `spinOnce()`

Single-pass dispatch: checks every registered sink for a new frame and fires callbacks. Returns the number of frames dispatched across all sinks. Use this when embedding libvisionpipe in your own event loop.

#### `stop(timeoutMs)`

Sends `SIGTERM` (POSIX) / `TerminateProcess` (Windows) to the child VisionPipe process and waits up to `timeoutMs` for it to exit. If the process does not exit within the timeout it is force-killed.

#### `waitForExit(timeoutMs)`

Blocks until the child process exits. Pass `timeoutMs = 0` to wait indefinitely. Returns `true` if the process exited within the timeout.

#### `exitCode()`

Returns the exit code of the VisionPipe process after it has stopped, or `-1` while still running.

---

### VisionPipe

The main entry point. Holds configuration and creates `Session` objects.

```cpp
class VisionPipe {
public:
    VisionPipe();
    explicit VisionPipe(VisionPipeConfig config);

    // ── Configuration ────────────────────────────────────────────────────────

    void setExecutablePath(const std::string& path);
    void setParam(const std::string& key, const std::string& value);
    void setVerbose(bool v);
    void setNoDisplay(bool nd);
    const VisionPipeConfig& config() const;

    // ── Session Creation ─────────────────────────────────────────────────────

    std::unique_ptr<Session> run(const std::string& scriptPath);
    std::unique_ptr<Session> runString(const std::string& script,
                                       const std::string& name = "inline");

    // ── Utilities ────────────────────────────────────────────────────────────

    int  execute(const std::vector<std::string>& args);
    bool isAvailable() const;
    std::string version() const;
};
```

#### `run(scriptPath)`

Launches `visionpipe run <scriptPath>` as a child process with the configured options. Returns a `unique_ptr<Session>` on success, `nullptr` on launch failure.

#### `runString(script, name)`

Writes `script` to a temporary file, then calls `run()` on it. The temporary file is deleted when the returned `Session` is destroyed. `name` is used only for diagnostics and log messages.

```cpp
auto session = vp.runString(R"(
    video_cap(0)
    resize(640, 480)
    frame_sink("output")
)");
```

#### `execute(args)`

Runs `visionpipe <args>` synchronously (blocking) and returns the exit code. Useful for one-shot tasks such as validation or documentation generation.

#### `isAvailable()`

Checks whether the `visionpipe` executable (as configured by `executablePath`) can be found and executed. Returns `false` if the binary is missing or not executable.

#### `version()`

Runs `visionpipe --version` and returns the version string. Returns an empty string on failure.

---

## 4. C++ Usage Examples

### 4.1 Polling Loop

```cpp
#include <libvisionpipe/libvisionpipe.h>
#include <opencv2/highgui.hpp>

int main() {
    visionpipe::VisionPipe vp;

    if (!vp.isAvailable()) {
        std::cerr << "visionpipe not found\n";
        return 1;
    }

    auto session = vp.run("camera_pipeline.vsp");
    if (!session) {
        std::cerr << "Failed to launch pipeline\n";
        return 1;
    }

    cv::Mat frame;
    while (session->isRunning()) {
        if (session->grabFrame("output", frame)) {
            cv::imshow("VisionPipe", frame);
            if (cv::waitKey(1) == 27) {   // Esc to quit
                session->stop();
                break;
            }
        }
    }

    return session->exitCode() == 0 ? 0 : 1;
}
```

### 4.2 Callback-Driven Loop

```cpp
#include <libvisionpipe/libvisionpipe.h>
#include <atomic>
#include <csignal>

std::atomic<bool> g_running{true};

int main() {
    std::signal(SIGINT, [](int) { g_running = false; });

    visionpipe::VisionPipe vp;
    vp.setNoDisplay(true);

    auto session = vp.run("my_pipeline.vsp");

    session->onFrame("output", [](const cv::Mat& frame, uint64_t seq) {
        // This runs on the spin() thread — keep it fast
        std::printf("frame %llu  size=%dx%d\n",
                    (unsigned long long)seq, frame.cols, frame.rows);
    });

    // spin() blocks until the session ends or stop() is called
    while (g_running && session->isRunning()) {
        session->spinOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    session->stop();
    return 0;
}
```

### 4.3 Inline Script (runString)

```cpp
visionpipe::VisionPipe vp;
vp.setNoDisplay(true);
vp.setParam("device", "0");

auto session = vp.runString(R"(
    video_cap($device)
    resize(640, 480)
    cvt_color(COLOR_BGR2GRAY)
    frame_sink("gray")
)");

cv::Mat gray;
for (int i = 0; i < 100 && session->isRunning(); ++i) {
    if (session->grabFrame("gray", gray)) {
        // Process gray frame…
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

session->stop();
```

### 4.4 Multiple Sinks

```cpp
// .vsp script publishes two sinks:
//   frame_sink("rgb")    – raw color frame
//   frame_sink("depth")  – depth/disparity map

auto session = vp.run("stereo_pipeline.vsp");

session->onFrame("rgb",   [](const cv::Mat& f, uint64_t) { /* … */ });
session->onFrame("depth", [](const cv::Mat& f, uint64_t) { /* … */ });

session->spin();
```

### 4.5 VisionPipeConfig

```cpp
visionpipe::VisionPipeConfig cfg;
cfg.executablePath = "/opt/versys/bin/visionpipe";
cfg.noDisplay      = true;
cfg.verbose        = false;
cfg.sinkTimeoutMs  = 5000;
cfg.params["model"] = "/models/yolov8n.onnx";

visionpipe::VisionPipe vp(cfg);
auto session = vp.run("detection.vsp");
```

---

## 5. Python Bindings (pyvisionpipe)

The Python bindings mirror the C++ API. Frames are delivered as **NumPy arrays** (H×W×C, `uint8`).

### 5.1 Import

```python
# Via the high-level wrapper package (preferred)
import visionpipe

# Or directly via the C extension
import pyvisionpipe as visionpipe
```

### 5.2 Python API Overview

| C++ Member | Python Equivalent |
|------------|-------------------|
| `VisionPipe::run()` | `VisionPipe.run(path)` |
| `VisionPipe::runString()` | `VisionPipe.run_string(script, name)` |
| `VisionPipe::setExecutablePath()` | `VisionPipe.set_executable_path(path)` |
| `VisionPipe::setParam()` | `VisionPipe.set_param(key, value)` |
| `VisionPipe::setVerbose()` | `VisionPipe.set_verbose(v)` |
| `VisionPipe::setNoDisplay()` | `VisionPipe.set_no_display(v)` |
| `VisionPipe::isAvailable()` | `VisionPipe.is_available()` |
| `VisionPipe::version()` | `VisionPipe.version()` |
| `Session::grabFrame()` | `Session.grab_frame(name)` → `(bool, ndarray)` |
| `Session::onFrame()` | `Session.on_frame(name, callback)` |
| `Session::spin()` | `Session.spin(poll_interval_ms=1)` |
| `Session::spinOnce()` | `Session.spin_once()` → `int` |
| `Session::stop()` | `Session.stop(timeout_ms=3000)` |
| `Session::isRunning()` | `Session.is_running()` |
| `Session::state()` | `Session.state` |
| `Session::exitCode()` | `Session.exit_code` |
| `Session::sessionId()` | `Session.session_id` |

`Session` also supports the context manager protocol (`__enter__` / `__exit__`) which calls `stop()` on exit.

### 5.3 Polling Example

```python
import visionpipe
import time

vp = visionpipe.VisionPipe()
vp.set_no_display(True)

print(f"VisionPipe version: {vp.version()}")

session = vp.run("camera_pipeline.vsp")

while session.is_running():
    ok, frame = session.grab_frame("output")
    if ok:
        print(f"frame shape={frame.shape} dtype={frame.dtype}")
    else:
        time.sleep(0.001)

session.stop()
```

### 5.4 Callback Example

```python
import visionpipe

def on_frame(frame, seq):
    if seq % 100 == 0:
        print(f"seq={seq}  shape={frame.shape}")

vp = visionpipe.VisionPipe()
vp.set_no_display(True)

session = vp.run("camera_pipeline.vsp")
session.on_frame("output", on_frame)
session.spin()   # blocks; callbacks fire here
```

### 5.5 Inline Script (run_string)

```python
import visionpipe, time

vp = visionpipe.VisionPipe()
vp.set_no_display(True)

script = """
    video_cap(0)
    resize(640, 480)
    frame_sink("output")
"""

# Context manager ensures clean shutdown
with vp.run_string(script, "demo") as session:
    for _ in range(150):
        ok, frame = session.grab_frame("output")
        if ok:
            print(frame.shape)
        time.sleep(0.033)
```

### 5.6 OpenCV Integration

```python
import visionpipe
import cv2

vp = visionpipe.VisionPipe()
session = vp.run("pipeline.vsp")

while session.is_running():
    ok, frame = session.grab_frame("output")
    if ok:
        # frame is a numpy ndarray — pass directly to OpenCV
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        cv2.imshow("output", gray)
    if cv2.waitKey(1) == 27:
        session.stop()
        break

cv2.destroyAllWindows()
```

---

## 6. frame_sink() — VSP Integration

Any VisionPipe script that publishes frames to libvisionpipe consumers **must** include `frame_sink()` calls.

### Syntax

```vsp
frame_sink("sink_name")
```

The sink name must be a non-empty string literal. It is used as the shared memory segment identifier and must match the name passed to `grabFrame()` / `onFrame()` on the client side.

### Simple Pipeline

```vsp
# pipeline.vsp — publishes webcam frames to "output" sink
video_cap(0)
resize(640, 480)
frame_sink("output")
```

### Multiple Sinks

```vsp
# stereo.vsp — two sinks from stereo processing
video_cap(0)
pipeline stereo_proc
    stereo_capture_split()
    frame_sink("left")
    frame_sink("right")
end
exec_loop stereo_proc
```

### Sink with Processing

```vsp
video_cap(0)
resize(640, 480)
frame_sink("raw")          # full frame before processing

gaussian_blur(5, 5, 0)
canny(50, 150)
frame_sink("edges")        # processed edges
```

> **Note**: `frame_sink()` does not consume the Mat from the pipeline; it publishes a reference and passes the Mat unchanged to the next operation.

---

## 7. Error Handling

### Launch Failure

`run()` and `runString()` return `nullptr` if the child process cannot be started. Always check the return value:

```cpp
auto session = vp.run("myscript.vsp");
if (!session) {
    // Executable not found, permissions issue, or script not found
    return 1;
}
```

### Sink Timeout

If `frame_sink()` does not appear within `VisionPipeConfig::sinkTimeoutMs` (default 10 s), libvisionpipe logs a warning. `grabFrame()` will continue to return `false`; the session remains running (the child process may be producing other output).

### Abnormal Exit

When the child process exits with a non-zero code, `state()` transitions to `SessionState::ERROR`. Check `exitCode()` for the process exit status.

```cpp
session->waitForExit();
if (session->state() == visionpipe::SessionState::ERROR) {
    std::cerr << "Pipeline failed, exit code: " << session->exitCode() << "\n";
}
```

---

## 8. Platform Notes

| Platform | Process Spawn | Shared Memory | Status |
|----------|---------------|---------------|--------|
| Linux | `fork` + `execvp` | `shm_open` / `mmap` | ✅ Fully supported, tested on aarch64/x86-64 |
| macOS | `fork` + `execvp` | `shm_open` / `mmap` | ✅ Compiles and links; runtime tested |
| Windows | `CreateProcess` | Named shared memory | ✅ API complete; under validation |

### Linux-Specific Notes

- The `V4L2_NATIVE_BACKEND` CMake flag enables the native V4L2 camera backend (`-DV4L2_NATIVE_BACKEND=ON`). This is independent of libcamera and provides direct V4L2 ioctl access without going through OpenCV's VideoCapture layer. Requires `linux/videodev2.h`.
- `LIBCAMERA_BACKEND=ON` is required if you use a libcamera-based capture item in your .vsp script.

### macOS-Specific Notes

- Shared memory names on macOS must be 31 characters or fewer (system limit). libvisionpipe auto-truncates session IDs accordingly.
- GUI display (`imshow`) in the child process requires macOS 10.15+ with a native display session.

### Windows-Specific Notes

- The default executable name is `visionpipe.exe`.
- Named shared memory is implemented via `CreateFileMapping` / `MapViewOfFile`.
- Ensure `visionpipe.exe` is in `PATH` or set `executablePath` explicitly.

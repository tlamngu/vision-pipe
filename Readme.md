[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tlamngu/vision-pipe)

> ⚠️ **Security Warning**
>
> The ONLY official repository for vision-pipe is: https://github.com/tlamngu/vision-pipe
>
> The repository https://github.com/Basit3751/vision-pipe is an **UNOFFICIAL copy** and its releases have been identified as **MALWARE**. Do NOT download or run binaries from that repository.

# VisionPipe

VisionPipe is a powerful, domain-specific language (DSL) and high-performance image processing framework for building complex computer vision pipelines. Built on modern C++17 and powered by OpenCV, VisionPipe enables developers to create, manage, and execute sophisticated image processing workflows with minimal boilerplate code.

## Overview

VisionPipe is designed for developers, researchers, and vision engineers who need scalable, efficient image processing. Whether you're working with real-time video streams, machine learning inference pipelines, or complex image analysis workflows, VisionPipe provides clean, intuitive syntax combined with C++ performance — and, since v1.4.0, a first-class cross-platform SDK for embedding VisionPipe inside any C++ or Python application.

### Key Features

- **Domain-Specific Language (DSL)**: Simple, readable syntax specifically designed for vision pipeline definition
- **Modular Pipeline Architecture**: Compose complex pipelines from simple, reusable components
- **Real-Time Video Processing**: Efficient streaming with support for multiple input sources (OpenCV VideoCapture, libcamera, V4L2 native)
- **Deep Learning Integration**: Built-in DNN inference via OpenCV DNN module or ONNX Runtime
- **300+ Vision Operations**: Color space transforms, filtering, morphology, edge detection, feature extraction, stereo vision, and more
- **Hardware Acceleration**: OpenCL and CUDA support via OpenCV; SIMD auto-vectorization
- **Multi-Pipeline Orchestration**: Thread-safe concurrent pipeline execution
- **libvisionpipe SDK** *(new in v1.4.0)*: Embed VisionPipe in any C++ or Python application; receive frames via shared memory with zero-copy delivery
- **Python Bindings** *(new in v1.4.0)*: Full `pyvisionpipe` extension module — frames returned as NumPy arrays
- **V4L2 Native Backend** *(new in v1.4.0)*: Direct V4L2 ioctl camera access on Linux, bypassing OpenCV's VideoCapture layer
- **Cross-Platform**: Linux, macOS, Windows

## Release Status (v1.4.0)

⚠️ **Pre-Production Notice**: VisionPipe v1.x is not yet production-ready. Please review the stability status below.

### What's New in v1.4.0

| Feature | Description |
|---------|-------------|
| **libvisionpipe SDK** | Cross-platform C++ shared library to embed VisionPipe as a sidecar process. Zero-copy shared-memory frame delivery via `frame_sink()`. |
| **Python bindings (pyvisionpipe)** | pybind11-based Python extension. Frames delivered as NumPy arrays. Supports polling, callback, `run_string()`, and context manager patterns. |
| **V4L2 Native Backend** | Direct Linux V4L2 camera access without OpenCV VideoCapture. Enable with `-DV4L2_NATIVE_BACKEND=ON`. |
| **`runString()` API** | Launch a VisionPipe pipeline directly from a C++/Python string without creating a file on disk. |
| **Graceful Ctrl+C shutdown** | SIGINT now calls `runtime.stop()` cleanly instead of hard-aborting the process. |

### Stability by Component

| Component | Status | Notes |
|-----------|--------|-------|
| **Core Pipeline Engine** | ✅ Stable | Well-tested, production-quality |
| **ONNX Runtime Backend** | ✅ Stable | Fully tested |
| **OpenCV DNN Module** | ✅ Stable | Extensively tested |
| **libvisionpipe SDK** | ⏳ Beta | New in v1.4.0; Linux/macOS tested, Windows under validation |
| **Python Bindings** | ⏳ Beta | New in v1.4.0; API stable, broader testing ongoing |
| **V4L2 Native Backend** | ⏳ Beta | Linux only, tested on aarch64 |
| **Iceoryx2 IPC Support** | ⚠️ Untested | Not yet validated; use with caution |
| **OpenCV Operations** | ⏳ Partial | Some operations not thoroughly tested |

### Known Issues

**Memory Leaks in Cache Management**: Under specific conditions where the cache is not properly utilized, pipelines may experience memory leaks. The primary issue was resolved in v1.2; similar errors may persist in edge cases. If you encounter memory-related issues, ensure proper cache initialization and refer to `examples/test_cache.vsp`.

**Libcamera Dependencies Issue**: Due to a design constraint introduced with the libcamera integration, all builds currently require `-DLIBCAMERA_BACKEND=ON` to target camera services. This is a known issue planned for resolution in a future release.

### For Production Users

Current v1.x releases are suitable for research, prototyping, non-critical applications, and evaluation. Wait for v2.x or an explicit "stable preview" tag for production deployment.

## Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 6+, or MSVC 2017+)
- CMake 3.16 or higher
- OpenCV 4.x (`core imgproc imgcodecs videoio highgui calib3d`)

### Building from Source

```bash
git clone https://github.com/tlamngu/vision-pipe.git
cd vision-pipe
mkdir build && cd build
cmake .. -DOpenCV_DIR=<path-to-opencv>
cmake --build . --config Release
```

### Running a Pipeline

Create `hello.vsp`:

```vsp
pipeline main
    video_cap(0)
    draw_text("Hello from VisionPipe", 50, 50)
    imshow("Hello VisionPipe")
    wait_key(30)
end

exec_loop main
```

```bash
visionpipe run hello.vsp
```

### Using libvisionpipe (C++)

Add `frame_sink("output")` to your .vsp script:

```vsp
video_cap(0)
resize(640, 480)
frame_sink("output")
```

Then consume frames from your C++ application:

```cpp
#include <libvisionpipe/libvisionpipe.h>

visionpipe::VisionPipe vp;
auto session = vp.run("camera_pipeline.vsp");

session->onFrame("output", [](const cv::Mat& frame, uint64_t seq) {
    // frame is backed by shared memory — zero-copy
    cv::imshow("live", frame);
    cv::waitKey(1);
});

session->spin();  // block and dispatch frames
```

Build with `-DVISIONPIPE_WITH_LIBVISIONPIPE=ON` and link against `visionpipe_client`.

### Using pyvisionpipe (Python)

Build with `-DVISIONPIPE_WITH_PYTHON=ON`:

```python
import visionpipe, time

vp = visionpipe.VisionPipe()
vp.set_no_display(True)

session = vp.run("camera_pipeline.vsp")

while session.is_running():
    ok, frame = session.grab_frame("output")
    if ok:
        print(frame.shape)   # e.g. (480, 640, 3)
    time.sleep(0.001)

session.stop()
```

See [docs/libvisionpipe.md](docs/libvisionpipe.md) for the full API reference.

### V4L2 Native Backend

Enable direct V4L2 camera capture on Linux:

```bash
cmake .. -DV4L2_NATIVE_BACKEND=ON
```

In your .vsp script, use the V4L2-native capture item instead of `video_cap()`. This bypasses OpenCV's VideoCapture and provides lower latency access to camera controls.

## Build Configuration

### Dependencies

| Dependency | Minimum | Recommended | Notes |
|------------|---------|-------------|-------|
| OpenCV | 4.8.0 | 4.14+ | Required |
| pybind11 | 3.0.2 | latest | Required for Python bindings |
| ONNX Runtime | recent | latest | Optional; required for `VISIONPIPE_WITH_ONNXRUNTIME` |
| Iceoryx2 | 0.8.0 | latest stable | Optional; required for `VISIONPIPE_WITH_ICEORYX2` |
| FastCV | vendor SDK | vendor SDK | Optional; required for `VISIONPIPE_WITH_FASTCV` |

### Available Build Flags

| Feature | CMake Flag | Default | Description |
|---------|-----------|---------|-------------|
| **CLI Application** | `VISIONPIPE_BUILD_CLI` | ON | Build the `visionpipe` command-line interpreter |
| **Unit Tests** | `VISIONPIPE_BUILD_TESTS` | OFF | Build test suite |
| **Examples** | `VISIONPIPE_BUILD_EXAMPLES` | OFF | Build C++ example programs |
| **Documentation** | `VISIONPIPE_BUILD_DOCS` | OFF | Generate documentation |
| **CUDA Support** | `VISIONPIPE_WITH_CUDA` | OFF | NVIDIA GPU acceleration |
| **OpenCL Support** | `VISIONPIPE_WITH_OPENCL` | ON | AMD/Intel/NVIDIA GPU acceleration |
| **DNN Module** | `VISIONPIPE_WITH_DNN` | ON | OpenCV deep learning module |
| **ONNX Runtime** | `VISIONPIPE_WITH_ONNXRUNTIME` | ON | Alternative DNN inference backend |
| **FastCV** | `VISIONPIPE_WITH_FASTCV` | OFF | FastCV extensions (Qualcomm SDK required) |
| **Iceoryx2 IPC** | `VISIONPIPE_WITH_ICEORYX2` | OFF | Inter-process communication via Iceoryx2 |
| **IP Camera Streaming** | `VISIONPIPE_IP_CAM` | OFF | HTTP-based MJPEG IP streaming |
| **libcamera Backend** | `LIBCAMERA_BACKEND` | OFF | Native libcamera integration |
| **V4L2 Native Backend** | `V4L2_NATIVE_BACKEND` | OFF | Direct V4L2 Linux camera access |
| **libvisionpipe SDK** | `VISIONPIPE_WITH_LIBVISIONPIPE` | ON | Build `libvisionpipe_client` shared library |
| **libvisionpipe Example** | `LIB_VISIONPIPE_EXAMPLE` | OFF | Build the bundled C++ SDK example |
| **Python Bindings** | `VISIONPIPE_WITH_PYTHON` | OFF | Build `pyvisionpipe` Python extension (⚠️ requires `VISIONPIPE_WITH_LIBVISIONPIPE=ON`) |
| **SIMD Optimization** | `VISIONPIPE_ENABLE_SIMD` | ON | CPU SIMD vectorization (AVX2/SSE) |
| **Link-Time Optimization** | `VISIONPIPE_ENABLE_LTO` | ON | Aggressive compiler optimization |

### Example Build Commands

```bash
# Full SDK build with Python bindings
cmake .. \
  -DVISIONPIPE_WITH_LIBVISIONPIPE=ON \
  -DVISIONPIPE_WITH_PYTHON=ON \
  -DLIB_VISIONPIPE_EXAMPLE=ON

# Enable V4L2 native camera backend (Linux only)
cmake .. -DV4L2_NATIVE_BACKEND=ON

# Enable IP streaming (MJPEG)
cmake .. -DVISIONPIPE_IP_CAM=ON

# Enable ONNX Runtime + libcamera
cmake .. \
  -DVISIONPIPE_WITH_ONNXRUNTIME=ON \
  -DLIBCAMERA_BACKEND=ON

# Disable SIMD/LTO for debugging
cmake .. \
  -DVISIONPIPE_ENABLE_SIMD=OFF \
  -DVISIONPIPE_ENABLE_LTO=OFF
```

## Architecture

### Core Components

#### Pipeline Engine

The pipeline execution engine chains image processing operations, manages intermediate results, supports sequential and threaded execution, and provides efficient memory management.

#### Interpreter

The VisionPipe interpreter parses and executes `.vsp` script files, providing access to 300+ built-in vision operations with full control flow support (conditionals, loops, branches).

#### Pipeline Modules

| Module | Purpose |
|--------|---------|
| **Advanced** | Background subtraction, optical flow, keypoint detection |
| **Arithmetic** | Pixel-level mathematical operations |
| **Color** | Color space conversions (RGB, HSV, LAB, etc.) |
| **Conditional** | Branching and conditional execution |
| **Control** | Loop and execution control structures |
| **Display** | Window management and visualization |
| **DNN** | Deep neural network inference |
| **Draw** | Geometric drawing primitives |
| **Edge** | Edge detection (Canny, Sobel, etc.) |
| **Feature** | Feature detection and extraction (ORB, SIFT, etc.) |
| **Filter** | Image filtering (Gaussian, bilateral, etc.) |
| **GUI** | Interactive trackbars and controls |
| **Math** | Mathematical operations |
| **Morphology** | Erosion, dilation, etc. |
| **Stereo** | Stereo vision and 3D reconstruction |
| **Tensor** | Multi-dimensional array operations |
| **Transform** | Resize, rotate, warp, etc. |
| **Video I/O** | Video capture, encoding, file handling |

### libvisionpipe Architecture

```
 Your App (C++ or Python)
        │
        │ libvisionpipe API
        ▼
 ┌────────────────┐       fork/exec
 │ VisionPipe     ├───────────────────► visionpipe CLI (child process)
 │ Session        │                           │
 └───────┬────────┘                    runs .vsp script
         │                                    │
         │ POSIX shm / Win32 named memory     │ frame_sink("name")
         └────────────────────────────────────┘
               zero-copy frame delivery
```

## Examples

### 1. Real-Time Video Processing

`get-started.vsp` — reads webcam, applies trackbar-controlled filters, shows live preview.

### 2. Image Classification

`image_classification.vsp` — loads a pre-trained DNN model, preprocesses frames, runs inference, displays classification results.

### 3. Object Detection

`object_detection.vsp` — YOLO/SSD models with real-time bounding boxes.

### 4. Stereo Vision

`stereo.vsp` — stereo rectification, disparity computation, 3D depth mapping.

### 5. libvisionpipe SDK

`examples/example_pyvisionpipe.py` — demonstrates polling, callbacks, `run_string()`, and context manager patterns from Python.

## Language Features

### Pipeline Definition

```vsp
pipeline name
    operation1(param1, param2)
    operation2(param)
end

exec_loop name
```

### Control Flow

```vsp
if condition
    operation1()
else
    operation2()
end

loop 10
    operation()
end
```

### Data Types

- `int` — integer values
- `float` — floating-point numbers
- `bool` — boolean values
- `string` — text strings
- `Mat` — image matrices (OpenCV `cv::Mat`)

## Documentation

| Document | Description |
|----------|-------------|
| [docs/visionpipe-docs.md](docs/visionpipe-docs.md) | Full .vsp language and item reference (400+ functions) |
| [docs/libvisionpipe.md](docs/libvisionpipe.md) | libvisionpipe C++ SDK and Python bindings API reference |
| [docs/visionpipe-docs.html](docs/visionpipe-docs.html) | Interactive HTML documentation |

## Project Structure

```
vision-pipe/
├── include/                  # Public headers
│   ├── interpreter/          # Interpreter API
│   ├── libvisionpipe/        # libvisionpipe SDK header
│   ├── pipeline/             # Pipeline engine API
│   └── utils/                # Utilities
├── src/                      # Implementation
│   ├── cli/                  # CLI entry point
│   ├── interpreter/          # Interpreter & built-in items
│   ├── libvisionpipe/        # libvisionpipe SDK implementation
│   ├── pipeline/             # Pipeline engine
│   └── utils/                # Camera device managers, HTTP server
├── python/                   # Python bindings
│   ├── pyvisionpipe.cpp      # pybind11 C++ extension
│   └── visionpipe/           # Python wrapper package
├── examples/                 # Example .vsp scripts and Python programs
├── docs/                     # Documentation
├── cmake/                    # CMake helper modules
└── CMakeLists.txt
```

## System Requirements

### Supported Platforms

| OS | Status | Notes |
|----|--------|-------|
| **Linux** | ✅ Supported | Ubuntu 16.04+; aarch64 and x86-64 tested |
| **Windows** | ✅ Supported | Windows 10/11; MSVC 2017+ |
| **macOS** | ⏳ Partial | libvisionpipe SDK compiles and runs; full CLI testing ongoing |

### Minimum Hardware

- **CPU**: x86-64 with SSE4.1, or ARMv8 (aarch64)
- **Memory**: 2 GB RAM (8 GB+ recommended for DNN workloads)
- **Compiler**: GCC 7+, Clang 6+, or MSVC 2017+

## Use Cases

1. **Real-Time Video Analysis** — surveillance, monitoring, live streaming
2. **Machine Learning Pipelines** — preprocessing, DNN inference, post-processing
3. **Embedded Vision** — libvisionpipe SDK integration in robotics and IoT applications
4. **Industrial Vision** — quality control, defect detection, measurement
5. **Research** — rapid prototyping of vision algorithms
6. **Data Processing** — batch image processing and analysis

## Contributing

Contributions are welcome! Please open issues and pull requests on the GitHub repository.

## License

VisionPipe is licensed under the **Versys-GPL-License-v1**. See [Versys-GPL-License-v1.md](Versys-GPL-License-v1.md) for full terms.

## Support

- Open an issue on GitHub
- Review documentation in `docs/`
- See example scripts in `examples/`

---

**Current Version**: 1.4.0

VisionPipe is actively developed and maintained by Versys Engineers.

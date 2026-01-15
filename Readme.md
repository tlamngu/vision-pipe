# VisionPipe

VisionPipe is a powerful, domain-specific language (DSL) and high-performance image processing framework designed for building complex computer vision pipelines. Built on modern C++17 and powered by OpenCV, VisionPipe enables developers to create, manage, and execute sophisticated image processing workflows with minimal boilerplate code.

## Overview

VisionPipe is designed for developers, researchers, and vision engineers who need to build scalable, efficient image processing applications. Whether you're working with real-time video streams, designing machine learning inference pipelines, or implementing complex image analysis workflows, VisionPipe provides a clean, intuitive syntax combined with powerful C++ performance.

### Key Features

- **Domain-Specific Language (DSL)**: A simple, readable syntax specifically designed for vision pipeline definition
- **Modular Pipeline Architecture**: Build complex pipelines by composing simple, reusable components
- **Real-Time Video Processing**: Efficient streaming processing with support for multiple input sources
- **Deep Learning Integration**: Built-in support for DNN inference via OpenCV DNN module or ONNX Runtime
- **Advanced Image Processing**: Comprehensive library of 300+ vision operations including:
  - Color space transformations
  - Filtering and morphological operations
  - Edge detection and feature extraction
  - Stereo vision and 3D reconstruction
  - Background subtraction
  - Optical flow calculation
  - Image arithmetic and geometric transforms
- **Hardware Acceleration**: OpenCL support for GPU acceleration and SIMD optimizations
- **Multi-Pipeline Orchestration**: Execute multiple pipelines concurrently with thread-safe operation
- **Cross-Platform**: Windows, Linux, and macOS support
- **Interactive GUI**: Built-in visualization with trackbars and parameter controls

## Release Status (v1.2.1)

⚠️ **Pre-Production Notice**: VisionPipe v1.x is not yet production-ready. Please review the stability status below.

### Stability by Component

| Component | Status | Notes |
|-----------|--------|-------|
| **ONNX Runtime Backend** | ✅ Stable | Fully tested and verified to run reliably |
| **OpenCV DNN Module** | ✅ Stable | Extensively tested and stable in the stable channel |
| **Core Pipeline Engine** | ✅ Stable | Well-tested and production-quality |
| **Iceoryx2 IPC Support** | ⚠️ Untested | Not yet validated; use with caution |
| **OpenCV Operations** | ⏳ Partial | Some operations have not been thoroughly tested |
| **Overall Coverage** | ⏳ Partial | Not all items have been comprehensively tested |

### Important Notes

- This release incorporates AI-generated code that has been reviewed and verified by Versys Engineers
- Production deployment is **not recommended** until v2.x or any release marked with "stable preview"
- Core vision operations are mature, but edge cases and comprehensive compatibility testing are still ongoing

### Known Issues

**Memory Leaks in Cache Management**: Under specific conditions where the cache is not properly utilized, pipelines may experience memory leaks. While the primary issue was resolved in v1.2, similar errors may persist in edge cases or specific usage patterns. If you encounter memory-related issues:
- Ensure proper cache initialization and cleanup in your pipelines
- Refer to the cache management examples in `examples/test_cache.vsp`
- Report specific conditions where this occurs to help improve future releases

### For Production Users

If you require production-ready software, please wait for v2.x or look for releases explicitly marked as "stable preview". Current v1.x releases are suitable for:
- Research and development
- Prototyping
- Non-critical applications
- Testing and evaluation

### New features

Now supports IP streaming of frames under feature flag of `-DVISIONPIPE_IP_CAM=ON`. You may check `examples/ip_stream_items.vsp` for details.  

## Quick Start

### Installation

#### Prerequisites
- C++17 compatible compiler (GCC, Clang, or MSVC)
- CMake 3.16 or higher
- OpenCV 4.x

#### Building from Source

```bash
# Clone the repository
git clone <repository-url>
cd vision-pipe

# Create build directory
mkdir build
cd build

# Configure and build
cmake .. -DOpenCV_DIR=<path-to-opencv>
cmake --build . --config Release

# (Optional) Install
cmake --install .
```

### Basic Example

Create a simple `hello.vsp` file:

```vsp
# Simple camera stream with text overlay
pipeline main
    video_cap(0)
    draw_text("Hello from VisionPipe", 50, 50)
    imshow("Hello VisionPipe")
    wait_key(30)
end

exec_loop main
```

Run it with:

```bash
visionpipe.exe run hello.vsp
```

## Architecture

### Core Components

#### **Pipeline Engine**
The heart of VisionPipe is the pipeline execution engine, which:
- Chains image processing operations together
- Manages intermediate results and data caching
- Supports both sequential and threaded execution
- Provides efficient memory management

#### **Interpreter**
The VisionPipe interpreter:
- Parses `.vsp` script files
- Manages the runtime execution environment
- Provides access to 300+ built-in vision operations
- Handles control flow, loops, and conditional logic

#### **Pipeline Modules**

| Module | Purpose |
|--------|---------|
| **Advanced** | Background subtraction, optical flow, keypoint detection |
| **Arithmetic** | Pixel-level mathematical operations |
| **Color** | Color space conversions (RGB, HSV, LAB, etc.) |
| **Conditional** | Branching and conditional execution |
| **Control** | Loop and execution control structures |
| **Display** | Window management and visualization |
| **DNN** | Deep neural network inference (classification, detection) |
| **Draw** | Geometric drawing (lines, circles, rectangles, text) |
| **Edge** | Edge detection algorithms (Canny, Sobel, etc.) |
| **Feature** | Feature detection and extraction (ORB, SIFT, etc.) |
| **Filter** | Image filtering (Gaussian, bilateral, etc.) |
| **GUI** | Interactive trackbars and controls |
| **Math** | Mathematical operations and analysis |
| **Morphology** | Morphological operations (erosion, dilation, etc.) |
| **Stereo** | Stereo vision and 3D reconstruction |
| **Tensor** | Multi-dimensional array operations |
| **Transform** | Image transformations (resize, rotate, warp, etc.) |
| **Video I/O** | Video capture, encoding, and file handling |

## Examples

### 1. Real-Time Video Processing with Controls

The `get-started.vsp` example demonstrates:
- Reading from webcam
- Creating interactive control windows with trackbars
- Applying filters based on user input
- Real-time visualization

### 2. Image Classification with DNN

The `image_classification.vsp` example shows:
- Loading pre-trained neural network models
- Preprocessing input images
- Running inference
- Displaying classification results

### 3. Object Detection

The `object_detection.vsp` example demonstrates:
- Using YOLO or SSD models
- Drawing bounding boxes around detected objects
- Real-time detection on video streams

### 4. Advanced Camera Processing

The `advance_camera_process.vsp` example includes:
- Camera calibration
- Distortion correction
- Advanced preprocessing

### 5. Stereo Vision

The `stereo.vsp` example showcases:
- Stereo rectification
- Disparity computation
- 3D depth mapping

## Build Configuration

VisionPipe offers numerous build options to customize features and optimize performance:

### Available Build Features

VisionPipe provides extensive customization options. Here are the main build features you can enable or disable:

| Feature | CMake Flag | Default | Description |
|---------|-----------|---------|-------------|
| **CLI Application** | `VISIONPIPE_BUILD_CLI` | ON | Build command-line interpreter |
| **Unit Tests** | `VISIONPIPE_BUILD_TESTS` | OFF | Build test suite |
| **Examples** | `VISIONPIPE_BUILD_EXAMPLES` | OFF | Build example applications |
| **Documentation** | `VISIONPIPE_BUILD_DOCS` | OFF | Generate documentation |
| **CUDA Support** | `VISIONPIPE_WITH_CUDA` | OFF | NVIDIA GPU acceleration |
| **OpenCL Support** | `VISIONPIPE_WITH_OPENCL` | ON | AMD/Intel/NVIDIA GPU acceleration |
| **DNN Module** | `VISIONPIPE_WITH_DNN` | ON | OpenCV deep learning module |
| **ONNX Runtime** | `VISIONPIPE_WITH_ONNXRUNTIME` | ON | Alternative DNN inference backend |
| **FastCV** | `VISIONPIPE_WITH_FASTCV` | OFF | FastCV extensions (Qualcomm) |
| **Iceoryx2 IPC** | `VISIONPIPE_WITH_ICEORYX2` | OFF | Inter-process communication |
| **SIMD Optimization** | `VISIONPIPE_ENABLE_SIMD` | ON | CPU SIMD vectorization (AVX2/SSE) |
| **Link-Time Optimization** | `VISIONPIPE_ENABLE_LTO` | ON | Aggressive compiler optimization |

### Feature Flags Example

```cmake
# Enable/disable specific features during build
cmake .. \
  -DVISIONPIPE_BUILD_CLI=ON \
  -DVISIONPIPE_WITH_CUDA=OFF \
  -DVISIONPIPE_WITH_ONNXRUNTIME=ON \
  -DVISIONPIPE_WITH_FASTCV=OFF \
  -DVISIONPIPE_WITH_ICEORYX2=OFF
```

### Performance Options

```cmake
# Enable SIMD optimizations and link-time optimization
cmake .. \
  -DVISIONPIPE_ENABLE_SIMD=ON \
  -DVISIONPIPE_ENABLE_LTO=ON
```

### Hardware Acceleration

- **OpenCL**: GPU acceleration via OpenCV's OpenCL module
- **CUDA**: GPU acceleration for supported operations
- **SIMD**: Auto-vectorization using AVX2/SSE
- **ONNX Runtime**: High-performance DNN inference backend

## Language Features

### Pipeline Definition

```vsp
pipeline name
    # Operations are chained sequentially
    operation1(param1, param2)
    operation2(param)
    operation3()
end
```

### Control Flow

```vsp
# Conditional execution
if condition
    operation1()
else
    operation2()
end

# Loop execution
loop 10
    operation()
end

# Infinite loop with break
exec_loop pipeline_name
```

### Data Types

- **int**: Integer values
- **float**: Floating-point numbers
- **bool**: Boolean values
- **string**: Text strings
- **Mat**: Image matrices (OpenCV cv::Mat)

## Performance Characteristics

VisionPipe is optimized for performance:

- **SIMD Optimizations**: Auto-vectorization on modern CPUs (AVX2, SSE)
- **Link-Time Optimization (LTO)**: Aggressive code optimization
- **Memory Efficiency**: Smart caching of intermediate results
- **Parallel Execution**: Multi-threaded pipeline groups for concurrent processing
- **GPU Support**: OpenCL and CUDA acceleration available

## Documentation

Comprehensive documentation is auto-generated and available in:
- [Full Documentation](docs/visionpipe-docs.md) - Complete API reference with 400+ functions
- [HTML Documentation](docs/visionpipe-docs.html) - Interactive documentation

## Project Structure

```
vision-pipe/
├── include/              # Public header files
│   ├── interpreter/      # Interpreter interface
│   ├── pipeline/         # Pipeline architecture
│   └── utils/            # Utility functions
├── src/                  # Implementation
│   ├── cli/              # Command-line interface
│   ├── interpreter/      # Interpreter implementation
│   ├── pipeline/         # Pipeline engine
│   └── utils/            # Utilities
├── examples/             # Example .vsp scripts
├── docs/                 # Documentation
├── cmake/                # CMake configuration
└── CMakeLists.txt        # Build configuration
```

## System Requirements

### Supported Operating Systems

| OS | Status | Notes |
|----|--------|-------|
| **Windows** | ✅ Fully Supported | Windows 7+, tested on Windows 10/11 |
| **Linux** | ✅ Supported | Ubuntu 16.04+, tested on Ubuntu 20.04+ (comprehensive testing ongoing) |
| **macOS** | 🗓️ Planned | Support in development, not yet available |

### Hardware Requirements

#### Minimum Requirements
- **CPU**: x86-64 processor with SSE4.1 support
- **Memory**: 2GB RAM
- **Compiler**: GCC 7+, Clang 6+, or MSVC 2017+

#### Recommended
- **CPU**: Modern x86-64 with AVX2 support
- **GPU**: NVIDIA (CUDA) or AMD/Intel (OpenCL)
- **Memory**: 8GB+ RAM for large image processing

## Dependencies

### Core
- **OpenCV**: 4.x (core, imgproc, videoio, highgui, calib3d)

### Optional
- **CUDA Toolkit**: For NVIDIA GPU acceleration
- **OpenCL**: For general-purpose GPU acceleration
- **ONNX Runtime**: For alternative DNN inference
- **Iceoryx2**: For inter-process communication

## Use Cases

VisionPipe is ideal for:

1. **Real-Time Video Analysis**: Surveillance, monitoring, live streaming
2. **Machine Learning Pipeline**: Preprocessing, DNN inference, post-processing
3. **Industrial Vision**: Quality control, defect detection, measurement
4. **Robotics**: Vision-guided navigation and object manipulation
5. **Medical Imaging**: Image analysis and visualization
6. **Research**: Rapid prototyping of vision algorithms
7. **Data Processing**: Batch image processing and analysis

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## License

VisionPipe is licensed under the **Versys-GPL-License-v1.md**. Please refer to the license file in the repository root for full terms and conditions.

## Support

For issues, questions, or feature requests, please:
- Open an issue on the GitHub repository
- Check the documentation in `docs/`
- Review example scripts in `examples/`

## Version

**Current Version**: 1.2.0

VisionPipe is actively developed and maintained.

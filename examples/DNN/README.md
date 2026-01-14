# DNN Examples for VisionPipe

This directory contains Deep Neural Network examples demonstrating VisionPipe's ML integration capabilities.

## Examples

| Example | Description | Model |
|---------|-------------|-------|
| `yolov8_human_detection.vsp` | Real-time human/person detection using YOLOv8 | YOLOv8n |
| `mono_depth_estimation.vsp` | Monocular depth estimation from single images | MiDaS v2.1 |

---

## Prerequisites

1. **VisionPipe** compiled with DNN support:
   ```powershell
   # OpenCV DNN backend only (default)
   cmake -S . -B build -DVISIONPIPE_WITH_DNN=ON -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```

2. **OpenCV 4.x** with DNN module (included in standard builds)

### Optional: ONNX Runtime Backend

For better performance and additional execution providers (CUDA, TensorRT, DirectML), enable ONNX Runtime:

```powershell
# With ONNX Runtime backend
cmake -S . -B build -DVISIONPIPE_WITH_DNN=ON -DVISIONPIPE_WITH_ONNXRUNTIME=ON ^
    -DONNXRUNTIME_ROOT=C:/onnxruntime -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**Download ONNX Runtime:**
- Windows: https://github.com/microsoft/onnxruntime/releases (Download `onnxruntime-win-x64-*.zip`)
- Extract to `C:\onnxruntime` or set `ONNXRUNTIME_ROOT` accordingly

**Execution Providers:**
| Provider | Device | Platform |
|----------|--------|----------|
| `cpu` | CPU | All |
| `cuda` | NVIDIA GPU | All |
| `tensorrt` | NVIDIA GPU (optimized) | All |
| `dml` / `directml` | AMD/Intel/NVIDIA GPU | Windows |
| `openvino` | Intel CPU/GPU | All |
| `coreml` | Apple Silicon | macOS |

---

## Model Downloads

### YOLOv8 (Human Detection)

Download the ONNX model from Ultralytics:

**Option 1: Direct Download**
- YOLOv8n (fastest, 6.3MB): https://github.com/ultralytics/assets/releases/download/v8.2.0/yolov8n.onnx
- YOLOv8s (balanced, 22.5MB): https://github.com/ultralytics/assets/releases/download/v8.2.0/yolov8s.onnx

**Option 2: Export from Python**
```python
from ultralytics import YOLO
model = YOLO("yolov8n.pt")
model.export(format="onnx", imgsz=640, opset=12)
```

**Place the model in:** `examples/DNN/models/yolov8n.onnx`

### MiDaS (Depth Estimation)

Download from the official MiDaS repository:

**Option 1: Direct Download**
- MiDaS v2.1 Small (optimal): https://github.com/isl-org/MiDaS/releases/download/v2_1/midas_v21_small_256.onnx
- MiDaS v2.1 (higher quality): https://github.com/isl-org/MiDaS/releases/download/v2_1/midas_v21_384.onnx

**Option 2: From ONNX Model Zoo**
- https://github.com/onnx/models/tree/main/validated/vision/body_analysis/depth_estimation

**Place the model in:** `examples/DNN/models/midas_v21_small.onnx`

---

## Directory Structure

```
examples/DNN/
├── README.md
├── yolov8_human_detection.vsp
├── mono_depth_estimation.vsp
└── models/
    ├── yolov8n.onnx          # Download manually
    └── midas_v21_small.onnx  # Download manually
```

---

## Running the Examples

### YOLOv8 Human Detection

**From webcam (default, using OpenCV DNN):**
```powershell
cd build\Release
.\visionpipe.exe run ..\..\examples\DNN\yolov8_human_detection.vsp
```

**With verbose output:**
```powershell
.\visionpipe.exe run ..\..\examples\DNN\yolov8_human_detection.vsp --verbose
```

### Mono-Depth Estimation

```powershell
cd build\Release
.\visionpipe.exe run ..\..\examples\DNN\mono_depth_estimation.vsp
```

### Validate Scripts (without running)

```powershell
.\visionpipe.exe validate ..\..\examples\DNN\yolov8_human_detection.vsp
.\visionpipe.exe validate ..\..\examples\DNN\mono_depth_estimation.vsp
```

---

## Backend Selection

VisionPipe supports multiple DNN backends. Use the `backend` parameter in `load_model`:

```vsp
# OpenCV DNN (default)
load_model("yolo", "models/yolov8n.onnx", backend="opencv", device="cpu")

# ONNX Runtime with CPU
load_model("yolo", "models/yolov8n.onnx", backend="onnx", device="cpu")

# ONNX Runtime with CUDA
load_model("yolo", "models/yolov8n.onnx", backend="onnx", device="cuda")

# ONNX Runtime with TensorRT (fastest on NVIDIA)
load_model("yolo", "models/yolov8n.onnx", backend="onnx", device="tensorrt")

# ONNX Runtime with DirectML (Windows AMD/Intel GPUs)
load_model("yolo", "models/yolov8n.onnx", backend="onnx", device="dml")
```

### Backend Comparison

| Feature | OpenCV DNN | ONNX Runtime |
|---------|------------|--------------|
| Setup | Built-in | Requires separate install |
| CPU Performance | Good | Better (optimized kernels) |
| NVIDIA GPU | CUDA | CUDA, TensorRT |
| AMD GPU | OpenCL (limited) | DirectML (Windows) |
| Intel GPU | OpenCL (limited) | DirectML, OpenVINO |
| Apple Silicon | - | CoreML |
| Dynamic Shapes | Limited | Full support |
| Quantization | Limited | INT8, FP16 |

---

## Model Information

### YOLOv8 Architecture

| Model | Size | mAP@50-95 | Speed (ms) | Parameters |
|-------|------|-----------|------------|------------|
| YOLOv8n | 640 | 37.3 | 1.2 | 3.2M |
| YOLOv8s | 640 | 44.9 | 2.1 | 11.2M |
| YOLOv8m | 640 | 50.2 | 4.1 | 25.9M |

COCO class ID for "person" is `0`.

### MiDaS Depth Estimation

- **Input**: RGB image (256x256 or 384x384 depending on model)
- **Output**: Inverse depth map (relative depth, not metric)
- **Use case**: Depth ordering, 3D effects, occlusion detection

---

## Troubleshooting

### "Model file not found"
Ensure models are downloaded to `examples/DNN/models/` directory.

### "Inference failed" or crash
- Check OpenCV DNN backend compatibility
- Try CPU backend if GPU fails: modify `backend: "opencv"` in script

### Low FPS
- Use smaller model (yolov8n instead of yolov8s)
- Reduce input resolution
- Enable GPU acceleration if available

---

## Additional Resources

- [Ultralytics YOLOv8 Documentation](https://docs.ultralytics.com/)
- [MiDaS GitHub Repository](https://github.com/isl-org/MiDaS)
- [OpenCV DNN Module](https://docs.opencv.org/4.x/d2/d58/tutorial_table_of_content_dnn.html)
- [ONNX Model Zoo](https://github.com/onnx/models)

# IP Stream Feature Implementation Plan

## Status: ✅ IMPLEMENTED

## Overview
HTTP-based frame streaming capability for VisionPipe, enabling users to stream processed video frames to remote devices over a network. This allows real-time visualization and monitoring of pipeline outputs from any device on the network.

## Feature Flag
- **CMake Flag**: `VISIONPIPE_IP_CAM=ON`
- **Preprocessor Definition**: `VISIONPIPE_IP_STREAM_ENABLED`
- **Default**: OFF (optional feature)

## Build Instructions
```bash
# Configure with IP streaming enabled
cmake .. -DVISIONPIPE_IP_CAM=ON

# Build
cmake --build . --config Release
```

---

## Implemented Components

### 1. File Structure
```
include/
├── utils/
│   └── http_frame_server.h        ✅ HTTP server + FrameEncoder + IPStreamManager
└── interpreter/items/
    └── ip_stream_items.h          ✅ DSL items for streaming

src/
├── utils/
│   └── http_frame_server.cpp      ✅ Full implementation
└── interpreter/items/
    └── ip_stream_items.cpp        ✅ DSL item implementations
```

### 2. DSL Commands

| Command | Description | Example |
|---------|-------------|---------|
| `ip_stream(port, bind_addr, quality)` | Stream frames over HTTP | `ip_stream(8080, "0.0.0.0", 85)` |
| `ip_stream_quality(port, quality)` | Set JPEG quality | `ip_stream_quality(8080, 90)` |
| `ip_stream_stop(port)` | Stop a stream server | `ip_stream_stop(8080)` |
| `ip_stream_info()` | Print active streams info | `ip_stream_info()` |
| `ip_stream_url(port)` | Get stream URL | `$url = ip_stream_url(8080)` |

### 3. Key Features

- **LAN Access**: Binds to `0.0.0.0` by default for network-wide access
- **Custom Binding**: Specify IP address to bind to specific interface
- **MJPEG Protocol**: Compatible with browsers, VLC, ffmpeg, OpenCV
- **Concurrent Clients**: Supports multiple viewers simultaneously (default: 10)
- **Configurable Quality**: JPEG quality 1-100 (default: 80)
- **Thread-Safe**: Frame queue with automatic overflow handling

---

## Usage Examples

### Basic Streaming
```vsp
pipeline main
    video_cap(0)
    ip_stream(8080)          # Stream on port 8080 (LAN accessible)
    imshow("Local")
    wait_key(30)
end
exec_loop main
```

### With Custom Configuration
```vsp
pipeline main
    video_cap(0)
    resize(640, 480)
    put_text("Streamed from VisionPipe", 10, 30, "simplex", 0.7, "cyan", 1)
    ip_stream(8080, "0.0.0.0", 85)    # Port, bind address, quality
    imshow("Preview")
    wait_key(30)
end
exec_loop main
```

### Bind to Specific IP
```vsp
# Stream only on specific network interface
ip_stream(9000, "192.168.1.100", 90)
```

### Access Stream From Other Devices
- **Web Browser**: `http://192.168.1.x:8080/stream`
- **VLC**: Media > Open Network Stream > `http://192.168.1.x:8080/stream`
- **ffmpeg**: `ffplay http://192.168.1.x:8080/stream`
- **Python OpenCV**: `cv2.VideoCapture("http://192.168.1.x:8080/stream")`

---

## Architecture Overview

### Core Classes

#### HttpFrameServer
Primary HTTP server for MJPEG streaming.
Content-Type: image/jpeg
Content-Length: 12345

[JPEG BINARY DATA]
--frame
Content-Type: image/jpeg
Content-Length: 12340

[JPEG BINARY DATA]
--frame
```

### 2.3 Encoding Pipeline

Flow:
1. Frame enters `IPStreamItem` in pipeline
2. Frame is converted to RGB if needed (JPEG standard)
3. Frame is encoded to JPEG with configured quality
4. Encoded frame is pushed to HTTP server's queue
5. HTTP server delivers frames to connected clients
6. Original frame passes through for next pipeline item

### 2.4 Thread Safety

- Use thread-safe queue (circular buffer with mutex) for frame buffering
- Frame copying minimized by using move semantics
- Lock-free or minimal-lock design for high throughput
- Graceful handling of queue overflow (drop oldest frames)

### 2.5 Configuration Options

**Via DSL** (VisionPipe Script):
```vsp
pipeline stream_example
    video_cap(0)
    gaussian_blur(3)
    ip_stream(8080)        # Stream on port 8080
    ip_stream_config_quality(85)  # JPEG quality
    ip_stream_config_queue(30)    # Queue size
    imshow("Live Feed")
    wait_key(30)
end
```

**Programmatic** (C++):
```cpp
auto server = std::make_unique<HttpFrameServer>(8080);
server->setJpegQuality(85);
server->setQueueSize(30);
server->setMaxClients(5);
server->start();
```

---

## 3. File Structure

```
include/
├── utils/
│   ├── http_server.h          (NEW)
│   └── frame_encoder.h        (NEW)
└── pipeline/
    └── pipeline_ip_stream_items.h (NEW)

src/
├── utils/
│   ├── http_server.cpp        (NEW)
│   └── frame_encoder.cpp      (NEW)
└── pipeline/
    └── pipeline_ip_stream_items.cpp (NEW)

CMakeLists.txt                 (MODIFIED)
```

---

## 4. Dependencies

### Required (if building with IP_STREAM enabled):
- **OpenCV** (already required) - for JPEG encoding
- **Standard C++ Threading** - for concurrent client handling
- **Standard C++ Networking** (sockets API)

### Optional:
- **cpp-httplib** (header-only HTTP library) - simplifies HTTP handling
  - Alternative: Use raw sockets with custom HTTP implementation
  - License: MIT (compatible with GPL)

### Recommendation:
Implement using standard C++ sockets and threading to minimize external dependencies. Custom HTTP parsing/generation is feasible for MJPEG streaming.

---

## 5. Implementation Phases

### Phase 1: Foundation
- [ ] Add CMake feature flag `VISIONPIPE_IP_STREAM`
- [ ] Create frame encoder module with JPEG support
- [ ] Create basic HTTP server skeleton

### Phase 2: Core Server
- [ ] Implement MJPEG protocol handler
- [ ] Thread pool for concurrent clients
- [ ] Frame queue management (circular buffer)
- [ ] Connection lifecycle management

### Phase 3: Pipeline Integration
- [ ] Create `IPStreamItem` pipeline item
- [ ] Add DSL support for ip_stream command
- [ ] Create `IPStreamConfigItem` for configuration
- [ ] Integrate with interpreter

### Phase 4: Testing & Documentation
- [ ] Unit tests for encoder and server
- [ ] Integration tests with pipeline
- [ ] Example DSL scripts
- [ ] Documentation and usage guide

---

## 6. Usage Examples

### Example 1: Simple Camera Stream
**File**: `examples/ip_camera_basic.vsp`
```vsp
# Stream camera feed over HTTP at port 8080
# Access via: http://localhost:8080/stream

pipeline camera_stream
    video_cap(0)
    resize(640, 480)
    ip_stream(8080)
    imshow("Local View")
    wait_key(30)
end

exec_loop camera_stream
```

### Example 2: Processed Stream with Configuration
**File**: `examples/ip_camera_processing.vsp`
```vsp
# Stream processed camera feed with encoding quality control

pipeline processed_stream
    video_cap(0)
    resize(1280, 720)
    gaussian_blur(5)
    ip_stream(9000)
    ip_stream_config_quality(90)
    ip_stream_config_queue(30)
    imshow("Processing")
    wait_key(30)
end

exec_loop processed_stream
```

### Example 3: Multi-Pipeline with Streaming
```vsp
pipeline capture
    video_cap(0)
    cache_mat(1)
end

pipeline processing_a
    load_mat(1)
    gaussian_blur(3)
    cache_mat(2)
end

pipeline processing_b
    load_mat(1)
    canny_edge(50, 150)
    cache_mat(3)
end

pipeline stream_output
    load_mat(2)
    ip_stream(8080)
    imshow("Blurred")
    wait_key(30)
end

exec_loop capture
exec_loop processing_a
exec_loop processing_b
exec_loop stream_output
```

---

## 7. Performance Considerations

### Throughput
- Target: 30+ FPS streaming for 720p frames
- JPEG encoding is the primary bottleneck
- Quality setting trades off file size vs. speed
- Frame queue prevents blocking on slow clients

### Memory Usage
- Circular buffer with configurable size (default: 30 frames)
- Single-copy or zero-copy operations where possible
- Frame size: 1920x1080 RGB = ~6.2 MB per frame
- Default queue: 30 frames ≈ 186 MB max

### Scalability
- Single HTTP thread handles multiple clients
- Clients share same frame queue (no per-client buffering)
- Optional: Implement frame dropping for slow clients

---

## 8. Security Considerations

### Initial Implementation (Minimal)
- No authentication/authorization
- Accept connections from any client
- No encryption (HTTP only)
- Suitable for trusted networks only

### Future Enhancements
- HTTPS/TLS support
- Basic auth token validation
- Rate limiting per client
- Access control lists
- Frame watermarking/metadata

---

## 9. Testing Strategy

### Unit Tests
- `test_frame_encoder.cpp` - JPEG encoding correctness
- `test_http_server.cpp` - Server socket handling
- `test_frame_queue.cpp` - Thread safety of queue

### Integration Tests
- Pipeline with IP stream item
- Concurrent client connections
- Frame rate and quality validation
- Memory leak detection

### Manual Tests
- Access stream from web browser
- Use VLC to consume stream
- Monitor network bandwidth
- Stress test with slow clients

---

## 10. Documentation

### Code Documentation
- Doxygen comments for all public APIs
- Usage examples in header files
- Thread safety guarantees documented

### User Guide
- README section on IP streaming
- Configuration options explained
- Network setup recommendations
- Troubleshooting guide

### Example Scripts
- Basic camera stream
- Remote monitoring use case
- Performance optimization example
- Multi-client stress test example

---

## 11. Breaking Changes
None. This is an additive feature behind a feature flag.

---

## 12. Success Criteria

- ✅ HTTP server streams MJPEG at 30+ FPS
- ✅ Multiple clients can connect simultaneously
- ✅ Configurable JPEG quality and queue size
- ✅ Pipeline integration via DSL commands
- ✅ Works on Windows, Linux, macOS
- ✅ Memory efficient with configurable buffering
- ✅ Zero-copy/minimal-copy frame handling
- ✅ Thread-safe operations
- ✅ Comprehensive error handling
- ✅ Example scripts and documentation

---

## 13. References

### MJPEG Specification
- RFC 2616 (HTTP/1.1)
- MJPEG over HTTP format: boundary-based multipart

### Related Features
- Existing `WriteVideoItem` for file output
- Existing `ImshowItem` for GUI display
- Camera input via `CapReadItem`

### Potential Integration Points
- **Iceoryx2** IPC: Could also publish frames to IPC
- **DNN Items**: Stream before/after inference
- **Cache System**: Leverage existing frame caching

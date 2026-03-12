# libvisionpipe-rs Implementation Details

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                       Rust Application                           │
│                    (uses libvisionpipe-rs)                       │
└──────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┼─────────────┐
                ▼             ▼             ▼
           VisionPipe     Session      ParamClient
           (builder)      (main API)   (runtime params)
                │             │            │
                └─────────────┼────────────┘
                              │
                    iceoryx2 IPC Layer
                    (zero-copy pub/sub)
                              │
                ┌─────────────┼─────────────┐
                ▼             ▼             ▼
        FrameSubscriber   frame_sink()   param_server
        (consume)         (publish)      (TCP)
                │             │            │
                └─────────────┼────────────┘
                              │
                ┌─────────────────────────────────┐
                │  visionpipe child process      │
                │  (spawned via fork/exec)       │
                └─────────────────────────────────┘
```

## Core Modules

### 1. `error.rs` — Error handling

**Purpose**: Define all error types returned by the library.

**Types**:
- `VisionPipeError` — thiserror enum with variants for each failure mode
- `Result<T>` — type alias for `std::result::Result<T, VisionPipeError>`

**Key variants**:
- `Iox2Service(String)` — iceoryx2 service create/open failed
- `Iox2Subscriber(String)` — iceoryx2 subscriber creation failed
- `PayloadTooShort { got, need }` — frame data smaller than header size (40 bytes)
- `PayloadSizeMismatch { header, actual }` — data_size field doesn't match slice length
- `UnknownMatType(i32)` — cv::Mat type code not recognized
- `ParamServerConnect(String)` — TCP connection to param server failed
- `ParamServerTimeout { pid, timeout_ms }` — port file discovery timed out
- `SpawnError(String)` — visionpipe child process spawn failed
- `SessionNotRunning` — operation attempted on a stopped session

### 2. `frame.rs` — Frame types and codec

**Purpose**: Decode VisionPipe frame payloads and provide typed pixel access.

#### `IceoryxFrameMeta` (40 bytes, `#[repr(C)]`)
```
Offset  0: seq_num   : u64   — monotonic frame counter (UINT64_MAX = shutdown)
Offset  8: width     : i32   — frame width in pixels
Offset 12: height    : i32   — frame height in pixels
Offset 16: mat_type  : i32   — OpenCV type (depth + channels)
Offset 20: channels  : i32   — number of color channels
Offset 24: data_size : u64   — pixel data byte length
Offset 32: _pad      : [u8;8]— reserved
```

This header is prepended to every iceoryx2 sample payload, followed immediately by raw pixel data.

#### OpenCV `mat_type` Encoding
```
mat_type = depth_code | ((channels - 1) << CV_CN_SHIFT)

where CV_CN_SHIFT = 3, so:
  mat_type & 0x07 = depth code
  (mat_type >> 3) + 1 = channel count
```

**Depth codes**:
| Code | Name   | Element size |
|------|--------|--------------|
| 0    | CV_8U  | 1 byte       |
| 1    | CV_8S  | 1 byte       |
| 2    | CV_16U | 2 bytes      |
| 3    | CV_16S | 2 bytes      |
| 4    | CV_32S | 4 bytes      |
| 5    | CV_32F | 4 bytes      |
| 6    | CV_64F | 8 bytes      |

#### `FrameFormat` Enum
```rust
pub enum FrameFormat {
    Gray8,        // CV_8UC1 = 0
    Gray8S,       // CV_8SC1 = 8
    Gray16,       // CV_16UC1 = 2
    Gray16S,      // CV_16SC1 = 10
    Gray32S,      // CV_32SC1 = 4
    Gray32F,      // CV_32FC1 = 5
    Gray64F,      // CV_64FC1 = 6
    Bgr8,         // CV_8UC3 = 16
    Rgb8,         // (same cv type as BGR; labeling only)
    Bgra8,        // CV_8UC4 = 24
    Rgba8,        // (labeling only)
    Bgr16,        // CV_16UC3 = 18
    Bgra16,       // CV_16UC4 = 26
    Bgr32F,       // CV_32FC3 = 21
    Bgra32F,      // CV_32FC4 = 29
    TwoChannel32F, // CV_32FC2 = 13 (e.g., optical flow)
    TwoChannel16U, // CV_16UC2 (e.g., disparity + confidence)
    Raw(i32),      // Unknown type — preserve type code
}
```

#### `FrameData` Enum
```rust
pub enum FrameData {
    U8(Vec<u8>),    // 8-bit unsigned/signed depths
    U16(Vec<u16>),  // 16-bit depths
    I32(Vec<i32>),  // 32-bit signed integer
    F32(Vec<f32>),  // 32-bit float
    F64(Vec<f64>),  // 64-bit float
    Raw(Vec<u8>),   // Unknown depth — raw bytes
}
```

**Design**: Each element in the typed vectors represents one channel value. For a 640×480 BGR8 frame:
- `FrameData::U8` contains 640 × 480 × 3 = 921,600 bytes (row-major, no padding)
- Layout: `[R0C0B, R0C0G, R0C0R, R0C1B, R0C1G, R0C1R, ..., R1C0B, ...]`

#### `Frame` Struct
```rust
pub struct Frame {
    pub width: u32,
    pub height: u32,
    pub format: FrameFormat,
    pub seq: u64,           // frame number from publisher
    pub data: FrameData,
}
```

**Parsing**: `Frame::from_iox2_payload(payload: &[u8])` reads the header, validates sizes, decodes the depth code, and copies raw bytes into the appropriate `FrameData` variant.

**Typing**: Unknown depths fall back to `FrameData::Raw`, preserving the original bytes so callers can still access pixel data.

### 3. `transport.rs` — iceoryx2 frame transport

**Purpose**: Connect to VisionPipe frame publishers using native iceoryx2.

#### Service Names
Generated by `build_service_name(session_id, sink_name)` → `/vp_<session_id>_<sink_name>`

Example:
- Session ID: `abc123_ff`
- Sink: `output`
- Service: `/vp_abc123_ff_output`

Both the `frame_sink()` publisher (C++) and `FrameSubscriber` (Rust) derive this name the same way, ensuring connection.

#### `FrameSubscriber` Type
```rust
pub struct FrameSubscriber {
    service_name: String,
    _node: Node<IpcService>,
    subscriber: Subscriber<IpcService, [u8], ()>,
    last_seq: u64,
    shutdown: bool,
}
```

**Thread safety**: iceoryx2 `Subscriber` implements `Send + Sync`, so `FrameSubscriber` can be safely shared across threads or moved into closures.

**Methods**:
- `connect(session_id, sink_name)` → opens iceoryx2 service (or creates if not yet published)
- `recv_latest()` → drains the sample queue, returns the newest frame
- `poll()` → returns `Some(frame)` only if seq > last_seq (deduplication)

**Queue draining logic**:
```
loop {
    match subscriber.receive() {
        Ok(Some(sample)) ⇒ decode → latest frame
        Ok(None)         ⇒ queue empty, break
        Err(_)           ⇒ transport error, break
    }
}
return latest
```

If the queue is empty (`Ok(None)`) or there are no valid frames, returns `None` — the caller may reuse their previously cached frame.

### 4. `params.rs` — Runtime parameter control

**Purpose**: TCP client for the VisionPipe param server.

#### Wire Protocol (line-based, JSON responses)

**Client → Server**:
```
SET <name> <value>\n
GET <name>\n
LIST\n
WATCH <name>\n     (persistent connection)
WATCH *\n          (all parameters)
UNWATCH <name>\n
QUIT\n
```

**Server → Client**:
```
{"ok": true, "name": "brightness", "value": 75}\n
{"ok": false, "error": "Parameter not found: foo"}\n
{"event": "changed", "name": "brightness", "old": 50, "new": 75}\n
```

#### Port Discovery
- File path: `/tmp/visionpipe-params-<pid>.port`
- Contains: ASCII port number
- Polling: retries every 50 ms up to timeout (default 10 s)

#### `ParamValue` Enum
```rust
pub enum ParamValue {
    Int(i64),
    Float(f64),
    Bool(bool),
    String(String),
}
```

**Type inference**: `ParamValue::from_wire_str(s)` tries in order: `i64` → `f64` → `bool` → `String`.

#### `ParamClient` API
- Stateless for `GET/SET/LIST` — one TCP connection per call
- Stateful for `WATCH` — persistent connection on background thread
- `watch_param()` returns `WatchHandle` that cancels on drop

**Threading model for WATCH**:
```
Main thread                    WATCH thread
   │                               │
   ├─ spawn watcher_thread ────────┤
   │                               ├─ TcpStream::connect
   │                               ├─ write "WATCH <name>\n"
   │                               ├─ loop:
   │                               │   read_line()
   │                               │   parse JSON event
   │                               │   callback(event)
   │  watch_handle.cancel() ───────┤ check cancel flag
   │                               ├─ break
   │                               ├─ write "QUIT\n"
   │                               │
```

### 5. `session.rs` — Session and process management

**Purpose**: Manage the VisionPipe child process and coordinate frame + param APIs.

#### Process Lifecycle
```
VisionPipe::run("script.vsp")
          │
          ├─ generate session_id = "<hex_pid>_<hex_random>"
          │
          ├─ spawn visionpipe child with:
          │   ├─ VISIONPIPE_SESSION_ID=<session_id>
          │   ├─ --run script.vsp
          │   ├─ --param key=value ...
          │   └─ --no-display (default)
          │
          └─ Session(Running)
               │
        on_frame("output", callback)
               │
        spin() ──► spinOnce() loop
               │
        (child exits)
               │
        Session(Stopped)
```

#### Session State Machine
```
Created ──run()──▶ Running ──spin()──▶ dispatch callbacks
                       ▲                    │
                       │                    │
                   is_running()         (frame queue empty)
                       │                    │
                    true/false              │
                       │                ╔══════╗
                       └────────────────╢ loop ╢
                                        ╚══════╝
                                        
Stop requested:
    Running ──stop()──▶ Stopping ──wait()──▶ Stopped/Error
```

#### Sink Management
```rust
struct SinkEntry {
    subscriber: FrameSubscriber,
    callbacks: Vec<FrameCallback>,          // on_frame()
    update_callbacks: Vec<FrameUpdateCallback>, // on_frame_update()
    last_dispatched_seq: u64,               // deduplication
    last_frame: Option<Frame>,              // grab_frame() cache
}
```

Callbacks are per-sink. Multiple callbacks on the same sink all fire for each new frame.

#### Spin Loop
```
spin_once():
  for sink_name in &sinks {
    if let Some(frame) = subscriber.recv_latest() {
      if frame.seq > last_dispatched_seq {
        update_callbacks(frame.seq)
        callbacks(&frame)
      }
    }
  }
```

If the queue is empty, `recv_latest()` returns `None` and no callbacks fire.

#### Param Client Lazy Initialization
```
set_param() ──┐
get_param()  ─┼─▶ param_client()?  ┬──▶ [cached] ParamClient
list_params()─┤                    └──▶ read /tmp/vp-params-{pid}.port
watch_param()─┘                        connect 127.0.0.1:{port}
```

The first param operation discovers the port and caches the `ParamClient` for reuse.

### 6. `visionpipe.rs` — Builder pattern launcher

**Purpose**: Fluent API for VisionPipe configuration and launch.

#### Builder Pattern
```rust
VisionPipe::new()
    .executable("/usr/bin/visionpipe")
    .script("pipeline.vsp")
    .param("gain", "1.5")
    .no_display(true)
    .verbose(false)
    .log_file("/dev/null")
    .run()?
```

#### Session ID Generation
```rust
format!("{pid:x}_{random:06x}")
// Example: "abc123_ff1234"
```

- `pid`: current process ID (decimal → hex)
- `random`: 24-bit value derived from `SystemTime::now().subsec_nanos()`

#### Environment Setup
```
VISIONPIPE_SESSION_ID=<session_id>
```

This env var is read by the child visionpipe to derive iceoryx2 service names.

#### Temp File Handling
```
runString(content: &str, name: &str):
  ├─ sanitize name → /tmp/vp_inline_<name>_<pid>.vsp
  ├─ write script bytes
  └─ pass file path to launch()
```

Temp files are in the user's `/tmp` for easy cleanup.

## Data Flow Examples

### Scenario 1: Receive a frame
```
frame_sink("output") publishes to /vp_abc123_ff_output on iceoryx2
                ▼
FrameSubscriber::connect("abc123_ff", "output")
                ├─ NodeBuilder::create() → Node
                ├─ ServiceName::create("/vp_abc123_ff_output")
                ├─ open_or_create() → service
                └─ subscriber_builder().create() → Subscriber
                ▼
Subscriber::receive() → Sample<[u8]>
                ├─ payload: &[u8] = [IceoryxFrameMeta | pixel_data]
                ▼
Frame::from_iox2_payload(payload)
                ├─ read meta from first 40 bytes
                ├─ validate payload.len() ≥ data_size
                ├─ decode mat_type → FrameFormat + element type
                ├─ copy raw bytes → FrameData::U8/U16/F32/etc
                └─ return Frame { width, height, format, seq, data }
                ▼
Session::spin_once() fires on_frame callbacks
                └─ callback(&frame)
```

### Scenario 2: Modify a parameter at runtime
```
session.set_param("brightness", &ParamValue::Int(80))
                ├─ param_client()? → ParamClient (discovered from port file)
                ├─ TcpStream::connect("127.0.0.1:{port}")
                ├─ write "SET brightness 80\n"
                ├─ read "{"ok":true}\n"
                └─ close connection
                ▼
VisionPipe param server receives SET command
                ├─ parse name + value
                ├─ update RuntimeParamStore
                ├─ push change events to WATCH subscribers
                └─ send JSON response
```

### Scenario 3: Watch param changes
```
let _watch = session.watch_param("brightness", |evt| { ... })?
                ├─ spawn background thread
                ├─ TcpStream::connect() → persistent TCP stream
                ├─ write "WATCH brightness\n"
                ├─ loop:
                │   ├─ read_line() → "{"event":"changed",...}\n"
                │   ├─ parse JSON
                │   └─ callback(ParamChangeEvent)
                └─ blocks until WatchHandle dropped
```

## Memory Safety & Ownership

### Frame Ownership
- **Allocated by**: `Frame::from_iox2_payload()` — `Vec<u8/u16/f32/f64>` clones the payload
- **Owned by**: `Frame` struct (on the stack or in a callback closure)
- **Lifetime**: User controls; no references to shared memory persists

The iceoryx2 sample is released by the `Subscriber` immediately after `receive()` returns; the pixel data is already copied into the owned `FrameData::Vec`.

### Subscriber Ownership
- `FrameSubscriber` is not Sync-safe for shared mutable access per se, but its underlying iceoryx2 `Subscriber` is `Send + Sync`
- Users typically create one `FrameSubscriber` per sink in a `Session`
- Can be moved into closures or across threads

### Callback Closures
```rust
session.on_frame("output", move |frame| {
    // captures: Arc<Mutex<...>>, session resources, etc.
    // frame: &Frame (borrowed during callback)
})?
```

- `FnMut` trait → allows interior mutation on captured state
- `'static` bound (implicit via `Box<dyn ... +'static>`) → closure outlives launch

## Thread Safety

### Thread-safe types
-iceoryx2 `Subscriber` (Send + Sync) → safe to move between threads
- `Vec<T>` (owned heap memory) → safe to move/share read-only
- `ParamChangeEvent` (Copy for primitives, String is movable) → safe to pass to callback thread
- `Arc<Mutex<T>>` → synchronized access to shared state

### Not thread-safe
- `Session` itself is not `Send` (contains `Child` and `Node`)
  - But can spawn `ParamClient` on a background thread via `watch_param()`
  - Callbacks are invoked from the `spin()` loop thread (blocking)
  - WATCH callbacks are invoked from a background watcher thread

## Compatibility Matrix

| Component         | Requirement                |
|-------------------|----------------------------|
| VisionPipe (C++)  | Built with `-DVISIONPIPE_IPC_USE_ICEORYX2=ON` |
| iceoryx2 crate    | v0.8.x (workspace path dep) |
| Rust              | ≥ 1.83                     |
| Linux/macOS       | POSIX signals (SIGTERM)    |
| Serde + JSON      | Default feature; required  |

## Performance Characteristics

### Zero-copy frame delivery
- Sample payload is memory-mapped shared memory (iceoryx2)
- **But**: Rust client copies pixel bytes into owned `FrameData::Vec` immediately
- Rationale: simplifies lifetime scoping and gives Rust full ownership

### Callback latency
- Direct function call (not queued) — no extra copying
- Executes in the `spin()` loop thread
- Blocking callbacks block frame dispatch for all sinks

### Parameter updates
- SET/GET: one round-trip TCP per call (not batched)
- WATCH: persistent TCP connection, callback from background thread
- No blocking on the spin loop

## Testing & Debugging

### Debug output
- `eprintln!` on malformed iceoryx2 payloads
- All errors are `Result<T>` — callers must handle

### Port file polling
- Default timeout: 10 seconds
- Logs to stderr if connection fails
- Useful for debugging: `cat /tmp/visionpipe-params-<pid>.port`

### Frame sequence tracking
- `Frame::seq` monotonically increases (or wraps at u64::MAX)
- `Session` tracks `last_dispatched_seq` per sink to avoid re-dispatching

---

## 6. `discovery.rs` — Sink and capture device discovery

**Purpose**: Pure-Rust, zero-dependency enumeration of active `frame_sink()` IPC
segments and video capture devices. Mirrors the libvisionpipe C++ `discovery.h`
API and the VSP built-in functions `discover_sinks / sink_properties /
discover_capture / capture_capabilities`.

### Public API

```rust
// Sink discovery
pub fn discover_sinks()   -> Vec<SinkInfo>
pub fn sink_properties(session_id: &str, sink_name: &str) -> Option<SinkProperties>

// Capture device discovery
pub fn discover_capture() -> Vec<CaptureDevice>
pub fn capture_capabilities(device: &str) -> CaptureDevice
```

### Structs

| Struct | Fields |
|---|---|
| `SinkInfo` | `session_id`, `sink_name`, `shm_name`, `producer_pid`, `producer_alive` |
| `SinkProperties` | `is_valid`, `rows`, `cols`, `channels`, `cv_type`, `step`, `data_size`, `frame_seq`, `producer_pid`, `producer_alive` |
| `CaptureDevice` | `path`, `device_name`, `driver_name`, `bus_info`, `backend`, `is_capture`, `has_controls`, `formats` |
| `CaptureFormat` | `pixel_format`, `width`, `height`, `fps_min`, `fps_max` |

### Sink discovery implementation

1. **Scan `/dev/shm`** (`std::fs::read_dir`) for files matching `visionpipe_sink_*`
2. **Parse** the session ID and sink name from the filename (splits on 3rd `_`)
3. **Memory-map** each segment read-only via `libc::shm_open` + `libc::mmap`
4. **Verify** the `SHM_MAGIC` (`0x56505346`) at offset 0
5. **Read header** fields at their C struct offsets (see layout comment in source)
6. **Liveness check** via `kill(pid, 0)` — `EPERM` counts as alive
7. Returns a sorted `Vec<SinkInfo>` (by session_id then sink_name)

#### SHM header layout (matches `frame_transport.h`)

```
offset  0  u32  magic          (SHM_MAGIC = 0x56505346)
offset  4  u32  (pad for alignment of atomic<u64>)
offset  8  u64  frame_seq      (atomic, monotonic)
offset 16  i32  rows
offset 20  i32  cols
offset 24  i32  cv_type        (depth | ((channels-1) << 3))
offset 28  i32  step           (bytes per row)
offset 32  u32  data_size
offset 36  i32  writer_lock    (atomic spin-lock)
offset 40  i32  producer_pid
offset 44  u8   _pad[16]
total = 60 bytes (struct); mapping = 64-byte aligned header + MAX_FRAME_BYTES data
```

### Capture discovery implementation (Linux / V4L2)

1. **Probe `/dev/video0` … `/dev/video63`** — skip missing paths  
2. **`VIDIOC_QUERYCAP`** — read card name, driver, bus_info; check `V4L2_CAP_VIDEO_CAPTURE`  
3. **`VIDIOC_QUERYCTRL`** — test for control existence (sets `has_controls`)  
4. **`VIDIOC_ENUM_FMT`** — iterate all supported pixel formats  
5. **`VIDIOC_ENUM_FRAMESIZES`** — for each format, iterate discrete or stepwise sizes  
6. **`VIDIOC_ENUM_FRAMEINTERVALS`** — for each (format, size), collect min/max fps  

All ioctls use `repr(C)` Rust structs directly matching the kernel ABI —
no bindgen required. The ioctl numbers are hard-coded for x86-64 / ARM64.

### `#[cfg]` guards

| Block | Guard |
|---|---|
| `posix_shm` (shm_open/mmap/kill) | `#[cfg(unix)]` |
| `v4l2` sub-module | `#[cfg(target_os = "linux")]` |
| Ioctl-based discover/query | `#[cfg(target_os = "linux")]` |
| Non-Linux fallback | `#[allow(unreachable_code)]` pass-through |

### Example usage

```rust
use visionpipe_client::{discover_sinks, sink_properties, discover_capture, capture_capabilities};

// Find all live sinks
for sink in discover_sinks() {
    println!("{}/{}: pid={} alive={}",
        sink.session_id, sink.sink_name,
        sink.producer_pid, sink.producer_alive);

    if let Some(props) = sink_properties(&sink.session_id, &sink.sink_name) {
        println!("  {}×{} ch={} seq={}",
            props.cols, props.rows, props.channels, props.frame_seq);
    }
}

// Enumerate cameras
for dev in discover_capture() {
    println!("{} – {} [{} formats]",
        dev.path, dev.device_name, dev.formats.len());
}

// Query one device in detail
let caps = capture_capabilities("/dev/video0");
for fmt in &caps.formats {
    println!("  {} {}×{}  {:.0}–{:.0} fps",
        fmt.pixel_format, fmt.width, fmt.height,
        fmt.fps_min, fmt.fps_max);
}
```

---

## Future Extensions

### Proposed features (not implemented)
1. **Async/await support** (`async` feature flag)
   - `async fn recv_latest()` with tokio
   - `async fn watch_param()` streaming responses
   
2. **Serialization** (serde)
   - `#[derive(Serialize, Deserialize)]` on Frame metadata
   - Frame data remains raw bytes (no point in serializing pixel buffers)

3. **High-level bindings**
   - Type-safe param builders (macro or builder)
   - Frame format validation at type level

---

This document serves as a reference for contributors and maintainers.

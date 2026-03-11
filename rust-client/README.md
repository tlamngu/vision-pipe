# visionpipe-client (Rust)

Native Rust client for [VisionPipe](../README.md), using **iceoryx2** for
zero-copy, lock-free inter-process frame delivery.

## Requirements

| Dependency | Version | Notes |
|------------|---------|-------|
| Rust       | ≥ 1.83  | `rust-version` in `Cargo.toml` |
| iceoryx2   | 0.8.x   | Local workspace at `../../utils/iceoryx2` |
| visionpipe | any     | Must be built with `-DVISIONPIPE_WITH_ICEORYX2=ON` and `-DVISIONPIPE_IPC_USE_ICEORYX2=ON` |

## Architecture

```
┌────────────────────────────────────────────────────────────┐
│  visionpipe process  (child — started by VisionPipe::run)  │
│                                                            │
│   frame_sink("output")                                     │
│      │  publishes via iceoryx2                             │
│      └─▶  service "/vp_<session_id>_output"  ─────────────┤─▶ FrameSubscriber
│                                                            │
│   param server (TCP, port in /tmp/visionpipe-params-…)     │
│      └────────────────────────────────────────────────────┤─▶ ParamClient
└────────────────────────────────────────────────────────────┘
```

Frame samples carry an `IceoryxFrameMeta` header (40 bytes) followed by raw
pixel data.  The Rust client decodes the OpenCV `mat_type` field into a typed
[`FrameFormat`] enum and stores pixels in a [`FrameData`] variant
(`U8`, `U16`, `I32`, `F32`, `F64`, or `Raw`).

## Quick start

Add to your `Cargo.toml`:

```toml
[dependencies]
visionpipe-client = { path = "/path/to/vision-pipe/rust-client" }
```

```rust
use visionpipe_client::VisionPipe;
use std::time::Duration;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut session = VisionPipe::new()
        .script("camera_pipeline.vsp")
        .no_display(true)
        .run()?;

    session.on_frame("output", |frame| {
        println!("#{} {}×{} {}", frame.seq, frame.width, frame.height, frame.format);
    })?;

    session.spin(Duration::from_millis(1))?;
    Ok(())
}
```

The `.vsp` script must include `frame_sink("output")` to publish frames:

```text
video_cap(0)
resize(640, 480)
frame_sink("output")
```

## Frame formats

| `FrameFormat` variant | cv::Mat type | Channels | Depth   |
|-----------------------|-------------|----------|---------|
| `Gray8`               | CV_8UC1  (0) | 1        | u8      |
| `Gray16`              | CV_16UC1 (2) | 1        | u16     |
| `Gray32F`             | CV_32FC1 (5) | 1        | f32     |
| `Gray64F`             | CV_64FC1 (6) | 1        | f64     |
| `Gray32S`             | CV_32SC1 (4) | 1        | i32     |
| `Bgr8`                | CV_8UC3 (16) | 3        | u8 BGR  |
| `Bgra8`               | CV_8UC4 (24) | 4        | u8 BGRA |
| `Bgr16`               | CV_16UC3(18) | 3        | u16 BGR |
| `Bgr32F`              | CV_32FC3(21) | 3        | f32 BGR |
| `TwoChannel32F`       | CV_32FC2(13) | 2        | f32     |
| `Raw(n)`              | any other    | varies   | raw bytes |

Data is always row-major, packed (no stride padding).  For colour frames,
channel order follows OpenCV conventions: **BGR**, not RGB.

## API reference

### `VisionPipe` (builder)

```rust
VisionPipe::new()
    .executable("/usr/local/bin/visionpipe")  // default: "visionpipe"
    .script("pipeline.vsp")
    .param("key", "value")                    // --param key=value
    .no_display(true)
    .verbose(false)
    .log_file("/dev/null")                    // suppress child output
    .run()
```

### `Session`

```rust
// Frame callbacks
session.on_frame("sink_name", |frame| { ... })?;
session.on_frame_update("sink_name", |seq| { ... })?;

// Polling
let frame: Option<Frame> = session.grab_frame("sink_name")?;

// Spin loop
session.spin(Duration::from_millis(1))?;  // blocks until process exits
session.spin_once()?;                      // dispatch one round, non-blocking

// Process control
session.stop(Duration::from_secs(3))?;
session.is_running();
session.wait_for_exit(Some(Duration::from_secs(5)));

// Runtime parameters
session.set_param("brightness", &ParamValue::Int(80))?;
session.set_param_str("gain", "1.5")?;
let v: Option<ParamValue> = session.get_param("brightness")?;
let all: HashMap<String, ParamValue> = session.list_params()?;

// Watch parameter changes (callback from background thread)
let handle = session.watch_param("brightness", |evt| {
    println!("{}: {} → {}", evt.name, evt.old_value, evt.new_value);
})?;
drop(handle);  // cancels the watch
```

### `Frame`

```rust
frame.width       // u32
frame.height      // u32
frame.format      // FrameFormat
frame.seq         // u64 — monotonic counter from the publisher
frame.data        // FrameData

// Typed access
match &frame.data {
    FrameData::U8(pixels)  => { /* &[u8] */ }
    FrameData::U16(pixels) => { /* &[u16] */ }
    FrameData::F32(pixels) => { /* &[f32] */ }
    FrameData::F64(pixels) => { /* &[f64] */ }
    FrameData::I32(pixels) => { /* &[i32] */ }
    FrameData::Raw(bytes)  => { /* &[u8] fallback */ }
}

// Whole frame as bytes regardless of depth
let bytes: &[u8] = frame.as_bytes();

// Per-pixel access (8-bit only)
if let Some(px) = frame.pixel_u8(row, col) { /* &[u8] of length channels */ }
```

### `ParamClient` (standalone, without spawning a process)

```rust
// Connect to an already-running visionpipe by PID
let client = ParamClient::for_pid(pid, Duration::from_secs(5))?;
client.set_param("brightness", &ParamValue::Int(80))?;
let v = client.get_param("brightness")?;
let all = client.list_params()?;
let handle = client.watch_param("*", |evt| { ... })?;
```

## Examples

```bash
# Stream frames from get-started.vsp
cargo run --example basic_stream -- get-started.vsp output

# Adjust runtime parameters
cargo run --example params_control -- runtime_params_demo.vsp

# Receive from multiple sinks simultaneously
cargo run --example multi_sink -- stereo_test.vsp left right
```

## Feature flags

| Flag                 | Default | Description                       |
|----------------------|---------|-----------------------------------|
| `iceoryx2-transport` | on      | Enable iceoryx2 IPC transport     |
| `async`              | off     | Tokio-based async helpers (future)|

## How session IDs work

When `VisionPipe::run()` starts the child process it generates a unique
session ID (format: `<hex_pid>_<hex_random>`) and passes it as the
`VISIONPIPE_SESSION_ID` environment variable.  The `frame_sink()` items in
the script read this variable and construct iceoryx2 service names as:

```
/vp_<session_id>_<sink_name>
```

The Rust subscriber connects to the same service names, ensuring isolation
between concurrent VisionPipe sessions on the same machine.

//! # visionpipe-client
//!
//! Native Rust client for [VisionPipe](https://github.com/versys/vision-pipe),
//! using **iceoryx2** for zero-copy, lock-free inter-process frame delivery.
//!
//! ## Architecture
//!
//! ```text
//! ┌─────────────────────────────────────────────────┐
//! │  visionpipe process  (spawned as child)         │
//! │  ┌────────────┐   iceoryx2 IPC   ┌────────────┐ │
//! │  │ frame_sink │ ──────────────── │ subscriber │ │ ← this library
//! │  │  ("output")│   /vp_<sid>_…    │            │ │
//! │  └────────────┘                  └────────────┘ │
//! │                   TCP params                    │
//! │  ┌──────────┐  ←────────────────  ParamClient   │
//! │  │param srv │                                   │
//! │  └──────────┘                                   │
//! └─────────────────────────────────────────────────┘
//! ```
//!
//! Frames are published by `frame_sink("name")` items in `.vsp` scripts over
//! iceoryx2 publish/subscribe services named `/vp_<session_id>_<sink_name>`.
//! Runtime parameters are accessed via a text-line TCP server that the
//! VisionPipe process starts automatically.
//!
//! ## Quick start
//!
//! ```rust,no_run
//! use visionpipe_client::{VisionPipe, frame::FrameFormat};
//! use std::time::Duration;
//!
//! let mut session = VisionPipe::new()
//!     .script("camera_pipeline.vsp")
//!     .no_display(true)
//!     .run()
//!     .expect("failed to start visionpipe");
//!
//! session
//!     .on_frame("output", |frame| {
//!         println!(
//!             "#{} {}×{} {} ({} bytes)",
//!             frame.seq, frame.width, frame.height,
//!             frame.format, frame.data.len()
//!         );
//!     })
//!     .expect("connect sink");
//!
//! // Block and dispatch until the process exits.
//! session.spin(Duration::from_millis(1)).unwrap();
//! ```
//!
//! ## Feature flags
//!
//! | Flag                  | Default | Description                             |
//! |-----------------------|---------|-----------------------------------------|
//! | `iceoryx2-transport`  | on      | Enable iceoryx2 IPC transport           |
//! | `async`               | off     | Enable tokio-based async helpers        |

pub mod error;
pub mod frame;
pub mod params;
pub mod session;
pub mod visionpipe;
pub mod discovery;

#[cfg(feature = "iceoryx2-transport")]
pub mod transport;

// Convenience re-exports at the crate root.
pub use error::{Result, VisionPipeError};
pub use frame::{Frame, FrameData, FrameFormat};
pub use params::{ParamChangeEvent, ParamClient, ParamValue, WatchHandle};
pub use session::{FrameCallback, FrameUpdateCallback, Session, SessionState};
pub use visionpipe::{VisionPipe, VisionPipeConfig};
pub use discovery::{
    CaptureDevice, CaptureFormat, SinkInfo, SinkProperties,
    capture_capabilities, discover_capture, discover_sinks, sink_properties,
};

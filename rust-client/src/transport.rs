//! iceoryx2-native frame transport for the VisionPipe Rust client.
//!
//! Each [`FrameSubscriber`] connects to one iceoryx2 publish/subscribe service
//! produced by a VisionPipe `frame_sink()` item.  Service names follow the
//! convention established in the C++ codebase:
//!
//! ```text
//! service_name = "/vp_" + session_id + "_" + sink_name
//! ```
//!
//! The payload of every sample is:
//! ```text
//! [ IceoryxFrameMeta (40 bytes) ][ raw pixel data (data_size bytes) ]
//! ```

use iceoryx2::port::subscriber::Subscriber;
use iceoryx2::prelude::*;

use crate::error::{Result, VisionPipeError};
use crate::frame::{Frame, SHUTDOWN_SEQ};

// ============================================================================
// Service-name helpers
// ============================================================================

/// Build the iceoryx2 service name for a given session + sink.
///
/// Matches the C++ `svcName(buildIox2ChannelName(sessionId, sinkName))`:
/// ```text
/// "/vp_<session_id>_<sink_name>"
/// ```
pub fn build_service_name(session_id: &str, sink_name: &str) -> String {
    format!("/vp_{}_{}", session_id, sink_name)
}

// ============================================================================
// FrameSubscriber
// ============================================================================

type IpcService = iceoryx2::service::ipc::Service;

/// A zero-copy iceoryx2 subscriber that receives [`Frame`]s from a single
/// VisionPipe `frame_sink()`.
///
/// The subscriber connects to the iceoryx2 service created by the VisionPipe
/// process at the matching `frame_sink("name")` call.
pub struct FrameSubscriber {
    /// The iceoryx2 service name (for diagnostics).
    service_name: String,
    /// The iceoryx2 node that owns this subscriber.
    _node: Node<IpcService>,
    /// The inner subscriber port.
    subscriber: Subscriber<IpcService, [u8], ()>,
    /// Sequence number of the most recently received frame (0 = none yet).
    last_seq: u64,
    /// Whether a shutdown sentinel has been received.
    shutdown: bool,
}

impl std::fmt::Debug for FrameSubscriber {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("FrameSubscriber")
            .field("service_name", &self.service_name)
            .field("last_seq", &self.last_seq)
            .field("shutdown", &self.shutdown)
            .finish()
    }
}

impl FrameSubscriber {
    /// Open (or create) the iceoryx2 publish/subscribe service and subscribe.
    ///
    /// Uses `open_or_create` so the subscriber can attach before or after the
    /// VisionPipe publisher has started.
    ///
    /// # Arguments
    /// * `session_id` — Session identifier from [`crate::VisionPipeConfig`].
    /// * `sink_name`  — Name passed to `frame_sink()` in the .vsp script.
    pub fn connect(session_id: &str, sink_name: &str) -> Result<Self> {
        let service_name = build_service_name(session_id, sink_name);

        let node = NodeBuilder::new()
            .create::<IpcService>()
            .map_err(|e| VisionPipeError::Iox2Service(format!("NodeBuilder failed: {e:?}")))?;

        let svc_name: ServiceName = service_name
            .as_str()
            .try_into()
            .map_err(|e| VisionPipeError::Iox2Service(format!("invalid service name '{service_name}': {e:?}")))?;

        let service = node
            .service_builder(&svc_name)
            .publish_subscribe::<[u8]>()
            .open_or_create()
            .map_err(|e| VisionPipeError::Iox2Service(format!("open_or_create '{service_name}': {e:?}")))?;

        let subscriber = service
            .subscriber_builder()
            .create()
            .map_err(|e| VisionPipeError::Iox2Subscriber(format!("subscriber_builder for '{service_name}': {e:?}")))?;

        Ok(FrameSubscriber {
            service_name,
            _node: node,
            subscriber,
            last_seq: 0,
            shutdown: false,
        })
    }

    /// The iceoryx2 service name this subscriber is attached to.
    pub fn service_name(&self) -> &str {
        &self.service_name
    }

    /// Whether a shutdown sentinel has been received from the publisher.
    pub fn is_shutdown(&self) -> bool {
        self.shutdown
    }

    /// Sequence number of the most recently received frame (0 = none yet).
    pub fn last_seq(&self) -> u64 {
        self.last_seq
    }

    /// Drain the subscriber queue and return the **latest** frame available.
    ///
    /// If multiple frames have queued up since the last call, all are drained
    /// but only the newest is returned (matching the C++ behaviour in
    /// `Iox2ChannelSessionImpl::receive()`).
    ///
    /// # Returns
    /// * `Ok(Some(frame))` — A new frame is available.
    /// * `Ok(None)`        — No new frame since the last call; caller may reuse
    ///                       a previously received frame.
    /// * `Err(_)`          — Transport error.
    pub fn recv_latest(&mut self) -> Result<Option<Frame>> {
        if self.shutdown {
            return Ok(None);
        }

        let mut latest: Option<Frame> = None;

        // `while let Some(sample)` pattern lets the compiler infer types.
        while let Some(sample) = self
            .subscriber
            .receive()
            .map_err(|e| VisionPipeError::Iox2Receive(format!("{e:?}")))?
        {
            let payload: &[u8] = sample.payload();

            // Check for shutdown sentinel before full parsing.
            if payload.len() >= 8 {
                let seq = u64::from_ne_bytes(payload[0..8].try_into().unwrap());
                if seq == SHUTDOWN_SEQ {
                    self.shutdown = true;
                    return Ok(None);
                }
            }

            match Frame::from_iox2_payload(payload) {
                Ok(frame) => {
                    self.last_seq = frame.seq;
                    latest = Some(frame);
                }
                Err(e) => {
                    // Log and skip malformed samples — don't abort the loop.
                    eprintln!("[visionpipe] malformed sample on '{}': {e}", self.service_name);
                }
            }
        }

        Ok(latest)
    }

    /// Poll for a new frame (non-blocking).
    ///
    /// Returns `Ok(Some(frame))` only if the frame's sequence number is higher
    /// than the previously seen one.  This avoids re-delivering the same frame
    /// to callers when the publisher is slower than the poll rate.
    pub fn poll(&mut self) -> Result<Option<Frame>> {
        let frame = self.recv_latest()?;
        Ok(frame.filter(|f| f.seq > self.last_seq || self.last_seq == 0))
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn service_name_format() {
        let name = build_service_name("abc123_ff", "output");
        assert_eq!(name, "/vp_abc123_ff_output");
    }
}

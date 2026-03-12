//! VisionPipe session management.
//!
//! A [`Session`] represents a running VisionPipe process.  It:
//!
//! - Spawns the `visionpipe` child process with the configured script and params.
//! - Sets `VISIONPIPE_SESSION_ID` in the child's environment so that
//!   `frame_sink()` items publish to the expected iceoryx2 service names.
//! - Manages [`FrameSubscriber`] connections for each requested sink.
//! - Provides callback-based (`on_frame`) and polling (`grab_frame`) APIs.
//! - Bridges to the runtime parameter server (`set_param`, `get_param`, etc.).

use std::collections::HashMap;
use std::process::Child;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use crate::error::Result;
use crate::frame::Frame;
use crate::params::{ParamChangeEvent, ParamClient, ParamValue, WatchHandle};
use crate::transport::FrameSubscriber;

// ============================================================================
// SessionState
// ============================================================================

/// Current lifecycle state of a [`Session`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SessionState {
    /// Session created but not yet started.
    Created,
    /// VisionPipe process is running.
    Running,
    /// Stop has been requested; waiting for process exit.
    Stopping,
    /// Process exited normally.
    Stopped,
    /// Process exited with a non-zero exit code.
    Error,
}

// ============================================================================
// FrameCallback types
// ============================================================================

/// Callback invoked with each new [`Frame`] from a named sink.
pub type FrameCallback = Box<dyn FnMut(&Frame) + Send + 'static>;

/// Callback invoked with the frame sequence number whenever a new frame
/// arrives (lightweight notification; no pixel data).
pub type FrameUpdateCallback = Box<dyn FnMut(u64) + Send + 'static>;

// ============================================================================
// Session
// ============================================================================

struct SinkEntry {
    subscriber: FrameSubscriber,
    callbacks: Vec<FrameCallback>,
    update_callbacks: Vec<FrameUpdateCallback>,
    last_dispatched_seq: u64,
    /// The most recently received frame (returned by `grab_frame` polling).
    last_frame: Option<Frame>,
}

/// A running VisionPipe session.
///
/// Created by [`crate::VisionPipe::run`] or [`crate::VisionPipe::run_string`].
/// The child process is killed when the `Session` is dropped.
pub struct Session {
    session_id: String,
    child: Child,
    state: SessionState,
    config_params: HashMap<String, String>,
    sinks: HashMap<String, Arc<Mutex<SinkEntry>>>,
    param_client: Option<Arc<ParamClient>>,
    /// How long to wait for the param server port file.
    param_port_timeout: Duration,
}

impl std::fmt::Debug for Session {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Session")
            .field("session_id", &self.session_id)
            .field("state", &self.state)
            .field("pid", &self.child.id())
            .finish()
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        if self.state == SessionState::Running {
            let _ = self.child.kill();
        }
    }
}

impl Session {
    /// Internal constructor — called from [`crate::VisionPipe`].
    pub(crate) fn new(
        session_id: String,
        child: Child,
        config_params: HashMap<String, String>,
        param_port_timeout: Duration,
    ) -> Self {
        Session {
            session_id,
            child,
            state: SessionState::Running,
            config_params,
            sinks: HashMap::new(),
            param_client: None,
            param_port_timeout,
        }
    }

    // ── Metadata ─────────────────────────────────────────────────────────

    /// The unique session identifier set as `VISIONPIPE_SESSION_ID` in the
    /// child process.  Used to compute iceoryx2 service names.
    pub fn session_id(&self) -> &str {
        &self.session_id
    }

    /// PID of the child VisionPipe process.
    pub fn pid(&self) -> u32 {
        self.child.id()
    }

    /// Current session state.
    pub fn state(&self) -> SessionState {
        self.state
    }

    /// Return `true` if the child process is still running.
    pub fn is_running(&mut self) -> bool {
        if self.state != SessionState::Running {
            return false;
        }
        match self.child.try_wait() {
            Ok(Some(status)) => {
                self.state = if status.success() {
                    SessionState::Stopped
                } else {
                    SessionState::Error
                };
                false
            }
            Ok(None) => true,
            Err(_) => false,
        }
    }

    // ── Sink management ───────────────────────────────────────────────────

    /// Connect to a `frame_sink("name")` and register a callback.
    ///
    /// The subscriber is opened lazily on first call for each unique `sink_name`.
    /// Multiple callbacks can be registered per sink.
    ///
    /// ```rust,ignore
    /// session.on_frame("output", |frame| {
    ///     println!("frame {} {}×{} {}", frame.seq, frame.width, frame.height, frame.format);
    /// })?;
    /// ```
    pub fn on_frame(
        &mut self,
        sink_name: impl Into<String>,
        callback: impl FnMut(&Frame) + Send + 'static,
    ) -> Result<()> {
        let sink_name = sink_name.into();
        let entry = self.get_or_create_sink(&sink_name)?;
        entry
            .lock()
            .expect("sink mutex poisoned")
            .callbacks
            .push(Box::new(callback));
        Ok(())
    }

    /// Register a lightweight frame-update notification (no pixel data).
    ///
    /// The callback receives only the new frame sequence number.  Call
    /// [`Session::grab_frame`] from inside the callback to access pixel data.
    pub fn on_frame_update(
        &mut self,
        sink_name: impl Into<String>,
        callback: impl FnMut(u64) + Send + 'static,
    ) -> Result<()> {
        let sink_name = sink_name.into();
        let entry = self.get_or_create_sink(&sink_name)?;
        entry
            .lock()
            .expect("sink mutex poisoned")
            .update_callbacks
            .push(Box::new(callback));
        Ok(())
    }

    /// Poll a sink for the latest frame (non-blocking).
    ///
    /// Returns `Ok(Some(frame))` when a new frame is available, `Ok(None)` when
    /// nothing new has arrived.
    pub fn grab_frame(&mut self, sink_name: &str) -> Result<Option<Frame>> {
        let entry = self.get_or_create_sink(sink_name)?;
        let mut lock = entry.lock().expect("sink mutex poisoned");

        let frame = lock.subscriber.recv_latest()?;
        if let Some(ref f) = frame {
            lock.last_frame = Some(f.clone());
        }
        Ok(frame)
    }

    /// Return the most-recently received frame for a sink without consuming
    /// a new one from the iceoryx2 queue (cache lookup only).
    pub fn last_frame(&self, sink_name: &str) -> Option<Frame> {
        self.sinks
            .get(sink_name)
            .and_then(|e| e.lock().ok())
            .and_then(|lock| lock.last_frame.clone())
    }

    fn get_or_create_sink(&mut self, sink_name: &str) -> Result<Arc<Mutex<SinkEntry>>> {
        if !self.sinks.contains_key(sink_name) {
            let subscriber = FrameSubscriber::connect(&self.session_id, sink_name)?;
            let entry = Arc::new(Mutex::new(SinkEntry {
                subscriber,
                callbacks: Vec::new(),
                update_callbacks: Vec::new(),
                last_dispatched_seq: 0,
                last_frame: None,
            }));
            self.sinks.insert(sink_name.to_owned(), entry);
        }
        Ok(Arc::clone(self.sinks.get(sink_name).unwrap()))
    }

    // ── Spin loop ─────────────────────────────────────────────────────────

    /// Dispatch frame callbacks once for every registered sink (non-blocking).
    ///
    /// Returns the total number of frames dispatched across all sinks.
    pub fn spin_once(&mut self) -> Result<usize> {
        let mut total = 0;
        // Collect sink names first to avoid immutable borrow while mutating.
        let sink_names: Vec<String> = self.sinks.keys().cloned().collect();

        for sink_name in &sink_names {
            let entry_arc = self.sinks.get(sink_name).unwrap().clone();
            let mut entry = entry_arc.lock().expect("sink mutex poisoned");

            if let Some(frame) = entry.subscriber.recv_latest()? {
                let seq = frame.seq;
                if seq == entry.last_dispatched_seq {
                    continue;
                }
                entry.last_dispatched_seq = seq;
                entry.last_frame = Some(frame.clone());

                // Fire frame-update callbacks first (lightweight).
                for cb in &mut entry.update_callbacks {
                    cb(seq);
                }
                // Fire full-frame callbacks.
                for cb in &mut entry.callbacks {
                    cb(&frame);
                }
                total += 1;
            }
        }
        Ok(total)
    }

    /// Block and dispatch frame callbacks until the session exits.
    ///
    /// # Arguments
    /// * `poll_interval` — How often to check for new frames.  Defaults to 1 ms.
    ///
    /// Returns `Ok(())` when the child process exits (or is stopped via
    /// [`Session::stop`]).
    pub fn spin(&mut self, poll_interval: Duration) -> Result<()> {
        while self.is_running() {
            self.spin_once()?;
            std::thread::sleep(poll_interval);
        }
        // Drain any remaining frames after the process exits.
        let _ = self.spin_once();
        Ok(())
    }

    // ── Process control ───────────────────────────────────────────────────

    /// Stop the VisionPipe child process.
    ///
    /// Sends SIGTERM and waits up to `timeout` for a graceful exit, then
    /// sends SIGKILL.
    pub fn stop(&mut self, timeout: Duration) -> Result<()> {
        if self.state != SessionState::Running {
            return Ok(());
        }
        self.state = SessionState::Stopping;

        // POSIX: send SIGTERM first.
        #[cfg(unix)]
        {
            libc_kill(self.child.id() as i32, libc::SIGTERM);
        }

        let deadline = Instant::now() + timeout;
        loop {
            if let Ok(Some(status)) = self.child.try_wait() {
                self.state = if status.success() {
                    SessionState::Stopped
                } else {
                    SessionState::Error
                };
                return Ok(());
            }
            if Instant::now() >= deadline {
                break;
            }
            std::thread::sleep(Duration::from_millis(20));
        }

        // Force kill.
        let _ = self.child.kill();
        let _ = self.child.wait();
        self.state = SessionState::Stopped;
        Ok(())
    }

    /// Wait for the child process to exit.
    ///
    /// Blocks until the process exits or until `timeout` elapses.
    /// Returns `true` if the process exited within the timeout.
    pub fn wait_for_exit(&mut self, timeout: Option<Duration>) -> bool {
        let deadline = timeout.map(|t| Instant::now() + t);
        loop {
            match self.child.try_wait() {
                Ok(Some(status)) => {
                    self.state = if status.success() {
                        SessionState::Stopped
                    } else {
                        SessionState::Error
                    };
                    return true;
                }
                Ok(None) => {}
                Err(_) => return false,
            }

            if let Some(dl) = deadline {
                if Instant::now() >= dl {
                    return false;
                }
            }
            std::thread::sleep(Duration::from_millis(10));
        }
    }

    // ── Runtime parameter API ─────────────────────────────────────────────

    /// Get (or lazily create) the param client for this session.
    fn param_client(&mut self) -> Result<Arc<ParamClient>> {
        if self.param_client.is_none() {
            let client = ParamClient::for_pid(self.pid(), self.param_port_timeout)?;
            self.param_client = Some(Arc::new(client));
        }
        Ok(Arc::clone(self.param_client.as_ref().unwrap()))
    }

    /// Set a runtime parameter in the VisionPipe script.
    ///
    /// Connects to the param TCP server in the child process.
    ///
    /// ```rust,ignore
    /// session.set_param("brightness", &ParamValue::Int(80))?;
    /// // or with a string shorthand:
    /// session.set_param_str("brightness", "80")?;
    /// ```
    pub fn set_param(&mut self, name: &str, value: &ParamValue) -> Result<()> {
        let client = self.param_client()?;
        client.set_param(name, value)
    }

    /// Set a parameter by string value (convenience wrapper).
    pub fn set_param_str(&mut self, name: &str, value: &str) -> Result<()> {
        let client = self.param_client()?;
        client.set_param_str(name, value)
    }

    /// Get the current value of a runtime parameter.
    pub fn get_param(&mut self, name: &str) -> Result<Option<ParamValue>> {
        let client = self.param_client()?;
        client.get_param(name)
    }

    /// List all current runtime parameters and their values.
    pub fn list_params(&mut self) -> Result<HashMap<String, ParamValue>> {
        let client = self.param_client()?;
        client.list_params()
    }

    /// Subscribe to changes on a named parameter (or `"*"` for all params).
    ///
    /// The `callback` is called from a background thread whenever the
    /// parameter changes.  Returns a [`WatchHandle`] that cancels on drop.
    ///
    /// ```rust,ignore
    /// let _handle = session.watch_param("brightness", |evt| {
    ///     println!("brightness changed: {} → {}", evt.old_value, evt.new_value);
    /// })?;
    /// ```
    pub fn watch_param(
        &mut self,
        param_name: impl Into<String>,
        callback: impl Fn(ParamChangeEvent) + Send + 'static,
    ) -> Result<WatchHandle> {
        let client = self.param_client()?;
        client.watch_param(param_name, callback)
    }

    /// The configuration parameters that were passed to visionpipe at startup.
    pub fn startup_params(&self) -> &HashMap<String, String> {
        &self.config_params
    }
}

// ─── Unix-only: send signal to child ────────────────────────────────────────
#[cfg(unix)]
fn libc_kill(pid: i32, sig: i32) -> i32 {
    // SAFETY: kill(2) is a standard POSIX syscall; no invariants are violated
    // by sending SIGTERM to a child process we own.
    unsafe { libc::kill(pid as libc::pid_t, sig as libc::c_int) }
}

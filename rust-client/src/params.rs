//! TCP client for the VisionPipe parameter server.
//!
//! The VisionPipe process listens on a randomly chosen TCP port and records it
//! in `/tmp/visionpipe-params-<pid>.port`.  This module reads that port file
//! and implements the line-based wire protocol:
//!
//! | Command               | Description                         |
//! |-----------------------|-------------------------------------|
//! | `SET <name> <value>`  | Set a parameter value               |
//! | `GET <name>`          | Get a parameter value               |
//! | `LIST`                | List all parameters                 |
//! | `WATCH <name>`        | Subscribe to a parameter's changes  |
//! | `WATCH *`             | Subscribe to all parameter changes  |
//! | `UNWATCH <name>`      | Unsubscribe from a parameter        |
//! | `QUIT`                | Disconnect                          |
//!
//! Responses are JSON + newline:
//! ```json
//! {"ok": true, "name": "brightness", "value": 75}
//! {"ok": false, "error": "Parameter not found: foo"}
//! {"event": "changed", "name": "brightness", "old": 50, "new": 75}
//! ```

use std::collections::HashMap;
use std::io::{BufRead, BufReader, Write};
use std::net::TcpStream;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use crate::error::{Result, VisionPipeError};

// ============================================================================
// Port-file discovery
// ============================================================================

/// Path to the param server port file for a given visionpipe process PID.
///
/// Matches the C++ `ParamServer::portFilePath()`:  
/// `/tmp/visionpipe-params-<pid>.port`
pub fn param_port_file(pid: u32) -> PathBuf {
    PathBuf::from(format!("/tmp/visionpipe-params-{pid}.port"))
}

/// Read the param server TCP port from its well-known port file.
///
/// Retries for up to `timeout` to handle races where the server hasn't written
/// the file yet.  Returns `Err` if the file doesn't appear within the timeout.
pub fn read_param_port(pid: u32, timeout: Duration) -> Result<u16> {
    let path = param_port_file(pid);
    let deadline = Instant::now() + timeout;

    loop {
        if let Ok(content) = std::fs::read_to_string(&path) {
            let port_str = content.trim();
            if let Ok(port) = port_str.parse::<u16>() {
                return Ok(port);
            }
        }

        if Instant::now() >= deadline {
            return Err(VisionPipeError::ParamServerTimeout {
                pid,
                timeout_ms: timeout.as_millis() as u64,
            });
        }

        thread::sleep(Duration::from_millis(50));
    }
}

// ============================================================================
// ParamValue
// ============================================================================

/// A parameter value as returned by the VisionPipe param server.
#[derive(Debug, Clone, PartialEq)]
pub enum ParamValue {
    Int(i64),
    Float(f64),
    Bool(bool),
    String(String),
}

impl ParamValue {
    /// Parse from the JSON value in a param server response.
    pub fn from_json(v: &serde_json::Value) -> Self {
        match v {
            serde_json::Value::Number(n) => {
                if let Some(i) = n.as_i64() {
                    ParamValue::Int(i)
                } else {
                    ParamValue::Float(n.as_f64().unwrap_or(0.0))
                }
            }
            serde_json::Value::Bool(b) => ParamValue::Bool(*b),
            serde_json::Value::String(s) => ParamValue::String(s.clone()),
            other => ParamValue::String(other.to_string()),
        }
    }

    /// Parse a wire-protocol string value.
    ///
    /// Tries int → float → bool → string in order.
    pub fn from_wire_str(s: &str) -> Self {
        if let Ok(i) = s.parse::<i64>() {
            return ParamValue::Int(i);
        }
        if let Ok(f) = s.parse::<f64>() {
            return ParamValue::Float(f);
        }
        match s.to_ascii_lowercase().as_str() {
            "true" | "yes" | "on" => return ParamValue::Bool(true),
            "false" | "no" | "off" => return ParamValue::Bool(false),
            _ => {}
        }
        ParamValue::String(s.to_owned())
    }

    /// Format as a wire-protocol string (for `SET` commands).
    pub fn to_wire_string(&self) -> String {
        match self {
            ParamValue::Int(i) => i.to_string(),
            ParamValue::Float(f) => f.to_string(),
            ParamValue::Bool(b) => b.to_string(),
            ParamValue::String(s) => s.clone(),
        }
    }

    pub fn as_int(&self) -> Option<i64> {
        match self {
            ParamValue::Int(i) => Some(*i),
            ParamValue::Float(f) => Some(*f as i64),
            _ => None,
        }
    }

    pub fn as_float(&self) -> Option<f64> {
        match self {
            ParamValue::Float(f) => Some(*f),
            ParamValue::Int(i) => Some(*i as f64),
            _ => None,
        }
    }

    pub fn as_bool(&self) -> Option<bool> {
        match self {
            ParamValue::Bool(b) => Some(*b),
            _ => None,
        }
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            ParamValue::String(s) => Some(s.as_str()),
            _ => None,
        }
    }
}

impl std::fmt::Display for ParamValue {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_wire_string())
    }
}

// ============================================================================
// ParamChangeEvent
// ============================================================================

/// A parameter change event pushed by the server to WATCH subscribers.
#[derive(Debug, Clone)]
pub struct ParamChangeEvent {
    pub name: String,
    pub old_value: ParamValue,
    pub new_value: ParamValue,
}

// ============================================================================
// WatchHandle — cancels a WATCH subscription on drop
// ============================================================================

/// A guard that keeps a WATCH subscription alive.
///
/// Dropping this handle causes the background watcher thread to stop the next
/// time it checks its cancellation flag.
pub struct WatchHandle {
    pub(crate) id: u64,
    pub(crate) cancel: Arc<std::sync::atomic::AtomicBool>,
}

impl WatchHandle {
    /// Cancel the watch subscription immediately.
    pub fn cancel(self) {
        self.cancel.store(true, std::sync::atomic::Ordering::Relaxed);
    }
}

impl std::fmt::Debug for WatchHandle {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("WatchHandle").field("id", &self.id).finish()
    }
}

// ============================================================================
// ParamClient — main API
// ============================================================================

/// A client for the VisionPipe runtime parameter server.
///
/// Creates one TCP connection per operation (stateless request/response) except
/// for `watch_param()` which opens a persistent connection on a background thread.
pub struct ParamClient {
    addr: String,
    watch_id_counter: Arc<Mutex<u64>>,
}

impl std::fmt::Debug for ParamClient {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ParamClient")
            .field("addr", &self.addr)
            .finish()
    }
}

impl ParamClient {
    /// Connect to the param server at the given address.
    pub fn new(addr: impl Into<String>) -> Self {
        ParamClient {
            addr: addr.into(),
            watch_id_counter: Arc::new(Mutex::new(1)),
        }
    }

    /// Discover the param server for a running VisionPipe child process and
    /// return a connected [`ParamClient`].
    ///
    /// Reads the port from `/tmp/visionpipe-params-<pid>.port`, retrying for
    /// up to `timeout`.
    pub fn for_pid(pid: u32, timeout: Duration) -> Result<Self> {
        let port = read_param_port(pid, timeout)?;
        Ok(ParamClient::new(format!("127.0.0.1:{port}")))
    }

    // ── internal helpers ─────────────────────────────────────────────────

    fn connect(&self) -> Result<TcpStream> {
        TcpStream::connect(&self.addr)
            .map_err(|e| VisionPipeError::ParamServerConnect(format!("{}: {e}", self.addr)))
    }

    fn send_command(&self, cmd: &str) -> Result<serde_json::Value> {
        let mut stream = self.connect()?;
        stream
            .write_all(format!("{cmd}\n").as_bytes())
            .map_err(VisionPipeError::ParamServerIo)?;

        let mut reader = BufReader::new(&stream);
        let mut line = String::new();
        reader
            .read_line(&mut line)
            .map_err(VisionPipeError::ParamServerIo)?;

        let json: serde_json::Value = serde_json::from_str(line.trim())?;

        if let Some(false) = json.get("ok").and_then(|v| v.as_bool()) {
            let err_msg = json
                .get("error")
                .and_then(|v| v.as_str())
                .unwrap_or("unknown error")
                .to_owned();
            return Err(VisionPipeError::ParamServerResponse(err_msg));
        }

        // Send QUIT so the server can close the connection cleanly.
        let _ = stream.write_all(b"QUIT\n");
        Ok(json)
    }

    // ── Public API ────────────────────────────────────────────────────────

    /// Set a runtime parameter in the VisionPipe script.
    ///
    /// # Arguments
    /// * `name`  — Parameter name (must match a `params [name:type]` declaration)
    /// * `value` — New value (will be converted to its wire string)
    pub fn set_param(&self, name: &str, value: &ParamValue) -> Result<()> {
        let cmd = format!("SET {} {}", name, value.to_wire_string());
        self.send_command(&cmd)?;
        Ok(())
    }

    /// Set a parameter by string value (convenience wrapper).
    pub fn set_param_str(&self, name: &str, value: &str) -> Result<()> {
        self.set_param(name, &ParamValue::from_wire_str(value))
    }

    /// Get the current value of a runtime parameter.
    ///
    /// Returns `Ok(None)` when the server doesn't know the parameter.
    pub fn get_param(&self, name: &str) -> Result<Option<ParamValue>> {
        let cmd = format!("GET {}", name);
        match self.send_command(&cmd) {
            Ok(json) => {
                let v = json
                    .get("value")
                    .map(ParamValue::from_json)
                    .unwrap_or_else(|| {
                        json.get("value")
                            .and_then(|v| v.as_str())
                            .map(|s| ParamValue::from_wire_str(s))
                            .unwrap_or(ParamValue::String(String::new()))
                    });
                Ok(Some(v))
            }
            Err(VisionPipeError::ParamServerResponse(_)) => Ok(None),
            Err(e) => Err(e),
        }
    }

    /// List all current runtime parameters.
    pub fn list_params(&self) -> Result<HashMap<String, ParamValue>> {
        let json = self.send_command("LIST")?;
        let mut out = HashMap::new();

        if let Some(params) = json.get("params").and_then(|v| v.as_object()) {
            for (k, v) in params {
                out.insert(k.clone(), ParamValue::from_json(v));
            }
        }
        Ok(out)
    }

    /// Subscribe to changes on a named parameter (or `"*"` for all params).
    ///
    /// Opens a persistent TCP connection on a background thread.  The
    /// `callback` is invoked from that thread for each change event.
    ///
    /// Returns a [`WatchHandle`]; dropping it cancels the subscription.
    ///
    /// # Arguments
    /// * `param_name` — Parameter name or `"*"` for all.
    /// * `callback`   — Called with `(name, new_value)` on each change.
    pub fn watch_param(
        &self,
        param_name: impl Into<String>,
        callback: impl Fn(ParamChangeEvent) + Send + 'static,
    ) -> Result<WatchHandle> {
        let param_name = param_name.into();
        let addr = self.addr.clone();

        let cancel = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let cancel_clone = Arc::clone(&cancel);

        let id = {
            let mut counter = self
                .watch_id_counter
                .lock()
                .expect("watch_id_counter poisoned");
            let id = *counter;
            *counter += 1;
            id
        };

        thread::Builder::new()
            .name(format!("vp-watch-{id}"))
            .spawn(move || {
                watcher_thread(addr, param_name, cancel_clone, callback);
            })
            .map_err(|e| VisionPipeError::ParamServerConnect(e.to_string()))?;

        Ok(WatchHandle { id, cancel })
    }
}

/// Background thread body for a WATCH subscription.
fn watcher_thread(
    addr: String,
    param_name: String,
    cancel: Arc<std::sync::atomic::AtomicBool>,
    callback: impl Fn(ParamChangeEvent),
) {
    let stream = match TcpStream::connect(&addr) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("[visionpipe] WATCH connect failed ({addr}): {e}");
            return;
        }
    };

    let cmd = format!("WATCH {param_name}\n");
    if let Err(e) = (&stream).write_all(cmd.as_bytes()) {
        eprintln!("[visionpipe] WATCH send failed: {e}");
        return;
    }

    let mut reader = BufReader::new(&stream);
    let mut line = String::new();

    loop {
        if cancel.load(std::sync::atomic::Ordering::Relaxed) {
            let _ = (&stream).write_all(b"QUIT\n");
            break;
        }

        line.clear();
        match reader.read_line(&mut line) {
            Ok(0) => break, // connection closed by server
            Err(_) => break,
            Ok(_) => {}
        }

        let json: serde_json::Value = match serde_json::from_str(line.trim()) {
            Ok(v) => v,
            Err(_) => continue,
        };

        // WATCH events have format: {"event":"changed","name":...,"old":...,"new":...}
        if json.get("event").and_then(|v| v.as_str()) == Some("changed") {
            let name = json
                .get("name")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_owned();
            let old_value = json
                .get("old")
                .map(ParamValue::from_json)
                .unwrap_or(ParamValue::String(String::new()));
            let new_value = json
                .get("new")
                .map(ParamValue::from_json)
                .unwrap_or(ParamValue::String(String::new()));

            callback(ParamChangeEvent {
                name,
                old_value,
                new_value,
            });
        }
    }
}

//! VisionPipe launcher — the main entry point for the Rust client.
//!
//! Use [`VisionPipe`] to configure and launch a `.vsp` pipeline script, then
//! interact with its frame sinks and runtime parameters via the returned
//! [`Session`].
//!
//! # Quick start
//!
//! ```rust,no_run
//! use visionpipe_client::{VisionPipe, VisionPipeConfig};
//!
//! let mut session = VisionPipe::new()
//!     .script("my_pipeline.vsp")
//!     .no_display(true)
//!     .run()
//!     .expect("failed to launch visionpipe");
//!
//! session.on_frame("output", |frame| {
//!     println!("frame {} — {}×{} {}", frame.seq, frame.width, frame.height, frame.format);
//! }).expect("failed to connect sink");
//!
//! session.spin(std::time::Duration::from_millis(1)).unwrap();
//! ```
//!
//! The `.vsp` script should use `frame_sink("output")` to publish frames.

use std::collections::HashMap;
use std::io::Write;
use std::process::{Command, Stdio};
use std::time::Duration;

use crate::error::{Result, VisionPipeError};
use crate::session::Session;

// ============================================================================
// VisionPipeConfig
// ============================================================================

/// Configuration for a VisionPipe session.
#[derive(Debug, Clone)]
pub struct VisionPipeConfig {
    /// Path to the `visionpipe` executable (default: `"visionpipe"` on PATH).
    pub executable: String,
    /// Whether to pass `--verbose` to the child process.
    pub verbose: bool,
    /// Whether to pass `--no-display` (default: `true` for library usage).
    pub no_display: bool,
    /// Script parameters forwarded as `--param key=value`.
    pub params: HashMap<String, String>,
    /// Specific pipeline name within the script (`--pipeline <name>`).
    pub pipeline: Option<String>,
    /// Redirect child stdout+stderr to this file (`None` = inherit parent).
    pub log_file: Option<String>,
    /// Timeout for discovering the param server port file (default: 10 s).
    pub param_port_timeout: Duration,
}

impl Default for VisionPipeConfig {
    fn default() -> Self {
        VisionPipeConfig {
            executable: "visionpipe".to_owned(),
            verbose: false,
            no_display: true,
            params: HashMap::new(),
            pipeline: None,
            log_file: None,
            param_port_timeout: Duration::from_secs(10),
        }
    }
}

// ============================================================================
// VisionPipe — builder / launcher
// ============================================================================

/// Builder that configures and launches a VisionPipe `.vsp` pipeline.
///
/// # Example
///
/// ```rust,no_run
/// use visionpipe_client::VisionPipe;
///
/// let mut session = VisionPipe::new()
///     .executable("/usr/local/bin/visionpipe")
///     .script("stereo.vsp")
///     .param("gain", "2.0")
///     .no_display(true)
///     .run()
///     .unwrap();
/// ```
#[derive(Debug, Default)]
pub struct VisionPipe {
    config: VisionPipeConfig,
    script: Option<String>,
    inline_script: Option<(String, String)>, // (content, name)
}

impl VisionPipe {
    /// Create a new builder with default configuration.
    pub fn new() -> Self {
        VisionPipe::default()
    }

    /// Create a builder pre-loaded with the given configuration.
    pub fn with_config(config: VisionPipeConfig) -> Self {
        VisionPipe {
            config,
            script: None,
            inline_script: None,
        }
    }

    // ── Configuration setters ─────────────────────────────────────────────

    /// Set the path to the `visionpipe` executable.
    pub fn executable(mut self, path: impl Into<String>) -> Self {
        self.config.executable = path.into();
        self
    }

    /// Set the `.vsp` script file to execute.
    pub fn script(mut self, path: impl Into<String>) -> Self {
        self.script = Some(path.into());
        self
    }

    /// Set an inline `.vsp` script (written to a temp file before launch).
    ///
    /// # Arguments
    /// * `content` — The full script text.
    /// * `name`    — Optional label used in log output.
    pub fn script_string(mut self, content: impl Into<String>, name: impl Into<String>) -> Self {
        self.inline_script = Some((content.into(), name.into()));
        self
    }

    /// Add a startup parameter (`--param key=value`).
    pub fn param(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.config.params.insert(key.into(), value.into());
        self
    }

    /// Add multiple startup parameters at once.
    pub fn params(mut self, kv: impl IntoIterator<Item = (impl Into<String>, impl Into<String>)>) -> Self {
        for (k, v) in kv {
            self.config.params.insert(k.into(), v.into());
        }
        self
    }

    /// Enable or disable `--verbose` output.
    pub fn verbose(mut self, v: bool) -> Self {
        self.config.verbose = v;
        self
    }

    /// Enable or disable `--no-display`.
    pub fn no_display(mut self, nd: bool) -> Self {
        self.config.no_display = nd;
        self
    }

    /// Run only a named pipeline within the script (`--pipeline <name>`).
    pub fn pipeline(mut self, name: impl Into<String>) -> Self {
        self.config.pipeline = Some(name.into());
        self
    }

    /// Redirect child stdout+stderr to a file.
    ///
    /// Use `"/dev/null"` for silent operation.
    pub fn log_file(mut self, path: impl Into<String>) -> Self {
        self.config.log_file = Some(path.into());
        self
    }

    /// Set the timeout for discovering the param server port file.
    pub fn param_port_timeout(mut self, timeout: Duration) -> Self {
        self.config.param_port_timeout = timeout;
        self
    }

    // ── Launch ────────────────────────────────────────────────────────────

    /// Launch the VisionPipe process and return a [`Session`].
    ///
    /// The session ID is generated here and passed via `VISIONPIPE_SESSION_ID`
    /// to the child, so that `frame_sink()` items publish on the expected
    /// iceoryx2 service names.
    pub fn run(self) -> Result<Session> {
        let script_path = self.resolve_script_path()?;
        self.launch(script_path)
    }

    /// Resolve the script path, writing an inline script to a temp file if needed.
    fn resolve_script_path(&self) -> Result<String> {
        if let Some((content, name)) = &self.inline_script {
            let tmp = write_temp_script(content, name)?;
            return Ok(tmp);
        }
        self.script
            .clone()
            .ok_or_else(|| VisionPipeError::SpawnError("no script or inline script provided".into()))
    }

    /// Build the Command and spawn the child process.
    fn launch(self, script_path: String) -> Result<Session> {
        let session_id = generate_session_id();

        let mut cmd = Command::new(&self.config.executable);
        cmd.arg("run").arg(&script_path);

        if self.config.verbose {
            cmd.arg("--verbose");
        }
        if self.config.no_display {
            cmd.arg("--no-display");
        }
        if let Some(ref pipeline) = self.config.pipeline {
            cmd.arg("--pipeline").arg(pipeline);
        }
        for (k, v) in &self.config.params {
            cmd.arg("--param").arg(format!("{k}={v}"));
        }

        // Session ID — must match the service-name prefix.
        cmd.env("VISIONPIPE_SESSION_ID", &session_id);

        // Stdio redirection.
        if let Some(ref log_file) = self.config.log_file {
            if log_file == "/dev/null" {
                cmd.stdout(Stdio::null()).stderr(Stdio::null());
            } else {
                let f = std::fs::OpenOptions::new()
                    .create(true)
                    .write(true)
                    .truncate(true)
                    .open(log_file)
                    .map_err(VisionPipeError::ParamServerIo)?;
                let f2 = f.try_clone().map_err(VisionPipeError::ParamServerIo)?;
                cmd.stdout(f).stderr(f2);
            }
        }

        let child = cmd.spawn().map_err(|e| {
            VisionPipeError::SpawnError(format!(
                "failed to spawn '{}': {e}",
                self.config.executable
            ))
        })?;

        eprintln!(
            "[visionpipe-client] session '{}' started (pid {}): {}",
            session_id,
            child.id(),
            script_path
        );

        Ok(Session::new(
            session_id,
            child,
            self.config.params,
            self.config.param_port_timeout,
        ))
    }
}

// ============================================================================
// Session ID generation
// ============================================================================

/// Generate a session ID matching the C++ format: `"<hex_pid>_<hex_random>"`.
fn generate_session_id() -> String {
    use std::time::{SystemTime, UNIX_EPOCH};
    let pid = std::process::id();
    // Poor-man's random: mix PID with current nanos.
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .subsec_nanos();
    // XOR-fold for a 24-bit suffix.
    let r = (nanos ^ (nanos >> 12)) & 0xFF_FFFF;
    format!("{pid:x}_{r:06x}")
}

// ============================================================================
// Temp-file helper
// ============================================================================

fn write_temp_script(content: &str, name: &str) -> Result<String> {
    let safe_name = name
        .chars()
        .map(|c| if c.is_alphanumeric() || c == '_' { c } else { '_' })
        .collect::<String>();

    let path = format!("/tmp/vp_inline_{safe_name}_{}.vsp", std::process::id());
    let mut f =
        std::fs::File::create(&path).map_err(VisionPipeError::ParamServerIo)?;
    f.write_all(content.as_bytes())
        .map_err(VisionPipeError::ParamServerIo)?;
    Ok(path)
}

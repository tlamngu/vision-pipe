use thiserror::Error;

/// Errors returned by the VisionPipe Rust client.
#[derive(Debug, Error)]
pub enum VisionPipeError {
    /// iceoryx2 service open/create failed (name, reason)
    #[error("iceoryx2 service error: {0}")]
    Iox2Service(String),

    /// iceoryx2 subscriber creation failed
    #[error("iceoryx2 subscriber error: {0}")]
    Iox2Subscriber(String),

    /// iceoryx2 receive error
    #[error("iceoryx2 receive error: {0}")]
    Iox2Receive(String),

    /// The received payload is too short to contain a valid frame header
    #[error("payload too short: got {got} bytes, need at least {need}")]
    PayloadTooShort { got: usize, need: usize },

    /// The payload data size field doesn't match the actual slice length
    #[error("payload data size mismatch: header says {header} bytes, slice has {actual}")]
    PayloadSizeMismatch { header: usize, actual: usize },

    /// cv::Mat type value is not recognised by the Rust client
    #[error("unknown cv::Mat type value {0}")]
    UnknownMatType(i32),

    /// Param server TCP connection failed
    #[error("param server connection failed: {0}")]
    ParamServerConnect(String),

    /// Param server I/O error
    #[error("param server I/O: {0}")]
    ParamServerIo(#[from] std::io::Error),

    /// Param server port file not found within timeout
    #[error("param server port not available for pid {pid} (timeout {timeout_ms}ms)")]
    ParamServerTimeout { pid: u32, timeout_ms: u64 },

    /// JSON parse error from param server response
    #[error("param server JSON error: {0}")]
    ParamServerJson(#[from] serde_json::Error),

    /// Param server reported an error in its response
    #[error("param server error: {0}")]
    ParamServerResponse(String),

    /// Failed to spawn the visionpipe child process
    #[error("failed to spawn visionpipe: {0}")]
    SpawnError(String),

    /// The session has already been stopped or has not been started yet
    #[error("session is not running")]
    SessionNotRunning,

    /// A named sink was not found (was it declared in the .vsp script?)
    #[error("sink '{0}' not found or not yet available")]
    SinkNotFound(String),

    /// A feature was requested that requires a compile-time feature flag
    #[error("feature not enabled: {0}")]
    FeatureNotEnabled(&'static str),
}

pub type Result<T> = std::result::Result<T, VisionPipeError>;

//! # discovery – VisionPipe sink and capture device discovery
//!
//! This module provides pure-Rust implementations of the same discovery APIs
//! that are available in libvisionpipe (C++) and the VSP built-in functions:
//!
//! | Function                                   | Description                                            |
//! |--------------------------------------------|--------------------------------------------------------|
//! | [`discover_sinks`]                         | Enumerate active `frame_sink()` IPC segments           |
//! | [`sink_properties`]                        | Read frame properties from an active sink              |
//! | [`discover_capture`]                       | Enumerate available video capture devices              |
//! | [`capture_capabilities`]                   | Query capabilities of a specific capture device        |
//!
//! ## Sink discovery
//!
//! ```rust,no_run
//! use visionpipe_client::discovery::{discover_sinks, sink_properties};
//!
//! let sinks = discover_sinks();
//! for s in &sinks {
//!     println!("{} / {}  pid={} alive={}", s.session_id, s.sink_name,
//!              s.producer_pid, s.producer_alive);
//!     if let Some(props) = sink_properties(&s.session_id, &s.sink_name) {
//!         println!("  {}×{} ch={} seq={}", props.cols, props.rows,
//!                  props.channels, props.frame_seq);
//!     }
//! }
//! ```
//!
//! ## Capture device discovery
//!
//! ```rust,no_run
//! use visionpipe_client::discovery::{discover_capture, capture_capabilities};
//!
//! for dev in discover_capture() {
//!     println!("{} – {} ({})", dev.path, dev.device_name, dev.driver_name);
//! }
//!
//! let caps = capture_capabilities("/dev/video0");
//! for fmt in &caps.formats {
//!     println!("  {} {}×{}  {:.1}–{:.1} fps",
//!              fmt.pixel_format, fmt.width, fmt.height,
//!              fmt.fps_min, fmt.fps_max);
//! }
//! ```

use std::path::PathBuf;

// ============================================================================
// Sink discovery
// ============================================================================

/// Information about one active `frame_sink()` IPC shared-memory segment.
#[derive(Debug, Clone)]
pub struct SinkInfo {
    /// Session ID of the producer (from the `VISIONPIPE_SESSION_ID` env var).
    pub session_id: String,
    /// Sink name – the argument passed to `frame_sink("name")`.
    pub sink_name: String,
    /// Full shared-memory segment name (e.g. `visionpipe_sink_<sid>_<name>`).
    pub shm_name: String,
    /// PID of the visionpipe producer process, as recorded in the SHM header.
    pub producer_pid: i32,
    /// `true` when `kill(pid, 0)` (POSIX) confirms the producer is alive.
    pub producer_alive: bool,
}

/// Frame properties read from an active sink's shared-memory header.
#[derive(Debug, Clone, Default)]
pub struct SinkProperties {
    /// `true` when the segment was found and the magic value matched.
    pub is_valid: bool,
    /// Frame height in pixels.
    pub rows: i32,
    /// Frame width in pixels.
    pub cols: i32,
    /// Number of colour channels (derived from `cv_type`).
    pub channels: i32,
    /// Raw OpenCV `Mat::type()` integer (e.g. `CV_8UC3 == 16`).
    pub cv_type: i32,
    /// Bytes per row (stride).
    pub step: i32,
    /// Actual frame payload bytes.
    pub data_size: u32,
    /// Monotonically increasing frame counter at the time of the query.
    pub frame_seq: u64,
    /// PID of the producer process.
    pub producer_pid: i32,
    /// `true` when the producer process is alive.
    pub producer_alive: bool,
}

// ─── SHM constants matching frame_transport.h ────────────────────────────────

const SHM_PREFIX: &str = "visionpipe_sink_";
const SHM_MAGIC: u32 = 0x5650_5346; // "VPSF"
const MAX_FRAME_BYTES: usize = 3840 * 2160 * 4;

// FrameShmHeader layout (matches the C++ struct, all fields little-endian):
//   u32  magic                   offset  0
//   u64  frame_seq (atomic)      offset  4   (8-byte aligned → offset 8)
//   i32  rows                    offset 16
//   i32  cols                    offset 20
//   i32  type                    offset 24
//   i32  step                    offset 28
//   u32  data_size               offset 32
//   i32  writer_lock (atomic)    offset 36
//   i32  producer_pid            offset 40
//   u8   _pad[16]                offset 44
//  Total = 60 bytes; next power-of-two aligned = 64
//
// NOTE: std::atomic<uint64_t> is 8 bytes + 8-byte alignment.
// The C++ compiler places `frameSeq` at offset 8 (after 4 bytes of magic + 4 bytes padding).

fn shm_total_size() -> usize {
    std::mem::size_of::<[u8; 64]>() + MAX_FRAME_BYTES
}

/// Parse `"visionpipe_sink_<sessionId>_<sinkName>"` from a bare shm filename.
///
/// The session ID has the form `<hex_pid>_<hex_random>` (exactly one `_`
/// between the two parts), so we split on the third `_` overall.
fn parse_shm_name(bare: &str) -> Option<(String, String)> {
    let rest = bare.strip_prefix(SHM_PREFIX)?;
    // Find the second '_' (splitting session_id from sink_name)
    let first = rest.find('_')?;
    let second = rest[first + 1..].find('_').map(|i| first + 1 + i)?;
    let session_id = rest[..second].to_owned();
    let sink_name = rest[second + 1..].to_owned();
    if sink_name.is_empty() {
        return None;
    }
    Some((session_id, sink_name))
}

#[cfg(unix)]
mod posix_shm {
    use super::*;
    use std::ffi::CString;

    /// Read the 64-byte SHM header for the named segment.
    /// Returns `None` if the segment does not exist, has wrong size, or magic mismatch.
    pub fn read_shm_header(shm_name: &str) -> Option<[u8; 64]> {
        let full_name = format!("/{}", shm_name);
        let cname = CString::new(full_name).ok()?;

        // SAFETY: shm_open is a standard POSIX call. We check the return value.
        let fd = unsafe { libc::shm_open(cname.as_ptr(), libc::O_RDONLY, 0o444) };
        if fd < 0 {
            return None;
        }

        let total = shm_total_size();
        let mapped = unsafe {
            libc::mmap(
                std::ptr::null_mut(),
                total,
                libc::PROT_READ,
                libc::MAP_SHARED,
                fd,
                0,
            )
        };
        unsafe { libc::close(fd) };

        if mapped == libc::MAP_FAILED {
            return None;
        }

        let mut header = [0u8; 64];
        unsafe { std::ptr::copy_nonoverlapping(mapped as *const u8, header.as_mut_ptr(), 64) };
        unsafe { libc::munmap(mapped, total) };

        // Verify magic (bytes 0–3, little-endian)
        let magic = u32::from_le_bytes([header[0], header[1], header[2], header[3]]);
        if magic != SHM_MAGIC {
            return None;
        }

        Some(header)
    }

    pub fn is_pid_alive(pid: i32) -> bool {
        if pid <= 0 {
            return true; // unknown – assume alive
        }
        // SAFETY: kill with signal 0 only checks process existence.
        let ret = unsafe { libc::kill(pid as libc::pid_t, 0) };
        ret == 0 || (ret != 0 && unsafe { *libc::__errno_location() } == libc::EPERM)
    }
}

/// Decode a SHM header byte slice into a [`SinkProperties`].
fn header_to_properties(header: &[u8; 64], producer_pid: i32, producer_alive: bool) -> SinkProperties {
    // frame_seq is at offset 8 (u64, little-endian)
    let frame_seq = u64::from_le_bytes(header[8..16].try_into().unwrap_or([0; 8]));
    let rows       = i32::from_le_bytes(header[16..20].try_into().unwrap_or([0; 4]));
    let cols       = i32::from_le_bytes(header[20..24].try_into().unwrap_or([0; 4]));
    let cv_type    = i32::from_le_bytes(header[24..28].try_into().unwrap_or([0; 4]));
    let step       = i32::from_le_bytes(header[28..32].try_into().unwrap_or([0; 4]));
    let data_size  = u32::from_le_bytes(header[32..36].try_into().unwrap_or([0; 4]));
    // channels = (cv_type >> 3) + 1  (OpenCV encoding)
    let channels = (cv_type >> 3) + 1;

    SinkProperties {
        is_valid: true,
        rows,
        cols,
        channels,
        cv_type,
        step,
        data_size,
        frame_seq,
        producer_pid,
        producer_alive,
    }
}

/// Enumerate all active `frame_sink()` IPC segments visible to this process.
///
/// On Linux/macOS this scans `/dev/shm` for files matching `visionpipe_sink_*`.
/// Each segment is opened read-only to verify the magic and read the producer PID.
///
/// Returns a sorted list of [`SinkInfo`] structs.
pub fn discover_sinks() -> Vec<SinkInfo> {
    let mut results = Vec::new();

    #[cfg(unix)]
    {
        let shm_dir = PathBuf::from("/dev/shm");
        if let Ok(entries) = std::fs::read_dir(&shm_dir) {
            for entry in entries.flatten() {
                let name = entry.file_name().to_string_lossy().into_owned();
                let Some((session_id, sink_name)) = parse_shm_name(&name) else {
                    continue;
                };

                let Some(header) = posix_shm::read_shm_header(&name) else {
                    continue;
                };

                let producer_pid =
                    i32::from_le_bytes(header[40..44].try_into().unwrap_or([0; 4]));
                let producer_alive = posix_shm::is_pid_alive(producer_pid);

                results.push(SinkInfo {
                    session_id,
                    sink_name,
                    shm_name: name,
                    producer_pid,
                    producer_alive,
                });
            }
        }
    }

    results.sort_by(|a, b| {
        a.session_id
            .cmp(&b.session_id)
            .then(a.sink_name.cmp(&b.sink_name))
    });
    results
}

/// Read the frame properties from an active sink without consuming any frames.
///
/// Opens the shared-memory segment for `(session_id, sink_name)` read-only,
/// validates the magic value, and returns a [`SinkProperties`] snapshot.
///
/// Returns `None` when the segment does not exist or fails validation.
pub fn sink_properties(session_id: &str, sink_name: &str) -> Option<SinkProperties> {
    let bare_name = format!("{}{}_{}",  SHM_PREFIX, session_id, sink_name);

    #[cfg(unix)]
    {
        let header = posix_shm::read_shm_header(&bare_name)?;
        let producer_pid =
            i32::from_le_bytes(header[40..44].try_into().unwrap_or([0; 4]));
        let producer_alive = posix_shm::is_pid_alive(producer_pid);
        return Some(header_to_properties(&header, producer_pid, producer_alive));
    }

    #[allow(unreachable_code)]
    None
}

// ============================================================================
// Capture device discovery
// ============================================================================

/// A single supported format/resolution entry for a capture device.
#[derive(Debug, Clone)]
pub struct CaptureFormat {
    /// FourCC pixel format string, e.g. `"YUYV"`, `"MJPG"`, `"NV12"`, `"SRGGB10"`.
    pub pixel_format: String,
    /// Frame width in pixels.
    pub width: u32,
    /// Frame height in pixels.
    pub height: u32,
    /// Minimum supported framerate.
    pub fps_min: f64,
    /// Maximum supported framerate.
    pub fps_max: f64,
}

/// Information about a video capture device.
#[derive(Debug, Clone, Default)]
pub struct CaptureDevice {
    /// Device path, e.g. `"/dev/video0"`.
    pub path: String,
    /// Human-readable card/device name.
    pub device_name: String,
    /// Kernel driver name (V4L2) or backend string.
    pub driver_name: String,
    /// USB bus path or PCIe slot string.
    pub bus_info: String,
    /// Enumeration backend: `"v4l2"`, `"libcamera"`, or `"opencv"`.
    pub backend: String,
    /// `true` when the device can produce video frames.
    pub is_capture: bool,
    /// `true` when V4L2 controls (exposure, focus, …) are available.
    pub has_controls: bool,
    /// Supported format × resolution combinations.
    pub formats: Vec<CaptureFormat>,
}

#[cfg(target_os = "linux")]
mod v4l2 {
    use super::{CaptureDevice, CaptureFormat};
    use std::fs::File;
    use std::os::unix::io::AsRawFd;

    // V4L2 ioctl numbers (from <linux/videodev2.h>)
    // These are architecture-specific; the values below match x86-64 and ARM.
    const VIDIOC_QUERYCAP: u64          = 0x8068_5600;
    const VIDIOC_ENUM_FMT: u64          = 0xC040_5602;
    const VIDIOC_ENUM_FRAMESIZES: u64   = 0xC02C_560A;
    const VIDIOC_ENUM_FRAMEINTERVALS: u64 = 0xC034_5614;
    const VIDIOC_QUERYCTRL: u64         = 0xC0445624;

    // V4L2_CAP_* flags
    const V4L2_CAP_VIDEO_CAPTURE: u32  = 0x0000_0001;
    const V4L2_CAP_DEVICE_CAPS: u32    = 0x8000_0000;

    // V4L2_FRMSIZE_TYPE_*
    const V4L2_FRMSIZE_TYPE_DISCRETE: u32   = 1;

    // V4L2_FRMIVAL_TYPE_*
    const V4L2_FRMIVAL_TYPE_DISCRETE: u32   = 1;

    // V4L2_CTRL_FLAG_NEXT_CTRL
    const V4L2_CTRL_FLAG_NEXT_CTRL: u32 = 0x8000_0000;

    // ─── repr(C) structs ──────────────────────────────────────────────────────

    #[repr(C)]
    struct V4L2Capability {
        driver:       [u8; 16],
        card:         [u8; 32],
        bus_info:     [u8; 32],
        version:      u32,
        capabilities: u32,
        device_caps:  u32,
        reserved:     [u32; 3],
    }

    #[repr(C)]
    struct V4L2FmtDesc {
        index:        u32,
        buf_type:     u32,
        flags:        u32,
        description:  [u8; 32],
        pixelformat:  u32,
        mbus_code:    u32,
        reserved:     [u32; 3],
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    struct V4L2Fract {
        numerator:   u32,
        denominator: u32,
    }

    #[repr(C)]
    union V4L2FrmSizeUnion {
        discrete:  V4L2FrmSizeDiscrete,
        stepwise:  V4L2FrmSizeStepwise,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    struct V4L2FrmSizeDiscrete {
        width:  u32,
        height: u32,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    struct V4L2FrmSizeStepwise {
        min_width:   u32,
        max_width:   u32,
        step_width:  u32,
        min_height:  u32,
        max_height:  u32,
        step_height: u32,
    }

    #[repr(C)]
    struct V4L2FrmSizeEnum {
        index:        u32,
        pixel_format: u32,
        frmsize_type: u32,
        sizes:        V4L2FrmSizeUnion,
        reserved:     [u32; 2],
    }

    #[repr(C)]
    union V4L2FrmIvalUnion {
        discrete: V4L2Fract,
        stepwise: V4L2FrmIvalStepwise,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    struct V4L2FrmIvalStepwise {
        min:  V4L2Fract,
        max:  V4L2Fract,
        step: V4L2Fract,
    }

    #[repr(C)]
    struct V4L2FrmIvalEnum {
        index:        u32,
        pixel_format: u32,
        width:        u32,
        height:       u32,
        frmival_type: u32,
        ivals:        V4L2FrmIvalUnion,
        reserved:     [u32; 2],
    }

    #[repr(C)]
    struct V4L2QueryCtrl {
        id:         u32,
        ctrl_type:  u32,
        name:       [u8; 32],
        minimum:    i32,
        maximum:    i32,
        step:       i32,
        default_value: i32,
        flags:      u32,
        reserved:   [u32; 2],
    }

    const V4L2_BUF_TYPE_VIDEO_CAPTURE: u32 = 1;

    fn fourcc_to_string(fourcc: u32) -> String {
        let bytes = [
            (fourcc & 0xFF) as u8,
            ((fourcc >> 8) & 0xFF) as u8,
            ((fourcc >> 16) & 0xFF) as u8,
            ((fourcc >> 24) & 0xFF) as u8,
        ];
        String::from_utf8_lossy(&bytes)
            .trim_end_matches(' ')
            .to_owned()
    }

    fn cstr_to_string(buf: &[u8]) -> String {
        let end = buf.iter().position(|&b| b == 0).unwrap_or(buf.len());
        String::from_utf8_lossy(&buf[..end]).into_owned()
    }

    fn fract_to_fps(n: u32, d: u32) -> f64 {
        if n == 0 { 30.0 } else { d as f64 / n as f64 }
    }

    pub fn query_v4l2_device(path: &str) -> CaptureDevice {
        let mut dev = CaptureDevice {
            path: path.to_owned(),
            backend: "v4l2".to_owned(),
            ..Default::default()
        };

        let file = match File::open(path) {
            Ok(f) => f,
            Err(_) => return dev,
        };
        let fd = file.as_raw_fd();

        // VIDIOC_QUERYCAP
        let mut cap = V4L2Capability {
            driver: [0; 16],
            card: [0; 32],
            bus_info: [0; 32],
            version: 0,
            capabilities: 0,
            device_caps: 0,
            reserved: [0; 3],
        };
        // SAFETY: the fd is valid, the struct is repr(C) of the correct size.
        let ret = unsafe { libc::ioctl(fd, VIDIOC_QUERYCAP, &mut cap as *mut _) };
        if ret < 0 {
            return dev;
        }

        dev.device_name = cstr_to_string(&cap.card);
        dev.driver_name = cstr_to_string(&cap.driver);
        dev.bus_info    = cstr_to_string(&cap.bus_info);

        let effective_caps = if cap.capabilities & V4L2_CAP_DEVICE_CAPS != 0 {
            cap.device_caps
        } else {
            cap.capabilities
        };
        dev.is_capture = (effective_caps & V4L2_CAP_VIDEO_CAPTURE) != 0;

        // Check control availability
        let mut qctrl = V4L2QueryCtrl {
            id:            V4L2_CTRL_FLAG_NEXT_CTRL,
            ctrl_type:     0,
            name:          [0; 32],
            minimum:       0,
            maximum:       0,
            step:          0,
            default_value: 0,
            flags:         0,
            reserved:      [0; 2],
        };
        dev.has_controls =
            unsafe { libc::ioctl(fd, VIDIOC_QUERYCTRL, &mut qctrl as *mut _) } == 0;

        if !dev.is_capture {
            return dev;
        }

        // VIDIOC_ENUM_FMT
        let mut fmtd = V4L2FmtDesc {
            index:       0,
            buf_type:    V4L2_BUF_TYPE_VIDEO_CAPTURE,
            flags:       0,
            description: [0; 32],
            pixelformat: 0,
            mbus_code:   0,
            reserved:    [0; 3],
        };

        loop {
            let ret = unsafe { libc::ioctl(fd, VIDIOC_ENUM_FMT, &mut fmtd as *mut _) };
            if ret < 0 {
                break;
            }
            let pix_fmt = fourcc_to_string(fmtd.pixelformat);

            // VIDIOC_ENUM_FRAMESIZES
            let mut fsize = V4L2FrmSizeEnum {
                index:        0,
                pixel_format: fmtd.pixelformat,
                frmsize_type: 0,
                sizes:        V4L2FrmSizeUnion {
                    discrete: V4L2FrmSizeDiscrete { width: 0, height: 0 },
                },
                reserved:     [0; 2],
            };

            let mut got_sizes = false;
            loop {
                let ret =
                    unsafe { libc::ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &mut fsize as *mut _) };
                if ret < 0 {
                    break;
                }
                got_sizes = true;

                let (w, h) = if fsize.frmsize_type == V4L2_FRMSIZE_TYPE_DISCRETE {
                    let d = unsafe { fsize.sizes.discrete };
                    (d.width, d.height)
                } else {
                    let s = unsafe { fsize.sizes.stepwise };
                    (s.max_width, s.max_height)
                };

                // VIDIOC_ENUM_FRAMEINTERVALS
                let mut fival = V4L2FrmIvalEnum {
                    index:        0,
                    pixel_format: fmtd.pixelformat,
                    width:        w,
                    height:       h,
                    frmival_type: 0,
                    ivals:        V4L2FrmIvalUnion {
                        discrete: V4L2Fract { numerator: 1, denominator: 30 },
                    },
                    reserved:     [0; 2],
                };

                let mut fps_min = f64::MAX;
                let mut fps_max = 0.0_f64;
                let mut got_fps = false;

                loop {
                    let ret =
                        unsafe { libc::ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &mut fival as *mut _) };
                    if ret < 0 {
                        break;
                    }
                    let fps = if fival.frmival_type == V4L2_FRMIVAL_TYPE_DISCRETE {
                        let d = unsafe { fival.ivals.discrete };
                        fract_to_fps(d.numerator, d.denominator)
                    } else {
                        let s = unsafe { fival.ivals.stepwise };
                        fps_min = fract_to_fps(s.max.numerator, s.max.denominator);
                        fps_max = fract_to_fps(s.min.numerator, s.min.denominator);
                        f64::MAX // marker – we already set range
                    };

                    if fps < f64::MAX {
                        fps_min = fps_min.min(fps);
                        fps_max = fps_max.max(fps);
                        got_fps = true;
                    }

                    fival.index += 1;
                    if fival.frmival_type != V4L2_FRMIVAL_TYPE_DISCRETE {
                        got_fps = true;
                        break;
                    }
                }

                if !got_fps {
                    fps_min = 1.0;
                    fps_max = 30.0;
                }
                if fps_min == f64::MAX {
                    fps_min = 1.0;
                }

                dev.formats.push(CaptureFormat {
                    pixel_format: pix_fmt.clone(),
                    width: w,
                    height: h,
                    fps_min,
                    fps_max,
                });

                fsize.index += 1;
                if fsize.frmsize_type != V4L2_FRMSIZE_TYPE_DISCRETE {
                    break;
                }
            }

            if !got_sizes {
                dev.formats.push(CaptureFormat {
                    pixel_format: pix_fmt,
                    width: 0,
                    height: 0,
                    fps_min: 1.0,
                    fps_max: 60.0,
                });
            }

            fmtd.index += 1;
        }

        dev
    }
}

/// Enumerate all available video capture devices.
///
/// On Linux this scans `/dev/video0` … `/dev/video63` via V4L2.
/// Returns a [`CaptureDevice`] per detected capture source.
pub fn discover_capture() -> Vec<CaptureDevice> {
    let mut results = Vec::new();

    #[cfg(target_os = "linux")]
    {
        for i in 0..64u32 {
            let path = format!("/dev/video{}", i);
            if !PathBuf::from(&path).exists() {
                continue;
            }
            let dev = v4l2::query_v4l2_device(&path);
            if !dev.device_name.is_empty() && dev.is_capture {
                results.push(dev);
            }
        }
    }

    results
}

/// Query detailed capabilities for a single capture device.
///
/// `device` can be a device path (`"/dev/video0"`) or a plain integer index
/// (`"0"` or passed as a numeric string).  The V4L2 backend is used on Linux.
/// Returns a [`CaptureDevice`] with `formats` fully populated.
pub fn capture_capabilities(device: &str) -> CaptureDevice {
    #[cfg(target_os = "linux")]
    {
        let path = if device.chars().all(|c| c.is_ascii_digit()) {
            format!("/dev/video{}", device)
        } else {
            device.to_owned()
        };
        return v4l2::query_v4l2_device(&path);
    }

    #[allow(unreachable_code)]
    CaptureDevice {
        path:       device.to_owned(),
        backend:    "opencv".to_owned(),
        is_capture: true,
        ..Default::default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_shm_name_roundtrip() {
        // Standard session ID with hex parts
        let bare = "visionpipe_sink_1a2b_3c4d_my_output";
        let result = parse_shm_name(bare);
        assert!(result.is_some());
        let (sid, name) = result.unwrap();
        assert_eq!(sid, "1a2b_3c4d");
        assert_eq!(name, "my_output");
    }

    #[test]
    fn parse_shm_name_rejects_wrong_prefix() {
        assert!(parse_shm_name("visionpipe_frame_abc_def_name").is_none());
        assert!(parse_shm_name("other_sink_abc_def_name").is_none());
    }

    #[test]
    fn discover_sinks_does_not_panic() {
        // May return empty on CI – just check it doesn't crash
        let _ = discover_sinks();
    }

    #[test]
    fn discover_capture_does_not_panic() {
        let _ = discover_capture();
    }
}

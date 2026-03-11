//! Frame types and format decoding from VisionPipe iceoryx2 payloads.
//!
//! VisionPipe publishes frames using the [`IceoryxFrameMeta`] header followed
//! immediately by raw pixel data.  The `mat_type` field encodes both the
//! per-element depth and the channel count using the OpenCV type encoding:
//!
//! ```text
//! mat_type = depth_code | ((channels - 1) << 3)
//! ```
//!
//! where `depth_code` is one of:
//!
//! | Code | Name    | Element size |
//! |------|---------|--------------|
//! | 0    | CV_8U   | 1 byte       |
//! | 1    | CV_8S   | 1 byte       |
//! | 2    | CV_16U  | 2 bytes      |
//! | 3    | CV_16S  | 2 bytes      |
//! | 4    | CV_32S  | 4 bytes      |
//! | 5    | CV_32F  | 4 bytes      |
//! | 6    | CV_64F  | 8 bytes      |

use crate::error::{Result, VisionPipeError};

// ============================================================================
// IceoryxFrameMeta — must match the C++ struct exactly (repr C)
// ============================================================================

/// Binary header prepended to every iceoryx2 sample published by VisionPipe.
///
/// Layout (total 40 bytes, alignment 8):
/// ```text
/// offset  0 | seq_num  : u64  — monotonic frame counter (UINT64_MAX = shutdown)
/// offset  8 | width    : i32  — frame width in pixels
/// offset 12 | height   : i32  — frame height in pixels
/// offset 16 | mat_type : i32  — OpenCV mat type encoding (see module docs)
/// offset 20 | channels : i32  — number of colour channels
/// offset 24 | data_size: u64  — pixel data byte length following this header
/// offset 32 | _pad     : [u8;8] — reserved
/// ```
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct IceoryxFrameMeta {
    pub seq_num: u64,
    pub width: i32,
    pub height: i32,
    /// OpenCV mat-type encoding: `depth_code | ((channels - 1) << 3)`
    pub mat_type: i32,
    pub channels: i32,
    pub data_size: u64,
    pub _pad: [u8; 8],
}

/// Size of the [`IceoryxFrameMeta`] header in bytes.
pub const FRAME_META_SIZE: usize = std::mem::size_of::<IceoryxFrameMeta>();

/// Shutdown sentinel value for [`IceoryxFrameMeta::seq_num`].
pub const SHUTDOWN_SEQ: u64 = u64::MAX;

// ============================================================================
// Depth codes (OpenCV CV_* constants)
// ============================================================================

pub const CV_DEPTH_MASK: i32 = 0x07;
pub const CV_CN_SHIFT: i32 = 3;

pub const CV_8U: i32 = 0;
pub const CV_8S: i32 = 1;
pub const CV_16U: i32 = 2;
pub const CV_16S: i32 = 3;
pub const CV_32S: i32 = 4;
pub const CV_32F: i32 = 5;
pub const CV_64F: i32 = 6;

/// Return the depth code embedded in a cv::Mat `type` value.
#[inline]
pub fn cv_type_depth(mat_type: i32) -> i32 {
    mat_type & CV_DEPTH_MASK
}

/// Return the number of channels embedded in a cv::Mat `type` value.
#[inline]
pub fn cv_type_channels(mat_type: i32) -> i32 {
    (mat_type >> CV_CN_SHIFT) + 1
}

/// Return the byte size of a single element for the given depth code.
#[inline]
pub fn cv_depth_elem_size(depth: i32) -> usize {
    match depth {
        CV_8U | CV_8S => 1,
        CV_16U | CV_16S => 2,
        CV_32S | CV_32F => 4,
        CV_64F => 8,
        _ => 0,
    }
}

// ============================================================================
// FrameFormat — named pixel layout
// ============================================================================

/// Pixel layout / colour format for a [`Frame`].
///
/// The format is decoded from the OpenCV mat-type field embedded in every
/// VisionPipe iceoryx2 sample.  Unknown type codes are represented as
/// [`FrameFormat::Raw`], preserving the raw `mat_type` value so callers can
/// still access the pixel bytes.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum FrameFormat {
    /// 8-bit grayscale  (`CV_8UC1 = 0`)
    Gray8,
    /// 8-bit signed grayscale  (`CV_8SC1 = 8`)
    Gray8S,
    /// 16-bit unsigned grayscale (`CV_16UC1 = 2`)
    Gray16,
    /// 16-bit signed grayscale (`CV_16SC1 = 10`)
    Gray16S,
    /// 32-bit signed integer grayscale (`CV_32SC1 = 4`)
    Gray32S,
    /// 32-bit float grayscale (`CV_32FC1 = 5`)
    Gray32F,
    /// 64-bit float grayscale (`CV_64FC1 = 6`)
    Gray64F,
    /// 3-channel 8-bit BGR (OpenCV default colour order) (`CV_8UC3 = 16`)
    Bgr8,
    /// 4-channel 8-bit BGRA (`CV_8UC4 = 24`)
    Bgra8,
    /// 3-channel 8-bit RGB (`CV_8UC3`, RGB byte order — same cv type as BGR)
    ///
    /// VisionPipe does not re-order channels; this variant is not auto-detected
    /// from the mat_type.  Scripts that publish RGB natively label frames so.
    /// Use [`FrameFormat::Bgr8`] unless you know the pipeline produces RGB.
    Rgb8,
    /// 4-channel 8-bit RGBA (`CV_8UC4`)
    Rgba8,
    /// 3-channel 16-bit unsigned colour (`CV_16UC3 = 18`)
    Bgr16,
    /// 4-channel 16-bit unsigned colour (`CV_16UC4 = 26`)
    Bgra16,
    /// 3-channel 32-bit float colour (`CV_32FC3 = 21`)
    Bgr32F,
    /// 4-channel 32-bit float colour (`CV_32FC4 = 29`)
    Bgra32F,
    /// 2-channel 32-bit float (e.g., optical flow XY) (`CV_32FC2 = 13`)
    TwoChannel32F,
    /// 2-channel 16-bit unsigned (e.g., disparity + confidence)
    TwoChannel16U,
    /// Unrecognised format — raw `mat_type` value is preserved.
    Raw(i32),
}

impl FrameFormat {
    /// Decode from a cv::Mat type integer.
    pub fn from_mat_type(mat_type: i32) -> Self {
        let depth = cv_type_depth(mat_type);
        let ch = cv_type_channels(mat_type);
        match (depth, ch) {
            (CV_8U, 1) => FrameFormat::Gray8,
            (CV_8S, 1) => FrameFormat::Gray8S,
            (CV_16U, 1) => FrameFormat::Gray16,
            (CV_16S, 1) => FrameFormat::Gray16S,
            (CV_32S, 1) => FrameFormat::Gray32S,
            (CV_32F, 1) => FrameFormat::Gray32F,
            (CV_64F, 1) => FrameFormat::Gray64F,
            (CV_8U, 3) => FrameFormat::Bgr8,
            (CV_8U, 4) => FrameFormat::Bgra8,
            (CV_16U, 3) => FrameFormat::Bgr16,
            (CV_16U, 4) => FrameFormat::Bgra16,
            (CV_32F, 2) => FrameFormat::TwoChannel32F,
            (CV_32F, 3) => FrameFormat::Bgr32F,
            (CV_32F, 4) => FrameFormat::Bgra32F,
            (CV_16U, 2) => FrameFormat::TwoChannel16U,
            _ => FrameFormat::Raw(mat_type),
        }
    }

    /// Return the OpenCV mat_type integer for this format.
    pub fn to_mat_type(self) -> i32 {
        match self {
            FrameFormat::Gray8 => CV_8U,
            FrameFormat::Gray8S => CV_8S,
            FrameFormat::Gray16 => CV_16U,
            FrameFormat::Gray16S => CV_16S,
            FrameFormat::Gray32S => CV_32S,
            FrameFormat::Gray32F => CV_32F,
            FrameFormat::Gray64F => CV_64F,
            FrameFormat::Bgr8 | FrameFormat::Rgb8 => CV_8U | (2 << CV_CN_SHIFT),
            FrameFormat::Bgra8 | FrameFormat::Rgba8 => CV_8U | (3 << CV_CN_SHIFT),
            FrameFormat::Bgr16 => CV_16U | (2 << CV_CN_SHIFT),
            FrameFormat::Bgra16 => CV_16U | (3 << CV_CN_SHIFT),
            FrameFormat::TwoChannel16U => CV_16U | (1 << CV_CN_SHIFT),
            FrameFormat::TwoChannel32F => CV_32F | (1 << CV_CN_SHIFT),
            FrameFormat::Bgr32F => CV_32F | (2 << CV_CN_SHIFT),
            FrameFormat::Bgra32F => CV_32F | (3 << CV_CN_SHIFT),
            FrameFormat::Raw(t) => t,
        }
    }

    /// Number of channels.
    pub fn channels(self) -> usize {
        cv_type_channels(self.to_mat_type()) as usize
    }

    /// Byte size of a single *pixel element* (one channel value).
    pub fn elem_size(self) -> usize {
        cv_depth_elem_size(cv_type_depth(self.to_mat_type()))
    }

    /// Byte size of one full pixel (all channels).
    pub fn pixel_size(self) -> usize {
        self.channels() * self.elem_size()
    }

    /// Expected byte length for a frame of the given dimensions.
    pub fn expected_data_len(self, width: u32, height: u32) -> usize {
        self.pixel_size() * (width as usize) * (height as usize)
    }
}

impl std::fmt::Display for FrameFormat {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            FrameFormat::Gray8 => write!(f, "Gray8"),
            FrameFormat::Gray8S => write!(f, "Gray8S"),
            FrameFormat::Gray16 => write!(f, "Gray16"),
            FrameFormat::Gray16S => write!(f, "Gray16S"),
            FrameFormat::Gray32S => write!(f, "Gray32S"),
            FrameFormat::Gray32F => write!(f, "Gray32F"),
            FrameFormat::Gray64F => write!(f, "Gray64F"),
            FrameFormat::Bgr8 => write!(f, "BGR8"),
            FrameFormat::Rgb8 => write!(f, "RGB8"),
            FrameFormat::Bgra8 => write!(f, "BGRA8"),
            FrameFormat::Rgba8 => write!(f, "RGBA8"),
            FrameFormat::Bgr16 => write!(f, "BGR16"),
            FrameFormat::Bgra16 => write!(f, "BGRA16"),
            FrameFormat::Bgr32F => write!(f, "BGR32F"),
            FrameFormat::Bgra32F => write!(f, "BGRA32F"),
            FrameFormat::TwoChannel32F => write!(f, "2CH-F32"),
            FrameFormat::TwoChannel16U => write!(f, "2CH-U16"),
            FrameFormat::Raw(t) => write!(f, "Raw({})", t),
        }
    }
}

// ============================================================================
// FrameData — typed pixel storage
// ============================================================================

/// Typed pixel data buffer.
///
/// Frames with 8-bit or 8-bit-signed depths use [`FrameData::U8`].
/// 16-bit depths use [`FrameData::U16`].  32-bit float uses [`FrameData::F32`].
/// 64-bit float uses [`FrameData::F64`].  32-bit signed integers use
/// [`FrameData::I32`].  All other / unknown depths fall back to [`FrameData::Raw`].
///
/// Slice ordering is row-major (width * channels values per row).
#[derive(Debug, Clone, PartialEq)]
pub enum FrameData {
    /// 8-bit unsigned (CV_8U) and 8-bit signed (CV_8S) depths — stored as bytes.
    U8(Vec<u8>),
    /// 16-bit unsigned or signed depths — stored as native-endian u16 words.
    U16(Vec<u16>),
    /// 32-bit signed integer depth (CV_32S).
    I32(Vec<i32>),
    /// 32-bit float depth (CV_32F).
    F32(Vec<f32>),
    /// 64-bit float depth (CV_64F).
    F64(Vec<f64>),
    /// Raw bytes for unsupported or unknown pixel depths.
    Raw(Vec<u8>),
}

impl FrameData {
    /// Total number of elements in this buffer (pixels × channels).
    pub fn len(&self) -> usize {
        match self {
            FrameData::U8(v) => v.len(),
            FrameData::U16(v) => v.len(),
            FrameData::I32(v) => v.len(),
            FrameData::F32(v) => v.len(),
            FrameData::F64(v) => v.len(),
            FrameData::Raw(v) => v.len(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// View the underlying bytes regardless of type.
    pub fn as_bytes(&self) -> &[u8] {
        match self {
            FrameData::U8(v) => v.as_slice(),
            FrameData::U16(v) => bytemuck_u16_as_u8(v),
            FrameData::I32(v) => bytemuck_i32_as_u8(v),
            FrameData::F32(v) => bytemuck_f32_as_u8(v),
            FrameData::F64(v) => bytemuck_f64_as_u8(v),
            FrameData::Raw(v) => v.as_slice(),
        }
    }

    /// Return a reference to the inner `Vec<u8>`, or `None` if stored differently.
    pub fn as_u8_slice(&self) -> Option<&[u8]> {
        match self {
            FrameData::U8(v) | FrameData::Raw(v) => Some(v),
            _ => None,
        }
    }

    /// Return a reference to the inner `Vec<u16>`, or `None`.
    pub fn as_u16_slice(&self) -> Option<&[u16]> {
        match self {
            FrameData::U16(v) => Some(v),
            _ => None,
        }
    }

    /// Return a reference to the inner `Vec<f32>`, or `None`.
    pub fn as_f32_slice(&self) -> Option<&[f32]> {
        match self {
            FrameData::F32(v) => Some(v),
            _ => None,
        }
    }

    /// Return a reference to the inner `Vec<f64>`, or `None`.
    pub fn as_f64_slice(&self) -> Option<&[f64]> {
        match self {
            FrameData::F64(v) => Some(v),
            _ => None,
        }
    }
}

// Safe byte-slice views (alignment is guaranteed by Vec allocation).
fn bytemuck_u16_as_u8(v: &[u16]) -> &[u8] {
    // SAFETY: u16 has no padding, alignment ≥ u8, total byte count is v.len()*2.
    unsafe { std::slice::from_raw_parts(v.as_ptr() as *const u8, v.len() * 2) }
}
fn bytemuck_i32_as_u8(v: &[i32]) -> &[u8] {
    unsafe { std::slice::from_raw_parts(v.as_ptr() as *const u8, v.len() * 4) }
}
fn bytemuck_f32_as_u8(v: &[f32]) -> &[u8] {
    unsafe { std::slice::from_raw_parts(v.as_ptr() as *const u8, v.len() * 4) }
}
fn bytemuck_f64_as_u8(v: &[f64]) -> &[u8] {
    unsafe { std::slice::from_raw_parts(v.as_ptr() as *const u8, v.len() * 8) }
}

// ============================================================================
// Frame — the main frame type
// ============================================================================

/// A decoded video frame produced by a VisionPipe `frame_sink()`.
///
/// Frames are received from iceoryx2 zero-copy shared memory.  The pixel data
/// is immediately *copied* into owned [`FrameData`] storage so the iceoryx2
/// sample can be released promptly.
///
/// # Pixel layout
///
/// Data is row-major, packed (no stride padding).  For multi-channel formats
/// the channel order follows OpenCV conventions: BGR for colour (not RGB).
///
/// ```text
/// pixel[row][col][ch] → data[row * width * channels + col * channels + ch]
/// ```
#[derive(Debug, Clone)]
pub struct Frame {
    /// Width in pixels.
    pub width: u32,
    /// Height in pixels.
    pub height: u32,
    /// Pixel format / colour layout.
    pub format: FrameFormat,
    /// Monotonically increasing frame sequence number from the publisher.
    pub seq: u64,
    /// Typed pixel data buffer.
    pub data: FrameData,
}

impl Frame {
    /// Construct a [`Frame`] by decoding raw bytes from an iceoryx2 sample.
    ///
    /// `payload` must be the full sample slice: `[IceoryxFrameMeta | pixel_data]`.
    pub fn from_iox2_payload(payload: &[u8]) -> Result<Self> {
        if payload.len() < FRAME_META_SIZE {
            return Err(VisionPipeError::PayloadTooShort {
                got: payload.len(),
                need: FRAME_META_SIZE,
            });
        }

        // SAFETY: we checked that payload is at least FRAME_META_SIZE bytes and
        // IceoryxFrameMeta is #[repr(C)] with no padding.
        let meta: IceoryxFrameMeta = unsafe {
            std::ptr::read_unaligned(payload.as_ptr() as *const IceoryxFrameMeta)
        };

        let data_size = meta.data_size as usize;
        let available = payload.len() - FRAME_META_SIZE;

        if data_size > available {
            return Err(VisionPipeError::PayloadSizeMismatch {
                header: data_size,
                actual: available,
            });
        }

        let pixel_bytes = &payload[FRAME_META_SIZE..FRAME_META_SIZE + data_size];
        let format = FrameFormat::from_mat_type(meta.mat_type);
        let data = decode_pixel_data(pixel_bytes, meta.mat_type)?;

        Ok(Frame {
            width: meta.width.max(0) as u32,
            height: meta.height.max(0) as u32,
            format,
            seq: meta.seq_num,
            data,
        })
    }

    /// Borrow the pixel data as a flat byte slice regardless of element type.
    pub fn as_bytes(&self) -> &[u8] {
        self.data.as_bytes()
    }

    /// Number of colour channels.
    pub fn channels(&self) -> usize {
        self.format.channels()
    }

    /// Expected byte length for this frame's dimensions and format.
    pub fn expected_len(&self) -> usize {
        self.format.expected_data_len(self.width, self.height)
    }

    /// Return pixel at (row, col) as a slice of element values (length = channels).
    ///
    /// Only available for 8-bit frames (returns `None` for other depths).
    pub fn pixel_u8(&self, row: u32, col: u32) -> Option<&[u8]> {
        let ch = self.channels();
        let idx = (row as usize) * (self.width as usize) * ch + (col as usize) * ch;
        match &self.data {
            FrameData::U8(v) | FrameData::Raw(v) => v.get(idx..idx + ch),
            _ => None,
        }
    }
}

/// Decode raw pixel bytes into the appropriate [`FrameData`] variant.
fn decode_pixel_data(bytes: &[u8], mat_type: i32) -> Result<FrameData> {
    let depth = cv_type_depth(mat_type);
    match depth {
        CV_8U | CV_8S => Ok(FrameData::U8(bytes.to_vec())),
        CV_16U | CV_16S => {
            if bytes.len() % 2 != 0 {
                return Err(VisionPipeError::PayloadSizeMismatch {
                    header: bytes.len(),
                    actual: bytes.len() & !1,
                });
            }
            let mut out = vec![0u16; bytes.len() / 2];
            // SAFETY: bytes is aligned to byte boundary; we copy into a fresh Vec.
            unsafe {
                std::ptr::copy_nonoverlapping(
                    bytes.as_ptr(),
                    out.as_mut_ptr() as *mut u8,
                    bytes.len(),
                );
            }
            Ok(FrameData::U16(out))
        }
        CV_32S => {
            if bytes.len() % 4 != 0 {
                return Err(VisionPipeError::PayloadSizeMismatch {
                    header: bytes.len(),
                    actual: bytes.len() & !3,
                });
            }
            let mut out = vec![0i32; bytes.len() / 4];
            unsafe {
                std::ptr::copy_nonoverlapping(
                    bytes.as_ptr(),
                    out.as_mut_ptr() as *mut u8,
                    bytes.len(),
                );
            }
            Ok(FrameData::I32(out))
        }
        CV_32F => {
            if bytes.len() % 4 != 0 {
                return Err(VisionPipeError::PayloadSizeMismatch {
                    header: bytes.len(),
                    actual: bytes.len() & !3,
                });
            }
            let mut out = vec![0f32; bytes.len() / 4];
            unsafe {
                std::ptr::copy_nonoverlapping(
                    bytes.as_ptr(),
                    out.as_mut_ptr() as *mut u8,
                    bytes.len(),
                );
            }
            Ok(FrameData::F32(out))
        }
        CV_64F => {
            if bytes.len() % 8 != 0 {
                return Err(VisionPipeError::PayloadSizeMismatch {
                    header: bytes.len(),
                    actual: bytes.len() & !7,
                });
            }
            let mut out = vec![0f64; bytes.len() / 8];
            unsafe {
                std::ptr::copy_nonoverlapping(
                    bytes.as_ptr(),
                    out.as_mut_ptr() as *mut u8,
                    bytes.len(),
                );
            }
            Ok(FrameData::F64(out))
        }
        _ => {
            // Unknown depth — preserve raw bytes so callers can still use them.
            Ok(FrameData::Raw(bytes.to_vec()))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn frame_format_round_trip() {
        for (fmt, expected_mat) in [
            (FrameFormat::Gray8, 0),
            (FrameFormat::Gray16, 2),
            (FrameFormat::Gray32F, 5),
            (FrameFormat::Bgr8, 16),
            (FrameFormat::Bgra8, 24),
            (FrameFormat::Bgr32F, 21),
        ] {
            assert_eq!(fmt.to_mat_type(), expected_mat, "{fmt:?}");
            assert_eq!(FrameFormat::from_mat_type(expected_mat), fmt, "{fmt:?}");
        }
    }

    #[test]
    fn frame_format_channels() {
        assert_eq!(FrameFormat::Gray8.channels(), 1);
        assert_eq!(FrameFormat::Bgr8.channels(), 3);
        assert_eq!(FrameFormat::Bgra8.channels(), 4);
    }

    #[test]
    fn frame_from_payload_too_short() {
        let result = Frame::from_iox2_payload(&[0u8; 10]);
        assert!(matches!(result, Err(VisionPipeError::PayloadTooShort { .. })));
    }
}

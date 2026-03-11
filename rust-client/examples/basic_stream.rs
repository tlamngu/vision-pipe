//! Basic example: launch a VisionPipe pipeline and receive frames from it.
//!
//! Usage:
//! ```bash
//! cargo run --example basic_stream -- my_pipeline.vsp output
//! ```
//!
//! The `.vsp` script should include at least one `frame_sink("output")` item.
//! Example script:
//! ```text
//! video_cap(0)
//! resize(640, 480)
//! frame_sink("output")
//! ```

use std::env;
use std::sync::{
    atomic::{AtomicU64, Ordering},
    Arc,
};
use std::time::{Duration, Instant};

use visionpipe_client::{Frame, VisionPipe};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    let script = args.get(1).map(String::as_str).unwrap_or("get-started.vsp");
    let sink = args.get(2).map(String::as_str).unwrap_or("output");

    println!("Launching: {} → sink '{sink}'", script);

    let mut session = VisionPipe::new()
        .script(script)
        .no_display(true)
        .verbose(false)
        .run()?;

    println!(
        "Session: {} (pid {})",
        session.session_id(),
        session.pid()
    );

    // ── Frame counter (shared with callback) ────────────────────────────────
    let frame_count = Arc::new(AtomicU64::new(0));
    let frame_count_cb = Arc::clone(&frame_count);
    let start = Instant::now();

    // ── Register frame callback ─────────────────────────────────────────────
    session.on_frame(sink, move |frame: &Frame| {
        let n = frame_count_cb.fetch_add(1, Ordering::Relaxed) + 1;

        // Print header on first frame.
        if n == 1 {
            println!(
                "First frame: {}×{} {} (seq {})",
                frame.width, frame.height, frame.format, frame.seq
            );
        }

        // Demonstrate typed data access.
        match &frame.data {
            visionpipe_client::FrameData::U8(pixels) => {
                if n == 1 {
                    // Safe: pixels is a plain Vec<u8>.
                    if let Some(px) = frame.pixel_u8(0, 0) {
                        println!(
                            "  pixel[0,0] = {:?} ({}ch)",
                            px,
                            frame.channels()
                        );
                    }
                    println!("  data buffer: {} bytes", pixels.len());
                }
            }
            visionpipe_client::FrameData::F32(floats) => {
                if n == 1 {
                    println!(
                        "  float frame: {} values, first={:.4}",
                        floats.len(),
                        floats.first().copied().unwrap_or(0.0)
                    );
                }
            }
            visionpipe_client::FrameData::U16(words) => {
                if n == 1 {
                    println!(
                        "  16-bit frame: {} values, first={}",
                        words.len(),
                        words.first().copied().unwrap_or(0)
                    );
                }
            }
            other => {
                if n == 1 {
                    println!("  frame data: {:?} ({} elements)", frame.format, other.len());
                }
            }
        }

        // Print FPS every 100 frames.
        if n % 100 == 0 {
            let elapsed = start.elapsed().as_secs_f64();
            println!(
                "  [{n} frames in {elapsed:.1}s] fps={:.1}",
                n as f64 / elapsed
            );
        }
    })?;

    // ── Spin until the process exits or Ctrl-C ──────────────────────────────
    println!("Receiving frames — press Ctrl-C to stop…");
    session.spin(Duration::from_millis(1))?;

    println!(
        "\nDone. Received {} frames in {:.2}s",
        frame_count.load(Ordering::Relaxed),
        start.elapsed().as_secs_f64()
    );

    Ok(())
}

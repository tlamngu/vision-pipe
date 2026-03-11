//! Multi-sink example: receive from multiple `frame_sink()` outputs simultaneously.
//!
//! Usage:
//! ```bash
//! cargo run --example multi_sink -- stereo_test.vsp left right
//! ```
//!
//! Expected `.vsp` script (e.g., stereo_test.vsp):
//! ```text
//! pipeline stereo
//!   video_cap(0) → left_cam
//!   video_cap(1) → right_cam
//!   frame_sink("left")
//!   frame_sink("right")
//! end
//! exec_loop stereo
//! ```

use std::collections::HashMap;
use std::env;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use visionpipe_client::{Frame, VisionPipe};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    let script = args.get(1).map(String::as_str).unwrap_or("stereo_test.vsp");
    let sinks: Vec<&str> = if args.len() > 2 {
        args[2..].iter().map(String::as_str).collect()
    } else {
        vec!["left", "right"]
    };

    println!("Launching: {script}");
    println!("Sinks: {sinks:?}");

    let mut session = VisionPipe::new()
        .script(script)
        .no_display(true)
        .run()?;

    println!("Session {} (pid {})", session.session_id(), session.pid());

    // Shared per-sink frame counters.
    let counters: Arc<Mutex<HashMap<String, u64>>> = Arc::new(Mutex::new(HashMap::new()));

    for sink_name in &sinks {
        let sink_name = sink_name.to_string();
        let sink_name_cb = sink_name.clone();
        let counters_clone = Arc::clone(&counters);

        session.on_frame(&sink_name, move |frame: &Frame| {
            let mut lock = counters_clone.lock().expect("counters poisoned");
            let count = lock.entry(sink_name_cb.clone()).or_insert(0);
            *count += 1;

            if *count == 1 {
                println!(
                    "[{sink_name_cb}] first frame: {}×{} {} seq={}",
                    frame.width, frame.height, frame.format, frame.seq
                );
            }
        })?;
    }

    let start = Instant::now();

    println!("\nReceiving from {} sinks — running for 10 s…", sinks.len());

    loop {
        session.spin_once()?;

        if !session.is_running() {
            println!("VisionPipe process exited.");
            break;
        }

        if start.elapsed() > Duration::from_secs(10) {
            println!("10 s elapsed — stopping.");
            break;
        }

        std::thread::sleep(Duration::from_millis(1));
    }

    session.stop(Duration::from_secs(3))?;

    // ── Summary ──────────────────────────────────────────────────────────────
    let elapsed = start.elapsed().as_secs_f64();
    let lock = counters.lock().expect("counters poisoned");
    println!("\nSummary ({elapsed:.1}s):");
    for sink_name in &sinks {
        let n = lock.get(*sink_name).copied().unwrap_or(0);
        println!(
            "  {sink_name}: {n} frames  ({:.1} fps)",
            n as f64 / elapsed.max(0.001)
        );
    }

    Ok(())
}

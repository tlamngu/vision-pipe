//! Params control example: launch a pipeline and dynamically adjust parameters.
//!
//! Usage:
//! ```bash
//! cargo run --example params_control -- runtime_params_demo.vsp
//! ```
//!
//! Expected `.vsp` script (runtime_params_demo.vsp):
//! ```text
//! params [
//!   brightness: int = 50,
//!   gain: float = 1.0,
//!   mode: string = "auto"
//! ]
//!
//! pipeline process
//!   video_cap(0)
//!   adjust_brightness(@brightness)
//!   frame_sink("output")
//! end
//!
//! exec_loop process
//! ```

use std::env;
use std::time::Duration;

use visionpipe_client::{ParamValue, VisionPipe};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    let script = args
        .get(1)
        .map(String::as_str)
        .unwrap_or("runtime_params_demo.vsp");

    println!("Launching: {script}");

    let mut session = VisionPipe::new()
        .script(script)
        .no_display(true)
        .run()?;

    println!("Session {} (pid {})", session.session_id(), session.pid());

    // Give the script a moment to start its param server.
    std::thread::sleep(Duration::from_millis(500));

    // ── List all params ─────────────────────────────────────────────────────
    match session.list_params() {
        Ok(params) => {
            println!("\nRuntime parameters:");
            for (name, value) in &params {
                println!("  {name} = {value}");
            }
        }
        Err(e) => eprintln!("list_params failed: {e}"),
    }

    // ── GET a single param ──────────────────────────────────────────────────
    match session.get_param("brightness") {
        Ok(Some(v)) => println!("\nbrightness = {v}"),
        Ok(None) => println!("\nbrightness: not found"),
        Err(e) => eprintln!("get brightness: {e}"),
    }

    // ── WATCH for changes ───────────────────────────────────────────────────
    let _watch = session.watch_param("*", |evt| {
        println!(
            "[watch] {} changed: {} → {}",
            evt.name, evt.old_value, evt.new_value
        );
    })?;

    // ── Receive frames while adjusting params ────────────────────────────────
    let mut frame_n = 0u64;
    session.on_frame("output", move |frame| {
        frame_n += 1;
        if frame_n <= 3 {
            println!(
                "frame {} — {}×{} {}",
                frame.seq, frame.width, frame.height, frame.format
            );
        }
    })?;

    let start = std::time::Instant::now();

    // Spin and periodically tweak params.
    loop {
        session.spin_once()?;

        let elapsed = start.elapsed();

        if elapsed > Duration::from_secs(2) && elapsed < Duration::from_secs(2) + Duration::from_millis(50) {
            println!("\nSetting brightness=75…");
            session.set_param("brightness", &ParamValue::Int(75))?;
        }

        if elapsed > Duration::from_secs(4) && elapsed < Duration::from_secs(4) + Duration::from_millis(50) {
            println!("Setting gain=1.5…");
            session.set_param("gain", &ParamValue::Float(1.5))?;
        }

        if elapsed > Duration::from_secs(6) && elapsed < Duration::from_secs(6) + Duration::from_millis(50) {
            println!("Setting mode=manual…");
            session.set_param_str("mode", "manual")?;
        }

        if elapsed > Duration::from_secs(8) {
            break;
        }

        std::thread::sleep(Duration::from_millis(1));
    }

    // ── Final param listing ─────────────────────────────────────────────────
    println!("\nFinal parameter state:");
    for (name, value) in session.list_params()? {
        println!("  {name} = {value}");
    }

    session.stop(Duration::from_secs(3))?;
    println!("Done.");
    Ok(())
}

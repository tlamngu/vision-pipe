#!/usr/bin/env python3
"""
Example: Using VisionPipe Python bindings

This demonstrates how to use libvisionpipe from Python to:
  1. Launch a .vsp pipeline as a sidecar process
  2. Receive frames via shared memory as numpy arrays
  3. Process frames with OpenCV / numpy

Requirements:
  - visionpipe executable in PATH (or set path explicitly)
  - pyvisionpipe module built (-DVISIONPIPE_WITH_PYTHON=ON)
  - numpy, opencv-python (optional, for display)

Usage:
  python3 example_pyvisionpipe.py [path_to_script.vsp]
"""

import sys
import signal
import time

# Import the C++ binding directly, or via the wrapper package
try:
    import visionpipe
except ImportError:
    import pyvisionpipe as visionpipe

running = True

def signal_handler(sig, frame):
    global running
    running = False

signal.signal(signal.SIGINT, signal_handler)


def example_grab_frame(script_path: str):
    """Polling mode: grab frames in a loop."""
    print("=== Polling mode example ===")

    vp = visionpipe.VisionPipe()
    vp.set_no_display(True)

    if not vp.is_available():
        print("Error: visionpipe executable not found in PATH")
        return 1

    print(f"VisionPipe version: {vp.version()}")

    session = vp.run(script_path)
    print(f"Session started: {session.session_id}")

    frame_count = 0
    start_time = time.time()

    while running and session.is_running():
        ok, frame = session.grab_frame("processed")
        if ok:
            frame_count += 1
            if frame_count % 100 == 0:
                elapsed = time.time() - start_time
                fps = frame_count / elapsed if elapsed > 0 else 0
                print(f"  Frames: {frame_count}  FPS: {fps:.1f}"
                      f"  Shape: {frame.shape}  Dtype: {frame.dtype}")
        else:
            time.sleep(0.001)

    session.stop()
    print(f"Total frames received: {frame_count}")
    return 0


def example_callback(script_path: str):
    """Callback mode: register frame handlers."""
    print("=== Callback mode example ===")

    vp = visionpipe.VisionPipe()
    vp.set_no_display(True)

    session = vp.run(script_path)

    frame_count = [0]

    def on_frame(frame, seq):
        frame_count[0] += 1
        if frame_count[0] % 100 == 0:
            print(f"  [callback] seq={seq}  shape={frame.shape}")

    session.on_frame("processed", on_frame)

    print("Receiving frames via callback... Press Ctrl+C to stop")
    # spin() blocks and dispatches callbacks (GIL is released internally)
    session.spin()

    session.stop()
    print(f"Total frames: {frame_count[0]}")
    return 0


def example_run_string():
    """Run a pipeline defined inline as a Python string."""
    print("=== runString() example ===")

    vp = visionpipe.VisionPipe()
    vp.set_no_display(True)

    script = """
        video_cap(0)
        resize(640, 480)
        frame_sink("output")
    """

    with vp.run_string(script, "inline_demo") as session:
        for _ in range(50):
            if not session.is_running():
                break
            ok, frame = session.grab_frame("output")
            if ok:
                print(f"  Got frame: {frame.shape}")
            time.sleep(0.033)  # ~30 fps

    print("Done")
    return 0


def example_context_manager(script_path: str):
    """Using Session as a context manager for automatic cleanup."""
    print("=== Context manager example ===")

    vp = visionpipe.VisionPipe()
    vp.set_no_display(True)

    with vp.run(script_path) as session:
        print(f"Session: {session}")
        for i in range(100):
            if not session.is_running():
                break
            ok, frame = session.grab_frame("processed")
            if ok:
                print(f"  Frame {i}: {frame.shape}")
            time.sleep(0.01)
    # session.stop() is called automatically by __exit__

    print("Session cleaned up automatically")
    return 0


if __name__ == "__main__":
    script = "examples/libvisionpipe_demo.vsp"
    if len(sys.argv) > 1:
        script = sys.argv[1]

    print(f"Script: {script}\n")
    sys.exit(example_grab_frame(script))

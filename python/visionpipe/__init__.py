"""
VisionPipe Python Bindings
==========================

Python interface for VisionPipe - launch vision processing pipelines
and receive frames as numpy arrays via zero-copy shared memory.

Quick start::

    import visionpipe

    vp = visionpipe.VisionPipe()
    session = vp.run("pipeline.vsp")

    while session.is_running():
        ok, frame = session.grab_frame("output")
        if ok:
            print(f"Frame: {frame.shape}")

    session.stop()

Or using a context manager::

    with vp.run("pipeline.vsp") as session:
        while session.is_running():
            ok, frame = session.grab_frame("output")
            if ok:
                process(frame)

Run a pipeline from a string::

    session = vp.run_string('''
        video_cap(0)
        resize(640, 480)
        frame_sink("output")
    ''')
"""

# Import everything from the C++ extension module
from pyvisionpipe import (
    VisionPipe,
    VisionPipeConfig,
    Session,
    SessionState,
)

__all__ = [
    "VisionPipe",
    "VisionPipeConfig",
    "Session",
    "SessionState",
]

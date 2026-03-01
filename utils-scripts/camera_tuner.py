#!/usr/bin/env python3
"""
camera_tuner.py – VisionPipe Camera Colour Calibration Server 
========================================================================
Architecture
------------
  Discovery (cameras / formats / controls):
      → v4l2-ctl
  Streaming + live sensor prop changes:
      → visionpipe run <_stream.vsp>
      → visionpipe execute 'v4l2_prop(…)'   (runs in a thread pool)

HTTP is served by ThreadingHTTPServer so every request gets its own
thread and expensive calls never block the camera-picker GET.

REST API
--------
  GET  /                       → camera_tuner.html
  GET  /stream                 → MJPEG proxy from visionpipe ip_stream(:8080)
  GET  /api/cameras            → list /dev/video* + sysfs names
  GET  /api/formats?device=    → v4l2-ctl --list-formats-ext
  GET  /api/controls?device=   → v4l2-ctl --list-ctrls  (main + subdev)
  POST /api/start              → generate .vsp + spawn visionpipe run
  POST /api/stop               → kill visionpipe stream process
  POST /api/prop               → v4l2-ctl --set-ctrl (fallback: visionpipe execute)
  POST /api/calibrate          → compute WB gains → apply via set-ctrl
  GET  /api/status             → {running, pid, log[20]}
  GET  /api/vsp                → last generated _stream.vsp text
  GET  /api/references         → reference colour chart values
  POST /api/vsp_preview        → generate vsp from body (dry-run, no start)
"""

import argparse
import concurrent.futures
import glob
import json
import logging
import os
import re
import shutil
import signal
import subprocess
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from socketserver import ThreadingMixIn
from typing import Optional
from urllib.parse import urlparse, parse_qs

import numpy as np

# ---------------------------------------------------------------------------
logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
log = logging.getLogger("camera_tuner")

SCRIPT_DIR = Path(__file__).parent
HTML_FILE  = SCRIPT_DIR / "camera_tuner.html"
VSP_FILE   = SCRIPT_DIR / "_stream.vsp"

REFERENCE_COLOURS = {
    "W":     [255, 255, 255],
    "Y":     [255, 255,   0],
    "C":     [  0, 255, 255],
    "G":     [  0, 255,   0],
    "M":     [255,   0, 255],
    "R":     [255,   0,   0],
    "BLUE":  [  0,   0, 255],
    "BLACK": [  0,   0,   0],
}

# ---------------------------------------------------------------------------
# Binary paths (resolved in main())
# ---------------------------------------------------------------------------
_vsp_bin  = "visionpipe"
_v4l2_ctl = "v4l2-ctl"


def _resolve_bin(hint: str, name: str) -> str:
    if hint and Path(hint).is_file():
        return hint
    for prefix in [SCRIPT_DIR.parent / "build", Path("/usr/local/bin"), Path("/usr/bin")]:
        p = prefix / name
        if p.is_file():
            return str(p)
    found = shutil.which(name)
    return found if found else name


# ---------------------------------------------------------------------------
# v4l2-ctl helpers  (discovery – fast, no visionpipe required)
# ---------------------------------------------------------------------------

def _run_v4l2_ctl(*args, timeout: int = 6) -> str:
    """Run v4l2-ctl and return combined stdout+stderr (never raises)."""
    try:
        r = subprocess.run(
            [_v4l2_ctl, *args],
            capture_output=True, text=True, timeout=timeout,
        )
        return (r.stdout or "") + (r.stderr or "")
    except Exception as e:
        log.warning("v4l2-ctl %s failed: %s", " ".join(str(a) for a in args), e)
        return ""


def list_video_devices() -> list:
    """
    Return [{path, name, subdev}] from v4l2-ctl --list-devices.
    Falls back to a /dev/video* glob if v4l2-ctl is absent.
    """
    output = _run_v4l2_ctl("--list-devices")
    devices = []
    if output.strip():
        current_name = ""
        for line in output.splitlines():
            stripped = line.strip()
            if not stripped:
                continue
            if not stripped.startswith("/dev/"):
                current_name = stripped.rstrip(":")
            elif "/dev/video" in stripped:
                devices.append({"path": stripped, "name": current_name, "subdev": ""})
        # Heuristic: attach subdev sibling
        for d in devices:
            d["subdev"] = _find_subdev_for_device(d["path"])

    if not devices:
        # Plain glob fallback
        for path in sorted(glob.glob("/dev/video*")):
            m = re.search(r"(\d+)$", path)
            name = path
            if m:
                sysfs = f"/sys/class/video4linux/video{m.group(1)}/name"
                try:
                    name = Path(sysfs).read_text().strip()
                except Exception:
                    pass
            devices.append({"path": path, "name": name, "subdev": _find_subdev_for_device(path)})

    return devices


def _find_subdev_for_device(device: str) -> str:
    """Best-effort subdev lookup via sysfs (works for simple direct-attach topologies)."""
    m = re.search(r"video(\d+)$", device)
    if not m:
        return ""
    idx = m.group(1)
    for sd in glob.glob(f"/sys/class/video4linux/video{idx}/device/../../*/v4l-subdev*"):
        return f"/dev/{Path(sd).name}"
    return ""


# Mbus format code → (v4l2_pixelformat, bayer_pattern)
_MBUS_TO_V4L2 = {
    # 8-bit Bayer
    0x3014: ("SRGGB8",  "RGGB"),
    0x300c: ("SGRBG8",  "GRBG"),
    0x300b: ("SGBRG8",  "GBRG"),
    0x300a: ("SBGGR8",  "BGGR"),
    # 10-bit Bayer
    0x300f: ("SRGGB10", "RGGB"),
    0x300d: ("SGRBG10", "GRBG"),
    0x300e: ("SGBRG10", "GBRG"),
    0x3009: ("SBGGR10", "BGGR"),
    # 12-bit Bayer
    0x3013: ("SRGGB12", "RGGB"),
    # YUV
    0x2008: ("YUYV",    ""),
    0x200f: ("UYVY",    ""),
}


def _mbus_name_to_v4l2(name: str) -> tuple:
    """Parse e.g. 'MEDIA_BUS_FMT_SRGGB8_1X8' → ('SRGGB8', 'RGGB')."""
    # Grab the middle token between FMT_ and the last _NxN part
    m = re.search(r'FMT_([A-Z0-9]+?)_\d+X\d+$', name)
    if not m:
        return "", ""
    tok = m.group(1)  # e.g. SRGGB8
    for v4l2, bayer in _MBUS_TO_V4L2.values():
        if v4l2 == tok:
            return v4l2, bayer
    return tok, ""


_sensor_subdevs_cache: Optional[list] = None
_sensor_subdevs_lock  = threading.Lock()
_sensor_subdevs_scanning = False  # True while a background scan is in progress

# Camera list cache – populated by the background discovery thread
_cameras_cache: Optional[list] = None
_cameras_lock   = threading.Lock()


def list_sensor_subdevs(force_refresh: bool = False) -> list:
    """
    Return all /dev/v4l-subdev* nodes that expose sensor controls.
    HTTP handlers ALWAYS return immediately from cache (possibly empty while
    the background discovery thread is still scanning).
    Only the background discovery thread should call this when cache is empty.
    """
    global _sensor_subdevs_cache, _sensor_subdevs_scanning
    with _sensor_subdevs_lock:
        if _sensor_subdevs_cache is not None and not force_refresh:
            return list(_sensor_subdevs_cache)
        if _sensor_subdevs_scanning:
            # Scan in progress on background thread – return stale / empty immediately
            return list(_sensor_subdevs_cache) if _sensor_subdevs_cache is not None else []
        # Mark as scanning so no other thread starts a duplicate scan
        _sensor_subdevs_scanning = True

    # ---- scan OUTSIDE the lock so HTTP requests are never blocked ----
    results = []
    for sd in sorted(glob.glob("/dev/v4l-subdev*")):
        out = _run_v4l2_ctl("-d", sd, "--list-ctrls", timeout=1)
        if re.search(r'analogue_gain|exposure|gain', out, re.I):
            node = Path(sd).name
            name_path = f"/sys/class/video4linux/{node}/name"
            name = ""
            try:
                name = Path(name_path).read_text().strip()
            except Exception:
                pass
            results.append({"path": sd, "name": name or node})

    with _sensor_subdevs_lock:
        _sensor_subdevs_cache = results
        _sensor_subdevs_scanning = False
    log.info("Subdev scan complete: %d sensor subdev(s) found", len(results))
    return list(results)


def list_formats_subdev(subdev: str) -> list:
    """
    Query a sensor subdev for discrete frame sizes via mbus codes.
    Returns format dicts compatible with list_formats() output.
    """
    mbus_out = _run_v4l2_ctl("-d", subdev, "--list-subdev-mbus-codes", "pad=0")
    results = []
    seen = set()
    for line in mbus_out.splitlines():
        m = re.match(r'\s*(0x[0-9a-fA-F]+):\s*(\S+)', line)
        if not m:
            continue
        code_val = int(m.group(1), 16)
        code_name = m.group(2)
        v4l2_fmt, bayer = _MBUS_TO_V4L2.get(code_val, _mbus_name_to_v4l2(code_name))
        if not v4l2_fmt:
            continue
        # Enumerate discrete frame sizes for this code
        sizes_out = _run_v4l2_ctl("-d", subdev, "--list-subdev-framesizes",
                                   f"pad=0,code={code_val}")
        for size_line in sizes_out.splitlines():
            # "Size Range: WxH - WxH" → pick the larger (max) pair
            ms = re.findall(r'(\d+)x(\d+)', size_line)
            if ms:
                w, h = int(ms[-1][0]), int(ms[-1][1])
                key = (v4l2_fmt, w, h)
                if key not in seen:
                    seen.add(key)
                    results.append({"format": v4l2_fmt, "width": w, "height": h,
                                     "fps": 30, "bayer": bayer})
    return results


def parse_formats_ext(output: str) -> list:
    """
    Parse v4l2-ctl --list-formats-ext output.
    Handles both Discrete and Continuous size types.
    For Continuous, records the max size so callers can offer presets.
    """
    results, seen = [], set()
    current_fmt = None
    for line in output.splitlines():
        m = re.search(r"\[\d+\]:\s+'([A-Z][A-Z0-9]+)'", line)
        if m:
            current_fmt = m.group(1)
            continue
        if not current_fmt:
            continue
        # Discrete:    Size: Discrete 1640x1232
        # Continuous:  Size: Continuous 1x1 - 8191x8191
        m = re.search(r"Size:\s+(\w+)\s+[\d]+x[\d]+(\s+-\s+(\d+)x(\d+))?", line)
        if m:
            size_type = m.group(1)  # Discrete or Continuous
            if size_type == "Discrete":
                ms = re.search(r'Size:\s+Discrete\s+(\d+)x(\d+)', line)
                if ms:
                    w, h = int(ms.group(1)), int(ms.group(2))
                    key = (current_fmt, w, h)
                    if key not in seen:
                        seen.add(key)
                        results.append({"format": current_fmt, "width": w,
                                         "height": h, "fps": 30,
                                         "continuous": False})
            elif size_type == "Continuous" and m.group(3):
                # Report max size only, mark as continuous
                w, h = int(m.group(3)), int(m.group(4))
                key = (current_fmt, w, h)
                if key not in seen:
                    seen.add(key)
                    results.append({"format": current_fmt, "width": w,
                                     "height": h, "fps": 30,
                                     "continuous": True})
            continue
        m = re.search(r"Interval:.*\((\d+(?:\.\d+)?)\s+fps\)", line)
        if m and results:
            results[-1]["fps"] = int(float(m.group(1)))
    return results


def list_formats(device: str, subdev: str = "") -> list:
    """List formats: prefer sensor subdev (discrete sizes), fall back to ISP node."""
    if subdev and Path(subdev).exists():
        fmts = list_formats_subdev(subdev)
        if fmts:
            return fmts
    output = _run_v4l2_ctl("-d", device, "--list-formats-ext")
    return parse_formats_ext(output)


def parse_ctrls(output: str) -> list:
    """
    Parse v4l2-ctl --list-ctrls output.
    Handles both:
      name 0xXXXX (int) : min=N max=N step=N default=N value=N
    """
    controls = []
    seen = set()
    for line in output.splitlines():
        m = re.match(
            r'\s{2,}([\w][\w ]*?)\s+0x[\da-fA-F]+\s+\(\w+\)\s*:'
            r'\s*min=(-?\d+)\s+max=(-?\d+)\s+step=(\d+)\s+default=(-?\d+)\s+value=(-?\d+)',
            line,
        )
        if not m:
            continue
        mn, mx = int(m.group(2)), int(m.group(3))
        if mn == 0 and mx == 0:
            continue
        name_raw = m.group(1).strip()
        key = name_raw.lower().replace(" ", "_").replace("(", "").replace(")", "")
        if key in seen:
            continue
        seen.add(key)
        controls.append({
            "name":    name_raw,
            "key":     key,
            "min":     mn,
            "max":     mx,
            "step":    int(m.group(4)),
            "default": int(m.group(5)),
            "current": int(m.group(6)),
        })
    return controls


def list_controls(device: str, subdev: str = "") -> list:
    """Merge controls from main device and optional subdev."""
    ctrls = parse_ctrls(_run_v4l2_ctl("-d", device, "--list-ctrls"))
    if subdev and Path(subdev).exists():
        existing_keys = {x["key"] for x in ctrls}
        for c in parse_ctrls(_run_v4l2_ctl("-d", subdev, "--list-ctrls")):
            if c["key"] not in existing_keys:
                c["source"] = "subdev"
                ctrls.append(c)
    return ctrls


# ---------------------------------------------------------------------------
# visionpipe execute helper  (live prop changes + streaming)
# ---------------------------------------------------------------------------
_vsp_executor = concurrent.futures.ThreadPoolExecutor(max_workers=4, thread_name_prefix="vsp")


def vsp_execute(item_call: str, timeout: int = 10) -> tuple:
    """Run: visionpipe execute '<item_call>' off the HTTP thread."""
    def _run():
        cmd = [_vsp_bin, "execute", item_call]
        log.info("vsp_execute: %s", item_call)
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
            return r.returncode, r.stdout or "", r.stderr or ""
        except subprocess.TimeoutExpired:
            return -1, "", "timeout"
        except FileNotFoundError:
            return -1, "", f"visionpipe not found: {_vsp_bin}"
    fut = _vsp_executor.submit(_run)
    return fut.result(timeout=timeout + 2)


def apply_prop(device: str, control: str, value, subdev: str = "") -> tuple:
    """
    Apply a V4L2 control.
    Strategy:
      1. Try v4l2-ctl on subdev (sensor controls live here)
      2. Try v4l2-ctl on main device
      3. Fall back to visionpipe execute v4l2_prop(...)
    """
    targets = []
    if subdev and Path(subdev).exists():
        targets.append(subdev)
    targets.append(device)

    for dev in targets:
        out = _run_v4l2_ctl("-d", dev, f"--set-ctrl={control}={int(value)}")
        if not re.search(r"error|failed|invalid|unknown", out, re.I):
            log.info("prop %s=%s on %s  OK", control, value, dev)
            return True, out.strip() or "ok"
        log.debug("v4l2-ctl %s=%s on %s → %s", control, value, dev, out.strip())

    # visionpipe execute fallback
    sd_arg = f', "{subdev}"' if subdev else ""
    call = f'v4l2_prop("{device}", "{control}", {int(value)}{sd_arg})'
    rc, out, err = vsp_execute(call)
    return rc == 0, (err if rc != 0 else out).strip()


# ---------------------------------------------------------------------------
# Stream process management
# ---------------------------------------------------------------------------
_proc_lock   = threading.Lock()   # guards _stream_proc pointer only
_log_lock    = threading.Lock()   # guards _stream_log list (separate to avoid
                                  # deadlock when stop_stream holds _proc_lock
                                  # while _reader_thread tries to log)
_stream_proc: Optional[subprocess.Popen] = None
_stream_log: list = []
_stream_cfg  = {}
_stream_ccm: Optional[list] = None   # last applied CCM (None = no CCM)


def _reader_thread(proc: subprocess.Popen):
    """Drain visionpipe stdout so the pipe never blocks."""
    try:
        for raw in proc.stdout:
            line = raw.rstrip()
            log.info("[vsp] %s", line)
            with _log_lock:
                _stream_log.append(line)
                if len(_stream_log) > 120:
                    _stream_log.pop(0)
    except Exception:
        pass


# ---------------------------------------------------------------------------
# VSP generator
# ---------------------------------------------------------------------------
def generate_vsp(cfg: dict, ccm: Optional[list] = None) -> str:
    d      = cfg.get("device",          "/dev/video3")
    sd     = cfg.get("subdev",          "")
    w      = cfg.get("width",           1640)
    h      = cfg.get("height",          1232)
    fmt    = cfg.get("format",          "SRGGB8")
    fps    = cfg.get("fps",             30)
    sport  = cfg.get("stream_port",     8080)
    qual   = cfg.get("quality",         90)
    patt   = cfg.get("pattern",         "RGGB")
    algo   = cfg.get("debayer_algo",    "bilinear")
    flip_v = cfg.get("flip_v",          False)
    flip_h = cfg.get("flip_h",          False)
    exp    = cfg.get("exposure",             1200)
    again  = cfg.get("analogue_gain",         80)
    dgain  = cfg.get("digital_gain",         256)
    r_pv   = cfg.get("red_pixel_value",      1023)
    gr_pv  = cfg.get("green_pixel_value_r",  1023)
    b_pv   = cfg.get("blue_pixel_value",     1023)
    gb_pv  = cfg.get("green_pixel_value_b",  1023)

    sd_arg = f', "{sd}"' if sd else ""

    lines = [
        f"# Auto-generated by camera_tuner.py – do not edit by hand",
        f"# Device: {d}  Format: {fmt}  {w}x{h} @{fps}fps",
        f"",
        f"pipeline setup",
        f'    v4l2_setup("{d}", {w}, {h}, "{fmt}", {fps}, 4{sd_arg})',
        f'    v4l2_prop("{d}", "vertical_flip",       {1 if flip_v else 0}{sd_arg})',
        f'    v4l2_prop("{d}", "horizontal_flip",     {1 if flip_h else 0}{sd_arg})',
        f'    v4l2_prop("{d}", "exposure",             {exp}{sd_arg})',
        f'    v4l2_prop("{d}", "analogue_gain",        {again}{sd_arg})',
        f'    v4l2_prop("{d}", "digital_gain",         {dgain}{sd_arg})',
        f'    v4l2_prop("{d}", "red_pixel_value",       {r_pv}{sd_arg})',
        f'    v4l2_prop("{d}", "green_pixel_value_r",   {gr_pv}{sd_arg})',
        f'    v4l2_prop("{d}", "blue_pixel_value",      {b_pv}{sd_arg})',
        f'    v4l2_prop("{d}", "green_pixel_value_b",   {gb_pv}{sd_arg})',
    ]

    # Embed CCM into setup so it is cached globally before main loop starts
    if ccm:
        row_strs = [
            "[" + ", ".join(f"{v:.6f}" for v in row) + "]"
            for row in ccm
        ]
        mat_str = "[" + ", ".join(row_strs) + "]"
        lines += [
            f'    ccm = mat_create({mat_str})',
            f'    to_cvmat(ccm)',
            f'    global_cache("ccm_matrix")',
        ]

    lines += [
        f'    print("Stream ready → http://0.0.0.0:{sport}/stream")',
        f"end",
        f"",
        f"pipeline main",
        f'    video_cap("{d}", "v4l2_native")',
        f'    debayer("{patt}", "{algo}", "bgr", 0)',
    ]
    if flip_v:
        lines.append('    flip("v")')
    if flip_h:
        lines.append('    flip("h")')

    # Apply CCM + normalise in the main processing loop
    if ccm:
        lines += [
            f'    transform("ccm_matrix")',
            f'    normalize(0, 255, "minmax", "8u")',
        ]

    lines += [
        f'    ip_stream({sport}, "0.0.0.0", {qual})',
        f"    fps(true, 10, {fps})",
        f"end",
        f"",
        f"exec_seq setup",
        f"exec_loop main",
    ]
    return "\n".join(lines) + "\n"


def start_stream(cfg: dict, ccm: Optional[list] = None) -> tuple:
    global _stream_proc, _stream_cfg, _stream_ccm
    stop_stream()
    vsp = generate_vsp(cfg, ccm=ccm)
    VSP_FILE.write_text(vsp)
    log.info("Generated VSP → %s\n%s", VSP_FILE, vsp)
    try:
        proc = subprocess.Popen(
            [_vsp_bin, "run", str(VSP_FILE)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            start_new_session=True,   # isolate from Python's signal group
        )
        # Fast-fail: give visionpipe 500 ms to crash on bad device/format
        time.sleep(0.5)
        if proc.poll() is not None:
            out = proc.stdout.read(4096)
            return False, f"visionpipe exited immediately (rc={proc.returncode}):\n{out}"
        with _log_lock:
            _stream_log.clear()
        with _proc_lock:
            _stream_proc = proc
            _stream_cfg  = dict(cfg)
            _stream_ccm  = ccm
        threading.Thread(target=_reader_thread, args=(proc,), daemon=True).start()
        return True, f"pid={proc.pid}"
    except Exception as e:
        return False, str(e)


def stop_stream() -> bool:
    global _stream_proc
    # Grab the proc reference and clear _stream_proc while holding the lock,
    # then wait *outside* the lock so _reader_thread can keep draining stdout
    # (a full pipe would prevent visionpipe from exiting cleanly).
    with _proc_lock:
        proc = _stream_proc
        if proc is None or proc.poll() is not None:
            return False
        _stream_proc = None
    # Send SIGINT to the entire process group (start_new_session=True was used)
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGINT)
    except ProcessLookupError:
        pass
    try:
        proc.wait(timeout=4)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except ProcessLookupError:
            pass
        proc.kill()
    return True


def stream_running() -> bool:
    with _proc_lock:
        return _stream_proc is not None and _stream_proc.poll() is None


def stream_status() -> dict:
    running = stream_running()
    with _proc_lock:
        pid = _stream_proc.pid if (_stream_proc and running) else None
    with _log_lock:
        last = list(_stream_log[-20:])
    return {"running": running, "pid": pid, "config": _stream_cfg, "log": last}


# ---------------------------------------------------------------------------
# Colour science helpers — following riskiest/color_calibration algorithm
# Ref: https://github.com/riskiest/color_calibration
#
# Pipeline:  sRGB 8-bit → linearize (gamma decode) → XYZ D65 → CIE Lab
#            CCM fitted in linear light, loss minimized using CIE76 ΔE in Lab
# ---------------------------------------------------------------------------

# sRGB D65 primaries → XYZ (IEC 61966-2-1)
_SRGB_TO_XYZ = np.array([
    [0.4124564, 0.3575761, 0.1804375],
    [0.2126729, 0.7151522, 0.0721750],
    [0.0193339, 0.1191920, 0.9503041],
], dtype=float)

# D65 whitepoint for Lab normalization
_D65_WHITE = np.array([0.95047, 1.00000, 1.08883], dtype=float)


def _srgb_to_linear(c: np.ndarray) -> np.ndarray:
    """sRGB gamma decode [0,1] → linear light (IEC 61966-2-1)."""
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def _linear_to_srgb(c: np.ndarray) -> np.ndarray:
    """Linear light → sRGB gamma encode [0,1]."""
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, 12.92 * c, 1.055 * (c ** (1.0 / 2.4)) - 0.055)


def _linear_rgb_to_lab(rgbl: np.ndarray) -> np.ndarray:
    """Nx3 linear sRGB [0,1] → CIE Lab D65/2°."""
    xyz = (rgbl @ _SRGB_TO_XYZ.T) / _D65_WHITE   # Nx3 normalized XYZ
    delta = 6.0 / 29.0
    f = np.where(xyz > delta ** 3,
                 np.cbrt(np.clip(xyz, 0.0, None)),
                 xyz / (3.0 * delta ** 2) + 4.0 / 29.0)
    L = 116.0 * f[:, 1] - 16.0
    a = 500.0 * (f[:, 0] - f[:, 1])
    b = 200.0 * (f[:, 1] - f[:, 2])
    return np.stack([L, a, b], axis=1)


def _cie76(lab1: np.ndarray, lab2: np.ndarray) -> np.ndarray:
    """Per-row CIE76 ΔE (fast perceptually uniform distance in Lab)."""
    return np.sqrt(np.sum((lab1 - lab2) ** 2, axis=1))


def _nelder_mead(f, x0: np.ndarray, max_iter: int = 800, tol: float = 1e-6) -> np.ndarray:
    """
    Pure-numpy Nelder-Mead simplex optimizer.
    Mirrors scipy.optimize.fmin — used by riskiest/color_calibration for CCM refinement.
    """
    n = len(x0)
    alpha, beta, gamma_c, sigma = 1.0, 2.0, 0.5, 0.5
    simplex = np.empty((n + 1, n), dtype=float)
    simplex[0] = x0.copy()
    for i in range(n):
        v = x0.copy()
        v[i] += 0.05 if abs(v[i]) < 1e-8 else 0.05 * abs(v[i])
        simplex[i + 1] = v
    fvals = np.array([f(s) for s in simplex])
    for _ in range(max_iter):
        order = np.argsort(fvals)
        simplex, fvals = simplex[order], fvals[order]
        if np.max(np.abs(simplex[1:] - simplex[0])) < tol:
            break
        centroid = simplex[:-1].mean(axis=0)
        xr = centroid + alpha * (centroid - simplex[-1])
        fr = f(xr)
        if fr < fvals[0]:
            xe = centroid + beta * (xr - centroid)
            fe = f(xe)
            simplex[-1], fvals[-1] = (xe, fe) if fe < fr else (xr, fr)
        elif fr < fvals[-2]:
            simplex[-1], fvals[-1] = xr, fr
        else:
            xc = centroid + gamma_c * (simplex[-1] - centroid)
            fc = f(xc)
            if fc < fvals[-1]:
                simplex[-1], fvals[-1] = xc, fc
            else:
                simplex[1:] = simplex[0] + sigma * (simplex[1:] - simplex[0])
                fvals[1:] = np.array([f(s) for s in simplex[1:]])
    return simplex[0]


# ---------------------------------------------------------------------------
# Colour calibration
# ---------------------------------------------------------------------------

def compute_wb_gains(samples: list) -> dict:
    """
    White-balance gains computed in linearised light so channel ratios are
    physically meaningful.  Luminance-weighted sum matching avoids dark-patch
    division blow-ups (follows riskiest/color_calibration white_balance init).
    """
    if not samples:
        raise ValueError("No samples provided")
    white_s   = [s for s in samples if s["patch"] == "W"]
    non_black = [s for s in samples if s["patch"] != "BLACK"]

    if white_s:
        m = _srgb_to_linear(
            np.clip(np.array(white_s[0]["measured"], dtype=float), 1.0, 255.0) / 255.0)
        t = _srgb_to_linear(
            np.array(REFERENCE_COLOURS["W"], dtype=float) / 255.0)
        r_gain = float(t[0] / max(m[0], 1e-6))
        g_gain = float(t[1] / max(m[1], 1e-6))
        b_gain = float(t[2] / max(m[2], 1e-6))
    elif non_black:
        meas = _srgb_to_linear(
            np.clip(np.array([s["measured"] for s in non_black], dtype=float), 1.0, 255.0) / 255.0)
        tgts = _srgb_to_linear(
            np.array([REFERENCE_COLOURS[s["patch"]] for s in non_black], dtype=float) / 255.0)
        luma_w = np.clip(0.2126*tgts[:,0] + 0.7152*tgts[:,1] + 0.0722*tgts[:,2], 1e-3, None)
        r_gain = float(np.sum(tgts[:,0] * luma_w) / np.sum(meas[:,0] * luma_w))
        g_gain = float(np.sum(tgts[:,1] * luma_w) / np.sum(meas[:,1] * luma_w))
        b_gain = float(np.sum(tgts[:,2] * luma_w) / np.sum(meas[:,2] * luma_w))
    else:
        return {}

    mx = max(r_gain, g_gain, b_gain) or 1.0
    r_n, g_n, b_n = r_gain / mx, g_gain / mx, b_gain / mx
    def pv(x): return int(round(np.clip(1023 * x, 0, 1023)))
    return {
        "red_pixel_value":     pv(r_n),
        "green_pixel_value_r": pv(g_n),
        "blue_pixel_value":    pv(b_n),
        "green_pixel_value_b": pv(g_n),
        "_norm": {"r": round(r_n, 4), "g": round(g_n, 4), "b": round(b_n, 4)},
    }


def delta_e(a: list, b: list) -> float:
    """CIE76 ΔE in Lab space — perceptually uniform, replaces raw Euclidean RGB."""
    al = _srgb_to_linear(np.clip(np.array(a, dtype=float), 0, 255) / 255.0)[np.newaxis, :]
    bl = _srgb_to_linear(np.clip(np.array(b, dtype=float), 0, 255) / 255.0)[np.newaxis, :]
    return float(_cie76(_linear_rgb_to_lab(al), _linear_rgb_to_lab(bl))[0])


def compute_ccm(samples: list) -> list:
    """
    3×3 CCM following riskiest/color_calibration algorithm:

      1. sRGB gamma-decode measured + reference into linear light.
      2. Luminance-weighted least-squares initial solution  (D_l = S_l @ M).
         → mirrors repo's initial_least_square()
      3. Nelder-Mead refinement minimising weighted CIE76 ΔE in Lab space.
         → mirrors repo's fitting() with distance='de76'

    VisionPipe applies  out_col = CCM × in_col  (column vectors), so the
    stored matrix is M.T (transpose of the row-vector fitting result).
    """
    valid = [s for s in samples
             if s["patch"] in REFERENCE_COLOURS and s["patch"] != "BLACK"]
    if len(valid) < 3:
        log.warning("compute_ccm: too few patches (%d, need ≥3) — returning identity", len(valid))
        return [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]

    # Step 1 — gamma linearise (crucial: CCM lives in linear light)
    src_l = _srgb_to_linear(
        np.clip(np.array([s["measured"] for s in valid], float), 1.0, 255.0) / 255.0)
    dst_l = _srgb_to_linear(
        np.array([REFERENCE_COLOURS[s["patch"]] for s in valid], float) / 255.0)

    # Step 2 — luminance weights: bright patches give more reliable ratios
    luma_w = np.clip(0.2126*dst_l[:,0] + 0.7152*dst_l[:,1] + 0.0722*dst_l[:,2], 0.05, 1.0)
    W = np.sqrt(luma_w[:, np.newaxis])
    M_init, _, _, _ = np.linalg.lstsq(src_l * W, dst_l * W, rcond=None)  # 3×3

    # Step 3 — Nelder-Mead refinement minimising weighted CIE76 ΔE
    dst_lab = _linear_rgb_to_lab(dst_l)

    def loss(m_flat: np.ndarray) -> float:
        pred_l   = np.clip(src_l @ m_flat.reshape(3, 3), 0.0, 1.0)
        pred_lab = _linear_rgb_to_lab(pred_l)
        de = _cie76(pred_lab, dst_lab)
        return float(np.dot(luma_w, de ** 2))

    m_opt = _nelder_mead(loss, M_init.flatten())
    error = (loss(m_opt) / len(valid)) ** 0.5
    log.info("CCM error (RMS CIE76 ΔE): %.3f", error)

    # M_opt: row-transform  D = S @ M.  VP needs column-transform → store M.T
    ccm = m_opt.reshape(3, 3).T.tolist()
    log.info("CCM (CIE76-optimized):\n%s",
             "\n".join(" ".join(f"{v:7.4f}" for v in row) for row in ccm))
    return ccm


# ---------------------------------------------------------------------------
# Snapshot: extract one JPEG frame from the live MJPEG stream
# ---------------------------------------------------------------------------
def get_snapshot_jpeg(stream_port: int) -> Optional[bytes]:
    """Read the MJPEG stream just long enough to extract one complete JPEG."""
    url = f"http://127.0.0.1:{stream_port}/stream"
    try:
        with urllib.request.urlopen(url, timeout=5) as resp:
            buf = bytearray()
            # Read until we collect a full JPEG (SOI … EOI)
            for _ in range(512):          # cap at ~2 MB of reading
                chunk = resp.read(4096)
                if not chunk:
                    break
                buf.extend(chunk)
                soi = buf.find(b'\xff\xd8')
                if soi < 0:
                    continue
                eoi = buf.find(b'\xff\xd9', soi + 2)
                if eoi >= 0:
                    return bytes(buf[soi: eoi + 2])
    except Exception as e:
        log.debug("snapshot: %s", e)
    return None


# ---------------------------------------------------------------------------
# Threaded HTTP server
# ---------------------------------------------------------------------------
class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


class TunerHandler(BaseHTTPRequestHandler):
    stream_port: int = 8080

    def log_message(self, fmt, *args):
        log.debug(fmt, *args)

    # ------------------------------------------------------------------ GET
    def do_GET(self):
        p = self.path.split("?")[0]

        if p in ("/", "/index.html"):
            self._file(HTML_FILE, "text/html; charset=utf-8")

        elif p == "/api/snapshot":
            # Returns one JPEG frame from the live stream (same-origin, no CORS)
            if not stream_running():
                self._json(503, {"error": "stream not running"})
                return
            jpeg = get_snapshot_jpeg(self.stream_port)
            if jpeg is None:
                self._json(503, {"error": "could not grab frame"})
                return
            self.send_response(200)
            self.send_header("Content-Type",   "image/jpeg")
            self.send_header("Content-Length", str(len(jpeg)))
            self.send_header("Cache-Control",  "no-store")
            self._cors()
            self.end_headers()
            self.wfile.write(jpeg)

        elif p == "/api/cameras":
            # Always return from cache so this endpoint never blocks.
            # The background discovery thread populates the caches; the UI
            # polls until cameras appear.
            with _cameras_lock:
                cameras = list(_cameras_cache) if _cameras_cache is not None else []
                ready   = _cameras_cache is not None
            subdevs = list_sensor_subdevs()   # fast: returns cached or empty
            self._json(200, {"cameras": cameras, "sensor_subdevs": subdevs,
                             "discovery_ready": ready})

        elif p == "/api/formats":
            dev = self._qs("device", "/dev/video0")
            sd  = self._qs("subdev", "")
            fmts = list_formats(dev, sd)
            self._json(200, {"formats": fmts, "source": "v4l2-ctl",
                             "used_subdev": sd})

        elif p == "/api/controls":
            dev = self._qs("device", "/dev/video0")
            sd  = self._qs("subdev", "") or _find_subdev_for_device(dev)
            self._json(200, {
                "controls":    list_controls(dev, sd),
                "subdev_used": sd,
                "source":      "v4l2-ctl",
            })

        elif p == "/api/status":
            self._json(200, stream_status())

        elif p == "/api/references":
            self._json(200, REFERENCE_COLOURS)

        elif p == "/api/vsp":
            self._json(200, {"vsp": VSP_FILE.read_text() if VSP_FILE.exists() else ""})

        else:
            self._json(404, {"error": "Not found"})

    # ----------------------------------------------------------------- POST
    def do_POST(self):
        body = self._body()
        if body is None:
            return
        p = self.path.split("?")[0]

        if p == "/api/start":
            ok, msg = start_stream(body)
            self._json(200 if ok else 500, {"ok": ok, "msg": msg, "status": stream_status()})

        elif p == "/api/stop":
            self._json(200, {"ok": stop_stream()})

        elif p == "/api/prop":
            dev  = body.get("device",  "/dev/video0")
            ctrl = body.get("control", "")
            val  = body.get("value",   0)
            sd   = body.get("subdev",  "") or _find_subdev_for_device(dev)
            
            with _proc_lock:
                if _stream_cfg:
                    _stream_cfg[ctrl] = val
                    
            ok, msg = apply_prop(dev, ctrl, val, sd)
            self._json(200 if ok else 500, {"ok": ok, "msg": msg})

        elif p == "/api/calibrate":
            samples = body.get("samples", [])
            dev     = body.get("device",  "/dev/video0")
            sd      = body.get("subdev",  "") or _find_subdev_for_device(dev)
            try:
                # WB gains (pixel value controls)
                gains = compute_wb_gains(samples)
                norm  = gains.pop("_norm", {})

                # If chaining multiple iterations, multiply WB gain ratios
                if _stream_cfg and body.get("apply", True) and "red_pixel_value" in gains:
                    old_r = _stream_cfg.get("red_pixel_value", 1023) / 1023.0
                    old_g = _stream_cfg.get("green_pixel_value_r", 1023) / 1023.0
                    old_b = _stream_cfg.get("blue_pixel_value", 1023) / 1023.0
                    
                    new_r = gains["red_pixel_value"] / 1023.0
                    new_g = gains["green_pixel_value_r"] / 1023.0
                    new_b = gains["blue_pixel_value"] / 1023.0
                    
                    comb_r = np.clip(old_r * new_r, 0, 1.0)
                    comb_g = np.clip(old_g * new_g, 0, 1.0)
                    comb_b = np.clip(old_b * new_b, 0, 1.0)
                    
                    mx = max(comb_r, comb_g, comb_b) or 1.0
                    comb_r, comb_g, comb_b = comb_r/mx, comb_g/mx, comb_b/mx
                    
                    def pv(x): return int(round(np.clip(1023*x, 0, 1023)))
                    gains["red_pixel_value"] = pv(comb_r)
                    gains["green_pixel_value_r"] = pv(comb_g)
                    gains["blue_pixel_value"] = pv(comb_b)
                    gains["green_pixel_value_b"] = pv(comb_g)
                    norm = {"r": round(comb_r, 4), "g": round(comb_g, 4), "b": round(comb_b, 4)}

                # Color Correction Matrix
                ccm = compute_ccm(samples)
                
                # Chain multiple iterations: if a previous CCM exists, compose them.
                # Both matrices are in VP column-vector convention (M.T of the row-fit).
                # Composing two VP transforms: out = CCM_new @ CCM_old @ in
                if _stream_ccm and body.get("apply", True):
                    old_mat = np.array(_stream_ccm, float)
                    new_mat = np.array(ccm, float)
                    ccm = (new_mat @ old_mat).tolist()

                patch_errors = [
                    {"patch": s["patch"],
                     "delta_e": round(delta_e(s["measured"], REFERENCE_COLOURS[s["patch"]]), 2),
                     "measured": s["measured"],
                     "target": REFERENCE_COLOURS[s["patch"]]}
                    for s in samples if s["patch"] in REFERENCE_COLOURS
                ]
                apply_errors = []
                if body.get("apply", True):
                    # Build updated stream config with new WB gain values
                    with _proc_lock:
                        new_cfg = dict(_stream_cfg) if _stream_cfg else {"device": dev, "subdev": sd}
                    new_cfg.update(gains)   # inject new pixel-value controls
                    # Stop current pipeline and restart with new VSP
                    # (embeds both WB gains and CCM into the generated VSP)
                    ok, msg = start_stream(new_cfg, ccm=ccm)
                    if not ok:
                        apply_errors.append(f"VSP restart failed: {msg}")
                    else:
                        log.info("Calibration applied — restarted VSP with CCM (pid %s)", msg)
                self._json(200, {"gains": gains, "wb_norm": norm,
                                 "ccm": ccm,
                                 "patch_errors": patch_errors,
                                 "apply_errors": apply_errors})
            except Exception as e:
                self._json(400, {"error": str(e)})

        elif p == "/api/vsp_preview":
            self._json(200, {"vsp": generate_vsp(body, ccm=body.get("ccm"))})

        else:
            self._json(404, {"error": "Unknown endpoint"})

    def do_OPTIONS(self):
        self.send_response(200)
        self._cors()
        self.end_headers()

    # --------------------------------------------------------------- helpers
    def _qs(self, key: str, default: str = "") -> str:
        qs = parse_qs(urlparse(self.path).query)
        return qs.get(key, [default])[0]

    def _file(self, path: Path, ct: str):
        try:
            data = path.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type",   ct)
            self.send_header("Content-Length", str(len(data)))
            self._cors()
            self.end_headers()
            self.wfile.write(data)
        except FileNotFoundError:
            self._json(404, {"error": f"File not found: {path}"})

    def _mjpeg(self):
        pass  # proxy removed; stream goes directly from visionpipe ip_stream to browser


    def _body(self) -> Optional[dict]:
        try:
            n = int(self.headers.get("Content-Length", 0))
            return json.loads(self.rfile.read(n))
        except Exception as e:
            self._json(400, {"error": f"Bad JSON: {e}"})
            return None

    def _json(self, code: int, data):
        body = json.dumps(data, indent=2).encode()
        self.send_response(code)
        self.send_header("Content-Type",   "application/json")
        self.send_header("Content-Length", str(len(body)))
        self._cors()
        self.end_headers()
        self.wfile.write(body)

    def _cors(self):
        self.send_header("Access-Control-Allow-Origin",  "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")


# ---------------------------------------------------------------------------
def make_handler(stream_port: int):
    class H(TunerHandler):
        pass
    H.stream_port = stream_port
    return H


def main():
    global _vsp_bin, _v4l2_ctl
    parser = argparse.ArgumentParser(description="VisionPipe Camera Tuner")
    parser.add_argument("--port",        type=int, default=5000)
    parser.add_argument("--host",        default="0.0.0.0")
    parser.add_argument("--stream-port", type=int, default=8080)
    parser.add_argument("--visionpipe",  default="",
                        help="Path to visionpipe binary (auto-detected)")
    parser.add_argument("--v4l2-ctl",    dest="v4l2_ctl", default="",
                        help="Path to v4l2-ctl binary (auto-detected)")
    args = parser.parse_args()

    _vsp_bin  = _resolve_bin(args.visionpipe, "visionpipe")
    _v4l2_ctl = _resolve_bin(args.v4l2_ctl,   "v4l2-ctl")

    log.info("visionpipe : %s", _vsp_bin)
    log.info("v4l2-ctl   : %s", _v4l2_ctl)

    # ----------------------------------------------------------------
    # Background discovery: runs BEFORE the HTTP server starts so that
    # by the time the browser connects the data is almost certainly ready.
    # Pre-set the scanning flag so that if an HTTP request somehow arrives
    # before the thread begins, it returns [] immediately instead of
    # triggering a second blocking scan on the request thread.
    # ----------------------------------------------------------------
    global _cameras_cache, _sensor_subdevs_scanning
    with _sensor_subdevs_lock:
        _sensor_subdevs_scanning = True   # block any HTTP-triggered scan

    def _discovery():
        global _cameras_cache, _sensor_subdevs_scanning
        cams = list_video_devices()
        with _cameras_lock:
            _cameras_cache = cams
        log.info("Discovery: %d camera(s) found: %s", len(cams), [c["path"] for c in cams])
        # Unlock subdev scan so list_sensor_subdevs() proceeds normally
        with _sensor_subdevs_lock:
            _sensor_subdevs_scanning = False
        list_sensor_subdevs()

    threading.Thread(target=_discovery, daemon=True, name="discovery").start()

    server = ThreadingHTTPServer((args.host, args.port), make_handler(args.stream_port))
    log.info("=" * 56)
    log.info("  VisionPipe Camera Tuner  (threaded, hybrid)")
    log.info("  Open: http://localhost:%d", args.port)
    log.info("  Ctrl+C to exit")
    log.info("=" * 56)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log.info("Stopping…")
        stop_stream()
        server.server_close()


if __name__ == "__main__":
    main()

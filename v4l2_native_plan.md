# Plan: V4L2 Native Backend, New Items & Compile Fix

**Overview:** Add a new `V4L2_NATIVE` camera backend housed in a standalone `V4L2DeviceManager` class that the existing `CameraDeviceManager` singleton delegates to. The backend uses kernel-level V4L2 APIs (`ioctl`, `mmap`, `poll`) for full format negotiation and deep control access. Pair it with a `v4l2_items` file for the VSP surface (setup, control, enumeration items). Add a richer `debayer` item to `color_items` (always compiled). Fix the bare `sys/mman.h` include that breaks non-Linux builds.

---

## Steps

### 1. Fix: `camera_device_manager.cpp` fails to compile when `LIBCAMERA_BACKEND=OFF`

There are two related causes:

#### 1a. CMake — base deps gated behind `LIBCAMERA_BACKEND`

`camera_device_manager.cpp` is **always** compiled (it is unconditionally appended to `VISIONPIPE_CORE_SOURCES`). However, its base runtime dependencies — e.g. `Threads::Threads` sub-requirements, any system libs transitively pulled in by `${LIBCAMERA_LIBRARIES}` such as `-lpthread` or `-latomic`, and/or `pkg-config`-discovered flags — are only linked to `visionpipe_core` inside the `if(LIBCAMERA_BACKEND)` block (CMakeLists.txt lines 393–395):

```cmake
# Currently only present when LIBCAMERA_BACKEND=ON:
if(LIBCAMERA_BACKEND)
    target_include_directories(visionpipe_core PUBLIC ${LIBCAMERA_INCLUDE_DIRS})
    target_link_libraries(visionpipe_core PUBLIC ${LIBCAMERA_LIBRARIES})
endif()
```

When `LIBCAMERA_BACKEND=OFF`, those transitive libs are absent, causing link failures for code in `camera_device_manager.cpp` that has nothing to do with libcamera.

**Fix:** Audit what `${LIBCAMERA_LIBRARIES}` provides (typically expands to something like `-lcamera -lcamera-base -lpthread`). Extract only the base deps that `camera_device_manager.cpp` requires unconditionally (e.g. `Threads::Threads` if not already fully satisfied, atomic, etc.) and add them to the unconditional `target_link_libraries(visionpipe_core PUBLIC ...)` block (lines 368–373). Leave `${LIBCAMERA_INCLUDE_DIRS}` and `${LIBCAMERA_LIBRARIES}` themselves inside the `if(LIBCAMERA_BACKEND)` guard.

#### 1b. Source — unguarded Linux-only header

In `src/utils/camera_device_manager.cpp`, `#include <sys/mman.h>` sits unconditionally at the top of the file. `mmap`/`munmap` are only used inside libcamera codepaths. Guard it:

```cpp
#ifdef VISIONPIPE_LIBCAMERA_ENABLED
#include <sys/mman.h>
#endif
```

This prevents the include from leaking into non-libcamera (or non-Linux) builds.

---

### 2. CMake: Add `V4L2_NATIVE_BACKEND` option

In `CMakeLists.txt`, after the existing `LIBCAMERA_BACKEND` option block, add:

- `option(V4L2_NATIVE_BACKEND "Enable native V4L2 backend (Linux only)" OFF)`
- Linux-only guard (`CMAKE_SYSTEM_NAME STREQUAL "Linux"`) with fallback warning
- Kernel header check (verify `linux/videodev2.h` is present via `check_include_file`)
- On success: `add_definitions(-DVISIONPIPE_V4L2_NATIVE_ENABLED)`
- Append to `VISIONPIPE_CORE_SOURCES`: `src/utils/v4l2_device_manager.cpp`
- Append to `VISIONPIPE_ITEMS_SOURCES`: `src/interpreter/items/v4l2_items.cpp`

---

### 3. New File: `include/utils/v4l2_device_manager.h`

Declare the `V4L2NativeConfig` struct and `V4L2DeviceManager` class. Guard everything with `#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED`.

**`V4L2NativeConfig` struct** fields:
- `width`, `height`, `fps` (int)
- `pixelFormat` (string — FourCC or named: `"YUYV"`, `"MJPG"`, `"NV12"`, `"SRGGB10"`, etc.)
- `bufferCount` (int, default 4)
- `io_method` (enum: `MMAP` only in first version)

**`V4L2DeviceManager`** (singleton, same pattern as `CameraDeviceManager`):
- `instance()` static method
- `acquireFrame(devicePath, frame)` → `bool`
- `prepareDevice(devicePath, config)` → `bool` (open + `VIDIOC_S_FMT` + `VIDIOC_REQBUFS` + mmap)
- `releaseDevice(devicePath)` → `void`
- `releaseAll()` → `void`
- `setControl(devicePath, controlId_or_name, value)` → `bool`
- `getControl(devicePath, controlId_or_name)` → `int`
- `listControls(devicePath)` → prints all User-class + Camera-class controls
- `listFormats(devicePath)` → prints all `VIDIOC_ENUM_FMT` + `VIDIOC_ENUM_FRAMESIZES`
- `getBayerPattern(devicePath)` → `std::string` (derived from active FourCC)
- Private `V4L2Session` struct holding fd, mmap'd buffers, format info, active config

---

### 4. New File: `src/utils/v4l2_device_manager.cpp`

Full implementation (Linux-only, guarded by `VISIONPIPE_V4L2_NATIVE_ENABLED`). Key internal sections:

**Includes (inside guard):**
```cpp
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>
```

**Format table** — map string → `uint32_t` V4L2 FourCC:
- YUV: `V4L2_PIX_FMT_YUYV`, `UYVY`, `NV12`, `NV21`, `YUV420`
- RGB: `V4L2_PIX_FMT_BGR24`, `RGB24`
- Compressed: `V4L2_PIX_FMT_MJPEG`
- 8-bit Bayer: `SBGGR8`, `SGBRG8`, `SGRBG8`, `SRGGB8`
- 10-bit Bayer: `SBGGR10`, `SGBRG10`, `SGRBG10`, `SRGGB10` (packed)
- 12-bit Bayer: `SBGGR12` ... `SRGGB12` (packed)
- 16-bit Bayer: `SBGGR16` ... `SRGGB16`

**`openDevice(path, config)`:**
1. `open(path, O_RDWR | O_NONBLOCK)`
2. `VIDIOC_QUERYCAP` — check `V4L2_CAP_VIDEO_CAPTURE` + `V4L2_CAP_STREAMING`
3. `VIDIOC_S_FMT` with requested width/height/pixfmt; read back negotiated values
4. Set framerate with `VIDIOC_S_PARM`
5. `VIDIOC_REQBUFS` with `V4L2_MEMORY_MMAP`
6. `VIDIOC_QUERYBUF` + `mmap` each buffer; `VIDIOC_QBUF` all initially
7. `VIDIOC_STREAMON`

**`acquireFrame(path, frame)` flow:**
1. `poll(fd, POLLIN, 2000ms timeout)`
2. `VIDIOC_DQBUF` to get filled buffer index
3. Convert raw buffer to `cv::Mat` based on active FourCC:
   - `YUYV`/`UYVY` → `CV_8UC2` (OpenCV handles YUV→BGR via `cvtColor`)
   - `MJPEG` → `imdecode` from buffer bytes
   - `NV12`/`NV21` → `CV_8UC1` with height `* 3/2`, then `cvtColor`
   - `BGR24`/`RGB24` → direct `CV_8UC3`
   - 8-bit Bayer → `CV_8UC1`
   - 10/12-bit packed Bayer → unpack to `CV_16UC1` (same algorithm as libcamera unpacker)
   - 16-bit Bayer → `CV_16UC1`
4. Re-enqueue buffer with `VIDIOC_QBUF`

**`setControl(path, name_or_id, value)`:**
1. Try to resolve name → V4L2 control ID using `VIDIOC_QUERYCTRL` iteration across `V4L2_CID_BASE` and `V4L2_CID_CAMERA_CLASS_BASE`
2. Use `VIDIOC_S_CTRL` for standard controls; `VIDIOC_S_EXT_CTRLS` for extended (camera-class) controls

**`getBayerPattern(path)`:** Map active FourCC to `"RGGB"`, `"BGGR"`, `"GBRG"`, `"GRBG"` using FourCC prefix.

---

### 5. Integrate into `CameraDeviceManager`

**In `include/utils/camera_device_manager.h`:**
- Add `V4L2_NATIVE` to `CameraBackend` enum
- Add conditional include of `v4l2_device_manager.h` inside `#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED`
- Add guarded public API: `setV4L2NativeConfig(sourceId, config)`, `setV4L2Control(sourceId, name, value)`, `getV4L2BayerPattern(sourceId)`, `listV4L2Controls(sourceId)`, `listV4L2Formats(sourceId)`

**In `src/utils/camera_device_manager.cpp`:**
- `parseBackend()`: add `"v4l2_native"` → `CameraBackend::V4L2_NATIVE`
- `acquireFrame()`: add `V4L2_NATIVE` branch → delegate to `V4L2DeviceManager::instance().acquireFrame(sourceId, frame)`
- `openCamera()`: add `V4L2_NATIVE` branch → `V4L2DeviceManager::instance().prepareDevice(sourceId, config)`
- `releaseCamera()`: add `V4L2_NATIVE` branch → `V4L2DeviceManager::instance().releaseDevice(sourceId)`
- `releaseAll()`: call `V4L2DeviceManager::instance().releaseAll()` when `VISIONPIPE_V4L2_NATIVE_ENABLED`
- New public delegation methods inside `#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED`

---

### 6. New File: `include/interpreter/items/v4l2_items.h`

Wrap all declarations with `#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED`. Declare `registerV4L2Items(ItemRegistry&)` outside guard (empty stub fallback). Item classes:

| Class | VSP function | Purpose |
|---|---|---|
| `V4L2SetupItem` | `v4l2_setup` | Configure and open V4L2 native device |
| `V4L2PropItem` | `v4l2_prop` | Set named V4L2 control (User + Camera class) |
| `V4L2GetPropItem` | `v4l2_get_prop` | Get current value of a V4L2 control |
| `V4L2ListControlsItem` | `v4l2_list_controls` | Print all controls with current values + ranges |
| `V4L2ListFormatsItem` | `v4l2_list_formats` | Print all supported formats + resolutions |
| `V4L2GetBayerItem` | `v4l2_get_bayer` | Return Bayer pattern string from active format |
| `V4L2EnumDevicesItem` | `v4l2_enum_devices` | Enumerate `/dev/video*` and their capabilities |

---

### 7. New File: `src/interpreter/items/v4l2_items.cpp`

Follow the exact same pattern as `src/interpreter/items/libcamera_items.cpp`. All implementations inside `#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED`.

**`V4L2SetupItem` params:** `source` (ANY), `width` (INT, 640), `height` (INT, 480), `pixel_format` (STRING, `"YUYV"`), `fps` (INT, 30), `buffer_count` (INT, 4)  
**`V4L2PropItem` params:** `source` (ANY), `control_name` (STRING), `value` (FLOAT)  
**`V4L2GetPropItem` params:** `source` (ANY), `control_name` (STRING) → returns INT  
**`V4L2ListControlsItem` params:** `source` (ANY) → prints, returns mat pass-through  
**`V4L2ListFormatsItem` params:** `source` (ANY)  
**`V4L2GetBayerItem` params:** `source` (ANY) → returns STRING  
**`V4L2EnumDevicesItem` params:** none → scans `/dev/video0` through `/dev/video31`

---

### 8. New `DebayerItem` in `color_items`

**In `include/interpreter/items/color_items.h`:** Add `class DebayerItem : public InterpreterItem`.

**In `src/interpreter/items/color_items.cpp`:** Implement and register.

**VSP function:** `debayer`  
**Params:**
- `pattern` (STRING, `"RGGB"`) — Bayer tile pattern: `RGGB`, `BGGR`, `GBRG`, `GRBG`
- `algorithm` (STRING, `"bilinear"`) — `"bilinear"` → `COLOR_BayerXX2BGR`, `"vng"` → `COLOR_BayerXX2BGR_VNG`, `"ea"` → `COLOR_BayerXX2BGR_EA`
- `output` (STRING, `"bgr"`) — `"bgr"` or `"rgb"`
- `bit_shift` (INT, 0) — right-shift for 10/12-bit packed inputs before debayering (e.g., `>>2` for 10-bit in 16-bit container)

Internally builds the correct `cv::ColorConversionCodes` from pattern + algorithm + output, then applies optional bit-shift, then calls `cv::cvtColor`. This replaces the need for users to remember `cvt_color("bayerrg2bgr")` and manually shift 10-bit data.

---

### 9. Wire into `all_items.cpp` / `all_items.h`

**`include/interpreter/items/all_items.h`:** Add `#include "interpreter/items/v4l2_items.h"`.

**`src/interpreter/items/all_items.cpp`:**
```cpp
#ifdef VISIONPIPE_V4L2_NATIVE_ENABLED
    registerV4L2Items(registry);
#endif
```
Place after the existing `#ifdef VISIONPIPE_LIBCAMERA_ENABLED` block.

---

## Decisions

- **`V4L2DeviceManager`:** Separate class — `CameraDeviceManager` delegates via conditional includes/calls, keeping the session API unified.
- **`DebayerItem`:** In `color_items` (always compiled) — useful for any Bayer source, not tied to V4L2 backend.
- **Controls scope:** User-class (`V4L2_CID_BASE`) + Camera-class (`V4L2_CID_CAMERA_CLASS_BASE`), resolved by name string.
- **V4L2 routing string:** `"v4l2_native"` to distinguish from existing `"v4l2"` (OpenCV's `cv::CAP_V4L2`).

---

## Verification

- **Fix check:** Build on Windows without any backend flags — no more `sys/mman.h` error.
- **V4L2_NATIVE build:** Rebuild on Linux with `-DV4L2_NATIVE_BACKEND=ON` — all new sources compile cleanly.
- **Feature test VSP script:**
  ```
  v4l2_setup("/dev/video0", 1920, 1080, "SRGGB10", 30)
  v4l2_list_controls("/dev/video0")
  v4l2_prop("/dev/video0", "ExposureTime", 5000)
  video(0, "v4l2_native") → debayer("RGGB", "ea", "bgr", 2)
  ```
- **Debayer:** Verify `debayer("RGGB", "vng")` on a `CV_16UC1` 10-bit frame from V4L2 produces correct color.
- **Disabled build:** Rebuild with `V4L2_NATIVE_BACKEND=OFF` — empty stub `registerV4L2Items()` compiles, no other changes visible.

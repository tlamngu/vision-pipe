# VisionPipe Documentation

Auto-generated documentation for VisionPipe interpreter items.

---

## Table of Contents

- [advanced](#advanced) (30 items)
- [arithmetic](#arithmetic) (28 items)
- [color](#color) (15 items)
- [conditional](#conditional) (24 items)
- [control](#control) (38 items)
- [display](#display) (27 items)
- [dnn](#dnn) (19 items)
- [draw](#draw) (19 items)
- [edge](#edge) (23 items)
- [feature](#feature) (20 items)
- [filter](#filter) (14 items)
- [gui](#gui) (16 items)
- [math](#math) (28 items)
- [morphology](#morphology) (14 items)
- [stereo](#stereo) (22 items)
- [tensor](#tensor) (49 items)
- [transform](#transform) (21 items)
- [video_io](#video_io) (22 items)

---

## advanced

### `apply_background_subtractor()`

Applies background subtractor to current frame

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `subtractor_cache` | string | Yes | Subtractor cache ID |
| `learning_rate` | float | No | Learning rate (-1 = auto) (default: `RuntimeValue(float: -1)`) |

**Example:**

```vsp
apply_background_subtractor("bg_sub", -1)
```

**Tags:** `background`, `subtraction`

---

### `background_subtractor_knn()`

Creates KNN background subtractor

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `history` | int | No | History length (default: `RuntimeValue(int: 500)`) |
| `dist2_threshold` | float | No | Distance threshold (default: `RuntimeValue(float: 400)`) |
| `detect_shadows` | bool | No | Detect shadows (default: `RuntimeValue(bool: true)`) |

**Example:**

```vsp
background_subtractor_knn(500, 400.0, true)
```

**Tags:** `background`, `subtraction`, `knn`

---

### `background_subtractor_mog2()`

Creates MOG2 background subtractor

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `history` | int | No | History length (default: `RuntimeValue(int: 500)`) |
| `var_threshold` | float | No | Variance threshold (default: `RuntimeValue(float: 16)`) |
| `detect_shadows` | bool | No | Detect shadows (default: `RuntimeValue(bool: true)`) |
| `cache_id` | string | No | Subtractor cache ID (default: `RuntimeValue(string: "bg_sub")`) |

**Example:**

```vsp
background_subtractor_mog2(500, 16.0, true, "bg_sub")
```

**Tags:** `background`, `subtraction`, `mog2`

---

### `calc_optical_flow_farneback()`

Calculates dense optical flow using Farneback algorithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `prev_cache` | string | Yes | Previous frame cache ID |
| `pyr_scale` | float | No | Pyramid scale (default: `RuntimeValue(float: 0.5)`) |
| `levels` | int | No | Number of pyramid levels (default: `RuntimeValue(int: 3)`) |
| `winsize` | int | No | Averaging window size (default: `RuntimeValue(int: 15)`) |
| `iterations` | int | No | Iterations at each level (default: `RuntimeValue(int: 3)`) |
| `poly_n` | int | No | Polynomial neighborhood size (default: `RuntimeValue(int: 5)`) |
| `poly_sigma` | float | No | Polynomial sigma (default: `RuntimeValue(float: 1.2)`) |
| `cache_id` | string | No | Flow cache ID (default: `RuntimeValue(string: "flow")`) |

**Example:**

```vsp
calc_optical_flow_farneback("prev_frame", 0.5, 3, 15, 3, 5, 1.2, "flow")
```

**Tags:** `optical_flow`, `farneback`, `dense`

---

### `calc_optical_flow_pyr_lk()`

Calculates sparse optical flow using Lucas-Kanade pyramid method

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `prev_cache` | string | Yes | Previous frame cache ID |
| `prev_points_cache` | string | Yes | Previous points cache ID |
| `win_size` | int | No | Search window size (default: `RuntimeValue(int: 21)`) |
| `max_level` | int | No | Maximum pyramid level (default: `RuntimeValue(int: 3)`) |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "flow")`) |

**Example:**

```vsp
calc_optical_flow_pyr_lk("prev_frame", "prev_points", 21, 3, "flow")
```

**Tags:** `optical_flow`, `lk`, `tracking`

---

### `color_change()`

Changes color of image region

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mask_cache` | string | Yes | Mask cache ID |
| `red_mul` | float | No | Red multiplier (default: `RuntimeValue(float: 1)`) |
| `green_mul` | float | No | Green multiplier (default: `RuntimeValue(float: 1)`) |
| `blue_mul` | float | No | Blue multiplier (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
color_change("mask", 1.5, 1.0, 0.5)
```

**Tags:** `color`, `change`

---

### `create_merge_mertens()`

Creates Mertens exposure fusion algorithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `contrast_weight` | float | No | Contrast weight (default: `RuntimeValue(float: 1)`) |
| `saturation_weight` | float | No | Saturation weight (default: `RuntimeValue(float: 1)`) |
| `exposure_weight` | float | No | Exposure weight (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
create_merge_mertens(1.0, 1.0, 0.0)
```

**Tags:** `hdr`, `mertens`, `fusion`

---

### `detail_enhance()`

Enhances image details

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `sigma_s` | float | No | Spatial sigma (default: `RuntimeValue(float: 10)`) |
| `sigma_r` | float | No | Range sigma (default: `RuntimeValue(float: 0.15)`) |

**Example:**

```vsp
detail_enhance(10, 0.15)
```

**Tags:** `enhance`, `detail`, `npr`

---

### `draw_optical_flow()`

Visualizes optical flow as arrows or HSV

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flow_cache` | string | Yes | Flow field cache ID |
| `mode` | string | No | Mode: arrows, hsv (default: `RuntimeValue(string: "hsv")`) |
| `step` | int | No | Arrow step for arrows mode (default: `RuntimeValue(int: 16)`) |
| `scale` | float | No | Arrow scale (default: `RuntimeValue(float: 2)`) |

**Example:**

```vsp
draw_optical_flow("flow", "hsv")
```

**Tags:** `optical_flow`, `visualization`

---

### `edge_preserving_filter()`

Applies edge-preserving smoothing

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flags` | string | No | Type: recursive, normconv (default: `RuntimeValue(string: "recursive")`) |
| `sigma_s` | float | No | Spatial sigma (default: `RuntimeValue(float: 60)`) |
| `sigma_r` | float | No | Range sigma (default: `RuntimeValue(float: 0.4)`) |

**Example:**

```vsp
edge_preserving_filter("recursive", 60, 0.4)
```

**Tags:** `filter`, `edge_preserving`, `npr`

---

### `fast_nl_means_denoising()`

Non-local means denoising for grayscale images

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `h` | float | No | Filter strength (default: `RuntimeValue(float: 10)`) |
| `template_size` | int | No | Template patch size (default: `RuntimeValue(int: 7)`) |
| `search_size` | int | No | Search window size (default: `RuntimeValue(int: 21)`) |

**Example:**

```vsp
fast_nl_means_denoising(10, 7, 21)
```

**Tags:** `denoise`, `nlm`

---

### `fast_nl_means_denoising_colored()`

Non-local means denoising for color images

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `h` | float | No | Filter strength (luminance) (default: `RuntimeValue(float: 10)`) |
| `h_color` | float | No | Filter strength (color) (default: `RuntimeValue(float: 10)`) |
| `template_size` | int | No | Template patch size (default: `RuntimeValue(int: 7)`) |
| `search_size` | int | No | Search window size (default: `RuntimeValue(int: 21)`) |

**Example:**

```vsp
fast_nl_means_denoising_colored(10, 10, 7, 21)
```

**Tags:** `denoise`, `nlm`, `color`

---

### `illumination_change()`

Changes illumination of image region

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mask_cache` | string | Yes | Mask cache ID |
| `alpha` | float | No | Brightness factor (default: `RuntimeValue(float: 0.2)`) |
| `beta` | float | No | Blend factor (default: `RuntimeValue(float: 0.4)`) |

**Example:**

```vsp
illumination_change("mask", 0.2, 0.4)
```

**Tags:** `illumination`, `lighting`

---

### `init_tracker()`

Initializes tracker with bounding box

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tracker_cache` | string | Yes | Tracker cache ID |
| `x` | int | Yes | Bounding box X |
| `y` | int | Yes | Bounding box Y |
| `width` | int | Yes | Bounding box width |
| `height` | int | Yes | Bounding box height |

**Example:**

```vsp
init_tracker("tracker", 100, 100, 50, 50)
```

**Tags:** `tracking`, `init`

---

### `inpaint()`

Restores image areas indicated by mask

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mask_cache` | string | Yes | Inpainting mask cache ID |
| `radius` | float | No | Inpainting radius (default: `RuntimeValue(float: 3)`) |
| `method` | string | No | Method: ns, telea (default: `RuntimeValue(string: "telea")`) |

**Example:**

```vsp
inpaint("mask", 3, "telea")
```

**Tags:** `inpaint`, `restore`

---

### `merge_exposures()`

Merges multiple exposures using Mertens fusion

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `images_cache` | string | Yes | Comma-separated image cache IDs |

**Example:**

```vsp
merge_exposures("exp1,exp2,exp3")
```

**Tags:** `hdr`, `merge`, `exposure`

---

### `multi_tracker()`

Creates multi-object tracker

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tracker_type` | string | No | Tracker type: kcf, csrt, mil (default: `RuntimeValue(string: "kcf")`) |
| `cache_id` | string | No | Multi-tracker cache ID (default: `RuntimeValue(string: "multi_tracker")`) |

**Example:**

```vsp
multi_tracker("csrt", "multi_tracker")
```

**Tags:** `tracking`, `multi`

---

### `pencil_sketch()`

Creates pencil sketch effect

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `sigma_s` | float | No | Spatial sigma (default: `RuntimeValue(float: 60)`) |
| `sigma_r` | float | No | Range sigma (default: `RuntimeValue(float: 0.07)`) |
| `shade_factor` | float | No | Shade factor (default: `RuntimeValue(float: 0.02)`) |
| `colored` | bool | No | Return colored version (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
pencil_sketch(60, 0.07, 0.02, false)
```

**Tags:** `sketch`, `pencil`, `npr`

---

### `seamless_clone()`

Seamlessly clones source into destination

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `src_cache` | string | Yes | Source image cache ID |
| `mask_cache` | string | Yes | Mask cache ID |
| `center_x` | int | Yes | Center X in destination |
| `center_y` | int | Yes | Center Y in destination |
| `mode` | string | No | Mode: normal, mixed, monochrome (default: `RuntimeValue(string: "normal")`) |

**Example:**

```vsp
seamless_clone("src", "mask", 200, 150, "normal")
```

**Tags:** `clone`, `seamless`, `blend`

---

### `stitcher()`

Stitches multiple images into panorama

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `images_cache` | string | Yes | Comma-separated image cache IDs |
| `mode` | string | No | Mode: panorama, scans (default: `RuntimeValue(string: "panorama")`) |

**Example:**

```vsp
stitcher("img1,img2,img3", "panorama")
```

**Tags:** `stitching`, `panorama`

---

### `stylization()`

Applies stylization effect

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `sigma_s` | float | No | Spatial sigma (default: `RuntimeValue(float: 60)`) |
| `sigma_r` | float | No | Range sigma (default: `RuntimeValue(float: 0.45)`) |

**Example:**

```vsp
stylization(60, 0.45)
```

**Tags:** `stylize`, `artistic`, `npr`

---

### `texture_flatting()`

Flattens texture in image region

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mask_cache` | string | Yes | Mask cache ID |
| `low_threshold` | float | No | Low edge threshold (default: `RuntimeValue(float: 30)`) |
| `high_threshold` | float | No | High edge threshold (default: `RuntimeValue(float: 45)`) |
| `kernel_size` | int | No | Gaussian kernel size (default: `RuntimeValue(int: 3)`) |

**Example:**

```vsp
texture_flatting("mask", 30, 45, 3)
```

**Tags:** `texture`, `flatten`

---

### `tonemap()`

Basic tonemapping

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `gamma` | float | No | Gamma value (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
tonemap(2.2)
```

**Tags:** `hdr`, `tonemap`

---

### `tonemap_drago()`

Drago tonemapping

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `gamma` | float | No | Gamma value (default: `RuntimeValue(float: 1)`) |
| `saturation` | float | No | Saturation (default: `RuntimeValue(float: 1)`) |
| `bias` | float | No | Bias (default: `RuntimeValue(float: 0.85)`) |

**Example:**

```vsp
tonemap_drago(1.0, 1.0, 0.85)
```

**Tags:** `hdr`, `tonemap`, `drago`

---

### `tonemap_mantiuk()`

Mantiuk tonemapping

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `gamma` | float | No | Gamma value (default: `RuntimeValue(float: 1)`) |
| `scale` | float | No | Contrast scale (default: `RuntimeValue(float: 0.7)`) |
| `saturation` | float | No | Saturation (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
tonemap_mantiuk(1.0, 0.7, 1.0)
```

**Tags:** `hdr`, `tonemap`, `mantiuk`

---

### `tonemap_reinhard()`

Reinhard tonemapping

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `gamma` | float | No | Gamma value (default: `RuntimeValue(float: 1)`) |
| `intensity` | float | No | Intensity (default: `RuntimeValue(float: 0)`) |
| `light_adapt` | float | No | Light adaptation (default: `RuntimeValue(float: 1)`) |
| `color_adapt` | float | No | Color adaptation (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
tonemap_reinhard(1.0, 0.0, 1.0, 0.0)
```

**Tags:** `hdr`, `tonemap`, `reinhard`

---

### `tracker_csrt()`

Creates CSRT tracker (more accurate but slower)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Tracker cache ID (default: `RuntimeValue(string: "tracker")`) |

**Example:**

```vsp
tracker_csrt("tracker")
```

**Tags:** `tracking`, `csrt`

---

### `tracker_kcf()`

Creates KCF tracker

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Tracker cache ID (default: `RuntimeValue(string: "tracker")`) |

**Example:**

```vsp
tracker_kcf("tracker")
```

**Tags:** `tracking`, `kcf`

---

### `tracker_mil()`

Creates MIL tracker

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Tracker cache ID (default: `RuntimeValue(string: "tracker")`) |

**Example:**

```vsp
tracker_mil("tracker")
```

**Tags:** `tracking`, `mil`

---

### `update_tracker()`

Updates tracker with new frame

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tracker_cache` | string | Yes | Tracker cache ID |
| `draw_bbox` | bool | No | Draw bounding box on image (default: `RuntimeValue(bool: true)`) |

**Example:**

```vsp
update_tracker("tracker", true)
```

**Tags:** `tracking`, `update`

---

## arithmetic

### `cart_to_polar()`

Converts Cartesian coordinates to polar

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `y_cache` | string | Yes | Y component cache |
| `angle_in_degrees` | bool | No | Angle in degrees (default: `RuntimeValue(bool: false)`) |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "polar")`) |

**Example:**

```vsp
cart_to_polar("y", true, "polar")
```

**Tags:** `polar`, `cartesian`, `convert`

---

### `count_non_zero()`

Counts non-zero elements

**Example:**

```vsp
count_non_zero()
```

**Tags:** `count`, `nonzero`

---

### `dct()`

Discrete Cosine Transform

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flags` | string | No | Flags: none, inverse, rows (default: `RuntimeValue(string: "none")`) |

**Example:**

```vsp
dct("none")
```

**Tags:** `dct`, `cosine`, `transform`

---

### `dft()`

Discrete Fourier Transform

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flags` | string | No | Flags: none, inverse, scale, rows, complex_output, real_output (default: `RuntimeValue(string: "none")`) |
| `nonzero_rows` | int | No | Non-zero input rows (0 = all) (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
dft("complex_output", 0)
```

**Tags:** `dft`, `fourier`, `frequency`

---

### `eigen()`

Computes eigenvalues and eigenvectors of symmetric matrix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_prefix` | string | No | Cache prefix for results (default: `RuntimeValue(string: "eigen")`) |

**Example:**

```vsp
eigen("eigen")
```

**Tags:** `matrix`, `eigen`, `eigenvalue`

---

### `exp()`

Calculates element-wise exponential

**Example:**

```vsp
exp()
```

**Tags:** `math`, `exp`

---

### `gemm()`

Generalized matrix multiplication

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `src2_cache` | string | Yes | Second matrix cache ID |
| `alpha` | float | Yes | Alpha multiplier |
| `src3_cache` | string | No | Third matrix cache ID (optional) (default: `RuntimeValue(string: "")`) |
| `beta` | float | No | Beta multiplier for src3 (default: `RuntimeValue(float: 0)`) |
| `flags` | string | No | Flags: none, at, bt, atbt (default: `RuntimeValue(string: "none")`) |

**Example:**

```vsp
gemm("B", 1.0, "C", 1.0, "none")
```

**Tags:** `matrix`, `multiply`, `gemm`

---

### `idct()`

Inverse Discrete Cosine Transform

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flags` | string | No | Flags: none, rows (default: `RuntimeValue(string: "none")`) |

**Example:**

```vsp
idct("none")
```

**Tags:** `dct`, `cosine`, `inverse`

---

### `idft()`

Inverse Discrete Fourier Transform

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flags` | string | No | Flags: none, scale, rows, real_output (default: `RuntimeValue(string: "scale+real_output")`) |
| `nonzero_rows` | int | No | Non-zero input rows (0 = all) (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
idft("scale+real_output", 0)
```

**Tags:** `dft`, `fourier`, `inverse`

---

### `invert()`

Computes matrix inverse

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `method` | string | No | Method: lu, svd, eig, cholesky (default: `RuntimeValue(string: "lu")`) |

**Example:**

```vsp
invert("lu")
```

**Tags:** `matrix`, `inverse`

---

### `ln()`

Calculates element-wise natural logarithm

**Example:**

```vsp
ln()
```

**Tags:** `math`, `log`

---

### `lut()`

Applies lookup table transformation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `lut_cache` | string | Yes | Lookup table cache ID (256 entries) |

**Example:**

```vsp
lut("gamma_table")
```

**Tags:** `lut`, `lookup`, `transform`

---

### `magnitude()`

Calculates magnitude of 2D vectors or complex values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `y_cache` | string | No | Y component cache (if not complex) (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
magnitude("y")
```

**Tags:** `magnitude`, `complex`, `vector`

---

### `mul_spectrums()`

Per-element multiplication of two Fourier spectrums

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `other_cache` | string | Yes | Second spectrum cache ID |
| `flags` | string | No | Flags: none, rows, conj (default: `RuntimeValue(string: "none")`) |

**Example:**

```vsp
mul_spectrums("kernel_dft", "none")
```

**Tags:** `dft`, `multiply`, `spectrum`

---

### `norm()`

Calculates matrix norm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `norm_type` | string | No | Type: l1, l2, linf, l2sqr (default: `RuntimeValue(string: "l2")`) |
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
norm("l2")
```

**Tags:** `norm`, `distance`

---

### `normalize()`

Normalizes array values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `alpha` | float | No | Lower range boundary or norm value (default: `RuntimeValue(float: 0)`) |
| `beta` | float | No | Upper range boundary (default: `RuntimeValue(float: 255)`) |
| `norm_type` | string | No | Type: minmax, l1, l2, linf (default: `RuntimeValue(string: "minmax")`) |
| `dtype` | string | No | Output depth: -1, 8u, 32f (default: `RuntimeValue(string: "-1")`) |
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
normalize(0, 255, "minmax", "8u")
```

**Tags:** `normalize`, `scale`

---

### `phase()`

Calculates phase angle of 2D vectors

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `y_cache` | string | Yes | Y component cache |
| `angle_in_degrees` | bool | No | Return angle in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
phase("y", true)
```

**Tags:** `phase`, `angle`, `vector`

---

### `polar_to_cart()`

Converts polar coordinates to Cartesian

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `angle_cache` | string | Yes | Angle component cache |
| `angle_in_degrees` | bool | No | Angle in degrees (default: `RuntimeValue(bool: false)`) |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "cart")`) |

**Example:**

```vsp
polar_to_cart("angle", true, "cart")
```

**Tags:** `polar`, `cartesian`, `convert`

---

### `pow()`

Raises every element to a power

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `power` | float | Yes | Exponent value |

**Example:**

```vsp
pow(2.0)
```

**Tags:** `math`, `power`

---

### `rand_shuffle()`

Randomly shuffles matrix elements

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `iter_factor` | float | No | Iteration factor (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
rand_shuffle(1.0)
```

**Tags:** `random`, `shuffle`

---

### `randn()`

Fills matrix with normally distributed random values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mean` | float | No | Mean value (default: `RuntimeValue(float: 0)`) |
| `stddev` | float | No | Standard deviation (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
randn(0, 50)
```

**Tags:** `random`, `normal`, `gaussian`

---

### `randu()`

Fills matrix with uniformly distributed random values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `low` | float | No | Lower bound (default: `RuntimeValue(float: 0)`) |
| `high` | float | No | Upper bound (default: `RuntimeValue(float: 255)`) |

**Example:**

```vsp
randu(0, 255)
```

**Tags:** `random`, `uniform`

---

### `reduce()`

Reduces matrix to vector by applying operation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dim` | int | Yes | Dimension to reduce: 0=rows, 1=cols |
| `rtype` | string | No | Operation: sum, avg, max, min (default: `RuntimeValue(string: "sum")`) |
| `dtype` | string | No | Output depth: -1, 32f, 64f (default: `RuntimeValue(string: "-1")`) |

**Example:**

```vsp
reduce(0, "sum", "32f")
```

**Tags:** `reduce`, `aggregate`

---

### `solve()`

Solves linear system Ax = b

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `b_cache` | string | Yes | Right-hand side matrix cache ID |
| `method` | string | No | Method: lu, svd, eig, cholesky, qr, normal (default: `RuntimeValue(string: "lu")`) |
| `cache_id` | string | No | Cache ID for solution (default: `RuntimeValue(string: "solution")`) |

**Example:**

```vsp
solve("b", "lu", "x")
```

**Tags:** `matrix`, `solve`, `linear`

---

### `sqrt()`

Calculates element-wise square root

**Example:**

```vsp
sqrt()
```

**Tags:** `math`, `sqrt`

---

### `sum()`

Calculates sum of all elements

**Example:**

```vsp
sum()
```

**Tags:** `sum`, `aggregate`

---

### `svd()`

Singular Value Decomposition

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flags` | string | No | Flags: none, modify_a, no_uv, full_uv (default: `RuntimeValue(string: "none")`) |
| `cache_prefix` | string | No | Cache prefix for results (default: `RuntimeValue(string: "svd")`) |

**Example:**

```vsp
svd("none", "svd")
```

**Tags:** `matrix`, `svd`, `decomposition`

---

### `transform()`

Applies matrix transformation to each pixel

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `matrix_cache` | string | Yes | Transformation matrix cache ID |

**Example:**

```vsp
transform("color_transform")
```

**Tags:** `transform`, `matrix`

---

## color

### `apply_colormap()`

Applies a colormap to grayscale image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `colormap` | string | No | Colormap: autumn, bone, jet, winter, rainbow, ocean, summer, spring, cool, hsv, pink, hot, parula, magma, inferno, plasma, viridis, cividis, twilight, twilight_shifted, turbo, deepgreen (default: `RuntimeValue(string: "jet")`) |

**Example:**

```vsp
apply_colormap("jet") | apply_colormap("viridis")
```

**Tags:** `colormap`, `visualization`, `pseudocolor`

---

### `brightness_contrast()`

Adjusts brightness and contrast

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `brightness` | float | No | Brightness adjustment (-100 to 100) (default: `RuntimeValue(float: 0)`) |
| `contrast` | float | No | Contrast multiplier (0.0 to 3.0) (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
brightness_contrast(10, 1.2)
```

**Tags:** `brightness`, `contrast`, `adjust`

---

### `calc_back_project()`

Calculates back projection of a histogram

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `hist` | string | Yes | Cache ID of histogram |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
calc_back_project("hist")
```

**Tags:** `histogram`, `backproject`, `probability`

---

### `calc_hist()`

Calculates histogram of the image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `channel` | int | No | Channel to calculate (default: 0) (default: `RuntimeValue(int: 0)`) |
| `hist_size` | int | No | Number of histogram bins (default: `RuntimeValue(int: 256)`) |
| `range_min` | float | No | Minimum range value (default: `RuntimeValue(float: 0)`) |
| `range_max` | float | No | Maximum range value (default: `RuntimeValue(float: 256)`) |

**Example:**

```vsp
calc_hist(0, 256)
```

**Tags:** `histogram`, `calculate`, `analyze`

---

### `clahe()`

Applies CLAHE (Contrast Limited Adaptive Histogram Equalization)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `clip_limit` | float | No | Contrast limit threshold (default: `RuntimeValue(float: 2)`) |
| `tile_grid_size` | int | No | Size of grid for tiles (default: `RuntimeValue(int: 8)`) |

**Example:**

```vsp
clahe(2.0, 8)
```

**Tags:** `histogram`, `clahe`, `contrast`, `adaptive`

---

### `compare_hist()`

Compares two histograms

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `other` | string | Yes | Cache ID of other histogram |
| `method` | string | No | Comparison method: correlation, chi_square, intersection, bhattacharyya, kl_div (default: `RuntimeValue(string: "correlation")`) |

**Example:**

```vsp
compare_hist("hist2", "correlation")
```

**Tags:** `histogram`, `compare`, `similarity`

---

### `cvt_color()`

Converts color space of the current frame

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `code` | string | Yes | Conversion code: bgr2gray, gray2bgr, bgr2rgb, rgb2bgr, bgr2hsv, hsv2bgr, bgr2hls, hls2bgr, bgr2lab, lab2bgr, bgr2luv, luv2bgr, bgr2xyz, xyz2bgr, bgr2ycrcb, ycrcb2bgr, bgr2yuv, yuv2bgr |

**Example:**

```vsp
cvt_color("bgr2gray") | cvt_color("bgr2hsv")
```

**Tags:** `color`, `convert`, `space`

---

### `equalize_hist()`

Equalizes histogram of grayscale image or Y channel of color

**Example:**

```vsp
equalize_hist()
```

**Tags:** `histogram`, `equalize`, `contrast`

---

### `extract_channel()`

Extracts a single channel from the image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `channel` | int | Yes | Channel index (0, 1, 2, ...) |

**Example:**

```vsp
extract_channel(0)  // Extract blue channel
```

**Tags:** `color`, `channel`, `extract`

---

### `gamma()`

Applies gamma correction

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `gamma` | float | No | Gamma value (>1 darkens, <1 brightens) (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
gamma(0.5)  // Brighten | gamma(2.0)  // Darken
```

**Tags:** `gamma`, `correction`, `adjust`

---

### `in_range()`

Checks if array elements lie between two values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `lower` | array | Yes | Lower bound [B, G, R] or scalar |
| `upper` | array | Yes | Upper bound [B, G, R] or scalar |

**Example:**

```vsp
in_range([100, 100, 100], [255, 255, 255])
```

**Tags:** `range`, `threshold`, `mask`

---

### `merge_channels()`

Merges separate channels into a multi-channel image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `channels` | array | Yes | Array of cache IDs for channels |

**Example:**

```vsp
merge_channels(["ch_0", "ch_1", "ch_2"])
```

**Tags:** `color`, `channel`, `merge`

---

### `mix_channels()`

Copies specified channels from input to output

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `from_to` | array | Yes | Pairs of [src_channel, dst_channel] |
| `output_channels` | int | No | Number of output channels (default: `RuntimeValue(int: 3)`) |

**Example:**

```vsp
mix_channels([0, 2, 2, 0])  // Swap B and R
```

**Tags:** `color`, `channel`, `mix`, `copy`

---

### `split_channels()`

Splits image into separate channels and caches them

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `prefix` | string | No | Cache prefix for channels (default: `RuntimeValue(string: "ch")`) |

**Example:**

```vsp
split_channels("bgr")  // Creates cache: bgr_0, bgr_1, bgr_2
```

**Tags:** `color`, `channel`, `split`

---

### `weighted_gray()`

Converts to grayscale with custom channel weights

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `w_b` | float | No | Blue channel weight (default: `RuntimeValue(float: 0.114)`) |
| `w_g` | float | No | Green channel weight (default: `RuntimeValue(float: 0.587)`) |
| `w_r` | float | No | Red channel weight (default: `RuntimeValue(float: 0.299)`) |

**Example:**

```vsp
weighted_gray(0.11, 0.59, 0.30)
```

**Tags:** `grayscale`, `convert`, `weighted`

---

## conditional

### `cache_if()`

Caches current Mat only if condition is true

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Boolean condition value or variable |
| `cache_id` | string | Yes | Cache identifier |

**Example:**

```vsp
cache_if(is_valid, "good_frame")
```

**Tags:** `conditional`, `cache`, `mat`

---

### `display_if()`

Displays Mat only if condition is true

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Boolean condition value or variable |
| `window_name` | string | No | Window name (default: `RuntimeValue(string: "Output")`) |

**Example:**

```vsp
display_if(show_debug, "Debug View")
```

**Tags:** `conditional`, `display`, `window`

---

### `is_defined()`

Checks if variable exists

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `var_name` | string | Yes | Variable name to check |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
is_defined("threshold", "has_threshold")
```

**Tags:** `conditional`, `check`, `variable`

---

### `is_even()`

Checks if value is even integer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | int | Yes | Value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
is_even(frame_count, "is_even_frame")
```

**Tags:** `conditional`, `check`, `even`

---

### `is_negative()`

Checks if value is negative (< 0)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
is_negative(diff, "is_shrinking")
```

**Tags:** `conditional`, `check`, `negative`

---

### `is_odd()`

Checks if value is odd integer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | int | Yes | Value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
is_odd(frame_count, "is_odd_frame")
```

**Tags:** `conditional`, `check`, `odd`

---

### `is_positive()`

Checks if value is positive (> 0)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
is_positive(diff, "is_growing")
```

**Tags:** `conditional`, `check`, `positive`

---

### `is_zero()`

Checks if value is zero

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
is_zero(counter, "is_start")
```

**Tags:** `conditional`, `check`, `zero`

---

### `logical_and()`

Logical AND of two boolean values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | bool | Yes | First boolean value or variable name |
| `b` | bool | Yes | Second boolean value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
logical_and(cond_a, cond_b, "both_true")
```

**Tags:** `conditional`, `logic`, `and`

---

### `logical_not()`

Logical NOT of a boolean value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | bool | Yes | Boolean value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
logical_not(is_enabled, "is_disabled")
```

**Tags:** `conditional`, `logic`, `not`

---

### `logical_or()`

Logical OR of two boolean values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | bool | Yes | First boolean value or variable name |
| `b` | bool | Yes | Second boolean value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
logical_or(cond_a, cond_b, "any_true")
```

**Tags:** `conditional`, `logic`, `or`

---

### `logical_xor()`

Logical XOR of two boolean values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | bool | Yes | First boolean value or variable name |
| `b` | bool | Yes | Second boolean value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
logical_xor(cond_a, cond_b, "one_true")
```

**Tags:** `conditional`, `logic`, `xor`

---

### `scalar_compare()`

Compares two values and stores boolean result

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | First value or variable name |
| `op` | string | Yes | Operator: ==, !=, <, <=, >, >= |
| `b` | float | Yes | Second value or variable name |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
scalar_compare(threshold, ">", 100, "is_high")
```

**Tags:** `conditional`, `compare`, `logic`

---

### `scalar_in_range()`

Checks if value is within range [min, max]

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Value to check or variable name |
| `min` | float | Yes | Minimum value |
| `max` | float | Yes | Maximum value |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
scalar_in_range(threshold, 50, 200, "is_valid")
```

**Tags:** `conditional`, `range`, `check`

---

### `select()`

Sets variable to one of two values based on condition

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Boolean condition value or variable |
| `true_value` | any | Yes | Value if condition is true |
| `false_value` | any | Yes | Value if condition is false |
| `result_var` | string | Yes | Variable name for result |

**Example:**

```vsp
select(is_valid, 255, 0, "output_value")
```

**Tags:** `conditional`, `select`, `ternary`

---

### `set_if()`

Sets variable only if condition is true

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Boolean condition value or variable |
| `var_name` | string | Yes | Variable name to set |
| `value` | any | Yes | Value to set |

**Example:**

```vsp
set_if(is_valid, "threshold", 128)
```

**Tags:** `conditional`, `set`, `variable`

---

### `skip_if_false()`

Sets skip count if condition is false (used with pipeline interpreter)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Boolean condition value or variable |
| `count` | int | No | Number of items to skip (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
skip_if_false(is_enabled, 3)
```

**Tags:** `conditional`, `skip`, `control`

---

### `skip_if_true()`

Sets skip count if condition is true (used with pipeline interpreter)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Boolean condition value or variable |
| `count` | int | No | Number of items to skip (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
skip_if_true(is_disabled, 3)
```

**Tags:** `conditional`, `skip`, `control`

---

### `toggle_var()`

Toggles a boolean variable

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `var_name` | string | Yes | Boolean variable name to toggle |

**Example:**

```vsp
toggle_var("is_enabled")
```

**Tags:** `conditional`, `toggle`, `boolean`

---

### `trigger_every()`

Triggers true every N increments of counter

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `counter_var` | string | Yes | Counter variable name |
| `interval` | int | Yes | Interval for triggering |
| `result_var` | string | Yes | Variable name for trigger result |

**Example:**

```vsp
trigger_every("frame_count", 10, "every_10th")
```

**Tags:** `conditional`, `trigger`, `interval`

---

### `trigger_on_change()`

Triggers true once when value changes

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Current value or variable |
| `state_var` | string | Yes | Variable to track previous value |
| `result_var` | string | Yes | Variable name for trigger result |

**Example:**

```vsp
trigger_on_change(slider_value, "_prev_slider", "slider_changed")
```

**Tags:** `conditional`, `trigger`, `change`

---

### `trigger_on_falling()`

Triggers true once when condition changes from true to false

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Current condition value or variable |
| `state_var` | string | Yes | Variable to track previous state |
| `result_var` | string | Yes | Variable name for trigger result |

**Example:**

```vsp
trigger_on_falling(button_pressed, "_prev_button", "just_released")
```

**Tags:** `conditional`, `trigger`, `falling`, `edge`

---

### `trigger_on_rising()`

Triggers true once when condition changes from false to true

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Current condition value or variable |
| `state_var` | string | Yes | Variable to track previous state |
| `result_var` | string | Yes | Variable name for trigger result |

**Example:**

```vsp
trigger_on_rising(button_pressed, "_prev_button", "just_pressed")
```

**Tags:** `conditional`, `trigger`, `rising`, `edge`

---

### `use_if()`

Loads cached Mat only if condition is true

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Boolean condition value or variable |
| `cache_id` | string | Yes | Cache identifier |

**Example:**

```vsp
use_if(need_previous, "previous_frame")
```

**Tags:** `conditional`, `cache`, `load`

---

## control

### `absdiff()`

Calculates absolute difference between mats

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID or scalar value |

**Example:**

```vsp
absdiff("background")
```

**Tags:** `arithmetic`, `absdiff`

---

### `add()`

Adds current mat with another mat or scalar

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID or scalar value |
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |
| `dtype` | string | No | Output depth (default: `RuntimeValue(string: "-1")`) |

**Example:**

```vsp
add("other_img")
```

**Tags:** `arithmetic`, `add`

---

### `add_weighted()`

Weighted sum of two mats

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `alpha` | float | Yes | Weight for current mat |
| `other_cache` | string | Yes | Second image cache ID |
| `beta` | float | Yes | Weight for second mat |
| `gamma` | float | No | Added scalar (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
add_weighted(0.7, "overlay", 0.3, 0)
```

**Tags:** `arithmetic`, `blend`, `weighted`

---

### `bitwise_and()`

Bitwise AND operation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID for second operand |
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
bitwise_and("mask")
```

**Tags:** `bitwise`, `and`

---

### `bitwise_not()`

Bitwise NOT (inversion) operation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
bitwise_not()
```

**Tags:** `bitwise`, `not`, `invert`

---

### `bitwise_or()`

Bitwise OR operation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID for second operand |
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
bitwise_or("mask")
```

**Tags:** `bitwise`, `or`

---

### `bitwise_xor()`

Bitwise XOR operation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID for second operand |
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
bitwise_xor("mask")
```

**Tags:** `bitwise`, `xor`

---

### `break()`

Breaks out of current loop (exec_loop, while)

**Example:**

```vsp
break()
```

**Tags:** `control`, `break`, `loop`

---

### `cache()`

Stores current image in cache with given ID

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | Yes | Cache identifier |

**Example:**

```vsp
cache("my_image")
```

**Tags:** `cache`, `store`, `memory`

---

### `clear_cache()`

Clears specified cache entry or all cache

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID to clear (empty = all) (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
clear_cache("my_image")
```

**Tags:** `cache`, `clear`, `memory`

---

### `clone()`

Creates a deep copy of current mat

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID for clone (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
clone("backup")
```

**Tags:** `mat`, `clone`, `copy`

---

### `continue()`

Continues to next loop iteration

**Example:**

```vsp
continue()
```

**Tags:** `control`, `continue`

---

### `convert_to()`

Converts mat to different depth with optional scaling

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `depth` | string | Yes | Target depth: 8u, 8s, 16u, 16s, 32s, 32f, 64f |
| `alpha` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |
| `beta` | float | No | Offset (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
convert_to("32f", 1.0/255.0, 0)
```

**Tags:** `mat`, `convert`, `depth`

---

### `copy_cache()`

Copies one cache entry to another

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Source cache ID |
| `dest` | string | Yes | Destination cache ID |

**Example:**

```vsp
copy_cache("src", "dst")
```

**Tags:** `cache`, `copy`

---

### `copy_to()`

Copies current mat to destination with optional mask

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dest_cache` | string | Yes | Destination cache ID |
| `mask_cache` | string | No | Optional mask cache ID (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
copy_to("dest", "mask")
```

**Tags:** `mat`, `copy`

---

### `create_mat()`

Creates a new mat with specified dimensions and type

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `rows` | int | Yes | Number of rows |
| `cols` | int | Yes | Number of columns |
| `type` | string | No | Type: 8uc1, 8uc3, 32fc1, etc. (default: `RuntimeValue(string: "8uc3")`) |
| `value` | float | No | Initial value (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
create_mat(480, 640, "8uc3", 0)
```

**Tags:** `mat`, `create`

---

### `decrement_var()`

Decrements a variable by specified amount

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Variable name |
| `amount` | float | No | Decrement amount (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
decrement_var("counter", 1)
```

**Tags:** `variable`, `decrement`

---

### `divide()`

Element-wise division

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID or scalar value |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
divide("normalizer", 1.0)
```

**Tags:** `arithmetic`, `divide`

---

### `eye()`

Creates an identity matrix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `size` | int | Yes | Matrix size (square) |
| `type` | string | No | Type: 32fc1, 64fc1 (default: `RuntimeValue(string: "32fc1")`) |

**Example:**

```vsp
eye(3, "64fc1")
```

**Tags:** `mat`, `identity`, `create`

---

### `get_var()`

Gets a variable value and prints it

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Variable name |

**Example:**

```vsp
get_var("threshold")
```

**Tags:** `variable`, `get`

---

### `global_cache()`

Stores current image in GLOBAL cache with given ID

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | Yes | Cache identifier |

**Example:**

```vsp
global_cache("my_image")
```

**Tags:** `cache`, `store`, `memory`, `global`

---

### `if()`

Conditional execution (requires interpreter support)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | string | Yes | Condition expression |

**Example:**

```vsp
if("$counter > 5")
```

**Tags:** `control`, `conditional`

---

### `increment_var()`

Increments a variable by specified amount

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Variable name |
| `amount` | float | No | Increment amount (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
increment_var("counter", 1)
```

**Tags:** `variable`, `increment`

---

### `list_cache()`

Lists all cache entries with sizes

**Example:**

```vsp
list_cache()
```

**Tags:** `cache`, `list`, `debug`

---

### `load_cache()`

Loads image from cache to current mat

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | Yes | Cache identifier to load |

**Example:**

```vsp
load_cache("my_image")
```

**Tags:** `cache`, `load`, `memory`

---

### `loop()`

Loop execution (requires interpreter support)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `count` | int | Yes | Number of iterations |

**Example:**

```vsp
loop(10)
```

**Tags:** `control`, `loop`

---

### `multiply()`

Element-wise multiplication

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID or scalar value |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
multiply("mask", 1.0)
```

**Tags:** `arithmetic`, `multiply`

---

### `ones()`

Creates a mat filled with ones

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `rows` | int | Yes | Number of rows |
| `cols` | int | Yes | Number of columns |
| `type` | string | No | Type: 8uc1, 8uc3, 32fc1, etc. (default: `RuntimeValue(string: "32fc1")`) |

**Example:**

```vsp
ones(480, 640, "32fc1")
```

**Tags:** `mat`, `ones`, `create`

---

### `promote_global()`

Promotes a local cache entry to global scope

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | Yes | Cache identifier to promote |

**Example:**

```vsp
promote_global("my_image")
```

**Tags:** `cache`, `global`, `promote`

---

### `reshape()`

Changes mat shape without copying data

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `channels` | int | Yes | New number of channels |
| `rows` | int | No | New number of rows (0 = auto) (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
reshape(1, 0)
```

**Tags:** `mat`, `reshape`

---

### `return()`

Returns from pipeline execution

**Example:**

```vsp
return()
```

**Tags:** `control`, `return`

---

### `roi()`

Extracts region of interest from current mat

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | X coordinate |
| `y` | int | Yes | Y coordinate |
| `width` | int | Yes | Width |
| `height` | int | Yes | Height |

**Example:**

```vsp
roi(100, 100, 200, 200)
```

**Tags:** `mat`, `roi`, `crop`

---

### `scale_add()`

Scaled addition: dst = scale*src1 + src2

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `scale` | float | Yes | Scale for current mat |
| `other_cache` | string | Yes | Second image cache ID |

**Example:**

```vsp
scale_add(0.5, "other")
```

**Tags:** `arithmetic`, `scale`

---

### `set_to()`

Sets mat pixels to specified value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Scalar value to set |
| `mask_cache` | string | No | Optional mask cache ID (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
set_to(0, "mask")
```

**Tags:** `mat`, `set`

---

### `set_var()`

Sets a variable in the execution context

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Variable name |
| `value` | float | Yes | Variable value |

**Example:**

```vsp
set_var("threshold", 127)
```

**Tags:** `variable`, `set`

---

### `subtract()`

Subtracts another mat or scalar from current mat

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `operand` | string | Yes | Cache ID or scalar value |
| `mask_cache` | string | No | Optional mask (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
subtract("background")
```

**Tags:** `arithmetic`, `subtract`

---

### `swap_cache()`

Swaps two cache entries

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache1` | string | Yes | First cache ID |
| `cache2` | string | Yes | Second cache ID |

**Example:**

```vsp
swap_cache("img1", "img2")
```

**Tags:** `cache`, `swap`

---

### `zeros()`

Creates a zero-filled mat

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `rows` | int | Yes | Number of rows |
| `cols` | int | Yes | Number of columns |
| `type` | string | No | Type: 8uc1, 8uc3, 32fc1, etc. (default: `RuntimeValue(string: "8uc3")`) |

**Example:**

```vsp
zeros(480, 640, "8uc3")
```

**Tags:** `mat`, `zeros`, `create`

---

## display

### `assert()`

Asserts condition and stops if false

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `condition` | bool | Yes | Condition to check |
| `message` | string | No | Error message (default: `RuntimeValue(string: "Assertion failed")`) |

**Example:**

```vsp
assert(true, "Mat should not be empty")
```

**Tags:** `assert`, `check`, `validation`

---

### `create_trackbar()`

Creates a trackbar and attaches it to window

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Trackbar name |
| `window` | string | Yes | Window name |
| `max_value` | int | Yes | Maximum value |
| `initial` | int | No | Initial value (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
create_trackbar("Threshold", "Output", 255, 128)
```

**Tags:** `trackbar`, `slider`, `gui`

---

### `debug()`

Prints debug information about current Mat

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `label` | string | No | Debug label (default: `RuntimeValue(string: "Mat")`) |

**Example:**

```vsp
debug("After blur")
```

**Tags:** `debug`, `info`, `inspect`

---

### `destroy_all_windows()`

Destroys all HighGUI windows

**Example:**

```vsp
destroy_all_windows()
```

**Tags:** `window`, `destroy`, `all`, `gui`

---

### `destroy_window()`

Destroys specified window

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Window name |

**Example:**

```vsp
destroy_window("Output")
```

**Tags:** `window`, `destroy`, `gui`

---

### `elapsed_time()`

Calculates elapsed time from cached tick

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tick_cache` | string | Yes | Cache ID of start tick |
| `unit` | string | No | Unit: ms, s, us (default: `RuntimeValue(string: "ms")`) |
| `print` | bool | No | Print result (default: `RuntimeValue(bool: true)`) |

**Example:**

```vsp
elapsed_time("start_tick", "ms", true)
```

**Tags:** `elapsed`, `timing`, `performance`

---

### `fps()`

Calculates and displays FPS

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `draw` | bool | No | Draw FPS on image (default: `RuntimeValue(bool: true)`) |
| `x` | int | No | Text X position (default: `RuntimeValue(int: 10)`) |
| `y` | int | No | Text Y position (default: `RuntimeValue(int: 30)`) |

**Example:**

```vsp
fps(true, 10, 30)
```

**Tags:** `fps`, `performance`, `display`

---

### `get_channels()`

Gets number of image channels

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "channels")`) |

**Example:**

```vsp
get_channels("ch")
```

**Tags:** `channels`, `info`

---

### `get_depth()`

Gets image depth (CV_8U, CV_32F, etc)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "depth")`) |

**Example:**

```vsp
get_depth("d")
```

**Tags:** `depth`, `type`, `info`

---

### `get_height()`

Gets image height

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "height")`) |

**Example:**

```vsp
get_height("h")
```

**Tags:** `height`, `size`, `info`

---

### `get_size()`

Gets image size (width, height)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_prefix` | string | No | Cache prefix (default: `RuntimeValue(string: "size")`) |

**Example:**

```vsp
get_size("img")
```

**Tags:** `size`, `dimensions`, `info`

---

### `get_tick()`

Gets current tick count for timing

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID for tick (default: `RuntimeValue(string: "tick")`) |

**Example:**

```vsp
get_tick("start_tick")
```

**Tags:** `tick`, `timing`, `performance`

---

### `get_trackbar_pos()`

Gets current trackbar position

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Trackbar name |
| `window` | string | Yes | Window name |
| `cache_id` | string | No | Cache ID for value (default: `RuntimeValue(string: "trackbar_val")`) |

**Example:**

```vsp
get_trackbar_pos("Threshold", "Output", "thresh")
```

**Tags:** `trackbar`, `value`, `get`

---

### `get_type()`

Gets image type (CV_8UC3, etc)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "type")`) |

**Example:**

```vsp
get_type("t")
```

**Tags:** `type`, `info`

---

### `get_width()`

Gets image width

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "width")`) |

**Example:**

```vsp
get_width("w")
```

**Tags:** `width`, `size`, `info`

---

### `imshow()`

Displays image in a window

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | No | Window name (default: `RuntimeValue(string: "Output")`) |

**Example:**

```vsp
imshow("Result")
```

**Tags:** `display`, `show`, `window`

---

### `log()`

Logs message with level

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `level` | string | Yes | Level: info, warn, error, debug |
| `message` | string | Yes | Log message |

**Example:**

```vsp
log("info", "Pipeline started")
```

**Tags:** `log`, `console`, `debug`

---

### `move_window()`

Moves window to specified position

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Window name |
| `x` | int | Yes | X position |
| `y` | int | Yes | Y position |

**Example:**

```vsp
move_window("Output", 100, 100)
```

**Tags:** `window`, `move`, `position`, `gui`

---

### `named_window()`

Creates a named window

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Window name |
| `flags` | string | No | Flags: normal, autosize, fullscreen, freeratio, keepratio (default: `RuntimeValue(string: "autosize")`) |

**Example:**

```vsp
named_window("Output", "normal")
```

**Tags:** `window`, `display`, `gui`

---

### `poll_key()`

Polls for key without blocking and returns the key code

**Example:**

```vsp
key = poll_key()
```

**Tags:** `input`, `key`, `poll`

---

### `print()`

Prints message to console

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `message` | string | Yes | Message to print |

**Example:**

```vsp
print("Processing complete")
```

**Tags:** `print`, `console`, `output`

---

### `resize_window()`

Resizes window to specified size

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Window name |
| `width` | int | Yes | Width |
| `height` | int | Yes | Height |

**Example:**

```vsp
resize_window("Output", 640, 480)
```

**Tags:** `window`, `resize`, `size`, `gui`

---

### `set_trackbar_pos()`

Sets trackbar position

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Trackbar name |
| `window` | string | Yes | Window name |
| `pos` | int | Yes | Position value |

**Example:**

```vsp
set_trackbar_pos("Threshold", "Output", 128)
```

**Tags:** `trackbar`, `value`, `set`

---

### `set_window_title()`

Sets window title

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Window name |
| `title` | string | Yes | New title |

**Example:**

```vsp
set_window_title("Output", "Processed Image")
```

**Tags:** `window`, `title`, `gui`

---

### `start_timer()`

Starts a named timer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | No | Timer name (default: `RuntimeValue(string: "default")`) |

**Example:**

```vsp
start_timer("processing")
```

**Tags:** `timer`, `start`, `performance`

---

### `stop_timer()`

Stops timer and reports elapsed time

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | No | Timer name (default: `RuntimeValue(string: "default")`) |
| `print` | bool | No | Print result (default: `RuntimeValue(bool: true)`) |

**Example:**

```vsp
stop_timer("processing", true)
```

**Tags:** `timer`, `stop`, `performance`

---

### `wait_key()`

Waits for a key press and returns the key code

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `delay` | int | No | Delay in ms (0=infinite) (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
key = wait_key(30)
```

**Tags:** `input`, `key`, `wait`

---

## dnn

### `blob_from_image()`

Create DNN blob from image for inference

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `scale` | float | No | Scale factor (default: 1/255) (default: `RuntimeValue(float: 0.00392157)`) |
| `width` | int | No | Target width (0 = original) (default: `RuntimeValue(int: 0)`) |
| `height` | int | No | Target height (0 = original) (default: `RuntimeValue(int: 0)`) |
| `swap_rb` | bool | No | Swap R and B channels (default: `RuntimeValue(bool: true)`) |

**Example:**

```vsp
blob_from_image(1/255.0, 640, 640, true)
```

**Tags:** `preprocess`, `blob`, `dnn`

---

### `decode_yolo()`

Decode YOLO model output to detections

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `version` | string | No | YOLO version: v5, v8, v10, v11 (default: `RuntimeValue(string: "v8")`) |
| `conf_thresh` | float | No | Confidence threshold (default: `RuntimeValue(float: 0.25)`) |
| `orig_width` | int | No | Original image width (default: `RuntimeValue(int: 0)`) |
| `orig_height` | int | No | Original image height (default: `RuntimeValue(int: 0)`) |
| `input_size` | int | No | Model input size (default: `RuntimeValue(int: 640)`) |

**Example:**

```vsp
decode_yolo("v8", 0.25) -> "detections"
```

**Tags:** `postprocess`, `yolo`, `detection`, `dnn`

---

### `draw_classification()`

Draw classification results on the current frame

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `predictions` | string | Yes | Cache ID of predictions matrix |
| `labels` | string | No | Path to labels file (default: `RuntimeValue(string: "")`) |
| `position` | string | No | Text position: top-left, top-right, bottom-left, bottom-right (default: `RuntimeValue(string: "top-left")`) |

**Example:**

```vsp
draw_classification("predictions", labels="imagenet.txt")
```

**Tags:** `draw`, `classification`, `visualization`, `dnn`

---

### `draw_detections()`

Draw detection boxes on the current frame

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `detections` | string | Yes | Cache ID of detections matrix |
| `labels` | string | No | Path to labels file (default: `RuntimeValue(string: "")`) |
| `show_conf` | bool | No | Show confidence scores (default: `RuntimeValue(bool: true)`) |
| `thickness` | int | No | Box line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
draw_detections("detections", labels="coco.names")
```

**Tags:** `draw`, `detection`, `visualization`, `dnn`

---

### `letterbox()`

Resize image with letterbox (maintain aspect ratio with padding)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `width` | int | Yes | Target width |
| `height` | int | Yes | Target height |
| `pad_color` | int | No | Padding color (grayscale 0-255) (default: `RuntimeValue(int: 114)`) |

**Example:**

```vsp
letterbox(640, 640)
```

**Tags:** `preprocess`, `resize`, `letterbox`, `dnn`

---

### `list_models()`

List all loaded models

**Example:**

```vsp
list_models()
```

**Tags:** `model`, `list`, `dnn`

---

### `load_model()`

Load an ML model for inference

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Unique identifier for the model |
| `path` | string | Yes | Path to model file |
| `backend` | string | No | Backend: opencv, onnx, tensorrt (default: `RuntimeValue(string: "opencv")`) |
| `device` | string | No | Device: cpu, cuda, cuda:0, opencl (default: `RuntimeValue(string: "cpu")`) |
| `input_width` | int | No | Input width (0 = auto) (default: `RuntimeValue(int: 0)`) |
| `input_height` | int | No | Input height (0 = auto) (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
load_model("yolo", "models/yolov8n.onnx", backend="opencv", device="cuda")
```

**Tags:** `model`, `load`, `dnn`, `ml`, `inference`

---

### `model_get_outputs()`

Get all DNN outputs as array of [name, matrix] pairs

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Model identifier |

**Example:**

```vsp
model_get_outputs("yolo")
```

**Tags:** `model`, `output`, `all`, `tensor`, `dnn`

---

### `model_infer()`

Run inference on the current frame using a loaded model

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Model identifier |

**Example:**

```vsp
model_infer("yolo") -> "output"
```

**Tags:** `model`, `inference`, `dnn`, `forward`

---

### `model_info()`

Get information about a loaded model

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Model identifier |

**Example:**

```vsp
model_info("yolo")
```

**Tags:** `model`, `info`, `dnn`

---

### `model_output()`

Get specific output layer from model inference

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Model identifier |
| `layer` | string | Yes | Output layer name |

**Example:**

```vsp
model_output("yolo", "output0")
```

**Tags:** `model`, `output`, `dnn`

---

### `model_output_matrix()`

Get raw DNN output as Matrix (2D tensor)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Model identifier |
| `layer` | any | No | Layer name or index (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
model_output_matrix("detector", "output0") -> my_matrix
```

**Tags:** `model`, `output`, `matrix`, `tensor`, `dnn`

---

### `model_output_vector()`

Get raw DNN output as Vector (1D tensor)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Model identifier |
| `layer` | any | No | Layer name or index (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
model_output_vector("classifier", 0) -> my_vector
```

**Tags:** `model`, `output`, `vector`, `tensor`, `dnn`

---

### `nms_boxes()`

Apply Non-Maximum Suppression to detections

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `iou_thresh` | float | No | IoU threshold for NMS (default: `RuntimeValue(float: 0.45)`) |
| `conf_thresh` | float | No | Confidence threshold (default: `RuntimeValue(float: 0.25)`) |

**Example:**

```vsp
nms_boxes(0.45, 0.25)
```

**Tags:** `postprocess`, `nms`, `detection`, `dnn`

---

### `normalize_mean_std()`

Normalize image with mean and std (ImageNet-style)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mean` | array | Yes | Per-channel mean values [R, G, B] |
| `std` | array | Yes | Per-channel std values [R, G, B] |

**Example:**

```vsp
normalize_mean_std([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
```

**Tags:** `preprocess`, `normalize`, `dnn`

---

### `sigmoid()`

Apply sigmoid activation

**Example:**

```vsp
sigmoid()
```

**Tags:** `tensor`, `activation`, `sigmoid`, `dnn`

---

### `softmax()`

Apply softmax activation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `axis` | int | No | Axis for softmax (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
softmax()
```

**Tags:** `tensor`, `activation`, `softmax`, `dnn`

---

### `topk()`

Get top-k predictions from classification output

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `k` | int | No | Number of top predictions (default: `RuntimeValue(int: 5)`) |

**Example:**

```vsp
topk(5) -> "predictions"
```

**Tags:** `tensor`, `classification`, `topk`, `dnn`

---

### `unload_model()`

Unload a previously loaded model

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | string | Yes | Model identifier to unload |

**Example:**

```vsp
unload_model("yolo")
```

**Tags:** `model`, `unload`, `dnn`

---

## draw

### `add_border()`

Adds border around image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `top` | int | No | Top border (default: `RuntimeValue(int: 10)`) |
| `bottom` | int | No | Bottom border (default: `RuntimeValue(int: 10)`) |
| `left` | int | No | Left border (default: `RuntimeValue(int: 10)`) |
| `right` | int | No | Right border (default: `RuntimeValue(int: 10)`) |
| `type` | string | No | Type: constant, replicate, reflect, wrap, reflect_101 (default: `RuntimeValue(string: "constant")`) |
| `color` | string | No | Border color (for constant) (default: `RuntimeValue(string: "black")`) |

**Example:**

```vsp
add_border(10, 10, 10, 10, "constant", "white")
```

**Tags:** `border`, `padding`, `frame`

---

### `arrowed_line()`

Draws an arrow

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x1` | int | Yes | Start X |
| `y1` | int | Yes | Start Y |
| `x2` | int | Yes | End X (arrow head) |
| `y2` | int | Yes | End Y (arrow head) |
| `color` | string | No | Arrow color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 1)`) |
| `tip_length` | float | No | Arrow tip length ratio (default: `RuntimeValue(float: 0.1)`) |

**Example:**

```vsp
arrowed_line(0, 0, 100, 100, "red", 2, 0.1)
```

**Tags:** `draw`, `arrow`, `line`

---

### `circle()`

Draws a circle

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | Center X |
| `y` | int | Yes | Center Y |
| `radius` | int | Yes | Radius |
| `color` | string | No | Circle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (-1 for filled) (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
circle(100, 100, 50, "blue", -1)
```

**Tags:** `draw`, `circle`, `primitive`

---

### `color_overlay()`

Applies color overlay to image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `color` | string | No | Overlay color (default: `RuntimeValue(string: "blue")`) |
| `alpha` | float | No | Opacity (0-1) (default: `RuntimeValue(float: 0.3)`) |

**Example:**

```vsp
color_overlay("blue", 0.3)
```

**Tags:** `overlay`, `color`, `tint`

---

### `draw_crosshair()`

Draws crosshair at center or specified position

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | No | Center X (-1 for image center) (default: `RuntimeValue(int: -1)`) |
| `y` | int | No | Center Y (-1 for image center) (default: `RuntimeValue(int: -1)`) |
| `size` | int | No | Crosshair size (default: `RuntimeValue(int: 20)`) |
| `color` | string | No | Crosshair color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 1)`) |
| `gap` | int | No | Center gap (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
draw_crosshair(-1, -1, 30, "red", 2, 5)
```

**Tags:** `draw`, `crosshair`, `center`

---

### `draw_grid()`

Draws grid lines on image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cell_width` | int | No | Grid cell width (default: `RuntimeValue(int: 50)`) |
| `cell_height` | int | No | Grid cell height (default: `RuntimeValue(int: 50)`) |
| `color` | string | No | Grid color (default: `RuntimeValue(string: "gray")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
draw_grid(50, 50, "gray", 1)
```

**Tags:** `draw`, `grid`, `overlay`

---

### `draw_histogram()`

Draws histogram visualization

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `width` | int | No | Histogram width (default: `RuntimeValue(int: 256)`) |
| `height` | int | No | Histogram height (default: `RuntimeValue(int: 200)`) |
| `color` | string | No | Histogram color (default: `RuntimeValue(string: "white")`) |
| `background` | string | No | Background color (default: `RuntimeValue(string: "black")`) |

**Example:**

```vsp
draw_histogram(256, 200, "white", "black")
```

**Tags:** `histogram`, `draw`, `visualization`

---

### `draw_labeled_rect()`

Draws rectangle with text label

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | Top-left X |
| `y` | int | Yes | Top-left Y |
| `width` | int | Yes | Width |
| `height` | int | Yes | Height |
| `label` | string | Yes | Label text |
| `color` | string | No | Rectangle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |
| `font_scale` | float | No | Font scale (default: `RuntimeValue(float: 0.5)`) |

**Example:**

```vsp
draw_labeled_rect(10, 10, 100, 50, "Object", "green", 2, 0.5)
```

**Tags:** `draw`, `rectangle`, `label`, `detection`

---

### `draw_marker()`

Draws a marker at specified position

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | Marker X |
| `y` | int | Yes | Marker Y |
| `type` | string | No | Type: cross, tilted_cross, star, diamond, square, triangle_up, triangle_down (default: `RuntimeValue(string: "cross")`) |
| `color` | string | No | Marker color (default: `RuntimeValue(string: "green")`) |
| `size` | int | No | Marker size (default: `RuntimeValue(int: 20)`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
draw_marker(100, 100, "star", "red", 30, 2)
```

**Tags:** `draw`, `marker`, `point`

---

### `draw_rotated_rect()`

Draws a rotated rectangle

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | Center X |
| `y` | int | Yes | Center Y |
| `width` | int | Yes | Width |
| `height` | int | Yes | Height |
| `angle` | float | Yes | Rotation angle in degrees |
| `color` | string | No | Rectangle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
draw_rotated_rect(100, 100, 80, 40, 45, "red", 2)
```

**Tags:** `draw`, `rectangle`, `rotated`

---

### `ellipse()`

Draws an ellipse

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | Center X |
| `y` | int | Yes | Center Y |
| `axes_x` | int | Yes | Half-size X axis |
| `axes_y` | int | Yes | Half-size Y axis |
| `angle` | float | No | Rotation angle (default: `RuntimeValue(float: 0)`) |
| `start_angle` | float | No | Start angle (default: `RuntimeValue(float: 0)`) |
| `end_angle` | float | No | End angle (default: `RuntimeValue(float: 360)`) |
| `color` | string | No | Ellipse color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (-1 for filled) (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
ellipse(100, 100, 50, 30, 45, 0, 360, "blue", 2)
```

**Tags:** `draw`, `ellipse`, `primitive`

---

### `fill_convex_poly()`

Fills convex polygon from cached points

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `points_cache` | string | Yes | Points cache ID |
| `color` | string | No | Fill color (default: `RuntimeValue(string: "green")`) |

**Example:**

```vsp
fill_convex_poly("points", "red")
```

**Tags:** `draw`, `polygon`, `convex`, `fill`

---

### `fill_poly()`

Fills polygon from cached points

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `points_cache` | string | Yes | Points cache ID |
| `color` | string | No | Fill color (default: `RuntimeValue(string: "green")`) |

**Example:**

```vsp
fill_poly("points", "blue")
```

**Tags:** `draw`, `polygon`, `fill`

---

### `get_text_size()`

Calculates text bounding box size

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `text` | string | Yes | Text string |
| `font` | string | No | Font face (default: `RuntimeValue(string: "simplex")`) |
| `scale` | float | No | Font scale (default: `RuntimeValue(float: 1)`) |
| `thickness` | int | No | Text thickness (default: `RuntimeValue(int: 1)`) |
| `cache_prefix` | string | No | Cache prefix for size (default: `RuntimeValue(string: "text_size")`) |

**Example:**

```vsp
get_text_size("Hello", "simplex", 1.0, 1, "ts")
```

**Tags:** `text`, `size`, `measure`

---

### `line()`

Draws a line segment

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x1` | int | Yes | Start X |
| `y1` | int | Yes | Start Y |
| `x2` | int | Yes | End X |
| `y2` | int | Yes | End Y |
| `color` | string | No | Line color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 1)`) |
| `line_type` | string | No | Type: 4, 8, aa (default: `RuntimeValue(string: "8")`) |

**Example:**

```vsp
line(0, 0, 100, 100, "red", 2)
```

**Tags:** `draw`, `line`, `primitive`

---

### `overlay_image()`

Overlays cached image on current image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `overlay_cache` | string | Yes | Overlay image cache ID |
| `x` | int | Yes | Top-left X position |
| `y` | int | Yes | Top-left Y position |
| `alpha` | float | No | Opacity (0-1) (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
overlay_image("logo", 10, 10, 0.5)
```

**Tags:** `overlay`, `image`, `composite`

---

### `polylines()`

Draws polylines from cached points

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `points_cache` | string | Yes | Points cache ID |
| `closed` | bool | No | Close polyline (default: `RuntimeValue(bool: false)`) |
| `color` | string | No | Line color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
polylines("points", true, "green", 2)
```

**Tags:** `draw`, `polyline`, `primitive`

---

### `put_text()`

Draws text on image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `text` | string | Yes | Text to draw |
| `x` | int | Yes | Bottom-left X |
| `y` | int | Yes | Bottom-left Y |
| `font` | string | No | Font: simplex, plain, duplex, complex, triplex, complex_small, script_simplex, script_complex, italic (default: `RuntimeValue(string: "simplex")`) |
| `scale` | float | No | Font scale (default: `RuntimeValue(float: 1)`) |
| `color` | string | No | Text color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Text thickness (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
put_text("Hello", 10, 30, "simplex", 1.0, "white", 2)
```

**Tags:** `draw`, `text`, `label`

---

### `rectangle()`

Draws a rectangle

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | Top-left X |
| `y` | int | Yes | Top-left Y |
| `width` | int | Yes | Width |
| `height` | int | Yes | Height |
| `color` | string | No | Rectangle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (-1 for filled) (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
rectangle(10, 10, 100, 50, "red", 2)
```

**Tags:** `draw`, `rectangle`, `primitive`

---

## edge

### `approx_poly_dp()`

Approximates contour with polygon

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `epsilon` | float | No | Approximation accuracy (default: `RuntimeValue(float: 0.02)`) |
| `closed` | bool | No | Closed curve (default: `RuntimeValue(bool: true)`) |
| `cache_id` | string | No | Cache ID of contours (default: `RuntimeValue(string: "contours")`) |

**Example:**

```vsp
approx_poly_dp(0.02, true, "contours")
```

**Tags:** `contour`, `polygon`, `approximate`

---

### `arc_length()`

Calculates contour perimeter

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `closed` | bool | No | Closed contour (default: `RuntimeValue(bool: true)`) |
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |

**Example:**

```vsp
arc_length(true, "contours")
```

**Tags:** `contour`, `perimeter`, `length`

---

### `auto_canny()`

Automatic Canny edge detection using median-based thresholds

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `sigma` | float | No | Threshold adjustment factor (default: `RuntimeValue(float: 0.33)`) |

**Example:**

```vsp
auto_canny() | auto_canny(0.5)
```

**Tags:** `edge`, `canny`, `automatic`

---

### `bounding_rect()`

Calculates bounding rectangle of contours and draws them

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `color` | string | No | Rectangle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
bounding_rect("contours", "green", 2)
```

**Tags:** `contour`, `bounding`, `rect`

---

### `canny()`

Finds edges using Canny algorithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `threshold1` | float | Yes | First threshold for hysteresis |
| `threshold2` | float | Yes | Second threshold for hysteresis |
| `aperture_size` | int | No | Sobel aperture size (3, 5, 7) (default: `RuntimeValue(int: 3)`) |
| `l2_gradient` | bool | No | Use L2 norm for gradient (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
canny(50, 150) | canny(100, 200, 5, true)
```

**Tags:** `edge`, `canny`, `detection`

---

### `contour_area()`

Calculates contour area

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `oriented` | bool | No | Return signed area (default: `RuntimeValue(bool: false)`) |
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |

**Example:**

```vsp
contour_area(false, "contours")
```

**Tags:** `contour`, `area`

---

### `convex_hull()`

Finds convex hull of contours

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `color` | string | No | Hull color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
convex_hull("contours", "green", 2)
```

**Tags:** `contour`, `convex`, `hull`

---

### `convexity_defects()`

Finds convexity defects of contour

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |

**Example:**

```vsp
convexity_defects("contours")
```

**Tags:** `contour`, `convexity`, `defects`

---

### `draw_contours()`

Draws contours on image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Cache ID of contours (default: `RuntimeValue(string: "contours")`) |
| `idx` | int | No | Contour index (-1 for all) (default: `RuntimeValue(int: -1)`) |
| `color` | string | No | Color: green, red, blue, white, etc (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (-1 for filled) (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
draw_contours("contours", -1, "green", 2)
```

**Tags:** `contours`, `draw`, `edge`

---

### `find_contours()`

Finds contours in binary image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `mode` | string | No | Mode: external, list, ccomp, tree (default: `RuntimeValue(string: "external")`) |
| `method` | string | No | Method: none, simple, tc89_l1, tc89_kcos (default: `RuntimeValue(string: "simple")`) |
| `cache_id` | string | No | Cache ID for contours (default: `RuntimeValue(string: "contours")`) |

**Example:**

```vsp
find_contours("external", "simple", "contours")
```

**Tags:** `contours`, `find`, `edge`

---

### `fit_ellipse()`

Fits ellipse to contour points

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `color` | string | No | Ellipse color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
fit_ellipse("contours", "green", 2)
```

**Tags:** `contour`, `ellipse`, `fit`

---

### `fit_line()`

Fits line to 2D point set

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `dist_type` | string | No | Distance type: l2, l1, l12, fair, welsch, huber (default: `RuntimeValue(string: "l2")`) |
| `color` | string | No | Line color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
fit_line("contours", "l2", "green", 2)
```

**Tags:** `contour`, `line`, `fit`

---

### `hough_circles()`

Finds circles using Hough Transform

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dp` | float | No | Inverse accumulator resolution ratio (default: `RuntimeValue(float: 1)`) |
| `min_dist` | float | No | Minimum distance between centers (default: `RuntimeValue(float: 20)`) |
| `param1` | float | No | Canny edge threshold (default: `RuntimeValue(float: 100)`) |
| `param2` | float | No | Accumulator threshold (default: `RuntimeValue(float: 30)`) |
| `min_radius` | int | No | Minimum radius (default: `RuntimeValue(int: 0)`) |
| `max_radius` | int | No | Maximum radius (default: `RuntimeValue(int: 0)`) |
| `color` | string | No | Circle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
hough_circles(1, 20, 100, 30, 10, 100, "green", 2)
```

**Tags:** `hough`, `circles`, `detection`

---

### `hough_lines()`

Finds lines using Standard Hough Transform

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `rho` | float | No | Distance resolution (default: `RuntimeValue(float: 1)`) |
| `theta` | float | No | Angle resolution (degrees) (default: `RuntimeValue(float: 1)`) |
| `threshold` | int | No | Accumulator threshold (default: `RuntimeValue(int: 100)`) |
| `color` | string | No | Line color (default: `RuntimeValue(string: "red")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
hough_lines(1, 1, 100, "red", 2)
```

**Tags:** `hough`, `lines`, `detection`

---

### `hough_lines_p()`

Finds line segments using Probabilistic Hough Transform

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `rho` | float | No | Distance resolution (default: `RuntimeValue(float: 1)`) |
| `theta` | float | No | Angle resolution (degrees) (default: `RuntimeValue(float: 1)`) |
| `threshold` | int | No | Accumulator threshold (default: `RuntimeValue(int: 50)`) |
| `min_line_length` | float | No | Minimum line length (default: `RuntimeValue(float: 50)`) |
| `max_line_gap` | float | No | Maximum gap between segments (default: `RuntimeValue(float: 10)`) |
| `color` | string | No | Line color (default: `RuntimeValue(string: "red")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
hough_lines_p(1, 1, 50, 50, 10, "red", 2)
```

**Tags:** `hough`, `lines`, `probabilistic`

---

### `hu_moments()`

Calculates 7 Hu invariant moments

**Example:**

```vsp
hu_moments()
```

**Tags:** `moments`, `hu`, `invariant`

---

### `is_contour_convex()`

Tests whether contour is convex

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |

**Example:**

```vsp
is_contour_convex("contours")
```

**Tags:** `contour`, `convex`, `test`

---

### `match_shapes()`

Compares two shapes using Hu moments

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `template_cache` | string | Yes | Template contour cache ID |
| `method` | string | No | Method: i1, i2, i3 (default: `RuntimeValue(string: "i1")`) |

**Example:**

```vsp
match_shapes("template", "i1")
```

**Tags:** `match`, `shapes`, `compare`

---

### `min_area_rect()`

Finds minimum area rotated rectangle

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `color` | string | No | Rectangle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
min_area_rect("contours", "green", 2)
```

**Tags:** `contour`, `rotated`, `rect`, `min`

---

### `min_enclosing_circle()`

Finds minimum enclosing circle

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `color` | string | No | Circle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
min_enclosing_circle("contours", "green", 2)
```

**Tags:** `contour`, `circle`, `min`, `enclosing`

---

### `min_enclosing_triangle()`

Finds minimum enclosing triangle

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `color` | string | No | Triangle color (default: `RuntimeValue(string: "green")`) |
| `thickness` | int | No | Line thickness (default: `RuntimeValue(int: 2)`) |

**Example:**

```vsp
min_enclosing_triangle("contours", "green", 2)
```

**Tags:** `contour`, `triangle`, `min`, `enclosing`

---

### `moments()`

Calculates image moments

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `binary` | bool | No | Treat as binary image (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
moments() | moments(true)
```

**Tags:** `moments`, `analysis`

---

### `point_polygon_test()`

Tests point-contour relationship

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | float | Yes | Point X coordinate |
| `y` | float | Yes | Point Y coordinate |
| `cache_id` | string | No | Contour cache ID (default: `RuntimeValue(string: "contours")`) |
| `measure_dist` | bool | No | Measure signed distance (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
point_polygon_test(100, 100, "contours", true)
```

**Tags:** `point`, `polygon`, `test`

---

## feature

### `agast()`

Detects corners using AGAST algorithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `threshold` | int | No | Detection threshold (default: `RuntimeValue(int: 10)`) |
| `nonmax_suppression` | bool | No | Non-maximum suppression (default: `RuntimeValue(bool: true)`) |
| `type` | string | No | Type: 5_8, 7_12d, 7_12s, oast_9_16 (default: `RuntimeValue(string: "oast_9_16")`) |

**Example:**

```vsp
agast(10, true, "oast_9_16")
```

**Tags:** `agast`, `corners`, `features`

---

### `akaze()`

Detects AKAZE features and computes descriptors

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `threshold` | float | No | Detector threshold (default: `RuntimeValue(float: 0.001)`) |
| `n_octaves` | int | No | Maximum octaves (default: `RuntimeValue(int: 4)`) |
| `n_octave_layers` | int | No | Layers per octave (default: `RuntimeValue(int: 4)`) |
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "akaze")`) |

**Example:**

```vsp
akaze(0.001, 4, 4, "akaze")
```

**Tags:** `akaze`, `features`, `descriptors`

---

### `bf_matcher()`

Matches descriptors using brute-force matcher

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `query_cache` | string | Yes | Query descriptors cache ID |
| `train_cache` | string | Yes | Train descriptors cache ID |
| `norm_type` | string | No | Norm: l2, l1, hamming, hamming2 (default: `RuntimeValue(string: "l2")`) |
| `cross_check` | bool | No | Cross-check matches (default: `RuntimeValue(bool: false)`) |
| `k` | int | No | K nearest neighbors (default: `RuntimeValue(int: 2)`) |
| `ratio_thresh` | float | No | Lowe's ratio threshold (default: `RuntimeValue(float: 0.75)`) |

**Example:**

```vsp
bf_matcher("orb1", "orb2", "hamming", false, 2, 0.75)
```

**Tags:** `matcher`, `brute-force`, `features`

---

### `brisk()`

Detects BRISK features and computes descriptors

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `thresh` | int | No | AGAST detection threshold (default: `RuntimeValue(int: 30)`) |
| `octaves` | int | No | Detection octaves (default: `RuntimeValue(int: 3)`) |
| `pattern_scale` | float | No | Pattern scale (default: `RuntimeValue(float: 1)`) |
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "brisk")`) |

**Example:**

```vsp
brisk(30, 3, 1.0, "brisk")
```

**Tags:** `brisk`, `features`, `descriptors`

---

### `corner_harris()`

Detects corners using Harris corner detector

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `block_size` | int | No | Neighborhood size (default: `RuntimeValue(int: 2)`) |
| `ksize` | int | No | Sobel aperture (default: `RuntimeValue(int: 3)`) |
| `k` | float | No | Harris parameter (default: `RuntimeValue(float: 0.04)`) |
| `threshold` | float | No | Response threshold (0-1) (default: `RuntimeValue(float: 0.01)`) |

**Example:**

```vsp
corner_harris(2, 3, 0.04, 0.01)
```

**Tags:** `corners`, `harris`, `features`

---

### `corner_min_eigen_val()`

Calculates minimum eigenvalue for corner detection

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `block_size` | int | No | Neighborhood size (default: `RuntimeValue(int: 3)`) |
| `ksize` | int | No | Sobel aperture (default: `RuntimeValue(int: 3)`) |

**Example:**

```vsp
corner_min_eigen_val(3, 3)
```

**Tags:** `corners`, `eigenvalue`, `features`

---

### `corner_sub_pix()`

Refines corner locations to sub-pixel accuracy

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `win_size` | int | No | Half window size (default: `RuntimeValue(int: 5)`) |
| `zero_zone` | int | No | Half dead zone size (-1 to disable) (default: `RuntimeValue(int: -1)`) |
| `max_count` | int | No | Maximum iterations (default: `RuntimeValue(int: 30)`) |
| `epsilon` | float | No | Convergence threshold (default: `RuntimeValue(float: 0.001)`) |

**Example:**

```vsp
corner_sub_pix(5, -1, 30, 0.001)
```

**Tags:** `corners`, `subpixel`, `refinement`

---

### `draw_keypoints()`

Draws keypoints on image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `color` | string | No | Keypoint color (default: `RuntimeValue(string: "green")`) |
| `rich` | bool | No | Draw rich keypoints with size and orientation (default: `RuntimeValue(bool: true)`) |

**Example:**

```vsp
draw_keypoints("green", true)
```

**Tags:** `draw`, `keypoints`, `features`

---

### `draw_matches()`

Draws feature matches between two images

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `img1_cache` | string | Yes | First image cache ID |
| `img2_cache` | string | Yes | Second image cache ID |

**Example:**

```vsp
draw_matches("img1", "img2")
```

**Tags:** `draw`, `matches`, `features`

---

### `fast()`

Detects corners using FAST algorithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `threshold` | int | No | Detection threshold (default: `RuntimeValue(int: 10)`) |
| `nonmax_suppression` | bool | No | Non-maximum suppression (default: `RuntimeValue(bool: true)`) |
| `type` | string | No | Type: 9_16, 7_12, 5_8 (default: `RuntimeValue(string: "9_16")`) |

**Example:**

```vsp
fast(10, true, "9_16")
```

**Tags:** `fast`, `corners`, `features`

---

### `find_homography()`

Finds perspective transformation between two planes

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `src_points_cache` | string | Yes | Source points cache ID |
| `dst_points_cache` | string | Yes | Destination points cache ID |
| `method` | string | No | Method: 0, ransac, lmeds, rho (default: `RuntimeValue(string: "ransac")`) |
| `ransac_reproj_threshold` | float | No | RANSAC threshold (default: `RuntimeValue(float: 3)`) |
| `cache_id` | string | No | Cache ID for homography (default: `RuntimeValue(string: "homography")`) |

**Example:**

```vsp
find_homography("src", "dst", "ransac", 3.0, "H")
```

**Tags:** `homography`, `perspective`, `transform`

---

### `flann_matcher()`

Matches descriptors using FLANN matcher

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `query_cache` | string | Yes | Query descriptors cache ID |
| `train_cache` | string | Yes | Train descriptors cache ID |
| `k` | int | No | K nearest neighbors (default: `RuntimeValue(int: 2)`) |
| `ratio_thresh` | float | No | Lowe's ratio threshold (default: `RuntimeValue(float: 0.7)`) |

**Example:**

```vsp
flann_matcher("sift1", "sift2", 2, 0.7)
```

**Tags:** `matcher`, `flann`, `features`

---

### `good_features_to_track()`

Finds corners using Shi-Tomasi corner detection

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `max_corners` | int | No | Maximum corners to return (default: `RuntimeValue(int: 100)`) |
| `quality_level` | float | No | Quality level threshold (default: `RuntimeValue(float: 0.01)`) |
| `min_distance` | float | No | Minimum distance between corners (default: `RuntimeValue(float: 10)`) |
| `block_size` | int | No | Neighborhood size (default: `RuntimeValue(int: 3)`) |
| `use_harris` | bool | No | Use Harris detector (default: `RuntimeValue(bool: false)`) |
| `k` | float | No | Harris parameter (default: `RuntimeValue(float: 0.04)`) |

**Example:**

```vsp
good_features_to_track(100, 0.01, 10)
```

**Tags:** `corners`, `shi-tomasi`, `features`

---

### `kaze()`

Detects KAZE features and computes descriptors

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `extended` | bool | No | Extended descriptors (128 vs 64) (default: `RuntimeValue(bool: false)`) |
| `upright` | bool | No | Upright descriptors (default: `RuntimeValue(bool: false)`) |
| `threshold` | float | No | Detection threshold (default: `RuntimeValue(float: 0.001)`) |
| `n_octaves` | int | No | Maximum octaves (default: `RuntimeValue(int: 4)`) |
| `n_octave_layers` | int | No | Layers per octave (default: `RuntimeValue(int: 4)`) |
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "kaze")`) |

**Example:**

```vsp
kaze(false, false, 0.001, 4, 4, "kaze")
```

**Tags:** `kaze`, `features`, `descriptors`

---

### `match_template()`

Finds template matches in image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `template_cache` | string | Yes | Template image cache ID |
| `method` | string | No | Method: sqdiff, sqdiff_normed, ccorr, ccorr_normed, ccoeff, ccoeff_normed (default: `RuntimeValue(string: "ccoeff_normed")`) |

**Example:**

```vsp
match_template("template", "ccoeff_normed")
```

**Tags:** `template`, `matching`, `detection`

---

### `min_max_loc()`

Finds minimum and maximum values and their locations

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cache_prefix` | string | No | Cache prefix for results (default: `RuntimeValue(string: "minmax")`) |

**Example:**

```vsp
min_max_loc("minmax")
```

**Tags:** `min`, `max`, `location`

---

### `orb()`

Detects ORB features and computes descriptors

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `n_features` | int | No | Maximum features (default: `RuntimeValue(int: 500)`) |
| `scale_factor` | float | No | Pyramid scale factor (default: `RuntimeValue(float: 1.2)`) |
| `n_levels` | int | No | Pyramid levels (default: `RuntimeValue(int: 8)`) |
| `edge_threshold` | int | No | Edge threshold (default: `RuntimeValue(int: 31)`) |
| `cache_id` | string | No | Cache ID for descriptors (default: `RuntimeValue(string: "orb")`) |

**Example:**

```vsp
orb(500, 1.2, 8, 31, "orb")
```

**Tags:** `orb`, `features`, `descriptors`

---

### `perspective_transform_points()`

Applies perspective transformation to points

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `points_cache` | string | Yes | Points cache ID |
| `matrix_cache` | string | Yes | Transformation matrix cache ID |
| `output_cache` | string | No | Output points cache ID (default: `RuntimeValue(string: "transformed_points")`) |

**Example:**

```vsp
perspective_transform_points("points", "H", "output")
```

**Tags:** `perspective`, `transform`, `points`

---

### `sift()`

Detects SIFT features and computes descriptors

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `n_features` | int | No | Maximum features (0=unlimited) (default: `RuntimeValue(int: 0)`) |
| `n_octave_layers` | int | No | Layers per octave (default: `RuntimeValue(int: 3)`) |
| `contrast_threshold` | float | No | Contrast threshold (default: `RuntimeValue(float: 0.04)`) |
| `edge_threshold` | float | No | Edge threshold (default: `RuntimeValue(float: 10)`) |
| `sigma` | float | No | Gaussian sigma (default: `RuntimeValue(float: 1.6)`) |
| `cache_id` | string | No | Cache ID (default: `RuntimeValue(string: "sift")`) |

**Example:**

```vsp
sift(0, 3, 0.04, 10, 1.6, "sift")
```

**Tags:** `sift`, `features`, `descriptors`

---

### `simple_blob_detector()`

Detects blobs using SimpleBlobDetector

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `min_threshold` | float | No | Minimum threshold (default: `RuntimeValue(float: 10)`) |
| `max_threshold` | float | No | Maximum threshold (default: `RuntimeValue(float: 200)`) |
| `min_area` | float | No | Minimum blob area (default: `RuntimeValue(float: 25)`) |
| `max_area` | float | No | Maximum blob area (default: `RuntimeValue(float: 5000)`) |
| `min_circularity` | float | No | Minimum circularity (0-1) (default: `RuntimeValue(float: 0)`) |
| `min_convexity` | float | No | Minimum convexity (0-1) (default: `RuntimeValue(float: 0)`) |
| `min_inertia_ratio` | float | No | Minimum inertia ratio (0-1) (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
simple_blob_detector(10, 200, 25, 5000)
```

**Tags:** `blob`, `detector`, `features`

---

## filter

### `bilateral_filter()`

Applies bilateral filter (edge-preserving smoothing)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `d` | int | No | Diameter of pixel neighborhood (neg = computed from sigma) (default: `RuntimeValue(int: 9)`) |
| `sigma_color` | float | No | Filter sigma in color space (default: `RuntimeValue(float: 75)`) |
| `sigma_space` | float | No | Filter sigma in coordinate space (default: `RuntimeValue(float: 75)`) |
| `border` | string | No | Border type (default: `RuntimeValue(string: "default")`) |

**Example:**

```vsp
bilateral_filter(9, 75, 75)
```

**Tags:** `blur`, `filter`, `smooth`, `edge-preserving`, `bilateral`

---

### `blur()`

Applies simple averaging blur

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ksize` | int | No | Kernel size (default: `RuntimeValue(int: 5)`) |

**Example:**

```vsp
blur(5)
```

**Tags:** `blur`, `filter`, `average`

---

### `box_filter()`

Applies box filter (averaging filter with option for unnormalized)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ksize` | int | No | Kernel size (default: `RuntimeValue(int: 5)`) |
| `normalize` | bool | No | Normalize by kernel area (default: `RuntimeValue(bool: true)`) |
| `ddepth` | int | No | Output depth (-1 = same as input) (default: `RuntimeValue(int: -1)`) |
| `anchor_x` | int | No | Anchor X (-1 = center) (default: `RuntimeValue(int: -1)`) |
| `anchor_y` | int | No | Anchor Y (-1 = center) (default: `RuntimeValue(int: -1)`) |

**Example:**

```vsp
box_filter(5) | box_filter(3, false)
```

**Tags:** `blur`, `filter`, `box`, `average`

---

### `fast_nl_means()`

Non-local means denoising for grayscale images

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `h` | float | No | Filter strength (higher = more denoising) (default: `RuntimeValue(float: 10)`) |
| `template_size` | int | No | Template window size (odd) (default: `RuntimeValue(int: 7)`) |
| `search_size` | int | No | Search window size (odd) (default: `RuntimeValue(int: 21)`) |

**Example:**

```vsp
fast_nl_means(10) | fast_nl_means(3, 7, 21)
```

**Tags:** `denoise`, `filter`, `nlmeans`

---

### `fast_nl_means_colored()`

Non-local means denoising for color images

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `h` | float | No | Filter strength for luminance (default: `RuntimeValue(float: 10)`) |
| `h_color` | float | No | Filter strength for color components (default: `RuntimeValue(float: 10)`) |
| `template_size` | int | No | Template window size (odd) (default: `RuntimeValue(int: 7)`) |
| `search_size` | int | No | Search window size (odd) (default: `RuntimeValue(int: 21)`) |

**Example:**

```vsp
fast_nl_means_colored(10, 10)
```

**Tags:** `denoise`, `filter`, `nlmeans`, `color`

---

### `filter2d()`

Applies a custom convolution kernel

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `kernel` | array | Yes | Convolution kernel (flattened array) |
| `ksize` | int | No | Kernel size (sqrt of array length) (default: `RuntimeValue(int: 3)`) |
| `delta` | float | No | Value added to result (default: `RuntimeValue(float: 0)`) |
| `ddepth` | int | No | Output depth (-1 = same) (default: `RuntimeValue(int: -1)`) |

**Example:**

```vsp
filter2d([0,-1,0,-1,5,-1,0,-1,0], 3)
```

**Tags:** `filter`, `convolution`, `custom`

---

### `gaussian_blur()`

Applies Gaussian blur to the current frame

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ksize` | int | No | Kernel size (odd number) (default: `RuntimeValue(int: 5)`) |
| `sigma_x` | float | No | Sigma X (0 = auto from ksize) (default: `RuntimeValue(float: 0)`) |
| `sigma_y` | float | No | Sigma Y (0 = same as sigma_x) (default: `RuntimeValue(float: 0)`) |
| `border` | string | No | Border type: default, replicate, reflect, wrap, reflect_101 (default: `RuntimeValue(string: "default")`) |

**Example:**

```vsp
gaussian_blur(5) | gaussian_blur(7, 1.5)
```

**Tags:** `blur`, `filter`, `smooth`, `gaussian`

---

### `laplacian()`

Applies Laplacian operator for edge detection

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ksize` | int | No | Aperture size (1, 3, 5, 7) (default: `RuntimeValue(int: 3)`) |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |
| `delta` | float | No | Value added to result (default: `RuntimeValue(float: 0)`) |
| `ddepth` | int | No | Output depth (default: `RuntimeValue(int: -1)`) |

**Example:**

```vsp
laplacian(3)
```

**Tags:** `edge`, `filter`, `laplacian`, `derivative`

---

### `median_blur()`

Applies median blur (effective for salt-and-pepper noise)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ksize` | int | No | Kernel size (odd number) (default: `RuntimeValue(int: 5)`) |

**Example:**

```vsp
median_blur(5)
```

**Tags:** `blur`, `filter`, `denoise`, `median`

---

### `scharr()`

Applies Scharr operator (more accurate than Sobel for 3x3)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dx` | int | No | Order of X derivative (0 or 1) (default: `RuntimeValue(int: 1)`) |
| `dy` | int | No | Order of Y derivative (0 or 1) (default: `RuntimeValue(int: 0)`) |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |
| `delta` | float | No | Value added to result (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
scharr(1, 0) | scharr(0, 1)
```

**Tags:** `edge`, `gradient`, `scharr`, `derivative`

---

### `sep_filter2d()`

Applies separable filter (faster for large kernels)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `kernel_x` | array | Yes | Row kernel |
| `kernel_y` | array | Yes | Column kernel |
| `ddepth` | int | No | Output depth (default: `RuntimeValue(int: -1)`) |

**Example:**

```vsp
sep_filter2d([1,2,1], [1,2,1])
```

**Tags:** `filter`, `separable`, `convolution`

---

### `sharpen()`

Applies unsharp masking to sharpen the image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `amount` | float | No | Sharpening strength (0-3) (default: `RuntimeValue(float: 1)`) |
| `radius` | int | No | Blur radius for mask (default: `RuntimeValue(int: 5)`) |
| `threshold` | int | No | Threshold for sharpening (0-255) (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
sharpen(1.5) | sharpen(1.0, 3)
```

**Tags:** `sharpen`, `filter`, `enhance`

---

### `sobel()`

Applies Sobel operator for gradient calculation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dx` | int | No | Order of derivative in X (default: `RuntimeValue(int: 1)`) |
| `dy` | int | No | Order of derivative in Y (default: `RuntimeValue(int: 0)`) |
| `ksize` | int | No | Kernel size (1, 3, 5, 7) (default: `RuntimeValue(int: 3)`) |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |
| `delta` | float | No | Value added to result (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
sobel(1, 0) | sobel(0, 1)
```

**Tags:** `edge`, `gradient`, `sobel`, `derivative`

---

### `stack_blur()`

Applies stack blur (fast approximation of Gaussian blur)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ksize` | int | No | Kernel size (odd, 1+) (default: `RuntimeValue(int: 5)`) |

**Example:**

```vsp
stack_blur(5)
```

**Tags:** `blur`, `filter`, `fast`

---

## gui

### `button()`

Creates a button-like trackbar that triggers when clicked

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Button name |
| `window` | string | Yes | Window name |
| `clicked_var` | string | Yes | Variable set when clicked |

**Example:**

```vsp
button("Apply", "Control", "apply_clicked")
```

**Tags:** `gui`, `button`, `trigger`

---

### `check_button_click()`

Checks if button was clicked and resets the state

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Button name |
| `window` | string | Yes | Window name |
| `result_var` | string | Yes | Variable to store click state |

**Example:**

```vsp
check_button_click("Apply", "Control", "was_clicked")
```

**Tags:** `gui`, `button`, `check`

---

### `checkbox()`

Creates a checkbox using a trackbar bound to a boolean variable

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Checkbox name |
| `window` | string | Yes | Window name |
| `var_name` | string | Yes | Boolean variable name |
| `initial` | bool | No | Initial state (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
checkbox("Enable Filter", "Control", "filter_enabled", true)
```

**Tags:** `gui`, `checkbox`, `boolean`, `toggle`

---

### `color_picker_control()`

Creates 3 trackbars for BGR color selection

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `prefix` | string | Yes | Variable prefix for _b, _g, _r |
| `window` | string | Yes | Window name |
| `initial_b` | int | No | Initial blue value (default: `RuntimeValue(int: 0)`) |
| `initial_g` | int | No | Initial green value (default: `RuntimeValue(int: 0)`) |
| `initial_r` | int | No | Initial red value (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
color_picker_control("color", "Color Picker", 255, 0, 0)
```

**Tags:** `gui`, `color`, `picker`

---

### `control_panel()`

Creates a control panel window with common image processing controls

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Control panel window name |
| `preset` | string | Yes | Preset: threshold, blur, edge, morphology, color |

**Example:**

```vsp
control_panel("Controls", "threshold")
```

**Tags:** `gui`, `control`, `panel`, `preset`

---

### `get_checkbox()`

Gets checkbox state as boolean into a variable

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Checkbox name |
| `window` | string | Yes | Window name |
| `var_name` | string | Yes | Variable to store result |

**Example:**

```vsp
get_checkbox("Enable Filter", "Control", "is_enabled")
```

**Tags:** `gui`, `checkbox`, `boolean`

---

### `get_color_from_picker()`

Gets BGR values from color picker into individual variables

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `prefix` | string | Yes | Variable prefix |
| `window` | string | Yes | Window name |

**Example:**

```vsp
get_color_from_picker("color", "Color Picker")
```

**Tags:** `gui`, `color`, `picker`

---

### `get_hsv_range()`

Gets HSV range values from control into variables

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `prefix` | string | Yes | Variable prefix |
| `window` | string | Yes | Window name |

**Example:**

```vsp
get_hsv_range("hsv", "HSV Control")
```

**Tags:** `gui`, `hsv`, `color`, `range`

---

### `hsv_range_control()`

Creates 6 trackbars for HSV min/max range selection

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `prefix` | string | Yes | Variable prefix for h_min, h_max, etc. |
| `window` | string | Yes | Window name |

**Example:**

```vsp
hsv_range_control("hsv", "HSV Control")
```

**Tags:** `gui`, `hsv`, `color`, `range`

---

### `imshow_with_vars()`

Displays image with overlay showing variable values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `window` | string | Yes | Window name |
| `var_list` | string | Yes | Comma-separated list of variable names |

**Example:**

```vsp
imshow_with_vars("Debug", "threshold,blur_size,enabled")
```

**Tags:** `gui`, `display`, `debug`, `overlay`

---

### `read_control_panel()`

Reads all control panel values into variables with prefix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Control panel window name |
| `prefix` | string | Yes | Variable prefix |
| `preset` | string | Yes | Preset type used when creating |

**Example:**

```vsp
read_control_panel("Controls", "ctrl", "threshold")
```

**Tags:** `gui`, `control`, `panel`, `read`

---

### `set_trackbar_from_var()`

Sets trackbar position from a variable value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Trackbar name |
| `window` | string | Yes | Window name |
| `var_name` | string | Yes | Variable name containing value |

**Example:**

```vsp
set_trackbar_from_var("Threshold", "Control", "thresh")
```

**Tags:** `gui`, `trackbar`, `variable`

---

### `sync_trackbar_var()`

Syncs an existing trackbar with a variable bidirectionally

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Trackbar name |
| `window` | string | Yes | Window name |
| `var_name` | string | Yes | Variable name to sync |

**Example:**

```vsp
sync_trackbar_var("Threshold", "Control", "thresh")
```

**Tags:** `gui`, `trackbar`, `variable`, `sync`

---

### `trackbar_value()`

Gets trackbar value and stores in a variable

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Trackbar name |
| `window` | string | Yes | Window name |
| `var_name` | string | Yes | Variable name to store value |

**Example:**

```vsp
trackbar_value("Threshold", "Control", "thresh")
```

**Tags:** `gui`, `trackbar`, `value`

---

### `trackbar_var()`

Creates a trackbar bound to a variable that auto-updates

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | Yes | Trackbar name |
| `window` | string | Yes | Window name |
| `var_name` | string | Yes | Variable name to bind |
| `min_val` | int | Yes | Minimum value |
| `max_val` | int | Yes | Maximum value |
| `initial` | int | No | Initial value (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
trackbar_var("Threshold", "Control", "thresh", 0, 255, 128)
```

**Tags:** `gui`, `trackbar`, `variable`, `binding`

---

### `update_window_title()`

Updates window title with variable values using {var} placeholders

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `window` | string | Yes | Window name |
| `format` | string | Yes | Title format with {var} placeholders |

**Example:**

```vsp
update_window_title("Output", "Threshold: {threshold} - FPS: {fps}")
```

**Tags:** `gui`, `window`, `title`

---

## math

### `abs_scalar()`

Calculates absolute value of scalar

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
abs_scalar(-5.5, "result")
```

**Tags:** `math`, `abs`, `absolute`

---

### `acos()`

Calculates arc cosine (inverse cosine)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value [-1, 1] or variable name |
| `output_var` | string | Yes | Output variable name |
| `in_degrees` | bool | No | Output in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
acos(0.5, "result", true)
```

**Tags:** `math`, `trigonometry`, `acos`

---

### `asin()`

Calculates arc sine (inverse sine)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value [-1, 1] or variable name |
| `output_var` | string | Yes | Output variable name |
| `in_degrees` | bool | No | Output in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
asin(0.5, "result", true)
```

**Tags:** `math`, `trigonometry`, `asin`

---

### `atan()`

Calculates arc tangent (inverse tangent)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |
| `in_degrees` | bool | No | Output in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
atan(1.0, "result", true)
```

**Tags:** `math`, `trigonometry`, `atan`

---

### `atan2()`

Calculates arc tangent of y/x (2-argument form)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `y` | float | Yes | Y value or variable name |
| `x` | float | Yes | X value or variable name |
| `output_var` | string | Yes | Output variable name |
| `in_degrees` | bool | No | Output in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
atan2(y, x, "angle", true)
```

**Tags:** `math`, `trigonometry`, `atan2`, `angle`

---

### `ceil_scalar()`

Rounds up to integer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
ceil_scalar(3.1, "result")
```

**Tags:** `math`, `ceil`, `ceiling`, `rounding`

---

### `clamp()`

Clamps value to range [min, max]

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Value to clamp or variable name |
| `min` | float | Yes | Minimum value |
| `max` | float | Yes | Maximum value |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
clamp(value, 0, 255, "clamped")
```

**Tags:** `math`, `clamp`, `range`, `limit`

---

### `cos()`

Calculates cosine of value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |
| `in_degrees` | bool | No | Input in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
cos(angle, "result", true)
```

**Tags:** `math`, `trigonometry`, `cos`

---

### `exp_scalar()`

Calculates e^x

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
exp_scalar(2, "result")
```

**Tags:** `math`, `exp`, `exponential`

---

### `floor_scalar()`

Rounds down to integer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
floor_scalar(3.7, "result")
```

**Tags:** `math`, `floor`, `rounding`

---

### `lerp_scalar()`

Linear interpolation between two values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `start` | float | Yes | Start value or variable name |
| `end` | float | Yes | End value or variable name |
| `t` | float | Yes | Interpolation factor (0-1) |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
lerp_scalar(0, 100, 0.5, "result")
```

**Tags:** `math`, `lerp`, `interpolation`, `blend`

---

### `ln_scalar()`

Calculates natural logarithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
ln_scalar(2.718, "result")
```

**Tags:** `math`, `ln`, `log`, `logarithm`

---

### `log10_scalar()`

Calculates base-10 logarithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
log10_scalar(100, "result")
```

**Tags:** `math`, `log10`, `logarithm`

---

### `math_add()`

Adds two values and stores result

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | First value or variable name |
| `b` | float | Yes | Second value or variable name |
| `result_var` | string | Yes | Result variable name |

**Example:**

```vsp
math_add(var_a, var_b, "sum")
```

**Tags:** `math`, `add`, `addition`, `sum`

---

### `math_const()`

Sets variable to mathematical constant

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `constant` | string | Yes | Constant name: pi, e, phi, sqrt2 |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
math_const("pi", "pi_val")
```

**Tags:** `math`, `constant`, `pi`, `e`

---

### `math_div()`

Divides two values and stores result

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | Numerator or variable name |
| `b` | float | Yes | Denominator or variable name |
| `result_var` | string | Yes | Result variable name |

**Example:**

```vsp
math_div(var_a, var_b, "quotient")
```

**Tags:** `math`, `divide`, `division`, `quotient`

---

### `math_mod()`

Calculates modulo and stores result

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | Value or variable name |
| `b` | float | Yes | Modulus or variable name |
| `result_var` | string | Yes | Result variable name |

**Example:**

```vsp
math_mod(var_a, 10, "remainder")
```

**Tags:** `math`, `modulo`, `remainder`, `mod`

---

### `math_mul()`

Multiplies two values and stores result

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | First value or variable name |
| `b` | float | Yes | Second value or variable name |
| `result_var` | string | Yes | Result variable name |

**Example:**

```vsp
math_mul(var_a, var_b, "product")
```

**Tags:** `math`, `multiply`, `multiplication`, `product`

---

### `math_sub()`

Subtracts two values and stores result

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | First value or variable name |
| `b` | float | Yes | Second value or variable name |
| `result_var` | string | Yes | Result variable name |

**Example:**

```vsp
math_sub(var_a, var_b, "diff")
```

**Tags:** `math`, `subtract`, `subtraction`, `difference`

---

### `max_scalar()`

Returns maximum of two values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | First value or variable name |
| `b` | float | Yes | Second value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
max_scalar(a, b, "max_val")
```

**Tags:** `math`, `max`, `maximum`

---

### `min_scalar()`

Returns minimum of two values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | float | Yes | First value or variable name |
| `b` | float | Yes | Second value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
min_scalar(a, b, "min_val")
```

**Tags:** `math`, `min`, `minimum`

---

### `pow_scalar()`

Raises scalar to power

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `base` | float | Yes | Base value or variable name |
| `exponent` | float | Yes | Exponent value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
pow_scalar(2, 8, "result")
```

**Tags:** `math`, `pow`, `power`, `exponent`

---

### `rand_float()`

Generates random float in range [min, max)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `min` | float | Yes | Minimum value (inclusive) |
| `max` | float | Yes | Maximum value (exclusive) |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
rand_float(0.0, 1.0, "random_val")
```

**Tags:** `math`, `random`, `float`, `rand`

---

### `rand_int()`

Generates random integer in range [min, max]

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `min` | int | Yes | Minimum value (inclusive) |
| `max` | int | Yes | Maximum value (inclusive) |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
rand_int(0, 100, "random_val")
```

**Tags:** `math`, `random`, `integer`, `rand`

---

### `round_scalar()`

Rounds to nearest integer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
round_scalar(3.7, "result")
```

**Tags:** `math`, `round`, `rounding`

---

### `sin()`

Calculates sine of value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |
| `in_degrees` | bool | No | Input in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
sin(angle, "result", true)
```

**Tags:** `math`, `trigonometry`, `sin`

---

### `sqrt_scalar()`

Calculates square root of scalar

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |

**Example:**

```vsp
sqrt_scalar(16, "result")
```

**Tags:** `math`, `sqrt`, `square root`

---

### `tan()`

Calculates tangent of value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `value` | float | Yes | Input value or variable name |
| `output_var` | string | Yes | Output variable name |
| `in_degrees` | bool | No | Input in degrees (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
tan(angle, "result", true)
```

**Tags:** `math`, `trigonometry`, `tan`

---

## morphology

### `adaptive_threshold()`

Applies adaptive threshold using local neighborhood

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `maxval` | float | No | Maximum value (default: `RuntimeValue(float: 255)`) |
| `method` | string | No | Method: mean, gaussian (default: `RuntimeValue(string: "gaussian")`) |
| `type` | string | No | Type: binary, binary_inv (default: `RuntimeValue(string: "binary")`) |
| `block_size` | int | No | Neighborhood size (odd) (default: `RuntimeValue(int: 11)`) |
| `c` | float | No | Constant subtracted from mean (default: `RuntimeValue(float: 2)`) |

**Example:**

```vsp
adaptive_threshold(255, "gaussian", "binary", 11, 2)
```

**Tags:** `threshold`, `adaptive`, `segmentation`

---

### `connected_components()`

Labels connected components in binary image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `connectivity` | int | No | Connectivity: 4 or 8 (default: `RuntimeValue(int: 8)`) |

**Example:**

```vsp
connected_components(8)
```

**Tags:** `connected`, `components`, `label`

---

### `connected_components_with_stats()`

Labels components and computes statistics

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `connectivity` | int | No | Connectivity: 4 or 8 (default: `RuntimeValue(int: 8)`) |
| `cache_prefix` | string | No | Cache prefix for stats (default: `RuntimeValue(string: "cc")`) |

**Example:**

```vsp
connected_components_with_stats(8, "cc")
```

**Tags:** `connected`, `components`, `stats`

---

### `dilate()`

Dilates image using a structuring element

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `kernel_size` | int | No | Kernel size (default: `RuntimeValue(int: 3)`) |
| `shape` | string | No | Shape: rect, cross, ellipse (default: `RuntimeValue(string: "rect")`) |
| `iterations` | int | No | Number of iterations (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
dilate(3) | dilate(5, "ellipse", 2)
```

**Tags:** `dilate`, `morphology`

---

### `distance_transform()`

Calculates distance to nearest zero pixel

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `distance_type` | string | No | Type: l1, l2, c (default: `RuntimeValue(string: "l2")`) |
| `mask_size` | int | No | Mask size: 3, 5, 0 (precise) (default: `RuntimeValue(int: 3)`) |

**Example:**

```vsp
distance_transform("l2", 5)
```

**Tags:** `distance`, `transform`, `morphology`

---

### `erode()`

Erodes image using a structuring element

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `kernel_size` | int | No | Kernel size (default: `RuntimeValue(int: 3)`) |
| `shape` | string | No | Shape: rect, cross, ellipse (default: `RuntimeValue(string: "rect")`) |
| `iterations` | int | No | Number of iterations (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
erode(3) | erode(5, "ellipse", 2)
```

**Tags:** `erode`, `morphology`

---

### `fill_holes()`

Fills holes in binary image

**Example:**

```vsp
fill_holes()
```

**Tags:** `fill`, `holes`, `morphology`

---

### `get_structuring_element()`

Creates a structuring element and caches it

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `shape` | string | Yes | Shape: rect, cross, ellipse |
| `size` | int | Yes | Element size |
| `cache_id` | string | No | Cache ID to store element (default: `RuntimeValue(string: "kernel")`) |

**Example:**

```vsp
get_structuring_element("ellipse", 5, "my_kernel")
```

**Tags:** `kernel`, `structuring`, `morphology`

---

### `morphology_ex()`

Performs advanced morphological operations

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `op` | string | Yes | Operation: open, close, gradient, tophat, blackhat, hitmiss |
| `kernel_size` | int | No | Kernel size (default: `RuntimeValue(int: 3)`) |
| `shape` | string | No | Shape: rect, cross, ellipse (default: `RuntimeValue(string: "rect")`) |
| `iterations` | int | No | Number of iterations (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
morphology_ex("open", 5) | morphology_ex("gradient", 3, "ellipse")
```

**Tags:** `morphology`, `open`, `close`, `gradient`

---

### `otsu_threshold()`

Applies Otsu's automatic threshold

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `maxval` | float | No | Maximum value (default: `RuntimeValue(float: 255)`) |
| `type` | string | No | Type: binary, binary_inv (default: `RuntimeValue(string: "binary")`) |

**Example:**

```vsp
otsu_threshold() | otsu_threshold(255, "binary_inv")
```

**Tags:** `threshold`, `otsu`, `automatic`

---

### `remove_small_objects()`

Removes small connected components from binary image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `min_size` | int | Yes | Minimum object area in pixels |

**Example:**

```vsp
remove_small_objects(100)
```

**Tags:** `remove`, `small`, `connected`, `morphology`

---

### `skeletonize()`

Extracts skeleton of binary image

**Example:**

```vsp
skeletonize()
```

**Tags:** `skeleton`, `thin`, `morphology`

---

### `thinning()`

Applies morphological thinning using ximgproc

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `type` | string | No | Type: zhangsuen, guohall (default: `RuntimeValue(string: "zhangsuen")`) |

**Example:**

```vsp
thinning() | thinning("guohall")
```

**Tags:** `thin`, `skeleton`, `morphology`

---

### `threshold()`

Applies fixed-level threshold to image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `thresh` | float | Yes | Threshold value |
| `maxval` | float | No | Maximum value for binary thresholding (default: `RuntimeValue(float: 255)`) |
| `type` | string | No | Type: binary, binary_inv, trunc, tozero, tozero_inv (default: `RuntimeValue(string: "binary")`) |

**Example:**

```vsp
threshold(127) | threshold(100, 255, "binary_inv")
```

**Tags:** `threshold`, `binary`, `segmentation`

---

## stereo

### `calibrate_camera()`

Calibrates camera from object/image points

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `object_points_cache` | string | Yes | Object points cache |
| `image_points_cache` | string | Yes | Image points cache |
| `cache_prefix` | string | No | Cache prefix for results (default: `RuntimeValue(string: "calib")`) |

**Example:**

```vsp
calibrate_camera("obj_pts", "img_pts", "calib")
```

**Tags:** `calibration`, `camera`, `intrinsics`

---

### `disparity_to_depth()`

Converts disparity to depth using baseline and focal length

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `baseline` | float | Yes | Stereo baseline (distance between cameras) |
| `focal_length` | float | Yes | Camera focal length in pixels |

**Example:**

```vsp
disparity_to_depth(0.12, 700)
```

**Tags:** `disparity`, `depth`, `stereo`

---

### `disparity_wls_filter()`

Applies WLS filter to disparity map (requires ximgproc)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `lambda` | float | No | Regularization parameter (default: `RuntimeValue(float: 8000)`) |
| `sigma` | float | No | Sigma color parameter (default: `RuntimeValue(float: 1.5)`) |

**Example:**

```vsp
disparity_wls_filter(8000, 1.5)
```

**Tags:** `disparity`, `filter`, `wls`

---

### `draw_chessboard_corners()`

Draws detected chessboard corners on image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `pattern_width` | int | Yes | Pattern width |
| `pattern_height` | int | Yes | Pattern height |
| `corners_cache` | string | No | Corners cache ID (default: `RuntimeValue(string: "corners")`) |

**Example:**

```vsp
draw_chessboard_corners(9, 6, "corners")
```

**Tags:** `calibration`, `chessboard`, `draw`

---

### `find_chessboard_corners()`

Finds chessboard corners for calibration

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `pattern_width` | int | Yes | Number of inner corners per row |
| `pattern_height` | int | Yes | Number of inner corners per column |
| `flags` | string | No | Flags: adaptive_thresh, normalize_image, filter_quads, fast_check (default: `RuntimeValue(string: "adaptive_thresh+normalize_image")`) |
| `cache_id` | string | No | Cache ID for corners (default: `RuntimeValue(string: "corners")`) |

**Example:**

```vsp
find_chessboard_corners(9, 6, "adaptive_thresh+normalize_image", "corners")
```

**Tags:** `calibration`, `chessboard`, `corners`

---

### `find_circles_grid()`

Finds circle grid centers for calibration

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `pattern_width` | int | Yes | Number of circles per row |
| `pattern_height` | int | Yes | Number of circles per column |
| `symmetric` | bool | No | Symmetric grid pattern (default: `RuntimeValue(bool: true)`) |
| `cache_id` | string | No | Cache ID for centers (default: `RuntimeValue(string: "centers")`) |

**Example:**

```vsp
find_circles_grid(7, 5, true, "centers")
```

**Tags:** `calibration`, `circles`, `grid`

---

### `find_essential_mat()`

Calculates essential matrix from point correspondences

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `points1_cache` | string | Yes | First image points cache |
| `points2_cache` | string | Yes | Second image points cache |
| `cam_matrix` | string | Yes | Camera matrix cache |
| `method` | string | No | Method: ransac, lmeds (default: `RuntimeValue(string: "ransac")`) |
| `prob` | float | No | Confidence probability (default: `RuntimeValue(float: 0.999)`) |
| `threshold` | float | No | RANSAC threshold (default: `RuntimeValue(float: 1)`) |
| `cache_id` | string | No | Cache ID for E matrix (default: `RuntimeValue(string: "E")`) |

**Example:**

```vsp
find_essential_mat("pts1", "pts2", "K", "ransac", 0.999, 1.0, "E")
```

**Tags:** `essential`, `epipolar`, `geometry`

---

### `find_fundamental_mat()`

Calculates fundamental matrix from point correspondences

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `points1_cache` | string | Yes | First image points cache |
| `points2_cache` | string | Yes | Second image points cache |
| `method` | string | No | Method: 7point, 8point, ransac, lmeds (default: `RuntimeValue(string: "ransac")`) |
| `ransac_reproj_threshold` | float | No | RANSAC threshold (default: `RuntimeValue(float: 3)`) |
| `confidence` | float | No | Confidence level (default: `RuntimeValue(float: 0.99)`) |
| `cache_id` | string | No | Cache ID for F matrix (default: `RuntimeValue(string: "F")`) |

**Example:**

```vsp
find_fundamental_mat("pts1", "pts2", "ransac", 3.0, 0.99, "F")
```

**Tags:** `fundamental`, `epipolar`, `geometry`

---

### `init_undistort_rectify_map()`

Computes undistortion and rectification maps

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cam_matrix` | string | Yes | Camera matrix cache |
| `dist_coeffs` | string | Yes | Distortion coeffs cache |
| `R` | string | Yes | Rectification transform cache |
| `new_cam_matrix` | string | Yes | New camera matrix cache |
| `cache_prefix` | string | No | Output maps cache prefix (default: `RuntimeValue(string: "map")`) |

**Example:**

```vsp
init_undistort_rectify_map("K", "D", "R", "P", "map")
```

**Tags:** `undistort`, `rectify`, `map`

---

### `normalize_disparity()`

Normalizes disparity map for visualization

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `min_val` | float | No | Minimum value (default: `RuntimeValue(float: 0)`) |
| `max_val` | float | No | Maximum value (default: `RuntimeValue(float: 255)`) |

**Example:**

```vsp
normalize_disparity(0, 255)
```

**Tags:** `disparity`, `normalize`, `visualization`

---

### `project_points()`

Projects 3D points to image plane

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `object_points` | string | Yes | 3D object points cache |
| `rvec` | string | Yes | Rotation vector cache |
| `tvec` | string | Yes | Translation vector cache |
| `cam_matrix` | string | Yes | Camera matrix cache |
| `dist_coeffs` | string | Yes | Distortion coeffs cache |
| `cache_id` | string | No | Output 2D points cache (default: `RuntimeValue(string: "proj_points")`) |

**Example:**

```vsp
project_points("obj_pts", "rvec", "tvec", "K", "D", "proj_pts")
```

**Tags:** `project`, `3d`, `2d`

---

### `recover_pose()`

Recovers rotation and translation from essential matrix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `E_cache` | string | Yes | Essential matrix cache |
| `points1_cache` | string | Yes | First image points cache |
| `points2_cache` | string | Yes | Second image points cache |
| `cam_matrix` | string | Yes | Camera matrix cache |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "pose")`) |

**Example:**

```vsp
recover_pose("E", "pts1", "pts2", "K", "pose")
```

**Tags:** `pose`, `rotation`, `translation`

---

### `reproject_image_to_3d()`

Reprojects disparity image to 3D space

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `q_cache` | string | Yes | Q matrix cache ID |
| `handle_missing` | bool | No | Handle missing values (default: `RuntimeValue(bool: false)`) |
| `ddepth` | string | No | Output depth: 32f, 16s (default: `RuntimeValue(string: "32f")`) |

**Example:**

```vsp
reproject_image_to_3d("Q", true)
```

**Tags:** `stereo`, `3d`, `reproject`

---

### `solve_pnp()`

Finds object pose from 3D-2D correspondences

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `object_points` | string | Yes | 3D object points cache |
| `image_points` | string | Yes | 2D image points cache |
| `cam_matrix` | string | Yes | Camera matrix cache |
| `dist_coeffs` | string | Yes | Distortion coeffs cache |
| `method` | string | No | Method: iterative, p3p, ap3p, epnp, ippe, ippe_square, sqpnp (default: `RuntimeValue(string: "iterative")`) |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "pnp")`) |

**Example:**

```vsp
solve_pnp("obj_pts", "img_pts", "K", "D", "iterative", "pnp")
```

**Tags:** `pnp`, `pose`, `estimation`

---

### `solve_pnp_ransac()`

Finds object pose using RANSAC

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `object_points` | string | Yes | 3D object points cache |
| `image_points` | string | Yes | 2D image points cache |
| `cam_matrix` | string | Yes | Camera matrix cache |
| `dist_coeffs` | string | Yes | Distortion coeffs cache |
| `iterations` | int | No | RANSAC iterations (default: `RuntimeValue(int: 100)`) |
| `reproj_error` | float | No | Reprojection error threshold (default: `RuntimeValue(float: 8)`) |
| `confidence` | float | No | Confidence (default: `RuntimeValue(float: 0.99)`) |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "pnp_ransac")`) |

**Example:**

```vsp
solve_pnp_ransac("obj_pts", "img_pts", "K", "D", 100, 8.0, 0.99, "pnp")
```

**Tags:** `pnp`, `pose`, `ransac`

---

### `stereo_bm()`

Computes disparity using Block Matching algorithm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `right_cache` | string | Yes | Right image cache ID |
| `num_disparities` | int | No | Number of disparities (multiple of 16) (default: `RuntimeValue(int: 64)`) |
| `block_size` | int | No | Block size (odd number) (default: `RuntimeValue(int: 15)`) |
| `prefilter_type` | string | No | Type: normalized_response, xsobel (default: `RuntimeValue(string: "normalized_response")`) |
| `prefilter_size` | int | No | Prefilter size (5-255, odd) (default: `RuntimeValue(int: 9)`) |
| `prefilter_cap` | int | No | Prefilter cap (1-63) (default: `RuntimeValue(int: 31)`) |
| `texture_threshold` | int | No | Texture threshold (default: `RuntimeValue(int: 10)`) |
| `uniqueness_ratio` | int | No | Uniqueness ratio (default: `RuntimeValue(int: 15)`) |
| `speckle_window_size` | int | No | Speckle filter window (default: `RuntimeValue(int: 100)`) |
| `speckle_range` | int | No | Speckle filter range (default: `RuntimeValue(int: 32)`) |
| `disp12_max_diff` | int | No | Left-right disparity check (default: `RuntimeValue(int: 1)`) |

**Example:**

```vsp
stereo_bm("right", 64, 15)
```

**Tags:** `stereo`, `disparity`, `bm`

---

### `stereo_calibrate()`

Calibrates stereo camera setup

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `left_cache` | string | Yes | Left camera matrix cache |
| `right_cache` | string | Yes | Right camera matrix cache |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "stereo")`) |

**Example:**

```vsp
stereo_calibrate("left_cam", "right_cam", "stereo")
```

**Tags:** `stereo`, `calibration`

---

### `stereo_rectify()`

Computes rectification transforms for stereo cameras

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cam_matrix1` | string | Yes | Left camera matrix cache |
| `dist_coeffs1` | string | Yes | Left distortion coeffs cache |
| `cam_matrix2` | string | Yes | Right camera matrix cache |
| `dist_coeffs2` | string | Yes | Right distortion coeffs cache |
| `R` | string | Yes | Rotation matrix cache |
| `T` | string | Yes | Translation vector cache |
| `cache_prefix` | string | No | Output cache prefix (default: `RuntimeValue(string: "rect")`) |

**Example:**

```vsp
stereo_rectify("K1", "D1", "K2", "D2", "R", "T", "rect")
```

**Tags:** `stereo`, `rectify`

---

### `stereo_sgbm()`

Computes disparity using Semi-Global Block Matching

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `right_cache` | string | Yes | Right image cache ID |
| `min_disparity` | int | No | Minimum disparity (default: `RuntimeValue(int: 0)`) |
| `num_disparities` | int | No | Number of disparities (multiple of 16) (default: `RuntimeValue(int: 64)`) |
| `block_size` | int | No | Block size (odd number, 1-11) (default: `RuntimeValue(int: 5)`) |
| `p1` | int | No | P1 penalty (small disparity change) (default: `RuntimeValue(int: 0)`) |
| `p2` | int | No | P2 penalty (large disparity change) (default: `RuntimeValue(int: 0)`) |
| `disp12_max_diff` | int | No | Left-right check threshold (default: `RuntimeValue(int: 1)`) |
| `prefilter_cap` | int | No | Prefilter cap (default: `RuntimeValue(int: 63)`) |
| `uniqueness_ratio` | int | No | Uniqueness ratio (default: `RuntimeValue(int: 10)`) |
| `speckle_window_size` | int | No | Speckle filter window (default: `RuntimeValue(int: 100)`) |
| `speckle_range` | int | No | Speckle filter range (default: `RuntimeValue(int: 32)`) |
| `mode` | string | No | Mode: sgbm, hh, sgbm_3way, hh4 (default: `RuntimeValue(string: "sgbm")`) |

**Example:**

```vsp
stereo_sgbm("right", 0, 64, 5)
```

**Tags:** `stereo`, `disparity`, `sgbm`

---

### `triangulate_points()`

Triangulates 3D points from 2D correspondences

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `proj_mat1` | string | Yes | First projection matrix cache |
| `proj_mat2` | string | Yes | Second projection matrix cache |
| `points1` | string | Yes | First image points cache |
| `points2` | string | Yes | Second image points cache |
| `cache_id` | string | No | Output 4D points cache (default: `RuntimeValue(string: "points4d")`) |

**Example:**

```vsp
triangulate_points("P1", "P2", "pts1", "pts2", "pts4d")
```

**Tags:** `triangulate`, `3d`, `reconstruction`

---

### `undistort()`

Undistorts image using camera parameters

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `cam_matrix` | string | Yes | Camera matrix cache |
| `dist_coeffs` | string | Yes | Distortion coeffs cache |
| `new_cam_matrix` | string | No | New camera matrix cache (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
undistort("K", "D")
```

**Tags:** `undistort`, `calibration`

---

### `undistort_points()`

Undistorts 2D points

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `points_cache` | string | Yes | Points cache |
| `cam_matrix` | string | Yes | Camera matrix cache |
| `dist_coeffs` | string | Yes | Distortion coeffs cache |
| `output_cache` | string | No | Output points cache (default: `RuntimeValue(string: "undist_points")`) |

**Example:**

```vsp
undistort_points("pts", "K", "D", "undist_pts")
```

**Tags:** `undistort`, `points`

---

## tensor

### `mat_col()`

Get matrix column as vector

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |
| `col` | int | Yes | Column index |

**Example:**

```vsp
mat_col(m, 0)
```

**Tags:** `matrix`, `column`, `tensor`

---

### `mat_create()`

Create a matrix from nested arrays

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `data` | array | Yes | 2D array of values |

**Example:**

```vsp
mat_create([[1, 2], [3, 4]])
```

**Tags:** `matrix`, `create`, `tensor`

---

### `mat_det()`

Compute matrix determinant

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |

**Example:**

```vsp
mat_det(m)
```

**Tags:** `matrix`, `determinant`, `tensor`

---

### `mat_diag()`

Create a diagonal matrix from a vector

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `diagonal` | vector | Yes | Diagonal elements |

**Example:**

```vsp
mat_diag(vec(1, 2, 3))
```

**Tags:** `matrix`, `create`, `diagonal`, `tensor`

---

### `mat_eye()`

Create an identity matrix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `size` | int | Yes | Matrix size (NxN) |

**Example:**

```vsp
mat_eye(4)
```

**Tags:** `matrix`, `create`, `identity`, `tensor`

---

### `mat_flatten()`

Flatten matrix to vector

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |

**Example:**

```vsp
mat_flatten(m)
```

**Tags:** `matrix`, `flatten`, `tensor`

---

### `mat_get()`

Get matrix element at (row, col)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |
| `row` | int | Yes | Row index |
| `col` | int | Yes | Column index |

**Example:**

```vsp
mat_get(m, 0, 0)
```

**Tags:** `matrix`, `get`, `index`, `tensor`

---

### `mat_inv()`

Compute matrix inverse

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |

**Example:**

```vsp
mat_inv(m)
```

**Tags:** `matrix`, `inverse`, `tensor`

---

### `mat_ones()`

Create a matrix of ones

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `rows` | int | Yes | Number of rows |
| `cols` | int | Yes | Number of columns |

**Example:**

```vsp
mat_ones(3, 4)
```

**Tags:** `matrix`, `create`, `ones`, `tensor`

---

### `mat_reshape()`

Reshape matrix to new dimensions

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |
| `rows` | int | Yes | New rows |
| `cols` | int | Yes | New columns |

**Example:**

```vsp
mat_reshape(m, 2, 6)
```

**Tags:** `matrix`, `reshape`, `tensor`

---

### `mat_row()`

Get matrix row as vector

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |
| `row` | int | Yes | Row index |

**Example:**

```vsp
mat_row(m, 0)
```

**Tags:** `matrix`, `row`, `tensor`

---

### `mat_set()`

Set matrix element at (row, col)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |
| `row` | int | Yes | Row index |
| `col` | int | Yes | Column index |
| `value` | float | Yes | New value |

**Example:**

```vsp
mat_set(m, 0, 0, 5.0)
```

**Tags:** `matrix`, `set`, `index`, `tensor`

---

### `mat_shape()`

Get matrix dimensions [rows, cols]

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |

**Example:**

```vsp
mat_shape(m)
```

**Tags:** `matrix`, `shape`, `size`, `tensor`

---

### `mat_transpose()`

Transpose matrix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |

**Example:**

```vsp
mat_transpose(m)
```

**Tags:** `matrix`, `transpose`, `tensor`

---

### `mat_zeros()`

Create a zero matrix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `rows` | int | Yes | Number of rows |
| `cols` | int | Yes | Number of columns |

**Example:**

```vsp
mat_zeros(3, 4)
```

**Tags:** `matrix`, `create`, `zeros`, `tensor`

---

### `tensor_add()`

Add two tensors or scalar

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_add(v1, v2) | tensor_add(m, 5.0)
```

**Tags:** `tensor`, `add`, `arithmetic`

---

### `tensor_all()`

Check if all elements are true (non-zero)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Input tensor |

**Example:**

```vsp
tensor_all(v)
```

**Tags:** `tensor`, `logic`, `all`

---

### `tensor_and()`

Element-wise logical AND

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_and(v1, v2)
```

**Tags:** `tensor`, `logic`, `and`

---

### `tensor_any()`

Check if any element is true (non-zero)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Input tensor |

**Example:**

```vsp
tensor_any(v)
```

**Tags:** `tensor`, `logic`, `any`

---

### `tensor_div()`

Divide tensors (element-wise)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_div(v, 2.0)
```

**Tags:** `tensor`, `divide`, `arithmetic`

---

### `tensor_eq()`

Element-wise equality comparison

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_eq(v1, v2)
```

**Tags:** `tensor`, `compare`, `equal`

---

### `tensor_ge()`

Element-wise greater-than-or-equal comparison

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_ge(v1, v2)
```

**Tags:** `tensor`, `compare`, `greater-equal`

---

### `tensor_gt()`

Element-wise greater-than comparison

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_gt(v1, v2)
```

**Tags:** `tensor`, `compare`, `greater-than`

---

### `tensor_le()`

Element-wise less-than-or-equal comparison

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_le(v1, v2)
```

**Tags:** `tensor`, `compare`, `less-equal`

---

### `tensor_lt()`

Element-wise less-than comparison

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_lt(v1, v2)
```

**Tags:** `tensor`, `compare`, `less-than`

---

### `tensor_max()`

Maximum element value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Input tensor |

**Example:**

```vsp
tensor_max(v)
```

**Tags:** `tensor`, `reduce`, `max`

---

### `tensor_mean()`

Mean of all elements

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Input tensor |

**Example:**

```vsp
tensor_mean(v)
```

**Tags:** `tensor`, `reduce`, `mean`

---

### `tensor_min()`

Minimum element value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Input tensor |

**Example:**

```vsp
tensor_min(v)
```

**Tags:** `tensor`, `reduce`, `min`

---

### `tensor_mul()`

Multiply tensors (matrix mul or element-wise)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |
| `mode` | string | No | Mode: matmul, elementwise (default: `RuntimeValue(string: "matmul")`) |

**Example:**

```vsp
tensor_mul(m1, m2) | tensor_mul(v, 2.0)
```

**Tags:** `tensor`, `multiply`, `arithmetic`

---

### `tensor_ne()`

Element-wise not-equal comparison

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_ne(v1, v2)
```

**Tags:** `tensor`, `compare`, `not-equal`

---

### `tensor_not()`

Element-wise logical NOT

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Operand |

**Example:**

```vsp
tensor_not(v)
```

**Tags:** `tensor`, `logic`, `not`

---

### `tensor_or()`

Element-wise logical OR

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_or(v1, v2)
```

**Tags:** `tensor`, `logic`, `or`

---

### `tensor_print()`

Print tensor information

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Input tensor |

**Example:**

```vsp
tensor_print(v)
```

**Tags:** `tensor`, `debug`, `print`

---

### `tensor_sub()`

Subtract two tensors or scalar

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | First operand |
| `b` | any | Yes | Second operand |

**Example:**

```vsp
tensor_sub(v1, v2)
```

**Tags:** `tensor`, `subtract`, `arithmetic`

---

### `tensor_sum()`

Sum of all elements

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `a` | any | Yes | Input tensor |

**Example:**

```vsp
tensor_sum(v)
```

**Tags:** `tensor`, `reduce`, `sum`

---

### `to_cvmat()`

Convert Matrix to cv::Mat

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `m` | matrix | Yes | Input matrix |

**Example:**

```vsp
to_cvmat(m)
```

**Tags:** `tensor`, `convert`, `cvmat`

---

### `to_matrix()`

Convert cv::Mat to Matrix type

**Example:**

```vsp
to_matrix()
```

**Tags:** `tensor`, `convert`, `matrix`

---

### `vec()`

Create a vector from values or array

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `values` | any | No | Values as varargs or array (default: `RuntimeValue(void: void)`) |

**Example:**

```vsp
vec(1, 2, 3, 4) | vec([1.0, 2.0, 3.0])
```

**Tags:** `vector`, `create`, `tensor`

---

### `vec_dot()`

Compute dot product of two vectors

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `v1` | vector | Yes | First vector |
| `v2` | vector | Yes | Second vector |

**Example:**

```vsp
vec_dot(v1, v2)
```

**Tags:** `vector`, `dot`, `product`, `tensor`

---

### `vec_get()`

Get vector element at index

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `v` | vector | Yes | Input vector |
| `index` | int | Yes | Element index |

**Example:**

```vsp
vec_get(v, 0)
```

**Tags:** `vector`, `get`, `index`, `tensor`

---

### `vec_len()`

Get vector length

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `v` | vector | Yes | Input vector |

**Example:**

```vsp
vec_len(v)
```

**Tags:** `vector`, `length`, `size`, `tensor`

---

### `vec_linspace()`

Create a vector with linearly spaced values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `start` | float | Yes | Start value |
| `end` | float | Yes | End value |
| `num` | int | Yes | Number of points |

**Example:**

```vsp
vec_linspace(0, 1, 11)
```

**Tags:** `vector`, `create`, `linspace`, `tensor`

---

### `vec_norm()`

Compute vector norm

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `v` | vector | Yes | Input vector |
| `type` | string | No | Norm type: l1, l2, inf (default: `RuntimeValue(string: "l2")`) |

**Example:**

```vsp
vec_norm(v) | vec_norm(v, "l1")
```

**Tags:** `vector`, `norm`, `tensor`

---

### `vec_normalize()`

Normalize vector to unit length

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `v` | vector | Yes | Input vector |

**Example:**

```vsp
vec_normalize(v)
```

**Tags:** `vector`, `normalize`, `tensor`

---

### `vec_ones()`

Create a vector of ones

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `size` | int | Yes | Vector size |

**Example:**

```vsp
vec_ones(10)
```

**Tags:** `vector`, `create`, `ones`, `tensor`

---

### `vec_range()`

Create a vector with range of values

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `start` | float | Yes | Start value |
| `end` | float | Yes | End value (exclusive) |
| `step` | float | No | Step size (default: `RuntimeValue(float: 1)`) |

**Example:**

```vsp
vec_range(0, 10) | vec_range(0, 10, 2)
```

**Tags:** `vector`, `create`, `range`, `tensor`

---

### `vec_set()`

Set vector element at index

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `v` | vector | Yes | Input vector |
| `index` | int | Yes | Element index |
| `value` | float | Yes | New value |

**Example:**

```vsp
vec_set(v, 0, 5.0)
```

**Tags:** `vector`, `set`, `index`, `tensor`

---

### `vec_slice()`

Get vector slice

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `v` | vector | Yes | Input vector |
| `start` | int | Yes | Start index |
| `end` | int | Yes | End index (exclusive) |

**Example:**

```vsp
vec_slice(v, 0, 5)
```

**Tags:** `vector`, `slice`, `tensor`

---

### `vec_zeros()`

Create a zero vector

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `size` | int | Yes | Vector size |

**Example:**

```vsp
vec_zeros(10)
```

**Tags:** `vector`, `create`, `zeros`, `tensor`

---

## transform

### `build_pyramid()`

Builds a Gaussian pyramid and caches all levels

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `max_level` | int | Yes | Number of pyramid levels |
| `prefix` | string | No | Cache prefix for levels (default: `RuntimeValue(string: "pyr")`) |

**Example:**

```vsp
build_pyramid(4, "pyr")  // Creates pyr_0, pyr_1, pyr_2, pyr_3
```

**Tags:** `pyramid`, `build`, `levels`

---

### `convert_maps()`

Converts floating-point maps to fixed-point for faster remapping

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `map1` | string | Yes | Cache ID of first input map |
| `map2` | string | Yes | Cache ID of second input map |
| `dstmap1type` | int | No | Type of first output map (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
convert_maps("map1", "map2")
```

**Tags:** `remap`, `convert`, `optimize`

---

### `copy_make_border()`

Adds border around the image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `top` | int | Yes | Top border width |
| `bottom` | int | Yes | Bottom border width |
| `left` | int | Yes | Left border width |
| `right` | int | Yes | Right border width |
| `border_type` | string | No | Type: constant, replicate, reflect, wrap, reflect_101 (default: `RuntimeValue(string: "constant")`) |
| `value` | int | No | Border value for constant type (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
copy_make_border(10, 10, 10, 10, "constant", 255)
```

**Tags:** `border`, `transform`, `padding`

---

### `crop()`

Crops a rectangular region from the image

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | int | Yes | Left X coordinate |
| `y` | int | Yes | Top Y coordinate |
| `width` | int | Yes | Width of crop region |
| `height` | int | Yes | Height of crop region |

**Example:**

```vsp
crop(100, 100, 200, 200)
```

**Tags:** `crop`, `transform`, `roi`

---

### `flip()`

Flips image around vertical, horizontal, or both axes

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `flip_code` | string | Yes | Flip: horizontal, vertical, both (or h, v, hv) |

**Example:**

```vsp
flip("horizontal") | flip("v")
```

**Tags:** `flip`, `transform`, `mirror`

---

### `get_affine_transform()`

Calculates affine transform from 3 point correspondences

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `src_points` | array | Yes | Source points [[x1,y1], [x2,y2], [x3,y3]] |
| `dst_points` | array | Yes | Destination points [[x1,y1], [x2,y2], [x3,y3]] |
| `cache_id` | string | No | Cache ID to store matrix (default: `RuntimeValue(string: "affine_matrix")`) |

**Example:**

```vsp
get_affine_transform(src, dst, "my_affine")
```

**Tags:** `affine`, `transform`, `matrix`

---

### `get_perspective_transform()`

Calculates perspective transform from 4 point correspondences

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `src_points` | array | Yes | Source points [[x1,y1], [x2,y2], [x3,y3], [x4,y4]] |
| `dst_points` | array | Yes | Destination points |
| `cache_id` | string | No | Cache ID to store matrix (default: `RuntimeValue(string: "perspective_matrix")`) |

**Example:**

```vsp
get_perspective_transform(src, dst, "homography")
```

**Tags:** `perspective`, `transform`, `homography`, `matrix`

---

### `get_rotation_matrix_2d()`

Calculates 2D rotation matrix

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `angle` | float | Yes | Rotation angle in degrees |
| `center_x` | float | No | Center X (default: image center) (default: `RuntimeValue(float: -1)`) |
| `center_y` | float | No | Center Y (default: image center) (default: `RuntimeValue(float: -1)`) |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |
| `cache_id` | string | No | Cache ID to store matrix (default: `RuntimeValue(string: "rotation_matrix")`) |

**Example:**

```vsp
get_rotation_matrix_2d(45, -1, -1, 1.0, "rot")
```

**Tags:** `rotation`, `matrix`, `transform`

---

### `hstack()`

Stacks images horizontally

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `other` | string | Yes | Cache ID of image to stack on the right |

**Example:**

```vsp
hstack("right_image")
```

**Tags:** `stack`, `horizontal`, `concatenate`

---

### `invert_affine_transform()`

Inverts an affine transformation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `matrix` | string | Yes | Cache ID of affine matrix to invert |
| `cache_id` | string | No | Cache ID for inverted matrix (default: `RuntimeValue(string: "inverted_affine")`) |

**Example:**

```vsp
invert_affine_transform("affine", "affine_inv")
```

**Tags:** `affine`, `invert`, `transform`

---

### `mosaic()`

Creates a mosaic/grid from multiple cached images

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `images` | array | Yes | Array of cache IDs |
| `cols` | int | No | Number of columns (default: auto) (default: `RuntimeValue(int: -1)`) |
| `cell_width` | int | No | Cell width (default: auto) (default: `RuntimeValue(int: -1)`) |
| `cell_height` | int | No | Cell height (default: auto) (default: `RuntimeValue(int: -1)`) |

**Example:**

```vsp
mosaic(["img1", "img2", "img3", "img4"], 2)
```

**Tags:** `mosaic`, `grid`, `layout`

---

### `pyr_down()`

Blurs and downsamples image (Gaussian pyramid down)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dst_width` | int | No | Output width (default: half) (default: `RuntimeValue(int: -1)`) |
| `dst_height` | int | No | Output height (default: half) (default: `RuntimeValue(int: -1)`) |

**Example:**

```vsp
pyr_down() | pyr_down(320, 240)
```

**Tags:** `pyramid`, `downsample`, `blur`

---

### `pyr_up()`

Upsamples and blurs image (Gaussian pyramid up)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dst_width` | int | No | Output width (default: double) (default: `RuntimeValue(int: -1)`) |
| `dst_height` | int | No | Output height (default: double) (default: `RuntimeValue(int: -1)`) |

**Example:**

```vsp
pyr_up()
```

**Tags:** `pyramid`, `upsample`, `blur`

---

### `remap()`

Applies generic geometric transformation using lookup maps

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `map1` | string | Yes | Cache ID of first map (x coordinates) |
| `map2` | string | Yes | Cache ID of second map (y coordinates) |
| `interpolation` | string | No | Interpolation method (default: `RuntimeValue(string: "linear")`) |
| `border_mode` | string | No | Border mode (default: `RuntimeValue(string: "constant")`) |

**Example:**

```vsp
remap("map_x", "map_y")
```

**Tags:** `remap`, `transform`, `distortion`

---

### `resize()`

Resizes the current frame

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `width` | int | Yes | Target width in pixels (0 = compute from scale) |
| `height` | int | Yes | Target height in pixels (0 = compute from scale) |
| `interpolation` | string | No | Interpolation: nearest, linear, cubic, area, lanczos, linear_exact (default: `RuntimeValue(string: "linear")`) |
| `fx` | float | No | Scale factor along X (when width=0) (default: `RuntimeValue(float: 0)`) |
| `fy` | float | No | Scale factor along Y (when height=0) (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
resize(640, 480) | resize(0, 0, "linear", 0.5, 0.5)
```

**Tags:** `resize`, `transform`, `scale`

---

### `rotate()`

Rotates image by an arbitrary angle

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `angle` | float | Yes | Rotation angle in degrees |
| `center_x` | float | No | Center X (default: image center) (default: `RuntimeValue(float: -1)`) |
| `center_y` | float | No | Center Y (default: image center) (default: `RuntimeValue(float: -1)`) |
| `scale` | float | No | Scale factor (default: `RuntimeValue(float: 1)`) |
| `border_mode` | string | No | Border: constant, replicate, reflect, wrap (default: `RuntimeValue(string: "constant")`) |
| `border_value` | int | No | Border color for constant mode (default: `RuntimeValue(int: 0)`) |

**Example:**

```vsp
rotate(45) | rotate(90, -1, -1, 1.0)
```

**Tags:** `rotate`, `transform`

---

### `rotate90()`

Rotates image by 90, 180, or 270 degrees

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `code` | string | Yes | Rotation: 90, 180, 270, or cw, ccw, 180 |

**Example:**

```vsp
rotate90("90") | rotate90("cw")
```

**Tags:** `rotate`, `transform`, `90`

---

### `transpose()`

Transposes the image (swaps rows and columns)

**Example:**

```vsp
transpose()
```

**Tags:** `transpose`, `transform`

---

### `vstack()`

Stacks images vertically

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `other` | string | Yes | Cache ID of image to stack below |

**Example:**

```vsp
vstack("bottom_image")
```

**Tags:** `stack`, `vertical`, `concatenate`

---

### `warp_affine()`

Applies affine transformation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `matrix` | string | Yes | Cache ID of 2x3 transformation matrix |
| `width` | int | No | Output width (default: same as input) (default: `RuntimeValue(int: -1)`) |
| `height` | int | No | Output height (default: same as input) (default: `RuntimeValue(int: -1)`) |
| `interpolation` | string | No | Interpolation method (default: `RuntimeValue(string: "linear")`) |
| `border_mode` | string | No | Border mode (default: `RuntimeValue(string: "constant")`) |

**Example:**

```vsp
warp_affine("transform_matrix")
```

**Tags:** `warp`, `affine`, `transform`

---

### `warp_perspective()`

Applies perspective transformation

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `matrix` | string | Yes | Cache ID of 3x3 transformation matrix |
| `width` | int | No | Output width (default: `RuntimeValue(int: -1)`) |
| `height` | int | No | Output height (default: `RuntimeValue(int: -1)`) |
| `interpolation` | string | No | Interpolation method (default: `RuntimeValue(string: "linear")`) |
| `border_mode` | string | No | Border mode (default: `RuntimeValue(string: "constant")`) |

**Example:**

```vsp
warp_perspective("homography")
```

**Tags:** `warp`, `perspective`, `transform`, `homography`

---

## video_io

### `get_camera_capabilities()`

Gets camera capabilities and supported properties

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |

**Example:**

```vsp
get_camera_capabilities("0")
```

**Tags:** `camera`, `info`, `capabilities`

---

### `get_video_prop()`

Gets a video capture property

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Video source identifier |
| `prop` | string | Yes | Property: width, height, fps, frame_count, pos_frames, pos_msec, pos_avi_ratio, fourcc, format, brightness, contrast, saturation, hue, gain, exposure, auto_exposure, focus, autofocus, zoom |

**Example:**

```vsp
get_video_prop("0", "width")
```

**Tags:** `video`, `property`, `get`

---

### `imdecode()`

Decodes image from memory buffer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `buffer` | array | Yes | Image data buffer |
| `mode` | string | No | Decode mode: color, grayscale, unchanged (default: `RuntimeValue(string: "color")`) |

**Example:**

```vsp
imdecode(buffer, "color")
```

**Tags:** `image`, `decode`, `memory`

---

### `imencode()`

Encodes image to memory buffer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ext` | string | Yes | Image format extension (.jpg, .png, etc.) |
| `quality` | int | No | Encoding quality (default: `RuntimeValue(int: 95)`) |

**Example:**

```vsp
imencode(".jpg", 90)
```

**Tags:** `image`, `encode`, `memory`

---

### `list_cameras()`

Lists all available camera devices

**Example:**

```vsp
list_cameras()
```

**Tags:** `camera`, `list`, `enumerate`

---

### `load_camera_profile()`

Loads camera settings from a profile file

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `path` | string | Yes | Profile file path (.xml or .json) |

**Example:**

```vsp
load_camera_profile("0", "camera_profile.xml")
```

**Tags:** `camera`, `profile`, `load`, `settings`

---

### `load_image()`

Loads an image from a file

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `filename` | string | Yes | Input image file path |
| `mode` | string | No | Load mode: color, grayscale, unchanged, anydepth, anycolor (default: `RuntimeValue(string: "color")`) |

**Example:**

```vsp
load_image("input.png") | load_image("depth.tiff", "unchanged")
```

**Tags:** `image`, `input`, `load`, `read`

---

### `save_camera_profile()`

Saves camera settings to a profile file

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `path` | string | Yes | Profile file path (.xml or .json) |

**Example:**

```vsp
save_camera_profile("0", "camera_profile.xml")
```

**Tags:** `camera`, `profile`, `save`, `settings`

---

### `save_image()`

Saves the current frame to an image file

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `filename` | string | Yes | Output image file path |
| `quality` | int | No | JPEG quality (0-100) or PNG compression (0-9) (default: `RuntimeValue(int: 95)`) |

**Example:**

```vsp
save_image("frame.png") | save_image("frame.jpg", 90)
```

**Tags:** `image`, `output`, `save`, `write`

---

### `set_camera_backend()`

Sets camera backend API and buffer preferences

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `backend` | string | No | Backend: dshow, msmf, v4l2, avfoundation, gstreamer, ffmpeg (default: `RuntimeValue(string: "auto")`) |
| `buffer_size` | int | No | Buffer size (1-100 frames) (default: `RuntimeValue(int: 4)`) |

**Example:**

```vsp
set_camera_backend("0", "dshow", 1)
```

**Tags:** `camera`, `backend`, `buffer`, `control`

---

### `set_camera_exposure()`

Sets camera exposure mode and value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `mode` | string | No | Exposure mode: manual, auto, aperture_priority, shutter_priority (default: `RuntimeValue(string: "auto")`) |
| `value` | float | No | Exposure value (-13 to 13 for manual, 0-1 for auto) (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
set_camera_exposure("0", "manual", -5)
```

**Tags:** `camera`, `exposure`, `control`

---

### `set_camera_focus()`

Sets camera focus mode and value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `mode` | string | No | Focus mode: auto, manual, continuous, infinity, macro (default: `RuntimeValue(string: "auto")`) |
| `value` | int | No | Focus value (0-255, manual mode only) (default: `RuntimeValue(int: 128)`) |

**Example:**

```vsp
set_camera_focus("0", "manual", 200)
```

**Tags:** `camera`, `focus`, `control`

---

### `set_camera_image_adjustment()`

Sets camera image quality parameters

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `brightness` | float | No | Brightness (-100 to 100) (default: `RuntimeValue(float: 0)`) |
| `contrast` | float | No | Contrast (-100 to 100) (default: `RuntimeValue(float: 0)`) |
| `saturation` | float | No | Saturation (-100 to 100) (default: `RuntimeValue(float: 0)`) |
| `hue` | float | No | Hue (-180 to 180) (default: `RuntimeValue(float: 0)`) |
| `sharpness` | float | No | Sharpness (0-100) (default: `RuntimeValue(float: 50)`) |
| `gamma` | float | No | Gamma (50-300) (default: `RuntimeValue(float: 100)`) |

**Example:**

```vsp
set_camera_image_adjustment("0", 10, 20, 0, 0, 60, 100)
```

**Tags:** `camera`, `brightness`, `contrast`, `saturation`, `control`

---

### `set_camera_iso()`

Sets camera ISO/gain value

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `mode` | string | No | ISO mode: auto, manual (default: `RuntimeValue(string: "auto")`) |
| `value` | int | No | ISO value (100-6400) or gain (0-100) (default: `RuntimeValue(int: 400)`) |

**Example:**

```vsp
set_camera_iso("0", "manual", 800)
```

**Tags:** `camera`, `iso`, `gain`, `control`

---

### `set_camera_trigger()`

Sets camera trigger and strobe controls

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `trigger_mode` | string | No | Trigger mode: off, software, hardware (default: `RuntimeValue(string: "off")`) |
| `trigger_delay` | int | No | Delay in microseconds (0-1000000) (default: `RuntimeValue(int: 0)`) |
| `strobe_enabled` | bool | No | Enable strobe/flash (default: `RuntimeValue(bool: false)`) |

**Example:**

```vsp
set_camera_trigger("0", "software", 1000, false)
```

**Tags:** `camera`, `trigger`, `strobe`, `control`

---

### `set_camera_white_balance()`

Sets camera white balance mode and temperature

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `mode` | string | No | WB mode: auto, manual, daylight, cloudy, tungsten, fluorescent (default: `RuntimeValue(string: "auto")`) |
| `temperature` | int | No | Color temperature (2000-10000K) (default: `RuntimeValue(int: 5500)`) |

**Example:**

```vsp
set_camera_white_balance("0", "manual", 6500)
```

**Tags:** `camera`, `white_balance`, `control`

---

### `set_camera_zoom()`

Sets camera zoom, pan, and tilt

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Camera source identifier |
| `zoom` | float | No | Zoom level (1.0-10.0) (default: `RuntimeValue(float: 1)`) |
| `pan` | float | No | Pan angle (-180 to 180) (default: `RuntimeValue(float: 0)`) |
| `tilt` | float | No | Tilt angle (-90 to 90) (default: `RuntimeValue(float: 0)`) |

**Example:**

```vsp
set_camera_zoom("0", 2.5, 30, 0)
```

**Tags:** `camera`, `zoom`, `pan`, `tilt`, `control`

---

### `set_video_prop()`

Sets a video capture property

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Video source identifier |
| `prop` | string | Yes | Property name |
| `value` | float | Yes | Property value |

**Example:**

```vsp
set_video_prop("0", "exposure", -5)
```

**Tags:** `video`, `property`, `set`

---

### `video_cap()`

Captures video frames from a source (camera, file, or URL)

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | any | Yes | Source: device index, file path, or URL |
| `api` | string | No | API backend: auto, dshow, v4l2, ffmpeg (default: `RuntimeValue(string: "auto")`) |

**Example:**

```vsp
video_cap(0) | video_cap("video.mp4")
```

**Tags:** `video`, `capture`, `input`, `camera`

---

### `video_close()`

Closes a video capture or writer

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | No | Source identifier to close (empty = all) (default: `RuntimeValue(string: "")`) |

**Example:**

```vsp
video_close("0") | video_close()
```

**Tags:** `video`, `close`, `release`

---

### `video_seek()`

Seeks to a specific position in a video file

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source` | string | Yes | Video source identifier |
| `position` | float | Yes | Position (frame number or msec) |
| `unit` | string | No | Position unit: frames, msec, ratio (default: `RuntimeValue(string: "frames")`) |

**Example:**

```vsp
video_seek("video.mp4", 100, "frames")
```

**Tags:** `video`, `seek`, `position`

---

### `video_write()`

Writes frames to a video file

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `filename` | string | Yes | Output video file path |
| `fps` | float | No | Frames per second (default: `RuntimeValue(float: 30)`) |
| `fourcc` | string | No | FourCC codec code (default: `RuntimeValue(string: "MJPG")`) |
| `is_color` | bool | No | Write as color video (default: `RuntimeValue(bool: true)`) |

**Example:**

```vsp
video_write("output.avi", 30, "MJPG")
```

**Tags:** `video`, `output`, `write`, `record`

---


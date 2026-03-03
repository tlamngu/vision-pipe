/*
 * Malvar-He-Cutler high-quality linear interpolation demosaicing kernel.
 *
 * Based on:
 *   "HIGH-QUALITY LINEAR INTERPOLATION FOR DEMOSAICING OF
 *    BAYER-PATTERNED COLOR IMAGES"
 *   Henrique S. Malvar, Li-wei He, and Ross Cutler, 2004.
 *
 * Adapted from cldemosaic: https://github.com/nevion/cldemosaic
 * Copyright 2015 Jason Newton <nevion@gmail.com>  (MIT License)
 *
 * Modified to be self-contained (no external clcommons headers) and
 * to match OpenCV cv::ocl::Kernel argument convention:
 *
 *   KernelArg::ReadOnly(umat)  -> ptr, step_bytes, offset_bytes, rows, cols
 *   KernelArg::WriteOnly(umat) -> ptr, step_bytes, offset_bytes, rows, cols
 *
 * Compile-time macros:
 *   TILE_ROWS  - work-group height (default 4)
 *   TILE_COLS  - work-group width  (default 32)
 *
 * Output: BGR byte order (OpenCV convention).
 */

#ifndef TILE_ROWS
#define TILE_ROWS 4
#endif
#ifndef TILE_COLS
#define TILE_COLS 32
#endif

#define KERNEL_SIZE 5
#define HALF_K      2   /* KERNEL_SIZE / 2 */

#define APRON_ROWS  (TILE_ROWS + KERNEL_SIZE - 1)
#define APRON_COLS  (TILE_COLS + KERNEL_SIZE - 1)
#define N_APRON     (APRON_ROWS * APRON_COLS)
#define N_TILE      (TILE_ROWS * TILE_COLS)

/* Bayer pattern enum (identical to cldemosaic) */
#define BAYER_RGGB 0
#define BAYER_GRBG 1
#define BAYER_GBRG 2
#define BAYER_BGGR 3

/*
 * Reflect-101 border (OpenCV BORDER_REFLECT_101):
 *   idx=-1 -> 1, idx=-2 -> 2, idx=len -> len-2, etc.
 */
static inline int reflect101(int idx, int len)
{
    /* Odd reflection: distance from nearest edge pixel */
    if (idx < 0)    idx = -idx;
    if (idx >= len) idx = 2 * (len - 1) - idx;
    return idx;
}

/*
 * Tile-based Malvar-He-Cutler demosaicing kernel.
 *
 * Arguments (OpenCV KernelArg::ReadOnly/WriteOnly order):
 *   src        : __global const uchar*  - single-channel Bayer input
 *   src_step   : int                    - src bytes per row
 *   src_offset : int                    - byte offset into src buffer
 *   src_rows   : int                    - image height
 *   src_cols   : int                    - image width
 *   dst        : __global uchar*        - 3-channel BGR output
 *   dst_step   : int                    - dst bytes per row
 *   dst_offset : int                    - byte offset into dst buffer
 *   dst_rows   : int                    - (unused, same as src_rows)
 *   dst_cols   : int                    - (unused, same as src_cols)
 *   pattern    : int                    - Bayer enum above
 */
__kernel
__attribute__((reqd_work_group_size(TILE_COLS, TILE_ROWS, 1)))
void malvar_he_cutler_demosaic(
    __global const uchar* src, int src_step, int src_offset,
    int src_rows, int src_cols,
    __global       uchar* dst, int dst_step, int dst_offset,
    int dst_rows,  int dst_cols,
    int pattern)
{
    const int tile_col_block = get_group_id(0);
    const int tile_row_block = get_group_id(1);
    const int tile_col       = get_local_id(0);
    const int tile_row       = get_local_id(1);
    const int g_c            = get_global_id(0);
    const int g_r            = get_global_id(1);

    const bool valid = (g_r < src_rows) & (g_c < src_cols);

    /* ---- Fill shared-memory apron ---- */
    __local int apron[APRON_ROWS][APRON_COLS];

    const int flat_id = tile_row * TILE_COLS + tile_col;

    for (int task = flat_id; task < N_APRON; task += N_TILE)
    {
        const int ar  = task / APRON_COLS;
        const int ac  = task % APRON_COLS;

        int gr = (ar + tile_row_block * TILE_ROWS) - HALF_K;
        int gc = (ac + tile_col_block * TILE_COLS) - HALF_K;

        gr = reflect101(gr, src_rows);
        gc = reflect101(gc, src_cols);

        apron[ar][ac] = (int)src[src_offset + gr * src_step + gc];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    if (!valid) return;

    /* Apron coordinates for this pixel */
    const int a_c = tile_col + HALF_K;
    const int a_r = tile_row + HALF_K;

    /* i = col, j = row — paper uses this convention */
    const int i = a_c;
    const int j = a_r;

#define F(_i, _j)  apron[(_j)][(_i)]

    const int Fij = F(i, j);

    /* R1: symmetric 4,2,-1 cross — Green at R or B */
    const int R1 =
        (4*F(i,j)
         + 2*(F(i-1,j) + F(i+1,j) + F(i,j-1) + F(i,j+1))
         - F(i-2,j) - F(i+2,j) - F(i,j-2) - F(i,j+2)
        ) / 8;

    /* R2: left-right symmetric (theta) — R at G in red row, B at G in blue row */
    const int R2 =
        (  8*(F(i-1,j) + F(i+1,j))
         + 10*F(i,j)
         + F(i,j-2) + F(i,j+2)
         - 2*(F(i-1,j-1) + F(i+1,j-1) + F(i-1,j+1) + F(i+1,j+1)
              + F(i-2,j)  + F(i+2,j))
        ) / 16;

    /* R3: top-bottom symmetric (phi) — R at G in blue row, B at G in red row */
    const int R3 =
        (  8*(F(i,j-1) + F(i,j+1))
         + 10*F(i,j)
         + F(i-2,j) + F(i+2,j)
         - 2*(F(i-1,j-1) + F(i+1,j-1) + F(i-1,j+1) + F(i+1,j+1)
              + F(i,j-2)  + F(i,j+2))
        ) / 16;

    /* R4: symmetric 3/2 checker — R at B, B at R */
    const int R4 =
        (  12*F(i,j)
         - 3*(F(i-2,j) + F(i+2,j) + F(i,j-2) + F(i,j+2))
         + 4*(F(i-1,j-1) + F(i+1,j-1) + F(i-1,j+1) + F(i+1,j+1))
        ) / 16;

#undef F

    /* ---- Determine which sub-pixel this is ---- */
    const int red_col  = (pattern == BAYER_GRBG) | (pattern == BAYER_BGGR);
    const int red_row  = (pattern == BAYER_GBRG) | (pattern == BAYER_BGGR);
    const int blue_col = 1 - red_col;
    const int blue_row = 1 - red_row;

    const int r_mod_2 = g_r & 1;
    const int c_mod_2 = g_c & 1;

    const int in_red_row    = (r_mod_2 == red_row);
    const int in_blue_row   = (r_mod_2 == blue_row);
    const int is_red_pixel  = (r_mod_2 == red_row)  & (c_mod_2 == red_col);
    const int is_blue_pixel = (r_mod_2 == blue_row) & (c_mod_2 == blue_col);
    const int is_green      = !(is_red_pixel | is_blue_pixel);

    /* ---- Reconstruct R, G, B ---- */
    const uchar R = (uchar)clamp(
        Fij * is_red_pixel
        + R4 * is_blue_pixel
        + R2 * (is_green & in_red_row)
        + R3 * (is_green & in_blue_row),
        0, 255);

    const uchar G = (uchar)clamp(
        Fij * is_green
        + R1 * (!is_green),
        0, 255);

    const uchar B = (uchar)clamp(
        Fij * is_blue_pixel
        + R4 * is_red_pixel
        + R3 * (is_green & in_red_row)
        + R2 * (is_green & in_blue_row),
        0, 255);

    /* ---- Write BGR output (OpenCV channel order) ---- */
    const int dst_idx = dst_offset + g_r * dst_step + g_c * 3;
    dst[dst_idx + 0] = B;
    dst[dst_idx + 1] = G;
    dst[dst_idx + 2] = R;
}

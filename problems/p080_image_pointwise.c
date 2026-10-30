/* P080: Point-wise image operations (grayscale, negative, threshold, brightness/contrast)
 * Problem: A W x H colour image is stored as interleaved 8-bit RGB, row-major:
 *   pixel (y, x) is rgb[3*((long)y*W + x) + c], c = 0 (R), 1 (G), 2 (B).
 *   Compute (1) the grayscale image g = 0.299 R + 0.587 G + 0.114 B (rounded),
 *   stored row-major g[(long)y*W + x]; from g compute (2) the negative 255 - g,
 *   (3) the binary image 255 if g >= threshold else 0, and (4) brightness and
 *   contrast alpha*g + beta clamped to [0, 255]. Parallelise each operation
 *   with OpenMP, verify against the serial code and report the speedups.
 * Solution: every output pixel depends only on the same input pixel, so all
 *   pixels are independent: "#pragma omp parallel for collapse(2)" over (y, x)
 *   is race-free (each iteration writes its own output pixel). collapse(2)
 *   merges the two loops into one W*H iteration space, which balances well
 *   even when H is small. The serial and parallel versions call the same
 *   per-pixel helper, so the results are identical (exact check). Variant
 *   (2') uses "parallel for simd collapse(2)" so each thread's chunk is also
 *   vectorised (see Notes).
 * Source: lab extension (image processing).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p080_image_pointwise.c -o p080 -lm
 * Run:   ./p080 [W] [H]      (defaults W = 4000, H = 3000)
 */
#include "common.h"

typedef unsigned char u8;

#define THRESH 128
#define ALPHA  1.5           /* contrast */
#define BETA   -40.0         /* brightness */

/* Round to the nearest integer and clamp to [0, 255]. */
static inline u8 clamp255(double v) {
    if (v <= 0.0) return 0;
    if (v >= 255.0) return 255;
    return (u8)(v + 0.5);
}
static inline u8 gray_of(const u8 *p) { return clamp255(0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]); }
static inline u8 negative_of(u8 v)    { return (u8)(255 - v); }
static inline u8 threshold_of(u8 v)   { return v >= THRESH ? 255 : 0; }
static inline u8 bc_of(u8 v)          { return clamp255(ALPHA * v + BETA); }

/* (1) RGB -> gray */
static void gray_serial(const u8 *rgb, u8 *g, long W, long H) {
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) g[y * W + x] = gray_of(rgb + 3 * (y * W + x));
}
static void gray_parallel(const u8 *rgb, u8 *g, long W, long H) {
    #pragma omp parallel for collapse(2)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) g[y * W + x] = gray_of(rgb + 3 * (y * W + x));
}

/* (2) negative */
static void negative_serial(const u8 *g, u8 *out, long W, long H) {
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = negative_of(g[y * W + x]);
}
static void negative_parallel(const u8 *g, u8 *out, long W, long H) {
    #pragma omp parallel for collapse(2)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = negative_of(g[y * W + x]);
}

/* (2') negative with "parallel for simd collapse(2)" (OpenMP 4.0): threads share
 * the pixels AND each thread's chunk is vectorised (16 pixels per instruction). */
static void negative_simd_parallel(const u8 *g, u8 *out, long W, long H) {
    #pragma omp parallel for simd collapse(2)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = negative_of(g[y * W + x]);
}

/* (3) threshold to a binary image */
static void threshold_serial(const u8 *g, u8 *out, long W, long H) {
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = threshold_of(g[y * W + x]);
}
static void threshold_parallel(const u8 *g, u8 *out, long W, long H) {
    #pragma omp parallel for collapse(2)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = threshold_of(g[y * W + x]);
}

/* (4) brightness / contrast with clamping */
static void bc_serial(const u8 *g, u8 *out, long W, long H) {
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = bc_of(g[y * W + x]);
}
static void bc_parallel(const u8 *g, u8 *out, long W, long H) {
    #pragma omp parallel for collapse(2)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = bc_of(g[y * W + x]);
}

static int same_bytes(const u8 *a, const u8 *b, long n) { return memcmp(a, b, (size_t)n) == 0; }

int main(int argc, char *argv[]) {
    long W = arg_long(argc, argv, 1, 4000);
    long H = arg_long(argc, argv, 2, 3000);
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    long n = W * H;

    /* Worked example: 4 x 2 image. */
    u8 rgb8[24] = {255, 0, 0,   0, 255, 0,   0, 0, 255,   255, 255, 255,
                   0, 0, 0,     128, 128, 128,   200, 100, 50,   10, 20, 30};
    u8 g8[8], n8[8], t8[8], b8[8];
    gray_parallel(rgb8, g8, 4, 2);
    negative_parallel(g8, n8, 4, 2);
    threshold_parallel(g8, t8, 4, 2);
    bc_parallel(g8, b8, 4, 2);
    const u8 want_g[8] = {76, 150, 29, 255, 0, 128, 124, 18};
    printf("P080: point-wise image operations (threshold %d, alpha %.1f, beta %.0f)\n", THRESH, ALPHA, BETA);
    printf("4 x 2 example: pixel   RGB            gray  negative  threshold  bright/contrast\n");
    for (int p = 0; p < 8; p++)
        printf("               %d     (%3d,%3d,%3d)  %4d  %8d  %9d  %15d\n", p,
               rgb8[3 * p], rgb8[3 * p + 1], rgb8[3 * p + 2], g8[p], n8[p], t8[p], b8[p]);
    printf("gray values vs hand-computed: %s\n\n", ok_str(same_bytes(g8, want_g, 8)));

    u8 *rgb = xmalloc((size_t)n * 3);
    u8 *gs = xmalloc((size_t)n), *gp = xmalloc((size_t)n);
    u8 *os = xmalloc((size_t)n), *op = xmalloc((size_t)n);
    unsigned long long s = 80;
    for (long k = 0; k < 3 * n; k++) rgb[k] = (u8)(lcg_next(&s) & 255);
    omp_warmup();

    double ts = 0, tp = 0;
    printf("Image %ld x %ld = %ld pixels\n", W, H, n);
    for (int op_id = 0; op_id < 5; op_id++) {
        const char *name[5] = {"(1) RGB -> grayscale", "(2) negative 255 - g",
                               "(3) threshold g >= 128 ? 255 : 0", "(4) brightness/contrast, clamped",
                               "(2') negative with parallel for simd collapse(2)"};
        if (op_id == 0) BEST_TIME(ts, gray_serial(rgb, gs, W, H));
        if (op_id == 1 || op_id == 4) BEST_TIME(ts, negative_serial(gs, os, W, H));
        if (op_id == 2) BEST_TIME(ts, threshold_serial(gs, os, W, H));
        if (op_id == 3) BEST_TIME(ts, bc_serial(gs, os, W, H));
        printf("\n%s (check: equal to serial exactly)\n", name[op_id]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            int ok;
            if (op_id == 0) {
                BEST_TIME(tp, gray_parallel(rgb, gp, W, H));
                ok = same_bytes(gs, gp, n);
            } else {
                if (op_id == 1) BEST_TIME(tp, negative_parallel(gs, op, W, H));
                if (op_id == 2) BEST_TIME(tp, threshold_parallel(gs, op, W, H));
                if (op_id == 3) BEST_TIME(tp, bc_parallel(gs, op, W, H));
                if (op_id == 4) BEST_TIME(tp, negative_simd_parallel(gs, op, W, H));
                ok = same_bytes(os, op, n);
            }
            table_row(n, THREADS[k], ts, tp, ok);
        }
    }
    free(rgb); free(gs); free(gp); free(os); free(op);
    return 0;
}

/* Notes:
 * - Point-wise = embarrassingly parallel: no pixel reads another pixel's
 *   output, so no synchronisation is needed beyond the final barrier.
 * - collapse(2) makes one loop of W*H iterations (loops must be perfectly
 *   nested with bounds independent of each other); without it only the H rows
 *   are shared out, which is also fine when H >> threads.
 * - Clamp BEFORE converting to unsigned char: (u8)300 wraps to 44, and (u8)-5
 *   is undefined behaviour for a double. Round with +0.5 after clamping.
 * - Timings: gcc >= 12 at -O2 vectorises the serial negative/threshold loops
 *   (SIMD, 16 pixels per instruction) but not the loop inside the parallel
 *   region (it cannot prove input and output do not overlap), so 1 thread is
 *   much slower than serial. "parallel for simd" tells it to vectorise each
 *   thread's chunk as well (2'). Older gcc does not vectorise at -O2 at all.
 * - These kernels do ~1 operation per byte: memory-bound, so the speedup
 *   flattens at a few threads. Fusing operations (gray + threshold in one
 *   pass) saves a full pass over memory.
 * - Interleaved RGB: pixel p's channels are rgb[3p], rgb[3p+1], rgb[3p+2].
 */

/* P082: 3x3 median filter (salt-and-pepper noise removal)
 * Problem: Apply a 3x3 median filter to a W x H 8-bit grayscale image
 *   (row-major img[(long)y*W + x]): out(y,x) = median of the 9 pixels around
 *   (y,x). At the border, replicate the edge pixels (clamp coordinates into the
 *   image), so the output has the same size. Parallelise with OpenMP, verify
 *   against the serial filter, and show that salt-and-pepper noise (random
 *   pixels set to 0 or 255) is removed on a tiny example.
 * Solution: each output pixel is computed from the INPUT image only and
 *   written to a separate output image, so all pixels are independent:
 *   "#pragma omp parallel for collapse(2)" over (y, x) is race-free (writing
 *   into the input image in place would be wrong: neighbours would see
 *   already-filtered values). Per pixel: gather the 9 values into a private
 *   local array, insertion-sort it, take element 4 (the middle). Same code in
 *   serial and parallel -> identical output.
 * Source: lab extension (image processing).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p082_median_filter.c -o p082 -lm
 * Run:   ./p082 [W] [H] [noise%]   (defaults W = 2000, H = 1500, 5% noise)
 */
#include "common.h"

typedef unsigned char u8;

static inline long clampl(long v, long lo, long hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* Insertion sort of 9 values; return the median v[4]. */
static inline u8 median9(u8 v[9]) {
    for (int a = 1; a < 9; a++) {
        u8 key = v[a];
        int b = a - 1;
        while (b >= 0 && v[b] > key) { v[b + 1] = v[b]; b--; }
        v[b + 1] = key;
    }
    return v[4];
}

/* Median of the 3x3 neighbourhood of (y, x), border replicated. */
static inline u8 median_at(const u8 *in, long W, long H, long y, long x) {
    u8 v[9];                                   /* local: private to the thread */
    int k = 0;
    for (long dy = -1; dy <= 1; dy++) {
        long yy = clampl(y + dy, 0, H - 1);
        for (long dx = -1; dx <= 1; dx++) v[k++] = in[yy * W + clampl(x + dx, 0, W - 1)];
    }
    return median9(v);
}

static void median_serial(const u8 *in, u8 *out, long W, long H) {
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = median_at(in, W, H, y, x);
}

static void median_parallel(const u8 *in, u8 *out, long W, long H) {
    #pragma omp parallel for collapse(2)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) out[y * W + x] = median_at(in, W, H, y, x);
}

/* Smooth gradient, then about noise% of the pixels set to 0 or 255. */
static void make_images(u8 *clean, u8 *noisy, long W, long H, double noise_pct) {
    unsigned long long s = 82;
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) {
            long p = y * W + x;
            clean[p] = (u8)(20 + (200 * (x + y)) / (W + H));
            unsigned long r = lcg_next(&s) % 10000;
            if (r < (unsigned long)(noise_pct * 50.0))       noisy[p] = 0;     /* pepper */
            else if (r < (unsigned long)(noise_pct * 100.0)) noisy[p] = 255;   /* salt */
            else                                             noisy[p] = clean[p];
        }
}

static long count_diff(const u8 *a, const u8 *b, long n) {
    long c = 0;
    for (long p = 0; p < n; p++) c += (a[p] != b[p]);
    return c;
}

static void print_img(const char *title, const u8 *img, long W, long H) {
    printf("%s\n", title);
    for (long y = 0; y < H; y++) {
        for (long x = 0; x < W; x++) printf(" %3d", img[y * W + x]);
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    long W = arg_long(argc, argv, 1, 2000);
    long H = arg_long(argc, argv, 2, 1500);
    double noise = arg_double(argc, argv, 3, 5.0);
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    long n = W * H;

    /* Worked example: 7 x 7 image of 100s with salt (255) and pepper (0) pixels. */
    u8 t_in[49], t_out[49], t_clean[49];
    for (int p = 0; p < 49; p++) t_in[p] = t_clean[p] = 100;
    t_in[0] = 255; t_in[9] = 0; t_in[17] = 255; t_in[24] = 0; t_in[30] = 255; t_in[40] = 0; t_in[48] = 255;
    median_parallel(t_in, t_out, 7, 7);
    printf("P082: 3x3 median filter (border replicated)\n");
    print_img("7 x 7 noisy input:", t_in, 7, 7);
    print_img("after the median filter:", t_out, 7, 7);
    printf("noise removed: %s\n\n", ok_str(count_diff(t_out, t_clean, 49) == 0));

    u8 *clean = xmalloc((size_t)n), *noisy = xmalloc((size_t)n);
    u8 *os = xmalloc((size_t)n), *op = xmalloc((size_t)n);
    make_images(clean, noisy, W, H, noise);
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, median_serial(noisy, os, W, H));
    printf("Image %ld x %ld, %.1f%% salt-and-pepper noise\n", W, H, noise);
    printf("pixels different from the clean image: before %ld, after filtering %ld\n",
           count_diff(noisy, clean, n), count_diff(os, clean, n));
    printf("(check: output equal to the serial filter exactly)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(op, 0, (size_t)n);
        BEST_TIME(tp, median_parallel(noisy, op, W, H));
        table_row(n, THREADS[k], ts, tp, memcmp(os, op, (size_t)n) == 0);
    }
    free(clean); free(noisy); free(os); free(op);
    return 0;
}

/* Notes:
 * - Separate output image: filtering in place would let later pixels read
 *   already-filtered neighbours (different result, and a race in parallel).
 * - The 9-value buffer is declared inside the loop body/function, so each
 *   thread has its own copy (a shared buffer would be a race).
 * - Median vs mean: the median ignores up to 4 outliers among 9 values, so
 *   isolated 0/255 pixels vanish while edges stay sharp; a mean filter would
 *   smear them. A pixel survives only if >= 5 of its 9 neighbours are noise.
 * - Equal work per pixel -> static scheduling is fine; compute-bound (~20-40
 *   comparisons per pixel), so it scales better than point-wise operations.
 * - Faster medians: a 19-comparison sorting network, or reuse sorted columns
 *   when moving along a row.
 */

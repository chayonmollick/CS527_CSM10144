/* P081: Histogram equalisation of an 8-bit grayscale image
 * Problem: A W x H grayscale image (unsigned char, row-major img[(long)y*W + x])
 *   has low contrast. Equalise it: (1) histogram h[v] = number of pixels with
 *   value v (v = 0..255); (2) cumulative histogram cdf[v] = h[0] + ... + h[v];
 *   (3) lookup table lut[v] = round((cdf[v] - cdf_min) * 255 / (N - cdf_min)),
 *   where N = W*H and cdf_min = cdf of the darkest value present; (4) remap
 *   out[p] = lut[img[p]]. Parallelise with OpenMP, verify against the serial
 *   code exactly, and print a histogram summary before and after.
 * Solution: (1) many pixels hit the same bin, so h[img[p]]++ in a plain
 *   parallel for is a race. Version R: reduction(+:h[:256]) (array-section
 *   reduction, OpenMP 4.5): each thread gets a private zeroed copy of h, and
 *   the copies are added at the end. Version P (works with any OpenMP): a
 *   private local histogram per thread, then each thread adds it into h inside
 *   a critical section (256 adds per thread). Counting is integer, so both are
 *   exact. (2)-(3) are only 256 steps and the prefix sum is sequential: keep
 *   them serial. (4) each output pixel depends only on its input pixel and the
 *   read-only lut: "parallel for" is race-free.
 * Source: lab extension (image processing).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p081_histogram_equalization.c -o p081 -lm
 * Run:   ./p081 [W] [H]      (defaults W = 4000, H = 3000)
 */
#include "common.h"

typedef unsigned char u8;

static void hist_serial(const u8 *img, long n, long *h) {
    for (int v = 0; v < 256; v++) h[v] = 0;
    for (long p = 0; p < n; p++) h[img[p]]++;
}

/* Version R: array-section reduction (OpenMP 4.5, gcc >= 6). */
static void hist_reduction(const u8 *img, long n, long *h) {
    for (int v = 0; v < 256; v++) h[v] = 0;
    #pragma omp parallel for reduction(+:h[:256])
    for (long p = 0; p < n; p++) h[img[p]]++;
}

/* Version P: private histogram per thread, merged under critical. */
static void hist_private(const u8 *img, long n, long *h) {
    for (int v = 0; v < 256; v++) h[v] = 0;
    #pragma omp parallel
    {
        long local[256] = {0};
        #pragma omp for nowait                 /* no need to wait before merging */
        for (long p = 0; p < n; p++) local[img[p]]++;
        #pragma omp critical
        for (int v = 0; v < 256; v++) h[v] += local[v];
    }
}

/* cdf and lookup table: 256 entries, serial. */
static void build_lut(const long *h, long n, u8 *lut) {
    long cdf_min = 0, cdf = 0;
    for (int v = 0; v < 256; v++) if (h[v] > 0) { cdf_min = h[v]; break; }
    for (int v = 0; v < 256; v++) {
        cdf += h[v];
        if (n == cdf_min) lut[v] = (u8)v;                       /* one value only: keep it */
        else if (cdf <= cdf_min) lut[v] = 0;
        else lut[v] = (u8)(((cdf - cdf_min) * 255 + (n - cdf_min) / 2) / (n - cdf_min));
    }
}

static void remap_serial(const u8 *img, u8 *out, long n, const u8 *lut) {
    for (long p = 0; p < n; p++) out[p] = lut[img[p]];
}
static void remap_parallel(const u8 *img, u8 *out, long n, const u8 *lut) {
    #pragma omp parallel for
    for (long p = 0; p < n; p++) out[p] = lut[img[p]];
}

/* Whole equalisation. version 0 = serial, 1 = reduction, 2 = private + critical. */
static void equalize(const u8 *img, u8 *out, long n, long *h, int version) {
    u8 lut[256];
    if (version == 0) hist_serial(img, n, h);
    if (version == 1) hist_reduction(img, n, h);
    if (version == 2) hist_private(img, n, h);
    build_lut(h, n, lut);
    if (version == 0) remap_serial(img, out, n, lut);
    else              remap_parallel(img, out, n, lut);
}

/* min, max, mean and a 16-bin bar chart (bins of 16 gray levels). */
static void summary(const char *title, const long *h, long n) {
    int lo = 0, hi = 255;
    double mean = 0.0;
    while (lo < 255 && h[lo] == 0) lo++;
    while (hi > 0 && h[hi] == 0) hi--;
    for (int v = 0; v < 256; v++) mean += (double)v * (double)h[v];
    printf("%s: min %d, max %d, mean %.1f\n", title, lo, hi, mean / (double)n);
    long bin[16] = {0}, top = 1;
    for (int v = 0; v < 256; v++) bin[v / 16] += h[v];
    for (int b = 0; b < 16; b++) if (bin[b] > top) top = bin[b];
    for (int b = 0; b < 16; b++) {
        int len = (int)(40.0 * (double)bin[b] / (double)top + 0.5);
        printf("  %3d-%3d %10ld |", 16 * b, 16 * b + 15, bin[b]);
        for (int k = 0; k < len; k++) putchar('#');
        putchar('\n');
    }
}

int main(int argc, char *argv[]) {
    long W = arg_long(argc, argv, 1, 4000);
    long H = arg_long(argc, argv, 2, 3000);
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    long n = W * H;

    /* Low-contrast test image: sum of two random values -> values 90..170, peaked at 130. */
    u8 *img = xmalloc((size_t)n), *os = xmalloc((size_t)n), *op = xmalloc((size_t)n);
    unsigned long long s = 81;
    for (long p = 0; p < n; p++) img[p] = (u8)(90 + lcg_next(&s) % 41 + lcg_next(&s) % 41);
    long hs[256], hp[256], hout[256];
    omp_warmup();

    printf("P081: histogram equalisation, %ld x %ld image\n", W, H);
    double ts, tp;
    BEST_TIME(ts, equalize(img, os, n, hs, 0));
    summary("before", hs, n);
    hist_serial(os, n, hout);
    summary("after ", hout, n);

    const char *name[3] = {"", "R: reduction(+:h[:256]) + parallel remap",
                           "P: private histograms merged in critical + parallel remap"};
    for (int v = 1; v <= 2; v++) {
        printf("\nVersion %s (check: histogram and output image equal to serial)\n", name[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            memset(op, 0, (size_t)n);
            BEST_TIME(tp, equalize(img, op, n, hp, v));
            table_row(n, THREADS[k], ts, tp, same_longs(hs, hp, 256) && memcmp(os, op, (size_t)n) == 0);
        }
    }
    free(img); free(os); free(op);
    return 0;
}

/* Notes:
 * - Histogram race: h[v]++ is load/add/store; two threads with the same v lose
 *   counts. Fixes: reduction on the array (private copies), private histograms
 *   + merge, or "#pragma omp atomic" on every h[v]++ (correct but slow: heavy
 *   contention on the few popular bins).
 * - reduction(+:h[:256]) needs OpenMP 4.5 (gcc >= 6); with a pointer h the
 *   length must be given. The merge costs 256 x threads adds (negligible).
 * - The cdf is a prefix sum over 256 values: sequential and tiny, so serial.
 * - Equalisation spreads the values over 0..255 but cannot split one input
 *   level into several, so the output histogram has gaps (not perfectly flat).
 * - Remap is a gather through a 256-byte table (stays in L1): memory-bound.
 */

/* P083: CNN pooling layers (max and average pooling, k x k window, stride s)
 * Problem: Given an H x W feature map of doubles (row-major in[(long)i*W + j]),
 *   compute k x k pooling with stride s (no padding): the output has
 *   Ho = (H - k)/s + 1 rows and Wo = (W - k)/s + 1 columns (H/2 x W/2 for
 *   k = s = 2), and out[oi][oj] is the max (max pooling) or the mean (average
 *   pooling) of the window in[oi*s .. oi*s+k-1][oj*s .. oj*s+k-1].
 *   Parallelise with OpenMP, verify against the serial code, and print a
 *   4 x 4 -> 2 x 2 example.
 * Solution: each output cell reads its own window of the input and writes only
 *   out[oi][oj], so the output cells are independent: "#pragma omp parallel
 *   for collapse(2)" over (oi, oj) is race-free (overlapping windows when
 *   s < k are fine: the input is only read). The window loop inside a cell is
 *   the same in the serial and parallel code, so the results are identical,
 *   even for the average (same additions in the same order).
 * Source: lab extension (deep-learning kernels).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p083_pooling.c -o p083 -lm
 * Run:   ./p083 [H] [W]      (defaults H = 3000, W = 3000)
 */
#include "common.h"

static inline long out_size(long n, long k, long s) { return n < k ? 0 : (n - k) / s + 1; }

/* One output cell: max (is_max = 1) or average over the k x k window at (oi, oj). */
static inline double pool_cell(const double *in, long W, long k, long s, long oi, long oj, int is_max) {
    const double *win = in + (oi * s) * W + oj * s;          /* top-left of the window */
    double m = win[0], sum = 0.0;
    for (long di = 0; di < k; di++)
        for (long dj = 0; dj < k; dj++) {
            double v = win[di * W + dj];
            if (v > m) m = v;
            sum += v;
        }
    return is_max ? m : sum / (double)(k * k);
}

static void pool_serial(const double *in, long H, long W, long k, long s, int is_max, double *out) {
    long Ho = out_size(H, k, s), Wo = out_size(W, k, s);
    for (long oi = 0; oi < Ho; oi++)
        for (long oj = 0; oj < Wo; oj++) out[oi * Wo + oj] = pool_cell(in, W, k, s, oi, oj, is_max);
}

static void pool_parallel(const double *in, long H, long W, long k, long s, int is_max, double *out) {
    long Ho = out_size(H, k, s), Wo = out_size(W, k, s);
    #pragma omp parallel for collapse(2)
    for (long oi = 0; oi < Ho; oi++)
        for (long oj = 0; oj < Wo; oj++) out[oi * Wo + oj] = pool_cell(in, W, k, s, oi, oj, is_max);
}

int main(int argc, char *argv[]) {
    long H = arg_long(argc, argv, 1, 3000);
    long W = arg_long(argc, argv, 2, 3000);
    if (H < 3) H = 3;
    if (W < 3) W = 3;

    /* Worked example: 4 x 4 -> 2 x 2 with k = s = 2. */
    double ex[16] = {1, 3, 2, 4,
                     5, 6, 1, 2,
                     7, 2, 9, 0,
                     1, 8, 3, 4};
    double mx[4], av[4];
    const double want_max[4] = {6, 4, 8, 9}, want_avg[4] = {3.75, 2.25, 4.5, 4.0};
    pool_parallel(ex, 4, 4, 2, 2, 1, mx);
    pool_parallel(ex, 4, 4, 2, 2, 0, av);
    printf("P083: max and average pooling\n4 x 4 input:\n");
    for (int i = 0; i < 4; i++) printf("   %3.0f %3.0f | %3.0f %3.0f\n%s", ex[4 * i], ex[4 * i + 1],
                                       ex[4 * i + 2], ex[4 * i + 3], i == 1 ? "   --------+--------\n" : "");
    printf("2 x 2 max pooling:     [%g %g; %g %g]\n", mx[0], mx[1], mx[2], mx[3]);
    printf("2 x 2 average pooling: [%g %g; %g %g]\n", av[0], av[1], av[2], av[3]);
    printf("example vs hand-computed: %s\n\n",
           ok_str(close_doubles(mx, want_max, 4, 0.0) && close_doubles(av, want_avg, 4, 0.0)));

    double *in = xmalloc((size_t)H * W * sizeof(double));
    double *os = xmalloc((size_t)H * W * sizeof(double));     /* big enough for any output */
    double *op = xmalloc((size_t)H * W * sizeof(double));
    fill_double(in, H * W, -1.0, 1.0, 83);
    omp_warmup();

    struct { long k, s; int is_max; const char *name; } cfg[3] = {
        {2, 2, 1, "max pooling 2x2, stride 2"},
        {2, 2, 0, "average pooling 2x2, stride 2"},
        {3, 2, 1, "max pooling 3x3, stride 2 (overlapping windows)"}};
    for (int c = 0; c < 3; c++) {
        long k = cfg[c].k, s = cfg[c].s;
        long Ho = out_size(H, k, s), Wo = out_size(W, k, s);
        double ts, tp;
        BEST_TIME(ts, pool_serial(in, H, W, k, s, cfg[c].is_max, os));
        printf("%s: %ld x %ld -> %ld x %ld (check: equal to serial exactly)\n",
               cfg[c].name, H, W, Ho, Wo);
        table_header();
        for (int t = 0; t < NTHREADS; t++) {
            omp_set_num_threads(THREADS[t]);
            BEST_TIME(tp, pool_parallel(in, H, W, k, s, cfg[c].is_max, op));
            table_row(Ho * Wo, THREADS[t], ts, tp, close_doubles(os, op, Ho * Wo, 0.0));
        }
        printf("\n");
    }
    free(in); free(os); free(op);
    return 0;
}

/* Notes:
 * - Output size without padding: (H - k)/s + 1 (integer division); with
 *   padding p it is (H + 2p - k)/s + 1.
 * - Parallelise over OUTPUT cells (each written once) rather than input
 *   pixels (several outputs would update the same cell -> race).
 * - With channels/batches (N x C x H x W), use collapse(3) or collapse(4)
 *   over (n, c, oi, oj); the window loop stays inside.
 * - Pooling does k*k reads per output: memory-bound; s < k re-reads input
 *   (overlap), which the cache absorbs.
 * - Backprop of max pooling needs the argmax position of each window; store
 *   it in a second output array in the same loop.
 */

/* P000: Sum of squares (format example for the problem bank)
 * Problem: Given an array a[N] of doubles, compute S = a[0]^2 + a[1]^2 + ... + a[N-1]^2
 *   serially and with OpenMP. Verify the parallel result and report the time,
 *   speedup and efficiency for 1, 2, 4 and 8 threads.
 * Solution: every iteration adds into the same S, so a plain parallel for would
 *   race on S. reduction(+:S) gives each thread a private S (starting at 0) and
 *   adds the private copies together once at the end.
 * Source: pattern of Lab 7 Task 2.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p000_example.c -o p000 -lm
 * Run:   ./p000 [N]          (default N = 1e7)
 */
#include "common.h"

static double sumsq_serial(const double *a, long n) {
    double s = 0.0;
    for (long i = 0; i < n; i++) s += a[i] * a[i];
    return s;
}

static double sumsq_parallel(const double *a, long n) {
    double s = 0.0;
    #pragma omp parallel for reduction(+:s)
    for (long i = 0; i < n; i++) s += a[i] * a[i];
    return s;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    double *a = xmalloc((size_t)n * sizeof(double));
    fill_double(a, n, -1.0, 1.0, 42);
    omp_warmup();

    double rs = 0, rp = 0, ts, tp;
    BEST_TIME(ts, rs = sumsq_serial(a, n));
    printf("P000: sum of squares, N = %ld, serial S = %.6f\n", n, rs);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = sumsq_parallel(a, n));
        table_row(n, THREADS[k], ts, tp, close1(rs, rp, 1e-9));
    }
    free(a);
    return 0;
}

/* Notes:
 * - Without reduction: S += ... is load/add/store on a shared variable, so
 *   updates are lost (data race) and S comes out too small.
 * - The parallel sum adds in a different order, so compare with a tolerance.
 * - Memory-bound (one multiply-add per 8 bytes read): speedup flattens once
 *   memory bandwidth is saturated.
 */

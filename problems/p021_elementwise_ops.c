/* P021: Element-wise array operations, separate loops vs one fused loop
 * Problem: Given arrays a[N] and b[N], compute element by element
 *   c = a*b (Hadamard product), d = sqrt(|a|) + exp(-b*b) and e = max(a, b).
 *   Parallelise each operation with its own parallel for, then compute all three
 *   in ONE fused loop. Verify both against serial code and compare the fused and
 *   separate versions (memory traffic).
 * Solution: element i of c, d, e depends only on a[i] and b[i], so every loop is a
 *   plain "#pragma omp parallel for" with no shared writes (each i is written by one
 *   thread). Separate loops read a and b three times and start three parallel
 *   regions; the fused loop reads a[i], b[i] once (kept in registers) and writes
 *   c, d, e in a single pass: 5N instead of 9N doubles of memory traffic.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p021_elementwise_ops.c -o p021 -lm
 * Run:   ./p021 [N]   (default N = 2e6)
 */
#include "common.h"

/* Three separate passes over the data. */
static void separate_serial(const double *a, const double *b, double *c, double *d, double *e, long n) {
    for (long i = 0; i < n; i++) c[i] = a[i] * b[i];
    for (long i = 0; i < n; i++) d[i] = sqrt(fabs(a[i])) + exp(-b[i] * b[i]);
    for (long i = 0; i < n; i++) e[i] = fmax(a[i], b[i]);
}

static void separate_parallel(const double *a, const double *b, double *c, double *d, double *e, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) c[i] = a[i] * b[i];
    #pragma omp parallel for
    for (long i = 0; i < n; i++) d[i] = sqrt(fabs(a[i])) + exp(-b[i] * b[i]);
    #pragma omp parallel for
    for (long i = 0; i < n; i++) e[i] = fmax(a[i], b[i]);
}

/* One pass: a[i] and b[i] are loaded once and used for all three results. */
static void fused_serial(const double *a, const double *b, double *c, double *d, double *e, long n) {
    for (long i = 0; i < n; i++) {
        double x = a[i], y = b[i];
        c[i] = x * y;
        d[i] = sqrt(fabs(x)) + exp(-y * y);
        e[i] = fmax(x, y);
    }
}

static void fused_parallel(const double *a, const double *b, double *c, double *d, double *e, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        double x = a[i], y = b[i];          /* private: declared inside the loop */
        c[i] = x * y;
        d[i] = sqrt(fabs(x)) + exp(-y * y);
        e[i] = fmax(x, y);
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 2000000L);
    size_t bytes = (size_t)n * sizeof(double);
    double *a = xmalloc(bytes), *b = xmalloc(bytes);
    double *c0 = xmalloc(bytes), *d0 = xmalloc(bytes), *e0 = xmalloc(bytes);   /* serial reference */
    double *c = xmalloc(bytes), *d = xmalloc(bytes), *e = xmalloc(bytes);      /* parallel results */
    fill_double(a, n, -2.0, 2.0, 1);
    fill_double(b, n, -2.0, 2.0, 2);
    omp_warmup();

    double tsep, tfus, tp, tf;
    BEST_TIME(tsep, separate_serial(a, b, c0, d0, e0, n));
    BEST_TIME(tfus, fused_serial(a, b, c0, d0, e0, n));
    printf("P021: c = a*b, d = sqrt(|a|) + exp(-b^2), e = max(a,b), N = %ld\n", n);
    printf("memory traffic: separate reads a,b 3 times = 9N doubles = %.1f MB, fused = 5N = %.1f MB\n",
           9.0 * bytes / 1e6, 5.0 * bytes / 1e6);
    printf("%8s %12s %12s %8s %12s %12s %8s %10s %6s\n", "threads", "sep serial", "sep par", "speedup",
           "fused ser", "fused par", "speedup", "fused/sep", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, separate_parallel(a, b, c, d, e, n));
        int ok = close_doubles(c0, c, n, 1e-12) && close_doubles(d0, d, n, 1e-12) && close_doubles(e0, e, n, 1e-12);
        BEST_TIME(tf, fused_parallel(a, b, c, d, e, n));
        ok = ok && close_doubles(c0, c, n, 1e-12) && close_doubles(d0, d, n, 1e-12) && close_doubles(e0, e, n, 1e-12);
        printf("%8d %12.6f %12.6f %8.2f %12.6f %12.6f %8.2f %10.2f %6s\n", THREADS[k], tsep, tp,
               tp > 0 ? tsep / tp : 0.0, tfus, tf, tf > 0 ? tfus / tf : 0.0, tp > 0 ? tf / tp : 0.0, ok_str(ok));
    }
    free(a); free(b); free(c0); free(d0); free(e0); free(c); free(d); free(e);
    return 0;
}

/* Notes:
 * - No race: iteration i writes only c[i], d[i], e[i]; a and b are only read.
 * - Fusion = loop fusion: fewer passes over memory (5N vs 9N doubles) and one parallel
 *   region instead of three (one fork/join and one implicit barrier instead of three).
 * - fused/sep < 1 shows the saving. It is largest for cheap, memory-bound operations
 *   (like c = a*b alone); expensive math (exp, sqrt) hides part of the memory cost.
 * - Alternative without fusing: one parallel region with three "#pragma omp for nowait"
 *   loops (legal: the loops only read a, b and write different arrays, so no wait needed).
 * - Element-wise ops are "embarrassingly parallel": speedup is limited by memory
 *   bandwidth (cheap ops) or by the number of cores (expensive ops like exp).
 */

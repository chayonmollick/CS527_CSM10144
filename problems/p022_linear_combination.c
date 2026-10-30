/* P022: Linear combination of three vectors
 * Problem: Given vectors x, y, w of length N and scalars alpha, beta, gamma, compute
 *   z = alpha*x + beta*y + gamma*w and the 3-vector sum s = x + y + w, serially and
 *   with OpenMP. Verify both and explain their memory-bound behaviour (bytes moved
 *   per floating-point operation).
 * Solution: z[i] and s[i] depend only on x[i], y[i], w[i], so each loop is a plain
 *   "#pragma omp parallel for" (each i is written by exactly one thread, inputs are
 *   only read: race-free). Each element needs 3 loads + 1 store = 32 bytes for 5
 *   flops (z) or 2 flops (s), so the speed is set by memory bandwidth, not by the
 *   arithmetic; the program also prints the achieved bandwidth in GB/s.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p022_linear_combination.c -o p022 -lm
 * Run:   ./p022 [N] [alpha] [beta] [gamma]   (defaults N = 4e6, 2, -1, 0.5)
 */
#include "common.h"

static void lincomb_serial(double *z, const double *x, const double *y, const double *w,
                           double al, double be, double ga, long n) {
    for (long i = 0; i < n; i++) z[i] = al * x[i] + be * y[i] + ga * w[i];
}
static void lincomb_parallel(double *z, const double *x, const double *y, const double *w,
                             double al, double be, double ga, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) z[i] = al * x[i] + be * y[i] + ga * w[i];
}

static void sum3_serial(double *s, const double *x, const double *y, const double *w, long n) {
    for (long i = 0; i < n; i++) s[i] = x[i] + y[i] + w[i];
}
static void sum3_parallel(double *s, const double *x, const double *y, const double *w, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) s[i] = x[i] + y[i] + w[i];
}

/* GB/s for a loop that moves 32 bytes per element (3 reads + 1 write). */
static double gbs(long n, double t) { return t > 0 ? 32.0 * n / t / 1e9 : 0.0; }

int main(int argc, char *argv[]) {
    long n    = arg_long(argc, argv, 1, 4000000L);
    double al = arg_double(argc, argv, 2, 2.0);
    double be = arg_double(argc, argv, 3, -1.0);
    double ga = arg_double(argc, argv, 4, 0.5);
    size_t bytes = (size_t)n * sizeof(double);
    double *x = xmalloc(bytes), *y = xmalloc(bytes), *w = xmalloc(bytes);
    double *z0 = xmalloc(bytes), *z = xmalloc(bytes);   /* serial / parallel z */
    double *s0 = xmalloc(bytes), *s = xmalloc(bytes);   /* serial / parallel s */
    fill_double(x, n, -1.0, 1.0, 1);
    fill_double(y, n, -1.0, 1.0, 2);
    fill_double(w, n, -1.0, 1.0, 3);
    omp_warmup();
    double ts, tp, best;

    printf("P022: z = %g*x + %g*y + %g*w, N = %ld  (5 flops per 32 bytes = 6.4 bytes/flop)\n", al, be, ga, n);
    BEST_TIME(ts, lincomb_serial(z0, x, y, w, al, be, ga, n));
    best = ts;
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, lincomb_parallel(z, x, y, w, al, be, ga, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(z0, z, n, 1e-12));
        if (tp < best) best = tp;
    }
    printf("  bandwidth: serial %.1f GB/s, best parallel %.1f GB/s\n", gbs(n, ts), gbs(n, best));

    printf("P022: s = x + y + w, N = %ld  (2 flops per 32 bytes = 16 bytes/flop)\n", n);
    BEST_TIME(ts, sum3_serial(s0, x, y, w, n));
    best = ts;
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, sum3_parallel(s, x, y, w, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(s0, s, n, 1e-12));
        if (tp < best) best = tp;
    }
    printf("  bandwidth: serial %.1f GB/s, best parallel %.1f GB/s\n", gbs(n, ts), gbs(n, best));
    free(x); free(y); free(w); free(z0); free(z); free(s0); free(s);
    return 0;
}

/* Notes:
 * - Race-free: iteration i writes only z[i] (or s[i]); x, y, w are read-only.
 * - Arithmetic intensity = flops / bytes = 5/32 for z and 2/32 for s: far below what
 *   a core can compute per byte loaded, so both loops are memory-bound. Both take
 *   about the same time even though z does 2.5x more flops.
 * - Speedup flattens once the memory bus is saturated (often at 2-4 threads); more
 *   threads then only add overhead. Efficiency drops well below 1.
 * - Real traffic is a bit higher than 32 bytes: storing z first reads its cache line
 *   (write-allocate), so 40 bytes per element unless streaming stores are used.
 * - Fusing z and s in one loop would read x, y, w only once (48 instead of 64 bytes
 *   per element for both results): the same idea as P021.
 */

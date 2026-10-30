/* P020: SIMD vectorisation with omp simd and parallel for simd
 * Problem: For SAXPY (z = a*x + y) and the dot product s = x . y on arrays of N
 *   doubles, compare four versions: a plain loop, "#pragma omp simd" (one thread,
 *   vector instructions), "#pragma omp parallel for" (threads) and "#pragma omp
 *   parallel for simd" (threads + vectors); use simd reduction(+:s) for the dot
 *   product. Verify every version and explain SIMD vs threads.
 * Solution: SAXPY iterations are independent, so both threads and vector lanes can
 *   process different i at the same time. The dot product has a loop-carried sum, so
 *   it needs reduction(+:s): with simd each vector lane keeps its own partial sum,
 *   with parallel for each thread does, and with parallel for simd both (thread
 *   partials of lane partials), combined at the end. "omp simd" tells the compiler
 *   the loop is safe to vectorise (no aliasing, reordering the sum is allowed).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p020_simd_vectorise.c -o p020 -lm
 *   (add -fopt-info-vec to see the vectorised loops; on x86 add -march=native for AVX)
 * Run:   ./p020 [N]   (default N = 5e6)
 */
#include "common.h"

/* ---- SAXPY: z = a*x + y (written out-of-place so every run gives the same z) ---- */
static void saxpy_plain(double *z, const double *x, const double *y, double a, long n) {
    for (long i = 0; i < n; i++) z[i] = a * x[i] + y[i];
}
static void saxpy_simd(double *z, const double *x, const double *y, double a, long n) {
    #pragma omp simd
    for (long i = 0; i < n; i++) z[i] = a * x[i] + y[i];
}
static void saxpy_par(double *z, const double *x, const double *y, double a, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) z[i] = a * x[i] + y[i];
}
static void saxpy_parsimd(double *z, const double *x, const double *y, double a, long n) {
    #pragma omp parallel for simd
    for (long i = 0; i < n; i++) z[i] = a * x[i] + y[i];
}

/* ---- Dot product: s = sum x[i]*y[i] ---- */
static double dot_plain(const double *x, const double *y, long n) {
    double s = 0.0;                   /* one accumulator: every add waits for the previous */
    for (long i = 0; i < n; i++) s += x[i] * y[i];
    return s;
}
static double dot_simd(const double *x, const double *y, long n) {
    double s = 0.0;
    #pragma omp simd reduction(+:s)   /* one partial sum per vector lane */
    for (long i = 0; i < n; i++) s += x[i] * y[i];
    return s;
}
static double dot_par(const double *x, const double *y, long n) {
    double s = 0.0;
    #pragma omp parallel for reduction(+:s)
    for (long i = 0; i < n; i++) s += x[i] * y[i];
    return s;
}
static double dot_parsimd(const double *x, const double *y, long n) {
    double s = 0.0;
    #pragma omp parallel for simd reduction(+:s)
    for (long i = 0; i < n; i++) s += x[i] * y[i];
    return s;
}

/* One row of the comparison table; speedup is against the plain loop. */
static void row(const char *name, int threads, double t, double tbase, int ok) {
    printf("  %-20s %8d %12.6f %8.2f %6s\n", name, threads, t, t > 0 ? tbase / t : 0.0, ok_str(ok));
}
static void header(void) {
    printf("  %-20s %8s %12s %8s %6s\n", "version", "threads", "time(s)", "speedup", "check");
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 5000000L);
    const double a = 1.5;
    double *x  = xmalloc((size_t)n * sizeof(double));
    double *y  = xmalloc((size_t)n * sizeof(double));
    double *z0 = xmalloc((size_t)n * sizeof(double));   /* plain-loop result (reference) */
    double *z  = xmalloc((size_t)n * sizeof(double));   /* result of the version being tested */
    fill_double(x, n, -1.0, 1.0, 1);
    fill_double(y, n, -1.0, 1.0, 2);
    omp_warmup();
    double t0, t;

    printf("P020: SIMD vs threads, N = %ld\n", n);
    printf("SAXPY z = a*x + y:\n");
    header();
    BEST_TIME(t0, saxpy_plain(z0, x, y, a, n));
    row("plain loop", 1, t0, t0, 1);
    BEST_TIME(t, saxpy_simd(z, x, y, a, n));
    row("omp simd", 1, t, t0, close_doubles(z0, z, n, 1e-12));
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(t, saxpy_par(z, x, y, a, n));
        row("parallel for", THREADS[k], t, t0, close_doubles(z0, z, n, 1e-12));
    }
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(t, saxpy_parsimd(z, x, y, a, n));
        row("parallel for simd", THREADS[k], t, t0, close_doubles(z0, z, n, 1e-12));
    }

    double s0 = 0, s = 0;
    printf("Dot product s = x . y:\n");
    header();
    BEST_TIME(t0, s0 = dot_plain(x, y, n));
    row("plain loop", 1, t0, t0, 1);
    BEST_TIME(t, s = dot_simd(x, y, n));
    row("omp simd reduction", 1, t, t0, close1(s0, s, 1e-9));
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(t, s = dot_par(x, y, n));
        row("parallel for", THREADS[k], t, t0, close1(s0, s, 1e-9));
    }
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(t, s = dot_parsimd(x, y, n));
        row("parallel for simd", THREADS[k], t, t0, close1(s0, s, 1e-9));
    }
    printf("  s = %.10f (plain loop)\n", s0);
    free(x); free(y); free(z0); free(z);
    return 0;
}

/* Notes:
 * - SIMD = data-level parallelism inside ONE core: one instruction works on a whole
 *   vector register (ARM NEON 128 bit = 2 doubles; x86 SSE2 2, AVX2 4, AVX-512 8
 *   doubles). Threads = thread-level parallelism across cores. They multiply:
 *   parallel for simd uses both.
 * - The plain dot product is not vectorised at -O2/-O3: reordering a floating-point
 *   sum changes the result, so gcc keeps one serial chain of adds (each waits for the
 *   last). simd reduction(+:s) gives each lane its own partial sum and allows the
 *   reordering, so it is faster even on 1 thread (and differs in the last digits).
 * - SAXPY moves 24 bytes per 2 flops, so for large N it is memory-bound: SIMD and
 *   more threads help little once bandwidth is saturated. For a small N (./p020 20000)
 *   omp simd still helps but threads lose: start-up costs more than the work.
 * - gcc >= 12 already vectorises simple loops like SAXPY at -O2, so "plain" and
 *   "omp simd" can be equal there; gcc 9 at -O2 only vectorises loops marked omp simd.
 * - omp simd is a promise by the programmer: if iterations really depend on each
 *   other (a[i] = a[i-1] + ...) the vector code is wrong. Useful clauses: safelen(k),
 *   aligned(p:64), linear(j:1), simdlen(k).
 */

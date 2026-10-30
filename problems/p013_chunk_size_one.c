/* P013: Why schedule(static,1) or (dynamic,1) is a bad idea for b[i] = f(a[i])
 * Problem: For the cheap loop b[i] = 2*a[i] + 1, compare the schedules static,
 *   static,1, static,8, static,64, dynamic,1 and dynamic,64 on 1, 2, 4, 8 threads.
 *   Verify the output is identical for all of them and explain the timings.
 * Solution: every iteration writes only its own b[i], so all schedules are race-free and
 *   give bit-identical results. They differ in cost: with chunk 1 neighbouring elements
 *   go to different threads, so all threads write into the same cache lines of b
 *   (false sharing), each thread reads every cache line of a but uses only 1 element
 *   of it (no locality, no vectorisation), and dynamic,1 adds one trip to the shared
 *   scheduler counter per element. Larger chunks (or plain static) avoid all three.
 * Source: textbook Ex 19.7.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p013_chunk_size_one.c -o p013 -lm
 * Run:   ./p013 [N]          (default N = 4e6)
 */
#include "common.h"

static inline double f(double x) { return 2.0 * x + 1.0; }

static void k_serial(const double *a, double *b, long n) {
    for (long i = 0; i < n; i++) b[i] = f(a[i]);
}
static void k_static(const double *a, double *b, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) b[i] = f(a[i]);
}
static void k_static1(const double *a, double *b, long n) {
    #pragma omp parallel for schedule(static, 1)
    for (long i = 0; i < n; i++) b[i] = f(a[i]);
}
static void k_static8(const double *a, double *b, long n) {
    #pragma omp parallel for schedule(static, 8)
    for (long i = 0; i < n; i++) b[i] = f(a[i]);
}
static void k_static64(const double *a, double *b, long n) {
    #pragma omp parallel for schedule(static, 64)
    for (long i = 0; i < n; i++) b[i] = f(a[i]);
}
static void k_dynamic1(const double *a, double *b, long n) {
    #pragma omp parallel for schedule(dynamic, 1)
    for (long i = 0; i < n; i++) b[i] = f(a[i]);
}
static void k_dynamic64(const double *a, double *b, long n) {
    #pragma omp parallel for schedule(dynamic, 64)
    for (long i = 0; i < n; i++) b[i] = f(a[i]);
}

typedef void (*kernel_fn)(const double *, double *, long);

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 4000000L);
    if (n < 1) n = 1;
    double *a = xmalloc((size_t)n * sizeof(double));
    double *b = xmalloc((size_t)n * sizeof(double));
    double *ref = xmalloc((size_t)n * sizeof(double));
    fill_double(a, n, -1.0, 1.0, 13);
    omp_set_dynamic(0);
    omp_warmup();

    const char *name[6] = {"static", "static,1", "static,8", "static,64", "dynamic,1", "dynamic,64"};
    const kernel_fn fn[6] = {k_static, k_static1, k_static8, k_static64, k_dynamic1, k_dynamic64};
    double ts, t[NTHREADS];
    BEST_TIME(ts, k_serial(a, ref, n));
    printf("P013: b[i] = 2*a[i] + 1 with different schedules, N = %ld, serial time %.5f s\n", n, ts);
    printf("time in seconds for 1, 2, 4, 8 threads; speedup = serial / time on 8 threads\n");
    printf("%-12s %10s %10s %10s %10s %10s %6s\n", "schedule", "1 thr", "2 thr", "4 thr", "8 thr", "speedup", "check");
    int all_ok = 1;
    for (int s = 0; s < 6; s++) {
        int ok = 1;
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            memset(b, 0, (size_t)n * sizeof(double));       /* a skipped element would stay 0 */
            BEST_TIME(t[k], fn[s](a, b, n));
            if (memcmp(b, ref, (size_t)n * sizeof(double)) != 0) ok = 0;   /* bit-identical */
        }
        all_ok &= ok;
        printf("%-12s %10.5f %10.5f %10.5f %10.5f %10.2f %6s\n", name[s],
               t[0], t[1], t[2], t[3], ts / t[3], ok_str(ok));
    }
    printf("\nstatic,1 / dynamic,1: threads interleave element by element -> false sharing on b,\n"
           "every thread drags in every cache line of a, no vectorisation; dynamic,1 also takes\n"
           "the scheduler's shared counter once per element. Chunks covering a whole cache line\n"
           "(8 doubles on x86, 16 on Apple M) fix false sharing; plain static is best here.\n");
    printf("All schedules give the serial result exactly: %s\n", ok_str(all_ok));
    free(a); free(b); free(ref);
    return 0;
}

/* Notes:
 * - schedule(static) without a chunk = one contiguous block per thread: best locality,
 *   no scheduling cost; right for loops whose iterations all cost the same.
 * - Chunk 1 is only reasonable when each iteration is BIG and unequal (e.g. one row
 *   of a triangular matrix each), never for one cheap element per iteration.
 * - dynamic costs an atomic increment of a shared counter per chunk; with chunk 1 that
 *   is per element, which can cost more than the work itself. Use dynamic,64 or guided.
 * - A chunk should cover at least a cache line of the written array (64 B = 8 doubles
 *   on x86, 128 B = 16 doubles on Apple M) so that two threads rarely write the same line.
 * - The result is identical for every schedule because each b[i] is computed the same
 *   way by whichever thread gets i: scheduling changes who/when, not what.
 */

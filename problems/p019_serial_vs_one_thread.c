/* P019: Sequential code vs single-threaded OpenMP code
 * Problem: Time the loop a[i] = sqrt(b[i]) + c[i]*d (i = 0..N-1) written as plain
 *   sequential code and as an OpenMP "parallel for" run with ONE thread. Check that
 *   both produce the same a[], print both times and their ratio, and explain the
 *   difference (parallel-region overhead, the outlined function, optimisation level).
 * Solution: iterations are independent (a[i] needs only b[i] and c[i]), so a plain
 *   "#pragma omp parallel for" is race-free. With 1 thread the arithmetic is the same,
 *   so any extra time is OpenMP overhead: starting/joining the team, computing the
 *   thread's iteration range, and running the body as an "outlined" function
 *   (kernel_omp._omp_fn.0) that reaches a, b, c, d through a struct of pointers, so
 *   gcc may not inline it, propagate constants or vectorise it as well. The overhead is
 *   a fixed cost per region: it shows up for a small N called many times and
 *   disappears for a large N.
 * Source: textbook Exercise 19.6.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p019_serial_vs_one_thread.c -o p019 -lm
 *   Compare optimisation levels (add -fopt-info-vec to see which loops were vectorised):
 *     for o in 0 2 3; do gcc -O$o -fopenmp p019_serial_vs_one_thread.c -o p019_O$o -lm; ./p019_O$o; done
 * Run:   ./p019 [N] [small_N]   (defaults N = 1e7, small_N = 1000)
 */
#include "common.h"

/* Plain sequential loop. */
static void kernel_serial(double *a, const double *b, const double *c, double d, long n) {
    for (long i = 0; i < n; i++) a[i] = sqrt(b[i]) + c[i] * d;
}

/* The same loop with OpenMP; run with 1 thread it shows the pure OpenMP overhead. */
static void kernel_omp(double *a, const double *b, const double *c, double d, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) a[i] = sqrt(b[i]) + c[i] * d;
}

/* Average cost of one parallel region whose body does (almost) nothing.
 * The body must not be empty: gcc deletes an empty parallel region. */
static double region_cost(int threads, int calls) {
    omp_set_num_threads(threads);
    double t0 = omp_get_wtime();
    for (int r = 0; r < calls; r++) {
        #pragma omp parallel
        { volatile int id = omp_get_thread_num(); (void)id; }
    }
    return (omp_get_wtime() - t0) / calls;
}

int main(int argc, char *argv[]) {
    long n  = arg_long(argc, argv, 1, 10000000L);
    long ns = arg_long(argc, argv, 2, 1000L);          /* small array for the overhead test */
    if (ns > n) ns = n;
    const double d = 2.5;
    double *a1 = xmalloc((size_t)n * sizeof(double));  /* sequential result */
    double *a2 = xmalloc((size_t)n * sizeof(double));  /* OpenMP result */
    double *b  = xmalloc((size_t)n * sizeof(double));
    double *c  = xmalloc((size_t)n * sizeof(double));
    fill_double(b, n, 0.0, 100.0, 1);                  /* b >= 0 so sqrt is defined */
    fill_double(c, n, -1.0, 1.0, 2);
    omp_warmup();

    /* 1. Large N: one call, the region overhead is negligible next to the work. */
    double ts, t1;
    BEST_TIME(ts, kernel_serial(a1, b, c, d, n));
    omp_set_num_threads(1);
    BEST_TIME(t1, kernel_omp(a2, b, c, d, n));
    int ok = close_doubles(a1, a2, n, 1e-15);
    printf("P019: a[i] = sqrt(b[i]) + c[i]*d, sequential vs OpenMP with 1 thread\n");
    printf("Large N = %ld (one call):\n", n);
    printf("  sequential code          %12.6f s\n", ts);
    printf("  OpenMP, 1 thread         %12.6f s\n", t1);
    printf("  ratio omp1 / sequential  %12.3f    results %s\n", ts > 0 ? t1 / ts : 0.0, ok_str(ok));

    /* 2. Small N called many times: now the fixed per-region cost is visible. */
    long calls = ns > 0 ? 20000000L / ns : 1;            /* about 2e7 element updates in total */
    if (calls < 1) calls = 1;
    if (calls > 200000) calls = 200000;
    double tss, ts1;
    BEST_TIME(tss, for (long r = 0; r < calls; r++) kernel_serial(a1, b, c, d, ns));
    BEST_TIME(ts1, for (long r = 0; r < calls; r++) kernel_omp(a2, b, c, d, ns));
    int ok_small = close_doubles(a1, a2, ns, 1e-15);
    printf("Small N = %ld (called %ld times, time per call):\n", ns, calls);
    printf("  sequential code          %12.3f us\n", 1e6 * tss / calls);
    printf("  OpenMP, 1 thread         %12.3f us\n", 1e6 * ts1 / calls);
    printf("  ratio omp1 / sequential  %12.3f    results %s\n", tss > 0 ? ts1 / tss : 0.0, ok_str(ok_small));

    /* 3. Cost of entering and leaving one (almost empty) parallel region. */
    printf("Cost of one empty parallel region:\n");
    for (int k = 0; k < NTHREADS; k++)
        printf("  %d thread(s)              %12.3f us\n", THREADS[k], 1e6 * region_cost(THREADS[k], 5000));

    /* 4. The usual speedup table for large N (speedup against the SEQUENTIAL code). */
    printf("Speedup table, large N:\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        double tp;
        BEST_TIME(tp, kernel_omp(a2, b, c, d, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(a1, a2, n, 1e-15));
    }
    free(a1); free(a2); free(b); free(c);
    return 0;
}

/* Notes:
 * - 1-thread OpenMP is not free: GOMP_parallel() is called, a team (of 1) is set up,
 *   the iteration range is computed and the body runs as an outlined function. That
 *   costs roughly 0.1-1 us per region with 1 thread and several us with many threads.
 * - Outlined function: the loop body moves to kernel_omp._omp_fn.0, which gets the
 *   shared variables through a struct pointer; the compiler knows less (aliasing,
 *   constant d) so it may optimise less than the sequential loop, which can be inlined.
 * - Optimisation level: at -O0 both versions are slow and the ratio is about 1. At
 *   -O2/-O3 gcc does not vectorise sqrt (it must set errno for negative input); with
 *   -O3 -fno-math-errno it does, and the inlined sequential code can then beat the
 *   outlined loop even for large N (it may even drop the repeated identical small-N
 *   calls, giving ~0 us). Check with -fopt-info-vec.
 * - Fix for tiny loops: don't parallelise them, or use an if clause:
 *   #pragma omp parallel for if(n > 10000).
 * - Speedup must be measured against the best SEQUENTIAL code, not against the
 *   OpenMP code run with 1 thread (that would hide the overhead).
 */

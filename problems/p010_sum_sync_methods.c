/* P010: One sum, four ways to synchronise it: critical, atomic, local+critical, reduction
 * Problem: Compute S = sum_{i<N} sin(i)*cos(i) in parallel in four ways:
 *   (a) critical around every update, (b) atomic on every update, (c) accumulate in a
 *   thread-local variable and add it to S with one critical per thread, (d) the
 *   reduction clause. Verify each against the serial sum, time them for 1, 2, 4, 8
 *   threads and explain the ordering of the timings.
 * Solution: the term f(i) is computed outside any lock in all versions; only the
 *   update of the shared S is protected. (a) and (b) synchronise N times, (c) and (d)
 *   only once per thread, so (c) ~ (d) << (b) <= (a) typically. All four are race-free.
 * Source: textbook Ex 23.1 / 20.1.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p010_sum_sync_methods.c -o p010 -lm
 * Run:   ./p010 [N]          (default N = 2e6)
 */
#include "common.h"

static double f(long i) { return sin((double)i) * cos((double)i); }

static double sum_serial(long n) {
    double s = 0.0;
    for (long i = 0; i < n; i++) s += f(i);
    return s;
}

/* (a) a lock acquire/release for every term */
static double sum_critical(long n) {
    double s = 0.0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        double v = f(i);              /* the expensive part stays outside the lock */
        #pragma omp critical
        s += v;
    }
    return s;
}

/* (b) a hardware atomic read-modify-write for every term */
static double sum_atomic(long n) {
    double s = 0.0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        double v = f(i);
        #pragma omp atomic
        s += v;
    }
    return s;
}

/* (c) private partial sum, one critical per thread */
static double sum_local_critical(long n) {
    double s = 0.0;
    #pragma omp parallel
    {
        double local = 0.0;           /* private: declared inside the region */
        #pragma omp for
        for (long i = 0; i < n; i++) local += f(i);
        #pragma omp critical
        s += local;
    }
    return s;
}

/* (d) reduction: OpenMP does (c) for us */
static double sum_reduction(long n) {
    double s = 0.0;
    #pragma omp parallel for reduction(+:s)
    for (long i = 0; i < n; i++) s += f(i);
    return s;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 2000000L);
    omp_set_dynamic(0);
    omp_warmup();

    double ts, ta, tb, tc, td, rs = 0, ra = 0, rb = 0, rc = 0, rd = 0;
    BEST_TIME(ts, rs = sum_serial(n));
    printf("P010: S = sum sin(i)cos(i), N = %ld, serial S = %.10f, serial time %.4f s\n", n, rs, ts);
    printf("time in seconds, (speedup = serial / parallel)\n");
    printf("%8s %18s %18s %18s %18s %6s\n", "threads",
           "(a) critical", "(b) atomic", "(c) local+crit", "(d) reduction", "check");
    int all_ok = 1;
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(ta, ra = sum_critical(n));
        BEST_TIME(tb, rb = sum_atomic(n));
        BEST_TIME(tc, rc = sum_local_critical(n));
        BEST_TIME(td, rd = sum_reduction(n));
        int ok = close1(rs, ra, 1e-9) && close1(rs, rb, 1e-9) &&
                 close1(rs, rc, 1e-9) && close1(rs, rd, 1e-9);
        all_ok &= ok;
        printf("%8d %10.4f (%5.2f) %10.4f (%5.2f) %10.4f (%5.2f) %10.4f (%5.2f) %6s\n", THREADS[k],
               ta, ts / ta, tb, ts / tb, tc, ts / tc, td, ts / td, ok_str(ok));
    }
    printf("\nOrdering: (c) ~ (d) fastest: 1 synchronisation per thread, they scale.\n"
           "(a) and (b) synchronise N times and get SLOWER with more threads: the cache line\n"
           "holding S bounces between cores. Usually atomic < critical, but a double atomic\n"
           "add is a compare-and-swap retry loop, so under heavy contention it can be as slow\n"
           "as critical or slower (seen on ARM/Apple CPUs).\n");
    printf("All four sums agree with serial (rel. tol 1e-9): %s\n", ok_str(all_ok));
    return 0;
}

/* Notes:
 * - critical: a named or unnamed lock around a block; any code inside. atomic: a single
 *   scalar update (x op= expr, x++ ...) done by one hardware instruction (integers) or
 *   a compare-and-swap loop (doubles); usually cheaper than critical, one statement only.
 * - With 1 thread (a) and (b) still pay for the lock/atomic on every term; with more
 *   threads they get SLOWER than serial because every update is serialised and the
 *   cache line holding S ping-pongs between cores.
 * - (c) is exactly what reduction(+:s) does internally: private copy, initialised to 0,
 *   combined once at the end. Use reduction when it fits; (c) when the combine step is
 *   not a built-in operator (e.g. a struct or an array).
 * - The parallel sums add in a different order, so they differ from serial in the last
 *   bits: compare with a relative tolerance, never with ==.
 * - Always compute the expensive term BEFORE entering the critical/atomic, so only
 *   the tiny update is serialised.
 */

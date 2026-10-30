/* P031: Loop with an index conflict: x[i/2] += f(i)
 * Problem: Explain why the loop
 *       for (i = 0; i < n; i++) x[i/2] += f(i);
 *   cannot be parallelised as written. Demonstrate the race, then rewrite it so
 *   that it is parallel and correct, and also give a version with atomic updates.
 *   Verify both against the serial loop and compare their times.
 * Solution: iterations 2k and 2k+1 both update x[k] (read-modify-write), so if they
 *   run on different threads one update can be lost: a data race. Fix 1 (owner
 *   computes): loop over the OUTPUT index j and let iteration j do both updates,
 *   x[j] += f(2j) + f(2j+1); every x[j] is written by exactly one iteration, so a
 *   plain parallel for is race-free (and needs no synchronisation). Fix 2: keep the
 *   loop over i and make each update "#pragma omp atomic" (correct, but every update
 *   pays for an atomic instruction). f returns integers, so results compare exactly.
 * Source: textbook Exercise 19.3.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p031_index_conflict.c -o p031 -lm
 * Run:   ./p031 [n]   (default n = 2e7; x has (n+1)/2 elements)
 */
#include "common.h"

static long f(long i) { return i % 10 + 1; }   /* any function of i only */

/* Original loop. x[k] gets f(2k) + f(2k+1). */
static void scatter_serial(long *x, long n) {
    long m = (n + 1) / 2;
    for (long j = 0; j < m; j++) x[j] = 0;
    for (long i = 0; i < n; i++) x[i / 2] += f(i);
}

/* WRONG on purpose: parallel for on the original loop. schedule(static, 1) deals
 * i = 0, 1, 2, ... round-robin, so 2k and 2k+1 always run on different threads.
 * volatile forces a real load and store for each += (as in unoptimised code). */
static void scatter_race(long *x, long n) {
    long m = (n + 1) / 2;
    for (long j = 0; j < m; j++) x[j] = 0;
    volatile long *vx = x;
    #pragma omp parallel for schedule(static, 1)
    for (long i = 0; i < n; i++) vx[i / 2] += f(i);   /* RACE on vx[i/2] */
}

/* Fix 1: owner computes. Iteration j is the only one that touches x[j]. */
static void scatter_owner(long *x, long n) {
    long m = (n + 1) / 2;
    #pragma omp parallel for
    for (long j = 0; j < m; j++) {
        x[j] = 0;
        x[j] += f(2 * j);
        if (2 * j + 1 < n) x[j] += f(2 * j + 1);       /* odd n: last x[j] has one term */
    }
}

/* Fix 2: same loop as the original, each update made atomic. */
static void scatter_atomic(long *x, long n) {
    long m = (n + 1) / 2;
    #pragma omp parallel
    {
        #pragma omp for
        for (long j = 0; j < m; j++) x[j] = 0;
        /* implicit barrier: all of x is zero before anyone adds to it */
        #pragma omp for
        for (long i = 0; i < n; i++) {
            #pragma omp atomic
            x[i / 2] += f(i);
        }
    }
}

/* Number of elements of x that differ from the reference. */
static long count_wrong(const long *x, const long *ref, long m) {
    long bad = 0;
    for (long j = 0; j < m; j++) if (x[j] != ref[j]) bad++;
    return bad;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    if (n < 0) n = 0;
    long m = (n + 1) / 2;
    long *x0 = xmalloc((size_t)m * sizeof(long));    /* serial reference */
    long *x  = xmalloc((size_t)m * sizeof(long));
    omp_warmup();

    double ts, to, ta;
    BEST_TIME(ts, scatter_serial(x0, n));
    printf("P031: x[i/2] += f(i), n = %ld, x has %ld elements\n", n, m);

    printf("Race demo: parallel for on the original loop, schedule(static,1):\n");
    for (int k = 1; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        scatter_race(x, n);
        printf("  %d threads: %ld of %ld elements wrong (lost updates; varies per run, 0 = lucky)\n",
               THREADS[k], count_wrong(x, x0, m), m);
    }

    printf("Fixes: A = loop over output index j (owner computes), B = atomic update\n");
    printf("%8s %12s %12s %8s %12s %8s %6s\n", "threads", "serial(s)", "A(s)", "speedA", "B(s)", "speedB", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(to, scatter_owner(x, n));
        int ok = same_longs(x0, x, m);
        BEST_TIME(ta, scatter_atomic(x, n));
        ok = ok && same_longs(x0, x, m);
        printf("%8d %12.6f %12.6f %8.2f %12.6f %8.2f %6s\n", THREADS[k], ts, to,
               to > 0 ? ts / to : 0.0, ta, ta > 0 ? ts / ta : 0.0, ok_str(ok));
    }
    free(x0); free(x);
    return 0;
}

/* Notes:
 * - The conflict is a WRITE-WRITE dependence between iterations 2k and 2k+1 on x[k].
 *   x[k] += v is load, add, store; two threads can both load the old value and one
 *   store overwrites the other: a lost update.
 * - With the default schedule(static) the race only hits pairs split across a block
 *   boundary, so it may look correct by luck; schedule(static,1) exposes it.
 * - Owner computes ("loop over what you write") removes the conflict instead of
 *   synchronising it: no atomics, no critical, and one store per x[j] instead of two
 *   read-modify-writes, which is why it beats the serial loop even on 1 thread.
 * - atomic is correct and cheap when updates rarely collide, but it is still an
 *   atomic read-modify-write per update; critical would be far slower (a lock).
 * - Another fix: reduction(+:x[:m]) (OpenMP 4.5 array section), but every thread
 *   then needs its own copy of the whole array.
 */

/* P040: Loop-carried dependencies: classify them and parallelise correctly
 * Problem: For each loop below, say whether it has a loop-carried dependency
 *   and of which kind, then give a correct OpenMP version, verify it against
 *   the serial loop and report time and speedup for 1, 2, 4, 8 threads:
 *   (a) for i = 1..N-1:  a[i] = a[i-1] + b[i]
 *   (b) for i = 0..N-2:  a[i] = a[i+1] + c
 *   (c) for i = 0..N-1:  a[i] = 2*b[i];  c[i] = a[i] + 1
 *   (d) for i = k..N-1:  a[i] = a[i-k] + b[i]      (dependence distance k)
 *   (e) for i = 1..N-1:  a[i] = 2*b[i];  c[i] = a[i-1] + 1
 * Solution: (a) flow (true, read-after-write) dependence, distance 1: it is a
 *   prefix sum, a[i] = a[0] + b[1] + ... + b[i] -> blocked two-pass scan.
 *   (b) anti (write-after-read) dependence: iteration i must read the OLD
 *   a[i+1] -> copy a to old[] first, then a[i] = old[i+1] + c is independent.
 *   (c) only a dependence inside one iteration (not carried) -> plain parallel
 *   for, the fused loop is fine. (d) flow dependence at distance k: iterations
 *   i, i+k, i+2k, ... form k independent chains -> parallel for over the chains
 *   (at most k-way parallel), or blocks of k consecutive iterations are
 *   independent -> serial loop over blocks, omp for inside (a barrier per
 *   block). (e) carried flow dependence between two statements -> loop
 *   fission: first loop computes all a, implicit barrier, second loop all c.
 *   The naive `parallel for` versions of (a) and (b) are run to show the race.
 * Source: lecture slides (data dependence analysis); lab extension.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p040_loop_dependencies.c -o p040 -lm
 * Run:   ./p040 [N] [k]      (defaults N = 5e6, k = 1000)
 */
#include "common.h"

#define C_CONST 7       /* the constant c of loop (b) */

static long count_diff(const long *x, const long *y, long n) {
    long d = 0;
    for (long i = 0; i < n; i++) d += (x[i] != y[i]);
    return d;
}
static void copy_longs(long *dst, const long *src, long n) {
    memcpy(dst, src, (size_t)n * sizeof(long));
}

/* ======================= (a) a[i] = a[i-1] + b[i] ======================= */
static void a_serial(long *a, const long *b, long n) {
    for (long i = 1; i < n; i++) a[i] = a[i-1] + b[i];
}
static void a_naive(long *a, const long *b, long n) {   /* WRONG on purpose (race) */
    #pragma omp parallel for
    for (long i = 1; i < n; i++) a[i] = a[i-1] + b[i];
}
/* a[i] = a[0] + b[1] + ... + b[i]: scan of b over iterations 1..n-1, started at a[0]. */
static void a_parallel(long *a, const long *b, long n) {
    long *off = xmalloc((size_t)omp_get_max_threads() * sizeof(long));
    long m = n - 1;                                     /* number of iterations */
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = 1 + m * tid / nt, hi = 1 + m * (tid + 1) / nt;   /* my iterations */

        long s = 0;                                     /* pass 1: block total of b */
        for (long i = lo; i < hi; i++) s += b[i];
        off[tid] = s;
        #pragma omp barrier
        #pragma omp single
        {                                               /* off[t] = a[lo_t - 1] */
            long run = a[0];
            for (int t = 0; t < nt; t++) { long tot = off[t]; off[t] = run; run += tot; }
        }
        s = off[tid];                                   /* pass 2 */
        for (long i = lo; i < hi; i++) { s += b[i]; a[i] = s; }
    }
    free(off);
}

/* ======================= (b) a[i] = a[i+1] + c ========================== */
static void b_serial(long *a, long n) {
    for (long i = 0; i < n - 1; i++) a[i] = a[i+1] + C_CONST;   /* a[i+1] is still OLD */
}
static void b_naive(long *a, long n) {                  /* WRONG on purpose (race) */
    #pragma omp parallel for
    for (long i = 0; i < n - 1; i++) a[i] = a[i+1] + C_CONST;
}
static void b_parallel(long *a, long *old, long n) {
    #pragma omp parallel
    {
        #pragma omp for
        for (long i = 0; i < n; i++) old[i] = a[i];     /* snapshot of the old values */
        /* implicit barrier: old[] complete before anyone overwrites a[] */
        #pragma omp for
        for (long i = 0; i < n - 1; i++) a[i] = old[i+1] + C_CONST;
    }
}

/* ============== (c) a[i] = 2*b[i]; c[i] = a[i] + 1 ======================= */
static void c_serial(long *a, const long *b, long *c, long n) {
    for (long i = 0; i < n; i++) { a[i] = 2 * b[i]; c[i] = a[i] + 1; }
}
static void c_parallel(long *a, const long *b, long *c, long n) {
    #pragma omp parallel for                            /* a[i] is used in the SAME iteration */
    for (long i = 0; i < n; i++) { a[i] = 2 * b[i]; c[i] = a[i] + 1; }
}

/* ======================= (d) a[i] = a[i-k] + b[i] ======================= */
static void d_serial(long *a, const long *b, long n, long k) {
    for (long i = k; i < n; i++) a[i] = a[i-k] + b[i];
}
/* (d1) chain r = {k+r, 2k+r, ...}: each chain is serial, different chains are independent. */
static void d_chains(long *a, const long *b, long n, long k) {
    #pragma omp parallel for schedule(static)
    for (long r = 0; r < k; r++)
        for (long i = k + r; i < n; i += k) a[i] = a[i-k] + b[i];
}
/* (d2) iterations [s, s+k) only read [s-k, s), i.e. the previous block -> independent. */
static void d_blocks(long *a, const long *b, long n, long k) {
    #pragma omp parallel
    for (long s = k; s < n; s += k) {                   /* every thread runs this loop */
        long e = (s + k < n) ? s + k : n;
        #pragma omp for
        for (long i = s; i < e; i++) a[i] = a[i-k] + b[i];
        /* implicit barrier: block done before the next block reads it */
    }
}

/* ============== (e) a[i] = 2*b[i]; c[i] = a[i-1] + 1 ===================== */
static void e_serial(long *a, const long *b, long *c, long n) {
    for (long i = 1; i < n; i++) { a[i] = 2 * b[i]; c[i] = a[i-1] + 1; }
}
static void e_parallel(long *a, const long *b, long *c, long n) {   /* loop fission */
    #pragma omp parallel
    {
        #pragma omp for
        for (long i = 1; i < n; i++) a[i] = 2 * b[i];
        /* implicit barrier: every a[i-1] is ready */
        #pragma omp for
        for (long i = 1; i < n; i++) c[i] = a[i-1] + 1;
    }
}

/* =============================== driver ================================= */
int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 5000000L);
    long k = arg_long(argc, argv, 2, 1000L);
    if (n < 2) n = 2;
    if (k < 1) k = 1;
    if (k > n) k = n;
    long *a0 = xmalloc((size_t)n * sizeof(long)), *b = xmalloc((size_t)n * sizeof(long));
    long *a  = xmalloc((size_t)n * sizeof(long)), *c = xmalloc((size_t)n * sizeof(long));
    long *ra = xmalloc((size_t)n * sizeof(long)), *rc = xmalloc((size_t)n * sizeof(long));
    long *old = xmalloc((size_t)n * sizeof(long));
    for (long i = 0; i < n; i++) { a0[i] = (i * 37) % 201 - 100; b[i] = (i * 53) % 201 - 100; }
    omp_warmup();
    printf("P040: loop-carried dependencies, N = %ld, k = %ld\n", n, k);
    double ts, tp;
    int ok;

    /* Pattern for every case: reference = serial once from a fresh copy; each
     * parallel version is checked from a fresh copy, then timed. */
    printf("\n(a) a[i] = a[i-1] + b[i]: FLOW (true, RAW) dependence, distance 1 -> carried.\n"
           "    Strategy: it is a prefix sum -> blocked two-pass scan.\n");
    copy_longs(a, a0, n); a_serial(a, b, n); copy_longs(ra, a, n);
    BEST_TIME(ts, a_serial(a, b, n));
    omp_set_num_threads(8);
    copy_longs(a, a0, n); a_naive(a, b, n);
    printf("    naive parallel for (8 threads): %ld of %ld elements differ (race)\n",
           count_diff(a, ra, n), n);
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        copy_longs(a, a0, n); a_parallel(a, b, n); ok = same_longs(a, ra, n);
        BEST_TIME(tp, a_parallel(a, b, n));
        table_row(n, THREADS[t], ts, tp, ok);
    }

    printf("\n(b) a[i] = a[i+1] + c: ANTI (WAR) dependence -> carried (i reads what i+1 writes).\n"
           "    Strategy: read the OLD values from a copy: old = a; a[i] = old[i+1] + c.\n");
    copy_longs(a, a0, n); b_serial(a, n); copy_longs(ra, a, n);
    BEST_TIME(ts, (copy_longs(a, a0, n), b_serial(a, n)));
    omp_set_num_threads(8);
    copy_longs(a, a0, n); b_naive(a, n);
    printf("    naive parallel for (8 threads): %ld of %ld elements differ (race at block edges)\n",
           count_diff(a, ra, n), n);
    printf("    (timed runs include resetting a from the input: serial memcpy, parallel copy loop)\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        copy_longs(a, a0, n); b_parallel(a, old, n); ok = same_longs(a, ra, n);
        BEST_TIME(tp, (copy_longs(a, a0, n), b_parallel(a, old, n)));
        table_row(n, THREADS[t], ts, tp, ok);
    }

    printf("\n(c) a[i] = 2*b[i]; c[i] = a[i] + 1: dependence only INSIDE one iteration -> not carried.\n"
           "    Strategy: plain parallel for over the fused loop.\n");
    c_serial(a, b, c, n); copy_longs(ra, a, n); copy_longs(rc, c, n);
    BEST_TIME(ts, c_serial(a, b, c, n));
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        memset(a, 0, (size_t)n * sizeof(long)); memset(c, 0, (size_t)n * sizeof(long));
        c_parallel(a, b, c, n); ok = same_longs(a, ra, n) && same_longs(c, rc, n);
        BEST_TIME(tp, c_parallel(a, b, c, n));
        table_row(n, THREADS[t], ts, tp, ok);
    }

    printf("\n(d) a[i] = a[i-k] + b[i]: FLOW dependence at distance k = %ld -> carried.\n", k);
    copy_longs(a, a0, n); d_serial(a, b, n, k); copy_longs(ra, a, n);
    BEST_TIME(ts, d_serial(a, b, n, k));
    printf("    Strategy d1: k independent chains i = r, r+k, r+2k, ... -> parallel for over r\n"
           "    (at most k-way parallel).\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        copy_longs(a, a0, n); d_chains(a, b, n, k); ok = same_longs(a, ra, n);
        BEST_TIME(tp, d_chains(a, b, n, k));
        table_row(n, THREADS[t], ts, tp, ok);
    }
    printf("    Strategy d2: blocks of k consecutive iterations are independent -> serial loop\n"
           "    over blocks, omp for inside (one barrier per block: N/k = %ld barriers).\n", (n - 1) / k);
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        copy_longs(a, a0, n); d_blocks(a, b, n, k); ok = same_longs(a, ra, n);
        BEST_TIME(tp, d_blocks(a, b, n, k));
        table_row(n, THREADS[t], ts, tp, ok);
    }

    printf("\n(e) a[i] = 2*b[i]; c[i] = a[i-1] + 1: FLOW dependence between the two statements,\n"
           "    distance 1 -> carried. Strategy: loop fission (all a, barrier, then all c).\n");
    copy_longs(a, a0, n); copy_longs(c, a0, n); e_serial(a, b, c, n);
    copy_longs(ra, a, n); copy_longs(rc, c, n);
    BEST_TIME(ts, e_serial(a, b, c, n));
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        copy_longs(a, a0, n); copy_longs(c, a0, n);
        e_parallel(a, b, c, n); ok = same_longs(a, ra, n) && same_longs(c, rc, n);
        BEST_TIME(tp, e_parallel(a, b, c, n));
        table_row(n, THREADS[t], ts, tp, ok);
    }

    free(a0); free(b); free(a); free(c); free(ra); free(rc); free(old);
    return 0;
}

/* Notes:
 * - Test for a carried dependence: does iteration i touch a location that a
 *   DIFFERENT iteration writes? Flow/true (RAW): reads a value written earlier
 *   (a). Anti (WAR): reads a value a later iteration overwrites (b). Output
 *   (WAW): two iterations write the same location (e.g. a shared temp t ->
 *   fix with private(t), or lastprivate(t) if the last value is needed).
 * - Anti and output dependences are "false" (name) dependences: removed by
 *   copying/renaming (old[] in b, private variables). Flow dependences are real:
 *   they need a different algorithm (scan in a, chains/blocks in d, fission in e).
 * - Naive parallel for on (a): each thread starts from a stale a[lo-1], so whole
 *   blocks are wrong; on (b) only the block edges are wrong, which is why such
 *   bugs pass small tests. Results also change from run to run.
 * - (d): with k >= threads the chain version is fully parallel but strided
 *   (cache-unfriendly); with k < threads only k threads have work. The block
 *   version is contiguous but pays N/k barriers, so it needs large k.
 * - Distance-k with a pure copy a[i] = a[i-k] has a closed form a[i] = a[i % k]
 *   (fully parallel); look for such closed forms before building a scan.
 */

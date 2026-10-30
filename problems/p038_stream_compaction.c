/* P038: Stream compaction (parallel filter that keeps the order)
 * Problem: Given an int array a[N], copy the elements that satisfy a predicate
 *   (here: a[i] % 3 == 0) into a new array out[], in their original order, and
 *   return how many were copied. Parallelise with OpenMP, verify against the
 *   serial filter and report time, speedup and efficiency for 1, 2, 4, 8 threads.
 * Solution: the output position of a kept element = number of kept elements
 *   before it, which is a prefix sum of the 0/1 "keep" flags. Blocked two-pass
 *   in one parallel region: pass 1, each thread counts the kept elements of its
 *   block [lo, hi); barrier; single does an exclusive scan of the counts ->
 *   start[t] (and the total); implicit barrier; pass 2, each thread re-reads its
 *   block and writes kept elements to out[start[t]], out[start[t]+1], ...
 *   Threads write disjoint slices of out and blocks are in thread order, so the
 *   result is race-free and in the original order. No atomics, no locks.
 * Source: lab extension (scan application: filter / pack / compact).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p038_stream_compaction.c -o p038 -lm
 * Run:   ./p038 [N]          (default N = 1e7)
 */
#include "common.h"

/* The predicate: change only this for another filter (x > 0, x is prime, ...). */
static inline int keep(int x) { return x % 3 == 0; }

static long compact_serial(const int *a, int *out, long n) {
    long k = 0;
    for (long i = 0; i < n; i++)
        if (keep(a[i])) out[k++] = a[i];
    return k;
}

static long compact_parallel(const int *a, int *out, long n) {
    long *start = xmalloc((size_t)omp_get_max_threads() * sizeof(long));
    long total = 0;
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;       /* my block [lo, hi) */

        long c = 0;                                            /* pass 1: count */
        for (long i = lo; i < hi; i++) c += keep(a[i]);
        start[tid] = c;

        #pragma omp barrier                                    /* all counts written */
        #pragma omp single
        {                                                      /* exclusive scan of counts */
            long run = 0;
            for (int t = 0; t < nt; t++) { long ct = start[t]; start[t] = run; run += ct; }
            total = run;
        }                                                      /* implicit barrier */

        long pos = start[tid];                                 /* pass 2: write my slice */
        for (long i = lo; i < hi; i++)
            if (keep(a[i])) out[pos++] = a[i];
    }
    free(start);
    return total;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    if (n < 1) n = 1;
    int *a   = xmalloc((size_t)n * sizeof(int));
    int *ref = xmalloc((size_t)n * sizeof(int));     /* output can be as long as the input */
    int *out = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, -1000000, 1000000, 38);
    omp_warmup();

    long ks = 0, kp = 0;
    double ts, tp;
    BEST_TIME(ts, ks = compact_serial(a, ref, n));
    printf("P038: stream compaction (keep a[i] %% 3 == 0), N = %ld, kept = %ld\n", n, ks);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(out, 0, (size_t)n * sizeof(int));
        BEST_TIME(tp, kp = compact_parallel(a, out, n));
        table_row(n, THREADS[k], ts, tp, kp == ks && same_ints(out, ref, ks));
    }
    free(a); free(ref); free(out);
    return 0;
}

/* Notes:
 * - Why not `#pragma omp parallel for` with out[k++]? k is shared: k++ races,
 *   and even with atomic capture the ORDER would be lost (threads interleave).
 * - The position of each kept element is an exclusive prefix sum of the keep
 *   flags; per-thread counts make it a scan over p numbers instead of N.
 * - Pass 2 evaluates the predicate again (cheap here). For an expensive
 *   predicate, store the flags in pass 1 (char flag[N]) and reuse them.
 * - Barrier before single: all counts must exist. Implicit barrier after single:
 *   start[] must be final before anyone writes.
 * - Same pattern: remove zeros, select rows of a table, partition (P039),
 *   run-length encoding (P044), list matches in order (P054).
 */

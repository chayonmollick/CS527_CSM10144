/* P037: Exclusive prefix sum and prefix maximum (blocked two-pass scan)
 * Problem: Given an int array a[N], compute (a) the exclusive prefix sum
 *   out[0] = 0, out[i] = a[0] + a[1] + ... + a[i-1], and (b) the running
 *   maximum (prefix max) m[i] = max(a[0], ..., a[i]). Parallelise both with
 *   OpenMP, verify exactly against the serial loops and report the time,
 *   speedup and efficiency for 1, 2, 4 and 8 threads.
 * Solution: out[i] depends on out[i-1] (loop-carried), so a plain parallel for
 *   is wrong. Blocked two-pass scan in ONE parallel region: each thread takes a
 *   contiguous block [lo, hi), pass 1 computes its block total (sum or max),
 *   barrier, a single thread turns the totals into block offsets (exclusive
 *   scan over threads; identity 0 for +, INT_MIN for max), implicit barrier,
 *   pass 2 rescans the block starting from its offset. Each thread writes only
 *   its own block and the offsets are read only after the barriers -> race-free.
 *   Any associative operator (+, max, min, *, ...) works the same way.
 * Source: lab extension of the prefix-sum task (practice q06).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p037_exclusive_scan_prefix_max.c -o p037 -lm
 * Run:   ./p037 [N]          (default N = 1e7)
 */
#include "common.h"

/* ---------- serial ---------- */
static void exscan_serial(const int *a, long *out, long n) {
    long s = 0;
    for (long i = 0; i < n; i++) { out[i] = s; s += a[i]; }   /* write BEFORE adding */
}

static void prefix_max_serial(const int *a, int *m, long n) {
    int cur = INT_MIN;
    for (long i = 0; i < n; i++) { if (a[i] > cur) cur = a[i]; m[i] = cur; }
}

/* ---------- parallel: blocked two-pass scans ---------- */
static void exscan_parallel(const int *a, long *out, long n) {
    long *off = xmalloc((size_t)omp_get_max_threads() * sizeof(long));   /* >= team size */
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;      /* my block [lo, hi) */

        long s = 0;                                           /* pass 1: block total */
        for (long i = lo; i < hi; i++) s += a[i];
        off[tid] = s;

        #pragma omp barrier                                   /* all totals written */
        #pragma omp single
        {                                                     /* exclusive scan of totals */
            long run = 0;
            for (int t = 0; t < nt; t++) { long tot = off[t]; off[t] = run; run += tot; }
        }                                                     /* implicit barrier */

        s = off[tid];                                         /* pass 2: rescan from offset */
        for (long i = lo; i < hi; i++) { out[i] = s; s += a[i]; }
    }
    free(off);
}

static void prefix_max_parallel(const int *a, int *m, long n) {
    int *off = xmalloc((size_t)omp_get_max_threads() * sizeof(int));
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;

        int mx = INT_MIN;                                     /* pass 1: block max */
        for (long i = lo; i < hi; i++) if (a[i] > mx) mx = a[i];
        off[tid] = mx;                                        /* empty block -> INT_MIN */

        #pragma omp barrier
        #pragma omp single
        {                                                     /* off[t] = max of blocks 0..t-1 */
            int run = INT_MIN;
            for (int t = 0; t < nt; t++) { int b = off[t]; off[t] = run; if (b > run) run = b; }
        }

        int cur = off[tid];                                   /* pass 2 */
        for (long i = lo; i < hi; i++) { if (a[i] > cur) cur = a[i]; m[i] = cur; }
    }
    free(off);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    if (n < 1) n = 1;
    int  *a    = xmalloc((size_t)n * sizeof(int));
    long *sref = xmalloc((size_t)n * sizeof(long)), *sout = xmalloc((size_t)n * sizeof(long));
    int  *mref = xmalloc((size_t)n * sizeof(int)),  *mout = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, -1000000, 1000000, 37);
    omp_warmup();

    double ts, tp;
    printf("P037: exclusive prefix sum and prefix max, N = %ld\n", n);

    BEST_TIME(ts, exscan_serial(a, sref, n));
    printf("\n(a) exclusive prefix sum: out[N-1] = %ld\n", sref[n - 1]);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(sout, 0, (size_t)n * sizeof(long));
        BEST_TIME(tp, exscan_parallel(a, sout, n));
        table_row(n, THREADS[k], ts, tp, same_longs(sout, sref, n));
    }

    BEST_TIME(ts, prefix_max_serial(a, mref, n));
    printf("\n(b) prefix max: m[0] = %d, m[N-1] = %d\n", mref[0], mref[n - 1]);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(mout, 0, (size_t)n * sizeof(int));
        BEST_TIME(tp, prefix_max_parallel(a, mout, n));
        table_row(n, THREADS[k], ts, tp, same_ints(mout, mref, n));
    }

    free(a); free(sref); free(sout); free(mref); free(mout);
    return 0;
}

/* Notes:
 * - Exclusive vs inclusive: exclusive writes out[i] BEFORE adding a[i]
 *   (inclusive adds first). exclusive[i] = inclusive[i] - a[i] = inclusive[i-1].
 * - This is the "reduce-then-scan" form: pass 1 only reads (block total), pass 2
 *   writes once. The other form (scan block, then add offset) writes twice.
 * - The barrier before single: every off[tid] must be written before it is
 *   scanned. The implicit barrier after single: offsets must be final before
 *   pass 2 reads them. Dropping either (or single nowait) gives wrong output.
 * - Works for any associative operator with an identity (0 for +, INT_MIN for
 *   max, INT_MAX for min, 1 for *). Empty blocks (N < threads) contribute it.
 * - ~2N operations vs N serially, two streams over memory -> memory-bound,
 *   speedup well below the thread count.
 * - OpenMP 5.0 has reduction(inscan,+:s) + #pragma omp scan exclusive(s); not
 *   available in gcc 9, so the manual version is the one to know.
 */

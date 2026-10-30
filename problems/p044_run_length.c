/* P044: Run-length encoding (count runs; encode as (value, length) pairs)
 * Problem: A run is a maximal block of equal adjacent values. Given an int
 *   array a[N], (a) count the runs and (b) produce the run-length encoding:
 *   arrays val[] and len[] with one (value, length) pair per run, in order.
 *   Parallelise both with OpenMP, verify against the serial RLE and report
 *   time and speedup for 1, 2, 4 and 8 threads.
 * Solution: i starts a run iff i == 0 or a[i] != a[i-1]; this test reads only
 *   the input, so it is independent per i. (a) runs = number of run starts ->
 *   parallel for with reduction(+:runs). (b) a run's output slot = number of
 *   run starts before it (scan of the start marks). Blocked two-pass in one
 *   parallel region: pass 1, each thread counts the starts in its block;
 *   barrier; single scans the counts -> first slot of each thread (and the
 *   total R, plus the sentinel start[R] = N); implicit barrier; pass 2, each
 *   thread writes val[r] = a[i] and start[r] = i for its starts; barrier; then
 *   omp for: len[r] = start[r+1] - start[r]. Each thread writes its own slots
 *   -> race-free, and the runs come out in order. A run that crosses a block
 *   boundary is counted once: only the block holding its first element sees
 *   a start there.
 * Source: lab extension (scan application, like stream compaction P038).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p044_run_length.c -o p044 -lm
 * Run:   ./p044 [N]          (default N = 1e7)
 */
#include "common.h"

static inline int is_start(const int *a, long i) { return i == 0 || a[i] != a[i-1]; }

/* Test data: runs of random length 1..8 with values 0..9 (neighbouring runs may
 * get the same value and then merge into one longer run - that is fine). */
static void make_runs(int *a, long n, unsigned long long seed) {
    unsigned long long s = seed;
    long i = 0;
    while (i < n) {
        int v = (int)(lcg_next(&s) % 10);
        long len = 1 + (long)(lcg_next(&s) % 8);
        for (long j = 0; j < len && i < n; j++) a[i++] = v;
    }
}

/* ---------- (a) count runs ---------- */
static long count_runs_serial(const int *a, long n) {
    long r = 0;
    for (long i = 0; i < n; i++) r += is_start(a, i);
    return r;
}
static long count_runs_parallel(const int *a, long n) {
    long r = 0;
    #pragma omp parallel for reduction(+:r)
    for (long i = 0; i < n; i++) r += is_start(a, i);
    return r;
}

/* ---------- (b) run-length encoding ---------- */
static long rle_serial(const int *a, long n, int *val, long *len) {
    long r = 0;
    for (long i = 0; i < n; i++) {
        if (is_start(a, i)) { val[r] = a[i]; len[r] = 0; r++; }
        len[r - 1]++;
    }
    return r;
}

/* start[] needs room for R + 1 entries (sentinel start[R] = n). */
static long rle_parallel(const int *a, long n, int *val, long *len, long *start) {
    long *off = xmalloc((size_t)omp_get_max_threads() * sizeof(long));
    long nruns = 0;
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;

        long c = 0;                                     /* pass 1: count my run starts */
        for (long i = lo; i < hi; i++) c += is_start(a, i);
        off[tid] = c;

        #pragma omp barrier
        #pragma omp single
        {                                               /* exclusive scan -> my first slot */
            long run = 0;
            for (int t = 0; t < nt; t++) { long ct = off[t]; off[t] = run; run += ct; }
            nruns = run;
            start[nruns] = n;                           /* sentinel: end of the last run */
        }                                               /* implicit barrier */

        long r = off[tid];                              /* pass 2: write my runs */
        for (long i = lo; i < hi; i++)
            if (is_start(a, i)) { val[r] = a[i]; start[r] = i; r++; }

        #pragma omp barrier                             /* all start[] written */
        #pragma omp for
        for (long k = 0; k < nruns; k++) len[k] = start[k + 1] - start[k];
    }
    free(off);
    return nruns;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    if (n < 1) n = 1;
    int  *a     = xmalloc((size_t)n * sizeof(int));
    int  *vs    = xmalloc((size_t)n * sizeof(int)),  *vp = xmalloc((size_t)n * sizeof(int));
    long *ls    = xmalloc((size_t)n * sizeof(long)), *lp = xmalloc((size_t)n * sizeof(long));
    long *start = xmalloc((size_t)(n + 1) * sizeof(long));   /* worst case: every element a run */
    make_runs(a, n, 44);
    omp_warmup();
    double ts, tp;

    long cs = 0, cp = 0;
    BEST_TIME(ts, cs = count_runs_serial(a, n));
    printf("P044: run-length encoding, N = %ld, runs = %ld (avg length %.2f)\n",
           n, cs, (double)n / (double)cs);
    printf("\n(a) count runs: reduction(+) over the run-start test\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, cp = count_runs_parallel(a, n));
        table_row(n, THREADS[k], ts, tp, cp == cs);
    }

    long rs = 0, rp = 0;
    BEST_TIME(ts, rs = rle_serial(a, n, vs, ls));
    printf("\n(b) RLE (value, length) pairs: count starts, scan, write; first runs:");
    for (long r = 0; r < rs && r < 5; r++) printf(" (%d,%ld)", vs[r], ls[r]);
    printf("\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(vp, 0, (size_t)rs * sizeof(int)); memset(lp, 0, (size_t)rs * sizeof(long));
        BEST_TIME(tp, rp = rle_parallel(a, n, vp, lp, start));
        table_row(n, THREADS[k], ts, tp, rp == rs && same_ints(vp, vs, rs) && same_longs(lp, ls, rs));
    }
    free(a); free(vs); free(vp); free(ls); free(lp); free(start);
    return 0;
}

/* Notes:
 * - Reading a[i-1] across a block boundary is safe: the input is read-only.
 *   The serial "current run" state is what made the loop sequential; the
 *   run-start test removes it.
 * - Storing start[r] and taking differences avoids the hard case of a run that
 *   spans several blocks (its length would need contributions from several threads).
 * - Three barriers: before single (counts ready), after single (offsets and
 *   nruns ready), before the len loop (all start[] written).
 * - Decoding (RLE -> array) is the reverse: exclusive scan of len[] gives each
 *   run's output position, then parallel for over runs fills its range.
 * - Memory-bound: two light passes over the input; speedup flattens once
 *   bandwidth is saturated.
 */

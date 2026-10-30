/* P049: Parallel bucket sort of doubles uniform in [0, 1)
 * Problem: Sort N doubles drawn uniformly from [0, 1) with bucket sort: put
 *   x into bucket (int)(x * B) of B buckets, then sort each bucket and
 *   concatenate. Parallelise both phases with OpenMP, verify that the result
 *   is sorted and identical to qsort's output, and report time and speedup for
 *   1, 2, 4 and 8 threads.
 * Solution: phase 1 (distribute) is a parallel counting sort on the bucket
 *   number: each thread counts the buckets of its static block into its own
 *   row cnt[t][0..B); barrier; single scans in (bucket, thread) order to get
 *   each thread's write offset inside each bucket and each bucket's start;
 *   implicit barrier; each thread scatters its block to its own disjoint
 *   ranges -> no races, no locks. Barrier. Phase 2: buckets are disjoint
 *   slices of out[], so `omp for schedule(dynamic)` over buckets, each sorted
 *   with qsort; dynamic because bucket sizes vary randomly.
 * Source: lab extension (Lab 9 sorting; distribution sort = scatter + independent sorts).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p049_bucket_sort.c -o p049 -lm
 * Run:   ./p049 [N] [B]      (defaults N = 2e6, B = 1024)
 */
#include "common.h"

static int cmp_double(const void *x, const void *y) {
    double a = *(const double *)x, b = *(const double *)y;
    return (a > b) - (a < b);
}
static int is_sorted_d(const double *a, long n) {
    for (long i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}
static inline int bucket_of(double x, int nb) {
    int b = (int)(x * nb);
    if (b >= nb) b = nb - 1;                        /* guard against rounding / x = 1 */
    if (b < 0) b = 0;
    return b;
}

/* start[] has nb + 1 entries: bucket b is out[start[b] .. start[b+1]). */
static void bucket_serial(const double *in, double *out, long n, int nb, long *start) {
    for (int b = 0; b <= nb; b++) start[b] = 0;
    for (long i = 0; i < n; i++) start[bucket_of(in[i], nb) + 1]++;   /* counts, shifted by 1 */
    for (int b = 0; b < nb; b++) start[b + 1] += start[b];             /* inclusive scan -> starts */
    long *pos = xmalloc((size_t)nb * sizeof(long));
    for (int b = 0; b < nb; b++) pos[b] = start[b];
    for (long i = 0; i < n; i++) out[pos[bucket_of(in[i], nb)]++] = in[i];
    for (int b = 0; b < nb; b++)
        qsort(out + start[b], (size_t)(start[b + 1] - start[b]), sizeof(double), cmp_double);
    free(pos);
}

static void bucket_parallel(const double *in, double *out, long n, int nb, long *start) {
    long *off = xmalloc((size_t)omp_get_max_threads() * (size_t)nb * sizeof(long));  /* off[t*nb + b] */
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;
        long *my = off + (long)tid * nb;                    /* my row */

        for (int b = 0; b < nb; b++) my[b] = 0;             /* 1a: count my block */
        for (long i = lo; i < hi; i++) my[bucket_of(in[i], nb)]++;

        #pragma omp barrier
        #pragma omp single
        {                                                   /* 1b: scan in (bucket, thread) order */
            long run = 0;
            for (int b = 0; b < nb; b++) {
                start[b] = run;
                for (int t = 0; t < nt; t++) {
                    long c = off[(long)t * nb + b];
                    off[(long)t * nb + b] = run;
                    run += c;
                }
            }
            start[nb] = n;
        }                                                   /* implicit barrier */

        for (long i = lo; i < hi; i++)                      /* 1c: scatter to my ranges */
            out[my[bucket_of(in[i], nb)]++] = in[i];

        #pragma omp barrier                                 /* all buckets filled */
        #pragma omp for schedule(dynamic)                   /* 2: sort the buckets */
        for (int b = 0; b < nb; b++)
            qsort(out + start[b], (size_t)(start[b + 1] - start[b]), sizeof(double), cmp_double);
    }
    free(off);
}

int main(int argc, char *argv[]) {
    long n  = arg_long(argc, argv, 1, 2000000L);
    int  nb = (int)arg_long(argc, argv, 2, 1024L);
    if (n < 1) n = 1;
    if (nb < 1) nb = 1;
    double *in  = xmalloc((size_t)n * sizeof(double)), *ref = xmalloc((size_t)n * sizeof(double));
    double *out = xmalloc((size_t)n * sizeof(double));
    long *start = xmalloc((size_t)(nb + 1) * sizeof(long));
    fill_double(in, n, 0.0, 1.0, 49);
    omp_warmup();

    memcpy(ref, in, (size_t)n * sizeof(double));
    double t0 = omp_get_wtime();
    qsort(ref, (size_t)n, sizeof(double), cmp_double);
    double tq = omp_get_wtime() - t0;

    double ts, tp;
    BEST_TIME(ts, bucket_serial(in, out, n, nb, start));
    int ok_serial = is_sorted_d(out, n) && memcmp(out, ref, (size_t)n * sizeof(double)) == 0;
    printf("P049: bucket sort, N = %ld doubles in [0,1), B = %d buckets (~%ld per bucket)\n",
           n, nb, n / nb);
    printf("qsort %.6f s, serial bucket sort %.6f s, serial %s\n", tq, ts, ok_str(ok_serial));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(out, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, bucket_parallel(in, out, n, nb, start));
        table_row(n, THREADS[k], ts, tp,
                  is_sorted_d(out, n) && memcmp(out, ref, (size_t)n * sizeof(double)) == 0);
    }
    free(in); free(ref); free(out); free(start);
    return 0;
}

/* Notes:
 * - Why not one shared array per bucket with an atomic counter? Atomics on
 *   hot counters serialise, and the order inside a bucket would vary between
 *   runs; count + scan + scatter is deterministic and lock-free.
 * - Phase 2 is embarrassingly parallel: bucket b only touches
 *   out[start[b] .. start[b+1]). Bucket sizes differ, so schedule(dynamic).
 * - Uniform data -> about N/B per bucket, total sort work N log(N/B) instead
 *   of N log N. Skewed data (all values in a few buckets) destroys both the
 *   speedup and the load balance; choose bucket bounds from sampled quantiles.
 * - qsort is thread-safe (no shared state), so calling it from many threads on
 *   disjoint ranges is fine. rand() is not: generate data before the region.
 * - Barriers: before single (counts), after single (offsets), before the bucket
 *   sorts (all elements scattered).
 */

/* P039: Stable partition (evens first, then odds, both in original order)
 * Problem: Rearrange an int array a[N] so that all even numbers come first and
 *   all odd numbers after them, each group keeping its original relative order
 *   (stable partition). Parallelise with OpenMP, verify against the serial
 *   version and report time, speedup and efficiency for 1, 2, 4, 8 threads.
 * Solution: two stream compactions at once. Pass 1: each thread counts the
 *   evens E[t] and odds O[t] of its block [lo, hi). Barrier. single scans the
 *   counts: evens of thread t start at E[0]+...+E[t-1]; odds of thread t start
 *   at (total evens) + O[0]+...+O[t-1]. Implicit barrier. Pass 2: each thread
 *   writes its evens and odds to its own two disjoint slices of out[], in
 *   order -> stable and race-free. Out of place (out[]), copy back if needed.
 * Source: lab extension (scan application: split / partition step of radix sort).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p039_stable_partition.c -o p039 -lm
 * Run:   ./p039 [N]          (default N = 1e7)
 */
#include "common.h"

/* Elements with first_group(x) true go first. Change for another split rule. */
static inline int first_group(int x) { return x % 2 == 0; }

/* Returns the number of elements in the first group (= start of the second). */
static long partition_serial(const int *a, int *out, long n) {
    long ne = 0;
    for (long i = 0; i < n; i++) ne += first_group(a[i]);
    long pe = 0, po = ne;                       /* next even slot, next odd slot */
    for (long i = 0; i < n; i++) {
        if (first_group(a[i])) out[pe++] = a[i];
        else                   out[po++] = a[i];
    }
    return ne;
}

static long partition_parallel(const int *a, int *out, long n) {
    int maxt = omp_get_max_threads();
    long *se = xmalloc((size_t)maxt * sizeof(long));   /* count, then start of evens */
    long *so = xmalloc((size_t)maxt * sizeof(long));   /* count, then start of odds  */
    long total_even = 0;
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;

        long ce = 0;                                     /* pass 1: count my evens */
        for (long i = lo; i < hi; i++) ce += first_group(a[i]);
        se[tid] = ce;
        so[tid] = (hi - lo) - ce;                        /* the rest are odd */

        #pragma omp barrier
        #pragma omp single
        {
            long run = 0;                                /* evens fill out[0 .. E) */
            for (int t = 0; t < nt; t++) { long c = se[t]; se[t] = run; run += c; }
            total_even = run;
            for (int t = 0; t < nt; t++) { long c = so[t]; so[t] = run; run += c; }   /* odds after */
        }                                                /* implicit barrier */

        long pe = se[tid], po = so[tid];                 /* pass 2: my two slices */
        for (long i = lo; i < hi; i++) {
            if (first_group(a[i])) out[pe++] = a[i];
            else                   out[po++] = a[i];
        }
    }
    free(se); free(so);
    return total_even;
}

/* Independent sanity check: first ne are even, the rest odd. */
static int is_partitioned(const int *x, long n, long ne) {
    for (long i = 0; i < n; i++) if (first_group(x[i]) != (i < ne)) return 0;
    return 1;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    if (n < 1) n = 1;
    int *a   = xmalloc((size_t)n * sizeof(int));
    int *ref = xmalloc((size_t)n * sizeof(int));
    int *out = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, -1000000, 1000000, 39);
    omp_warmup();

    long es = 0, ep = 0;
    double ts, tp;
    BEST_TIME(ts, es = partition_serial(a, ref, n));
    printf("P039: stable partition evens | odds, N = %ld, evens = %ld, serial %s\n",
           n, es, ok_str(is_partitioned(ref, n, es)));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(out, 0, (size_t)n * sizeof(int));
        BEST_TIME(tp, ep = partition_parallel(a, out, n));
        table_row(n, THREADS[k], ts, tp, ep == es && same_ints(out, ref, n));
    }
    free(a); free(ref); free(out);
    return 0;
}

/* Notes:
 * - Stable = equal-class elements keep their input order. Holds because each
 *   thread scans its block left to right and blocks are laid out in thread
 *   order inside each group.
 * - The classic in-place two-pointer partition (swap from both ends, as in
 *   quicksort) is NOT stable and is inherently sequential; the out-of-place
 *   scan version is both stable and parallel.
 * - Only one count per thread is needed: odds = block length - evens.
 * - This is exactly one pass of a binary (1-bit) radix sort; P046 does the same
 *   with 256 groups (digits) instead of 2.
 * - Memory-bound (read twice, write once), so speedup flattens at 4-8 threads.
 */

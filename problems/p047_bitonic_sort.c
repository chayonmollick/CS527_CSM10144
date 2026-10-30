/* P047: Bitonic sort (sorting network), parallel compare-exchange stages
 * Problem: Sort an int array with the bitonic sorting network, parallelised
 *   with OpenMP. The network needs a power-of-two length; for other N pad with
 *   INT_MAX. Verify that the result is sorted and identical to qsort's output,
 *   and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: the network has log2(M)*(log2(M)+1)/2 stages (k = 2, 4, ..., M;
 *   j = k/2, ..., 1). In stage (k, j) element i is paired with l = i ^ j;
 *   the pair is sorted ascending if (i & k) == 0, else descending. Each index
 *   is in exactly ONE pair per stage and only the smaller index (i < l) does
 *   the compare-exchange, so the iterations of a stage touch disjoint
 *   elements -> `#pragma omp parallel for` over i is race-free. The implicit
 *   barrier at the end of each parallel for separates the stages. Padding
 *   with INT_MAX puts the dummies at the end; the first N are the answer.
 * Source: lecture slides (sorting networks); lab extension of odd-even sort.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p047_bitonic_sort.c -o p047 -lm
 * Run:   ./p047 [N]          (default N = 1e6, padded to 2^20)
 */
#include "common.h"

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}
static int is_sorted_int(const int *a, long n) {
    for (long i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}
static long next_pow2(long n) {
    long m = 1;
    while (m < n) m <<= 1;
    return m;
}

/* One compare-exchange of the pair (i, i^j) in stage (k, j). */
static inline void compare_exchange(int *a, long i, long j, long k) {
    long l = i ^ j;
    if (l > i) {                                    /* the smaller index owns the pair */
        int up = (i & k) == 0;                      /* direction of this k-block */
        if (up ? a[i] > a[l] : a[i] < a[l]) { int t = a[i]; a[i] = a[l]; a[l] = t; }
    }
}

static void bitonic_serial(int *a, long m) {       /* m must be a power of two */
    for (long k = 2; k <= m; k <<= 1)
        for (long j = k >> 1; j > 0; j >>= 1)
            for (long i = 0; i < m; i++) compare_exchange(a, i, j, k);
}

static void bitonic_parallel(int *a, long m) {
    for (long k = 2; k <= m; k <<= 1)
        for (long j = k >> 1; j > 0; j >>= 1) {
            #pragma omp parallel for schedule(static)
            for (long i = 0; i < m; i++) compare_exchange(a, i, j, k);
            /* implicit barrier: stage (k, j) finished before the next one */
        }
}

/* out[0..n) = sorted in[0..n). buf has room for next_pow2(n) ints. */
static void bitonic_sort(const int *in, int *out, long n, int *buf, int parallel) {
    long m = next_pow2(n);
    memcpy(buf, in, (size_t)n * sizeof(int));
    for (long i = n; i < m; i++) buf[i] = INT_MAX;  /* padding sorts to the end */
    if (parallel) bitonic_parallel(buf, m);
    else          bitonic_serial(buf, m);
    memcpy(out, buf, (size_t)n * sizeof(int));
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000000L);
    if (n < 1) n = 1;
    long m = next_pow2(n);
    int *in  = xmalloc((size_t)n * sizeof(int)), *ref = xmalloc((size_t)n * sizeof(int));
    int *out = xmalloc((size_t)n * sizeof(int)), *buf = xmalloc((size_t)m * sizeof(int));
    fill_int(in, n, -1000000000, 1000000000, 47);
    omp_warmup();

    memcpy(ref, in, (size_t)n * sizeof(int));
    double t0 = omp_get_wtime();
    qsort(ref, (size_t)n, sizeof(int), cmp_int);
    double tq = omp_get_wtime() - t0;

    double ts, tp;
    BEST_TIME(ts, bitonic_sort(in, out, n, buf, 0));
    int ok_serial = is_sorted_int(out, n) && memcmp(out, ref, (size_t)n * sizeof(int)) == 0;
    int lg = 0;
    while ((1L << lg) < m) lg++;
    printf("P047: bitonic sort, N = %ld padded to M = %ld (%d stages), qsort %.6f s, serial bitonic %s\n",
           n, m, lg * (lg + 1) / 2, tq, ok_str(ok_serial));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(out, 0, (size_t)n * sizeof(int));
        BEST_TIME(tp, bitonic_sort(in, out, n, buf, 1));
        table_row(n, THREADS[k], ts, tp,
                  is_sorted_int(out, n) && memcmp(out, ref, (size_t)n * sizeof(int)) == 0);
    }
    free(in); free(ref); free(out); free(buf);
    return 0;
}

/* Notes:
 * - Work O(M log^2 M) compare-exchanges (more than merge sort's N log N), but
 *   the pattern is data-independent and every stage is fully parallel (M/2
 *   independent pairs), with log M (log M + 1)/2 barriers in total.
 * - Why no race: in a stage each index belongs to exactly one pair (i, i^j),
 *   and only the iteration with i < i^j touches the pair. Without the `l > i`
 *   test both ends would swap the same pair (a race and a double swap).
 * - (i & k) == 0 picks ascending for even k-blocks and descending for odd ones,
 *   which builds the bitonic sequences merged in the next k.
 * - Padding with INT_MAX (largest value) keeps the real data in out[0..N);
 *   padding costs up to 2x work when N is just above a power of two.
 * - The stage loop can also be one parallel region with `#pragma omp for` per
 *   stage (fewer fork/joins); the implicit barrier of omp for still separates stages.
 */

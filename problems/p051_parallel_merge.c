/* P051: Parallel merge of two sorted arrays (co-rank / merge path)
 * Problem: Merge two sorted int arrays A (size m) and B (size n) into one
 *   sorted array C of size m + n, in parallel with OpenMP. Verify against the
 *   serial merge, including unequal sizes, empty inputs and many duplicates,
 *   and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: split the OUTPUT into p equal ranges [k0, k1). For a split point
 *   k, the co-rank i = co_rank(k) is how many of the first k outputs come from
 *   A (the other j = k - i come from B). It is found by binary search on i:
 *   if A[i] <= B[j-1], A[i] belongs before B[j-1], so i is too small; else i
 *   is large enough (ties go to A, as in the serial merge -> stable). Thread t
 *   computes i0 = co_rank(k0), i1 = co_rank(k1) and serially merges
 *   A[i0..i1) with B[k0-i0 .. k1-i1) into C[k0..k1). The output ranges are
 *   disjoint and A, B are only read -> race-free, and every thread gets the
 *   same amount of output whatever the data (perfect load balance).
 * Source: lab extension (fixes the serial final merge of chunked sorts, practice q30).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p051_parallel_merge.c -o p051 -lm
 * Run:   ./p051 [m] [n]      (defaults m = n = 1e7)
 */
#include "common.h"

/* Stable serial merge: on ties A's element goes first. */
static void merge_serial(const int *A, long m, const int *B, long n, int *C) {
    long i = 0, j = 0, k = 0;
    while (i < m && j < n) C[k++] = (A[i] <= B[j]) ? A[i++] : B[j++];
    while (i < m) C[k++] = A[i++];
    while (j < n) C[k++] = B[j++];
}

/* Number of elements of A among the first k elements of merge(A, B). */
static long co_rank(long k, const int *A, long m, const int *B, long n) {
    long lo = k > n ? k - n : 0;            /* at least k - n must come from A */
    long hi = k < m ? k : m;                /* at most min(k, m) can come from A */
    while (lo < hi) {
        long i = lo + (hi - lo) / 2;        /* try i from A and j = k - i from B */
        long j = k - i;                     /* here i < m and j >= 1 */
        if (A[i] <= B[j - 1]) lo = i + 1;   /* A[i] goes before B[j-1]: take more of A */
        else                  hi = i;
    }
    return lo;
}

static void merge_parallel(const int *A, long m, const int *B, long n, int *C) {
    long total = m + n;
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long k0 = total * tid / nt, k1 = total * (tid + 1) / nt;     /* my output range */
        long i0 = co_rank(k0, A, m, B, n), j0 = k0 - i0;
        long i1 = co_rank(k1, A, m, B, n), j1 = k1 - i1;
        merge_serial(A + i0, i1 - i0, B + j0, j1 - j0, C + k0);
    }
}

/* Sorted test data: x[0] = start, then steps of 0..maxgap (maxgap 0 = all equal). */
static void make_sorted(int *x, long len, int start, int maxgap, unsigned long long seed) {
    unsigned long long s = seed;
    int v = start;
    for (long i = 0; i < len; i++) {
        x[i] = v;
        v += (int)(lcg_next(&s) % (unsigned long)(maxgap + 1));
    }
}

/* Small edge cases: unequal sizes, empty inputs, duplicates, disjoint ranges. */
static int edge_cases(void) {
    struct { long m, n; int startA, gapA, startB, gapB; } cs[] = {
        {100000, 37, 0, 4, 0, 4},           /* very unequal sizes */
        {37, 100000, 0, 4, 0, 4},
        {100000, 99999, 0, 1, 0, 1},        /* many duplicates across A and B */
        {50000, 70001, 5, 0, 5, 0},         /* all elements equal */
        {1000, 1000, 0, 1, 1000000, 1},     /* all of A before all of B */
        {1000, 1000, 1000000, 1, 0, 1},     /* all of B before all of A */
        {1000, 0, 0, 3, 0, 3}, {0, 1000, 0, 3, 0, 3}, {0, 0, 0, 1, 0, 1},
        {1, 1, 7, 1, 7, 1}, {2, 3, 1, 2, 0, 2}, {7, 1, 0, 2, 5, 2},
    };
    int ncase = (int)(sizeof(cs) / sizeof(cs[0])), ok = 1;
    for (int c = 0; c < ncase; c++) {
        long m = cs[c].m, n = cs[c].n;
        int *A = xmalloc((size_t)m * sizeof(int)), *B = xmalloc((size_t)n * sizeof(int));
        int *R = xmalloc((size_t)(m + n) * sizeof(int)), *C = xmalloc((size_t)(m + n) * sizeof(int));
        make_sorted(A, m, cs[c].startA, cs[c].gapA, 100 + (unsigned long long)c);
        make_sorted(B, n, cs[c].startB, cs[c].gapB, 200 + (unsigned long long)c);
        merge_serial(A, m, B, n, R);
        for (int t = 0; t < NTHREADS; t++) {
            omp_set_num_threads(THREADS[t]);
            memset(C, 0xff, (size_t)(m + n) * sizeof(int));
            merge_parallel(A, m, B, n, C);
            if (!same_ints(C, R, m + n)) {
                ok = 0;
                printf("  edge case m = %ld, n = %ld, %d threads: WRONG\n", m, n, THREADS[t]);
            }
        }
        free(A); free(B); free(R); free(C);
    }
    return ok;
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 10000000L);
    long n = arg_long(argc, argv, 2, m);
    if (m < 0) m = 0;
    if (n < 0) n = 0;
    int *A = xmalloc((size_t)m * sizeof(int)), *B = xmalloc((size_t)n * sizeof(int));
    int *R = xmalloc((size_t)(m + n) * sizeof(int)), *C = xmalloc((size_t)(m + n) * sizeof(int));
    make_sorted(A, m, 0, 4, 51);                      /* steps 0..4: duplicates inside and across */
    make_sorted(B, n, 1, 4, 5151);
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, merge_serial(A, m, B, n, R));
    printf("P051: parallel merge by co-rank (merge path), m = %ld, n = %ld\n", m, n);
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        memset(C, 0, (size_t)(m + n) * sizeof(int));
        BEST_TIME(tp, merge_parallel(A, m, B, n, C));
        table_row(m + n, THREADS[t], ts, tp, same_ints(C, R, m + n));
    }
    printf("edge cases (unequal sizes, empty, duplicates, all equal, disjoint; 1-8 threads): %s\n",
           ok_str(edge_cases()));
    free(A); free(B); free(R); free(C);
    return 0;
}

/* Notes:
 * - Splitting the INPUTS evenly (A into p parts) does not work: the matching
 *   parts of B can be any size. Splitting the OUTPUT evenly and searching the
 *   co-ranks gives each thread exactly (m+n)/p elements to write.
 * - Each co-rank costs O(log min(m, n)); total O((m+n)/p + log n) per thread.
 * - Ties: the test A[i] <= B[j-1] sends equal keys to A first, matching the
 *   serial merge's `A[i] <= B[j]`, so the parallel merge is stable too.
 * - Race-free: A and B are read-only and C[k0..k1) belongs to one thread;
 *   no barrier is needed inside the region.
 * - Memory-bound (read m+n, write m+n), so speedup flattens with bandwidth.
 *   Used for the last merges of a parallel merge sort (P052) where the serial
 *   O(N) merge otherwise limits the speedup.
 */

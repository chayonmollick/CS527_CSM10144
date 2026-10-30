/* P050: k-th smallest element without sorting (quickselect, radix select, bisection)
 * Problem: Find the k-th smallest element (k = 1..N) of an unsorted int array
 *   without sorting it: (a) serial quickselect; (b) a parallel selection with
 *   OpenMP. Verify against a sorted copy for k = 1, N/2 and N, and report the
 *   time and speedup over quickselect for 1, 2, 4 and 8 threads.
 * Solution: (b) radix select: map each int to an order-preserving unsigned key
 *   (flip the sign bit) and fix the answer 8 bits at a time from the top. Each
 *   pass is a parallel histogram of the next 8-bit digit over the keys that
 *   still match the bits fixed so far (reduction(+:h[:256]), private copies ->
 *   race-free); walking the 256 counts tells which digit holds the k-th key
 *   and the new rank inside it. 4 passes, O(N) work, the input is only read.
 *   (c) bisection on the VALUE: the answer is the smallest v with
 *   count(a[i] <= v) >= k; each count is a parallel for with reduction(+),
 *   and binary search over [min, max] needs at most 32 counts.
 * Source: lab extension (order statistics; parallel histogram / counting).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p050_kth_smallest.c -o p050 -lm
 * Run:   ./p050 [N]          (default N = 5e6)
 */
#include "common.h"
#include <stdint.h>

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

/* ---------- (a) serial quickselect (reorders a) ---------- */
static int quickselect(int *a, long n, long k) {       /* k is 1-based */
    long lo = 0, hi = n - 1, target = k - 1;
    while (lo < hi) {
        int pivot = a[lo + (hi - lo) / 2];
        long i = lo, j = hi;
        while (i <= j) {                                /* Hoare partition */
            while (a[i] < pivot) i++;
            while (a[j] > pivot) j--;
            if (i <= j) { int t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
        }
        /* now a[lo..j] <= pivot, a[i..hi] >= pivot, a[j+1..i-1] == pivot */
        if (target <= j)      hi = j;
        else if (target >= i) lo = i;
        else                  return a[target];
    }
    return a[target];
}
static int quickselect_copy(const int *a, int *w, long n, long k) {
    memcpy(w, a, (size_t)n * sizeof(int));             /* quickselect permutes its input */
    return quickselect(w, n, k);
}

/* ---------- (b) parallel radix select ---------- */
static inline uint32_t to_key(int x) { return (uint32_t)x ^ 0x80000000u; }   /* keeps the order */
static inline int from_key(uint32_t u) { return (int)((long long)u - 2147483648LL); }

static int radix_select(const int *a, long n, long k) {
    uint32_t prefix = 0, mask = 0;                      /* candidates: (key & mask) == prefix */
    for (int shift = 24; shift >= 0; shift -= 8) {
        long h[256] = {0};
        #pragma omp parallel for reduction(+:h[:256])
        for (long i = 0; i < n; i++) {
            uint32_t u = to_key(a[i]);
            if ((u & mask) == prefix) h[(u >> shift) & 255]++;
        }
        int d = 0;
        while (k > h[d]) { k -= h[d]; d++; }            /* k-th candidate has digit d */
        prefix |= (uint32_t)d << shift;                 /* k is now its rank inside digit d */
        mask   |= 255u << shift;
    }
    return from_key(prefix);
}

/* ---------- (c) parallel bisection on the value ---------- */
static int bisect_select(const int *a, long n, long k) {
    int mn = INT_MAX, mx = INT_MIN;
    #pragma omp parallel for reduction(min:mn) reduction(max:mx)
    for (long i = 0; i < n; i++) { if (a[i] < mn) mn = a[i]; if (a[i] > mx) mx = a[i]; }
    long lo = mn, hi = mx;                              /* answer is in [lo, hi] */
    while (lo < hi) {
        long mid = lo + (hi - lo) / 2;
        long cnt = 0;
        #pragma omp parallel for reduction(+:cnt)
        for (long i = 0; i < n; i++) cnt += (a[i] <= mid);
        if (cnt >= k) hi = mid;                         /* mid is large enough */
        else          lo = mid + 1;
    }
    return (int)lo;
}

/* Check a method on k = 1, n/2, n against the sorted copy. */
static int check_all(int which, const int *a, int *w, const int *sorted, long n) {
    long ks[3] = {1, n / 2 > 0 ? n / 2 : 1, n};
    int ok = 1;
    for (int q = 0; q < 3; q++) {
        int v = which == 0 ? quickselect_copy(a, w, n, ks[q])
              : which == 1 ? radix_select(a, n, ks[q]) : bisect_select(a, n, ks[q]);
        ok = ok && v == sorted[ks[q] - 1];
    }
    return ok;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 5000000L);
    if (n < 1) n = 1;
    long kmid = n / 2 > 0 ? n / 2 : 1;
    int *a = xmalloc((size_t)n * sizeof(int)), *w = xmalloc((size_t)n * sizeof(int));
    int *sorted = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, -1000000000, 1000000000, 50);
    omp_warmup();
    memcpy(sorted, a, (size_t)n * sizeof(int));
    qsort(sorted, (size_t)n, sizeof(int), cmp_int);
    printf("P050: k-th smallest, N = %ld: k=1 -> %d, k=N/2 -> %d, k=N -> %d (from a sorted copy)\n",
           n, sorted[0], sorted[kmid - 1], sorted[n - 1]);

    double ts, tp;
    int r = 0;
    BEST_TIME(ts, r = quickselect_copy(a, w, n, kmid));
    printf("\n(a) serial quickselect (incl. copying the array): %.6f s, k = 1, N/2, N %s\n",
           ts, ok_str(check_all(0, a, w, sorted, n) && r == sorted[kmid - 1]));

    const char *names[2] = {"(b) parallel radix select, 4 passes of 8-bit histograms",
                            "(c) parallel bisection on the value, reduction(+) count per step"};
    for (int m = 1; m <= 2; m++) {
        printf("\n%s (serial = quickselect, timed for k = N/2, check = all three k)\n", names[m - 1]);
        table_header();
        for (int t = 0; t < NTHREADS; t++) {
            omp_set_num_threads(THREADS[t]);
            int ok = check_all(m, a, w, sorted, n);
            BEST_TIME(tp, r = (m == 1 ? radix_select(a, n, kmid) : bisect_select(a, n, kmid)));
            table_row(n, THREADS[t], ts, tp, ok && r == sorted[kmid - 1]);
        }
    }

    /* Many duplicates (values 0..99): k lands inside long runs of equal values. */
    long nd = n < 100000 ? n : 100000;
    fill_int(a, nd, 0, 99, 5050);
    memcpy(sorted, a, (size_t)nd * sizeof(int));
    qsort(sorted, (size_t)nd, sizeof(int), cmp_int);
    int okd = 1;
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        okd = okd && check_all(0, a, w, sorted, nd) && check_all(1, a, w, sorted, nd)
                  && check_all(2, a, w, sorted, nd);
    }
    printf("\nduplicates check (N = %ld, values 0..99, all methods, 1-8 threads): %s\n", nd, ok_str(okd));
    free(a); free(w); free(sorted);
    return 0;
}

/* Notes:
 * - Quickselect is O(N) expected (one side of each partition), but the
 *   partition is sequential and data-dependent: hard to parallelise directly.
 * - Radix select only READS the input (no copy, no swaps), and each pass is a
 *   parallel histogram: 4 passes over N, so with p threads ~4N/p time.
 *   Histogram race: h[d]++ from many threads -> reduction(+:h[:256]).
 * - Order-preserving key: x ^ 0x80000000 makes negative ints sort below
 *   positives as unsigned numbers (two's complement sign bit flip).
 * - Bisection: count(a[i] <= v) is monotone in v, so binary search finds the
 *   smallest v with count >= k = the k-th smallest. log2(max - min) <= 32 full
 *   passes: simplest to write, slower than radix select.
 * - Duplicates: both parallel methods count ties correctly (k is a rank, not a
 *   distinct value), which is why the duplicates case is tested separately.
 */

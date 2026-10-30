/* CS527 Computer Systems — OpenMP practice
 * Q12: Parallelise simple array rearrangements: (a) reverse an array IN PLACE,
 *      (b) rotate it left by k positions into a new array, and (c) swap every
 *      adjacent pair (a[0]<->a[1], a[2]<->a[3], ...).
 *  - reverse          : parallel for over the first half, swap a[i] <-> a[n-1-i]
 *  - rotate (new)     : parallel for, b[i] = a[(i+k) % n]
 *  - rotate (in place): three parallel reversals  rev(0,k) rev(k,n) rev(0,n)
 *  - pair swap        : parallel for over pairs p, swap a[2p] <-> a[2p+1]
 * Usage: ./q12 [N]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- serial reference versions ---------- */
static void reverse_range_serial(int *a, long lo, long hi) {   /* reverses a[lo..hi-1] */
    for (long i = lo, j = hi - 1; i < j; i++, j--) { int t = a[i]; a[i] = a[j]; a[j] = t; }
}
/* written as two plain copies (no %), so it is an independent check */
static void rotate_left_serial(const int *a, int *b, long n, long k) {
    k %= n;
    for (long i = 0; i < n - k; i++) b[i] = a[i + k];
    for (long i = n - k; i < n; i++) b[i] = a[i - (n - k)];
}
static void rotate_left_inplace_serial(int *a, long n, long k) {
    k %= n;
    reverse_range_serial(a, 0, k);
    reverse_range_serial(a, k, n);
    reverse_range_serial(a, 0, n);
}
static void swap_pairs_serial(int *a, long n) {
    for (long p = 0; p < n / 2; p++) { int t = a[2*p]; a[2*p] = a[2*p + 1]; a[2*p + 1] = t; }
}

/* ---------- parallel versions ---------- */
/* Iteration i touches only a[i] and a[n-1-i]. For i < n/2 we have i < n-1-i, so
 * the pairs {i, n-1-i} never overlap: every element is touched by exactly one
 * iteration -> no race. (Looping to n instead of n/2 would swap twice AND race.)
 * For odd n the middle element a[n/2] is left alone, as it should be. */
static void reverse_parallel(int *a, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n / 2; i++) {
        int t = a[i]; a[i] = a[n - 1 - i]; a[n - 1 - i] = t;
    }
}
static void reverse_range_parallel(int *a, long lo, long hi) {  /* reverses a[lo..hi-1] */
    long len = hi - lo;
    #pragma omp parallel for
    for (long i = 0; i < len / 2; i++) {
        int t = a[lo + i]; a[lo + i] = a[hi - 1 - i]; a[hi - 1 - i] = t;
    }
}
/* out-of-place: each b[i] is written once, a is only read -> no race */
static void rotate_left_parallel(const int *a, int *b, long n, long k) {
    k %= n;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) b[i] = a[(i + k) % n];
}
/* in place: rotate-left-by-k == reverse first k, reverse the rest, reverse all.
 * Each call is its own parallel for, and the implicit barrier at its end makes
 * the three steps run strictly one after another (they must, they overlap). */
static void rotate_left_inplace_parallel(int *a, long n, long k) {
    k %= n;
    reverse_range_parallel(a, 0, k);
    reverse_range_parallel(a, k, n);
    reverse_range_parallel(a, 0, n);
}
/* pair p owns elements 2p and 2p+1; pairs are disjoint -> no race */
static void swap_pairs_parallel(int *a, long n) {
    #pragma omp parallel for
    for (long p = 0; p < n / 2; p++) {
        int t = a[2*p]; a[2*p] = a[2*p + 1]; a[2*p + 1] = t;
    }
}

static int same(const int *x, const int *y, long n) {
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}
static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static int run(long n) {
    long k = n / 3 + 7;                      /* rotation amount (reduced mod n inside) */
    size_t bytes = (size_t)n * sizeof(int);
    int *orig = malloc(bytes), *work = malloc(bytes);
    int *revS = malloc(bytes), *rotS = malloc(bytes), *swpS = malloc(bytes);   /* serial results */
    if (!orig || !work || !revS || !rotS || !swpS) {
        fprintf(stderr, "alloc failed (N=%ld)\n", n);
        free(orig); free(work); free(revS); free(rotS); free(swpS); return 1;
    }
    for (long i = 0; i < n; i++) orig[i] = (int)i;
    /* first touch of every buffer, so page faults are not timed later */
    for (long i = 0; i < n; i++) work[i] = revS[i] = rotS[i] = swpS[i] = 0;

    /* serial references (timed; memcpy resets are not timed) */
    memcpy(revS, orig, bytes);
    double t0 = omp_get_wtime(); reverse_range_serial(revS, 0, n);   double ts_rev = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); rotate_left_serial(orig, rotS, n, k);      double ts_rot = omp_get_wtime() - t0;
    memcpy(work, orig, bytes);
    t0 = omp_get_wtime(); rotate_left_inplace_serial(work, n, k);    double ts_rin = omp_get_wtime() - t0;
    int ok_inplace_serial = same(work, rotS, n);   /* both serial rotations must agree */
    memcpy(swpS, orig, bytes);
    t0 = omp_get_wtime(); swap_pairs_serial(swpS, n);                double ts_swp = omp_get_wtime() - t0;

    printf("Q12: reverse / rotate-left by k=%ld / swap adjacent pairs (N = %ld)\n", k % n, n);
    printf("serial(s): reverse %.6f  rotate(new) %.6f  rotate(in-place) %.6f  pairswap %.6f\n",
           ts_rev, ts_rot, ts_rin, ts_swp);
    printf("serial rotate(new) vs serial rotate(in-place): %s\n", ok_inplace_serial ? "OK" : "WRONG");
    printf("%8s %10s %6s %10s %6s %11s %6s %10s %6s %6s\n", "threads", "reverse(s)", "sp",
           "rotNew(s)", "sp", "rotInPl(s)", "sp", "pairs(s)", "sp", "check");
    const int threads[] = {1, 2, 4, 8};
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        memcpy(work, orig, bytes);
        t0 = omp_get_wtime(); reverse_parallel(work, n);                 double tr = omp_get_wtime() - t0;
        int ok = same(work, revS, n);
        t0 = omp_get_wtime(); rotate_left_parallel(orig, work, n, k);    double tn = omp_get_wtime() - t0;
        ok = ok && same(work, rotS, n);
        memcpy(work, orig, bytes);
        t0 = omp_get_wtime(); rotate_left_inplace_parallel(work, n, k);  double tin = omp_get_wtime() - t0;
        ok = ok && same(work, rotS, n);
        memcpy(work, orig, bytes);
        t0 = omp_get_wtime(); swap_pairs_parallel(work, n);              double tw = omp_get_wtime() - t0;
        ok = ok && same(work, swpS, n);
        printf("%8d %10.6f %6.2f %10.6f %6.2f %11.6f %6.2f %10.6f %6.2f %6s\n", threads[ti],
               tr, speedup(ts_rev, tr), tn, speedup(ts_rot, tn), tin, speedup(ts_rin, tin),
               tw, speedup(ts_swp, tw), ok ? "OK" : "WRONG");
    }
    printf("\n");
    free(orig); free(work); free(revS); free(rotS); free(swpS);
    return 0;
}

int main(int argc, char **argv) {
    long sizes[] = {1000001L, 20000000L};    /* odd and even N; 5 int arrays x 2e7 = 400 MB */
    int nsizes = 2;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (sizes[0] < 1) { fprintf(stderr, "usage: %s [N>=1]\n", argv[0]); return 1; }
    #pragma omp parallel                     /* warm up the thread pool (not timed); */
    { (void)omp_get_thread_num(); }          /* GCC deletes a region with an empty body */
    for (int s = 0; s < nsizes; s++)
        if (run(sizes[s])) return 1;
    return 0;
}

/* Viva notes:
 * - Reverse: run the loop over the FIRST HALF only. Iteration i owns the pair
 *   (i, n-1-i); different i give disjoint pairs, so no two threads ever touch the
 *   same element and no lock/atomic is needed. Over the full range, i and n-1-i
 *   would both swap the same pair (undoing it) and two threads could race on it.
 * - Rotate into a new array is the easy case: read-only input, one write per
 *   output element. In place you cannot simply do a[i] = a[i+k] in parallel (a
 *   thread may read a[i+k] after another thread has overwritten it), hence the
 *   three-reversal trick; the three parallel for loops must run in order, which
 *   the implicit barrier at the end of each one guarantees.
 * - Pair swap: pair p owns 2p and 2p+1; for odd n the last element stays put.
 * - All three are pure data movement (no arithmetic), so they are memory-bandwidth
 *   bound: speedup flattens after a few threads. The `%` in rotate(new) costs an
 *   integer division per element; two copy loops (as in the serial version) are
 *   faster and just as parallel.
 * - Alternatives: schedule(static) is ideal here (equal work per iteration);
 *   `#pragma omp parallel for simd` could also vectorise the copy loops.
 */

/* P035: Mode and number of distinct values via a parallel histogram
 * Problem: Given an array a[N] of integers in a small range [0, R), find the mode
 *   (the most frequent value; ties go to the smallest value) and the number of
 *   distinct values, using OpenMP. Build a histogram in parallel, then find the
 *   mode and count the distinct values from it in parallel. Verify against serial
 *   code and time it.
 * Solution: histogram, two ways: (A) reduction(+:h[:R]), an OpenMP 4.5 array-section
 *   reduction: every thread gets a private zeroed copy of h[0..R-1], adds into it,
 *   and the copies are summed at the end; (B) the same by hand: a private histogram
 *   per thread ("omp for nowait") added into the shared one inside a critical
 *   section. Then over the R bins: reduction(max:maxc) and reduction(+:distinct) in
 *   one loop, and reduction(min:mode) over the bins with h[v] == maxc (smallest
 *   such value = tie rule). An argmax needs two passes because a built-in
 *   reduction cannot carry the (count, value) pair.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p035_mode_distinct.c -o p035 -lm
 * Run:   ./p035 [N] [R]   (defaults N = 2e7, R = 1000)
 */
#include "common.h"

/* reduction(+:h[:R]) gives each thread a private copy of R longs, which gcc places on
 * the thread's stack: for a large R that overflows the stack (crash), so A is skipped. */
#define MAX_R_STACK 100000

typedef struct { int mode; long mode_count; long distinct; } stats_t;

static void hist_serial(const int *a, long n, long *h, int r) {
    for (int v = 0; v < r; v++) h[v] = 0;
    for (long i = 0; i < n; i++) h[a[i]]++;
}

/* (A) array-section reduction: private copies of h[0..r-1] are summed at the end. */
static void hist_reduction(const int *a, long n, long *h, int r) {
    for (int v = 0; v < r; v++) h[v] = 0;
    #pragma omp parallel for reduction(+:h[:r])
    for (long i = 0; i < n; i++) h[a[i]]++;
}

/* (B) private histograms on the heap, merged in a critical section. */
static void hist_private(const int *a, long n, long *h, int r) {
    for (int v = 0; v < r; v++) h[v] = 0;
    #pragma omp parallel
    {
        long *mine = xmalloc((size_t)r * sizeof(long));
        for (int v = 0; v < r; v++) mine[v] = 0;
        #pragma omp for nowait
        for (long i = 0; i < n; i++) mine[a[i]]++;
        #pragma omp critical
        for (int v = 0; v < r; v++) h[v] += mine[v];
        free(mine);
    }
}

static stats_t stats_serial(const long *h, int r) {
    stats_t s = {-1, 0, 0};
    for (int v = 0; v < r; v++) {
        if (h[v] > s.mode_count) { s.mode_count = h[v]; s.mode = v; }   /* '>' keeps the smallest v on ties */
        if (h[v] > 0) s.distinct++;
    }
    return s;
}

static stats_t stats_parallel(const long *h, int r) {
    long maxc = 0, distinct = 0;
    #pragma omp parallel for reduction(max:maxc) reduction(+:distinct)
    for (int v = 0; v < r; v++) {
        if (h[v] > maxc) maxc = h[v];
        if (h[v] > 0) distinct++;
    }
    int mode = r;                                    /* r = "none found" */
    #pragma omp parallel for reduction(min:mode)
    for (int v = 0; v < r; v++)
        if (h[v] == maxc && v < mode) mode = v;      /* smallest value with the top count */
    stats_t s = {maxc > 0 ? mode : -1, maxc, distinct};   /* empty array: mode -1 */
    return s;
}

static int same_stats(stats_t x, stats_t y) {
    return x.mode == y.mode && x.mode_count == y.mode_count && x.distinct == y.distinct;
}

/* Run serial and both parallel histograms on a small hand-made array. */
static int hand_test(const int *a, long n, int r, int exp_mode, long exp_distinct) {
    long h0[16], h1[16], h2[16];
    hist_serial(a, n, h0, r);
    hist_reduction(a, n, h1, r);
    hist_private(a, n, h2, r);
    stats_t s0 = stats_serial(h0, r), s1 = stats_parallel(h1, r), s2 = stats_parallel(h2, r);
    return s0.mode == exp_mode && s0.distinct == exp_distinct && same_stats(s0, s1) && same_stats(s0, s2);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    int r  = (int)arg_long(argc, argv, 2, 1000);
    if (n < 0) n = 0;
    if (r < 1) r = 1;
    int *a = xmalloc((size_t)n * sizeof(int));
    long *h0 = xmalloc((size_t)r * sizeof(long)), *h = xmalloc((size_t)r * sizeof(long));
    fill_int(a, n, 0, r - 1, 17);
    for (long i = 0; i < n; i += 50) a[i] = r / 2;   /* make r/2 clearly the most frequent */
    omp_warmup();

    int t1[7] = {3, 1, 3, 2, 1, 3, 0};               /* mode 3, 4 distinct values */
    int t2[4] = {2, 1, 2, 1};                        /* tie 1 vs 2 -> mode 1, 2 distinct */
    printf("P035: hand tests: {3,1,3,2,1,3,0} %s, tie {2,1,2,1} %s, empty %s\n",
           ok_str(hand_test(t1, 7, 5, 3, 4)), ok_str(hand_test(t2, 4, 5, 1, 2)), ok_str(hand_test(t1, 0, 5, -1, 0)));

    stats_t s0 = {0, 0, 0}, sA = {0, 0, 0}, sB = {0, 0, 0};
    double ts, tA, tB;
    BEST_TIME(ts, (hist_serial(a, n, h0, r), s0 = stats_serial(h0, r)));
    printf("P035: N = %ld, R = %d: mode = %d (%ld times), %ld distinct values\n", n, r, s0.mode,
           s0.mode_count, s0.distinct);
    printf("A = reduction(+:h[:R]), B = private histograms + critical (both + parallel mode/distinct)\n");
    int use_a = r <= MAX_R_STACK;
    if (!use_a) printf("  R > %d: A skipped (its private copies live on the thread stacks); B uses the heap\n", MAX_R_STACK);
    printf("%8s %12s %12s %8s %12s %8s %6s\n", "threads", "serial(s)", "A(s)", "speedA", "B(s)", "speedB", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int ok = 1;
        tA = 0.0;
        if (use_a) {
            BEST_TIME(tA, (hist_reduction(a, n, h, r), sA = stats_parallel(h, r)));
            ok = same_longs(h0, h, r) && same_stats(s0, sA);
        }
        BEST_TIME(tB, (hist_private(a, n, h, r), sB = stats_parallel(h, r)));
        ok = ok && same_longs(h0, h, r) && same_stats(s0, sB);
        printf("%8d %12.6f %12.6f %8.2f %12.6f %8.2f %6s\n", THREADS[k], ts, tA,
               tA > 0 ? ts / tA : 0.0, tB, tB > 0 ? ts / tB : 0.0, ok_str(ok));
    }
    free(a); free(h0); free(h);
    return 0;
}

/* Notes:
 * - h[a[i]]++ in a plain parallel for is a race: two threads can hit the same bin.
 *   Fixes: private histograms (A or B), or "#pragma omp atomic" per element (correct
 *   but slow when many threads hit the same few bins).
 * - reduction(+:h[:R]) needs OpenMP 4.5 (gcc >= 6). Each thread's copy costs R
 *   elements and gcc puts it on the thread's stack: R = 1e6 longs (8 MB) crashes, so
 *   for a large R use B (heap), atomics, or raise OMP_STACKSIZE (worker threads).
 * - Argmax with ties: pass 1 finds the max count (reduction(max)), pass 2 the smallest
 *   value having it (reduction(min)). Integer results: must equal serial exactly.
 * - For R = 1000 the bin loops are tiny; parallelising them only pays for a large R.
 *   The histogram over N elements is where the time goes.
 */

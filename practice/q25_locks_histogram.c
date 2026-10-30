/* CS527 Computer Systems — OpenMP practice
 * Q25: Build a histogram with many bins (1024) in parallel. Compare the ways of
 *      protecting the shared bins, verify each against serial, and explain the timings.
 *  - critical per update     : ONE lock for all bins (coarse grain)
 *  - atomic per update       : hardware atomic add on just that bin
 *  - omp_lock_t per bin      : 1024 locks (fine grain): omp_init/set/unset/destroy_lock
 *  - private histogram per thread, merged into the shared one once at the end
 *  - reduction(+:h[:BINS])   : the same idea done by OpenMP (array-section reduction, 4.5+)
 * Usage: ./q25_locks_histogram [N]     (default N = 1e7)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BINS 1024
#define NMETHODS 5

/* deterministic hash of i -> bin 0..BINS-1 (Knuth multiplicative hash) */
static int bin_of(long i) {
    unsigned int h = (unsigned int)i * 2654435761u;
    return (int)((h >> 8) % BINS);
}

static void clear(long *h) { memset(h, 0, BINS * sizeof(long)); }

static void hist_serial(const int *a, long n, long *h) {
    clear(h);
    for (long i = 0; i < n; i++) h[a[i]]++;
}

/* one critical = one global lock: every update in every thread is serialised */
static void hist_critical(const int *a, long n, long *h, omp_lock_t *lk) {
    (void)lk;
    clear(h);
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp critical
        h[a[i]]++;
    }
}

/* atomic: only updates of the SAME bin conflict, and each is one instruction */
static void hist_atomic(const int *a, long n, long *h, omp_lock_t *lk) {
    (void)lk;
    clear(h);
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp atomic
        h[a[i]]++;
    }
}

/* one lock per bin: threads updating different bins do not wait for each other */
static void hist_locks(const int *a, long n, long *h, omp_lock_t *lk) {
    clear(h);
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        int b = a[i];
        omp_set_lock(&lk[b]);
        h[b]++;
        omp_unset_lock(&lk[b]);
    }
}

/* private histogram per thread: no sharing in the loop, BINS adds per thread to merge */
static void hist_private(const int *a, long n, long *h, omp_lock_t *lk) {
    (void)lk;
    clear(h);
    #pragma omp parallel
    {
        long local[BINS] = {0};
        #pragma omp for nowait
        for (long i = 0; i < n; i++) local[a[i]]++;
        #pragma omp critical
        for (int b = 0; b < BINS; b++) h[b] += local[b];
    }
}

/* array-section reduction: OpenMP makes the private copies and merges them */
static void hist_reduction(const int *a, long n, long *h, omp_lock_t *lk) {
    (void)lk;
    clear(h);
    #pragma omp parallel for reduction(+:h[:BINS])
    for (long i = 0; i < n; i++) h[a[i]]++;
}

static int equal_hist(const long *x, const long *y) {
    for (int b = 0; b < BINS; b++) if (x[b] != y[b]) return 0;
    return 1;
}

typedef void (*hist_fn)(const int *, long, long *, omp_lock_t *);

int main(int argc, char **argv) {
    long n = (argc > 1) ? atol(argv[1]) : 10000000L;   /* 1e7 */
    if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
    int *a = malloc((size_t)n * sizeof(int));
    omp_lock_t *lk = malloc(BINS * sizeof(omp_lock_t));
    if (!a || !lk) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < n; i++) a[i] = bin_of(i);
    for (int b = 0; b < BINS; b++) omp_init_lock(&lk[b]);

    const char *name[NMETHODS] = {"critical", "atomic", "lock per bin", "private+merge", "reduction"};
    hist_fn fn[NMETHODS] = {hist_critical, hist_atomic, hist_locks, hist_private, hist_reduction};
    long ref[BINS], h[BINS];

    printf("Q25: histogram of N = %ld values into %d bins\n", n, BINS);
    hist_serial(a, n, ref);                            /* warm-up run, not timed */
    double t0 = omp_get_wtime();
    hist_serial(a, n, ref);
    double ts = omp_get_wtime() - t0;
    long tot = 0, mn = ref[0], mx = ref[0];
    for (int b = 0; b < BINS; b++) {
        tot += ref[b];
        if (ref[b] < mn) mn = ref[b];
        if (ref[b] > mx) mx = ref[b];
    }
    printf("serial: %.4f s, total = %ld, bin counts between %ld and %ld\n", ts, tot, mn, mx);

    omp_set_num_threads(4);
    printf("\nCorrectness, 4 threads:\n");
    for (int m = 0; m < NMETHODS; m++) {
        fn[m](a, n, h, lk);
        printf("    %-14s %s\n", name[m], equal_hist(ref, h) ? "OK" : "WRONG");
    }

    const int threads[] = {1, 2, 4, 8};
    int all_ok = 1;
    printf("\nTiming: time in s (speedup = T_serial / T_parallel), T_serial = %.4f s\n", ts);
    printf("%-14s", "method");
    for (int ti = 0; ti < 4; ti++) printf("   %4d thread%s     ", threads[ti], threads[ti] > 1 ? "s" : " ");
    printf("\n");
    for (int m = 0; m < NMETHODS; m++) {
        printf("%-14s", name[m]);
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double p0 = omp_get_wtime();
            fn[m](a, n, h, lk);
            double tp = omp_get_wtime() - p0;
            all_ok = all_ok && equal_hist(ref, h);
            printf("  %8.4f (%6.2fx)", tp, tp > 0 ? ts / tp : 0.0);
        }
        printf("\n");
    }
    printf("all timed runs equal serial: %s\n", all_ok ? "OK" : "WRONG");

    for (int b = 0; b < BINS; b++) omp_destroy_lock(&lk[b]);
    free(lk); free(a);
    return 0;
}

/* Viva notes:
 * - Lock granularity: critical = one lock for all 1024 bins, so every update waits for
 *   every other (fully serial + lock overhead). One lock per bin = fine grain: threads
 *   only wait when they hit the same bin (rare with 1024 bins), but each update still
 *   costs a lock acquire + release.
 * - atomic beats per-bin locks: same fine granularity but one hardware instruction
 *   instead of lock + update + unlock. Yet both get SLOWER with more threads: the 1024
 *   bins fill only 128 cache lines (8 longs per 64-byte line) that every thread keeps
 *   writing, so the lines bounce between cores (true + false sharing).
 * - Contention = threads waiting for the same lock/line. Fewer bins (e.g. 10) means far
 *   more contention, so atomic/locks get much worse while private histograms don't care.
 * - Private histograms (or reduction(+:h[:BINS])) win: the loop touches only
 *   thread-local memory with no synchronisation; the merge costs BINS x threads adds,
 *   negligible when N >> BINS x threads. Each thread needs BINS extra memory.
 * - omp_lock_t must be initialised (omp_init_lock) before use and destroyed afterwards;
 *   omp_test_lock() tries without blocking; omp_nest_lock_t can be re-acquired by its owner.
 */

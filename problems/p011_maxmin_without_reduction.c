/* P011: Maximum AND minimum of an array without the reduction clause
 * Problem: Find max and min of int a[N] in parallel WITHOUT reduction:
 *   (a) critical around the comparison for every element,
 *   (b) per-thread local max/min, combined with one critical per thread,
 *   (c) per-thread results stored in an array indexed by thread id, combined serially,
 *   (d) "check before critical": if (a[i] > max) { critical { if (a[i] > max) max = a[i]; } }
 *   Explain why (d) needs the re-check inside the critical. Compare all with
 *   reduction(max:) / reduction(min:). Verify and time every version.
 * Solution: (a) serialises every element. (b) and (c) do the scan privately and
 *   synchronise once per thread. (d) enters the lock only when an element beats the
 *   current max, which for random data is rare; the outside read uses atomic read
 *   (and the write atomic write) so even the unlocked test is race-free. max and min
 *   use different NAMED criticals so they do not block each other.
 * Source: textbook Ex 20.2.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p011_maxmin_without_reduction.c -o p011 -lm
 * Run:   ./p011 [N]          (default N = 2e7; (a) is timed on the first min(N, 2e6) only)
 */
#include "common.h"

static void mm_serial(const int *a, long n, int *mx, int *mn) {
    int hi = INT_MIN, lo = INT_MAX;
    for (long i = 0; i < n; i++) {
        if (a[i] > hi) hi = a[i];
        if (a[i] < lo) lo = a[i];
    }
    *mx = hi; *mn = lo;
}

/* (a) one critical per element: correct, fully serialised */
static void mm_critical_each(const int *a, long n, int *mx, int *mn) {
    int hi = INT_MIN, lo = INT_MAX;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp critical
        {
            if (a[i] > hi) hi = a[i];
            if (a[i] < lo) lo = a[i];
        }
    }
    *mx = hi; *mn = lo;
}

/* (b) private max/min per thread, one critical per thread to combine */
static void mm_local_critical(const int *a, long n, int *mx, int *mn) {
    int hi = INT_MIN, lo = INT_MAX;
    #pragma omp parallel
    {
        int my_hi = INT_MIN, my_lo = INT_MAX;          /* private */
        #pragma omp for
        for (long i = 0; i < n; i++) {
            if (a[i] > my_hi) my_hi = a[i];
            if (a[i] < my_lo) my_lo = a[i];
        }
        #pragma omp critical
        {
            if (my_hi > hi) hi = my_hi;
            if (my_lo < lo) lo = my_lo;
        }
    }
    *mx = hi; *mn = lo;
}

/* (c) results array indexed by thread id, combined serially after the region */
static void mm_array(const int *a, long n, int *mx, int *mn) {
    int maxt = omp_get_max_threads();                  /* the team cannot be larger */
    int *t_hi = xmalloc((size_t)maxt * sizeof(int));
    int *t_lo = xmalloc((size_t)maxt * sizeof(int));
    int nt = 1;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int my_hi = INT_MIN, my_lo = INT_MAX;
        #pragma omp single
        nt = omp_get_num_threads();
        #pragma omp for
        for (long i = 0; i < n; i++) {
            if (a[i] > my_hi) my_hi = a[i];
            if (a[i] < my_lo) my_lo = a[i];
        }
        t_hi[id] = my_hi;                              /* each thread: its own slot, once */
        t_lo[id] = my_lo;
    }
    int hi = INT_MIN, lo = INT_MAX;                    /* serial combine */
    for (int t = 0; t < nt; t++) {
        if (t_hi[t] > hi) hi = t_hi[t];
        if (t_lo[t] < lo) lo = t_lo[t];
    }
    free(t_hi); free(t_lo);
    *mx = hi; *mn = lo;
}

/* (d) cheap unlocked test first, lock + RE-CHECK only when the element might win */
static void mm_check_then_critical(const int *a, long n, int *mx, int *mn) {
    int hi = INT_MIN, lo = INT_MAX;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        int v = a[i], cur;
        #pragma omp atomic read
        cur = hi;
        if (v > cur) {                                 /* might be a new max ... */
            #pragma omp critical(maxlock)
            {
                if (v > hi) {                          /* ... re-check: hi may have grown */
                    #pragma omp atomic write
                    hi = v;
                }
            }
        }
        #pragma omp atomic read
        cur = lo;
        if (v < cur) {
            #pragma omp critical(minlock)
            {
                if (v < lo) {
                    #pragma omp atomic write
                    lo = v;
                }
            }
        }
    }
    *mx = hi; *mn = lo;
}

/* (e) for comparison: the reduction clause */
static void mm_reduction(const int *a, long n, int *mx, int *mn) {
    int hi = INT_MIN, lo = INT_MAX;
    #pragma omp parallel for reduction(max:hi) reduction(min:lo)
    for (long i = 0; i < n; i++) {
        if (a[i] > hi) hi = a[i];
        if (a[i] < lo) lo = a[i];
    }
    *mx = hi; *mn = lo;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    if (n < 1) n = 1;
    long na = n < 2000000L ? n : 2000000L;     /* (a) is ~100x slower: time it on a prefix */
    int *a = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, -1000000000, 1000000000, 11);
    omp_set_dynamic(0);
    omp_warmup();

    typedef void (*mm_fn)(const int *, long, int *, int *);
    const mm_fn fn[5] = {mm_critical_each, mm_local_critical, mm_array,
                         mm_check_then_critical, mm_reduction};
    int smax, smin, amax, amin, pmax, pmin;
    double ts, tsa, t[5];
    BEST_TIME(ts, mm_serial(a, n, &smax, &smin));
    BEST_TIME(tsa, mm_serial(a, na, &amax, &amin));   /* serial reference for (a)'s prefix */
    printf("P011: max and min of N = %ld ints, serial max = %d, min = %d, serial time %.5f s\n",
           n, smax, smin, ts);
    printf("time in seconds (speedup = serial / parallel); (a) uses the first %ld elements\n"
           "and is compared with serial on those (%.5f s)\n", na, tsa);
    printf("%8s %16s %16s %16s %16s %16s %6s\n", "threads", "(a) crit/elem",
           "(b) local+crit", "(c) array[tid]", "(d) check+crit", "(e) reduction", "check");
    int all_ok = 1;
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int ok = 1;
        for (int v = 0; v < 5; v++) {
            long len = (v == 0) ? na : n;
            BEST_TIME(t[v], fn[v](a, len, &pmax, &pmin));
            if (v == 0 ? (pmax != amax || pmin != amin) : (pmax != smax || pmin != smin)) ok = 0;
        }
        all_ok &= ok;
        printf("%8d", THREADS[k]);
        for (int v = 0; v < 5; v++) printf(" %8.5f (%5.2f)", t[v], (v == 0 ? tsa : ts) / t[v]);
        printf(" %6s\n", ok_str(ok));
    }
    printf("\nWhy (d) re-checks inside the critical: between the unlocked test (v > max) and\n"
           "getting the lock, another thread may have stored a LARGER max. Without the re-check\n"
           "we would overwrite it with the smaller v (lost update). The unlocked test is only\n"
           "a filter; the decision is made under the lock.\n");
    printf("All versions match serial: %s\n", ok_str(all_ok));
    free(a);
    return 0;
}

/* Notes:
 * - (a) is correct but slower than serial: every element waits for the lock.
 * - (b)/(c) are the general "reduce by hand" pattern: private scan + one combine per
 *   thread. (c) needs no lock at all; slots are written once, so false sharing on
 *   t_hi[] does not matter. Size the array with omp_get_max_threads().
 * - (d) is fast for random data (a new max is rare: about ln N times per thread),
 *   but degrades towards (a) when data is sorted ascending (every element is a new max).
 * - Reading max outside the lock is a data race if done with a plain read; atomic read
 *   + atomic write (OpenMP 3.1) make it legal. Named criticals let the max and min
 *   updates proceed in parallel.
 * - reduction(max:)/(min:) for C/C++ exists since OpenMP 3.1 (gcc >= 4.7).
 */

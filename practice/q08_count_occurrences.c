/* CS527 Computer Systems — OpenMP practice
 * Q08: Given an integer array and a target value, find in parallel (a) how many
 *      times the target occurs, (b) the FIRST index and (c) the LAST index where
 *      it occurs, and (d) the list of ALL positions in ascending order.
 *  - count      : reduction(+:c)
 *  - first index: reduction(min:first), sentinel n  (n  = "not found")
 *  - last index : reduction(max:last),  sentinel -1 (-1 = "not found")
 *  - positions  : per-thread counts -> prefix sum of counts gives each thread's
 *                 offset -> each thread writes its own slice; schedule(static)
 *                 keeps the chunks in index order, so the output is sorted.
 * Usage: ./q08 [N]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- serial reference versions ---------- */
static long count_serial(const int *a, long n, int target) {
    long c = 0;
    for (long i = 0; i < n; i++) if (a[i] == target) c++;
    return c;
}
static long first_serial(const int *a, long n, int target) {
    for (long i = 0; i < n; i++) if (a[i] == target) return i;
    return -1;
}
static long last_serial(const int *a, long n, int target) {
    for (long i = n - 1; i >= 0; i--) if (a[i] == target) return i;
    return -1;
}
/* pos must have room for count(target) entries; returns how many were written */
static long positions_serial(const int *a, long n, int target, long *pos) {
    long k = 0;
    for (long i = 0; i < n; i++) if (a[i] == target) pos[k++] = i;
    return k;
}

/* ---------- parallel versions ---------- */
static long count_parallel(const int *a, long n, int target) {
    long c = 0;
    #pragma omp parallel for reduction(+:c)
    for (long i = 0; i < n; i++) if (a[i] == target) c++;
    return c;
}
/* Each thread keeps the smallest index it saw (private copy starts at LONG_MAX);
 * reduction(min:) combines them with the original value n (the sentinel). */
static long first_parallel(const int *a, long n, int target) {
    long first = n;
    #pragma omp parallel for reduction(min:first)
    for (long i = 0; i < n; i++) if (a[i] == target && i < first) first = i;
    return first == n ? -1 : first;
}
static long last_parallel(const int *a, long n, int target) {
    long last = -1;
    #pragma omp parallel for reduction(max:last)
    for (long i = 0; i < n; i++) if (a[i] == target && i > last) last = i;
    return last;
}
/* Two passes with the SAME schedule(static) loop: OpenMP guarantees thread t
 * gets the same contiguous chunk both times, and chunk t lies before chunk t+1.
 * off[t] = number of matches in chunks 0..t-1 = where thread t starts writing. */
static long positions_parallel(const int *a, long n, int target, long *pos) {
    int maxt = omp_get_max_threads();
    long *off = calloc((size_t)maxt + 1, sizeof(long));
    if (!off) { fprintf(stderr, "alloc failed\n"); return -1; }
    long total = 0;
    #pragma omp parallel
    {
        int t = omp_get_thread_num(), nt = omp_get_num_threads();
        long c = 0;
        #pragma omp for schedule(static)
        for (long i = 0; i < n; i++) if (a[i] == target) c++;
        off[t + 1] = c;                      /* my count, shifted by one slot */
        #pragma omp barrier                  /* wait until every count is in */
        #pragma omp single
        {
            for (int s = 0; s < nt; s++) off[s + 1] += off[s];   /* prefix sum */
            total = off[nt];
        }                                    /* implicit barrier: offsets ready */
        long k = off[t];
        #pragma omp for schedule(static)
        for (long i = 0; i < n; i++) if (a[i] == target) pos[k++] = i;
    }
    free(off);
    return total;
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static int same_positions(const long *x, long kx, const long *y, long ky) {
    if (kx != ky) return 0;
    for (long i = 0; i < kx; i++) if (x[i] != y[i]) return 0;
    return 1;
}

static int run(long n) {
    int *a = malloc((size_t)n * sizeof(int));
    if (!a) { fprintf(stderr, "alloc failed (N=%ld)\n", n); return 1; }
    for (long i = 0; i < n; i++) a[i] = (int)((i * 7 + 3) % 100);  /* values 0..99 */
    a[n / 3] = 1000;                         /* a value that appears exactly once */

    printf("Q08: count / first / last / all positions of a target (N = %ld)\n", n);
    const int targets[] = {42, 1000, -5};
    const char *what[]  = {"many", "once", "never"};
    omp_set_num_threads(8);
    printf("correctness (8 threads):\n");
    for (int t = 0; t < 3; t++) {
        int tg = targets[t];
        long cs = count_serial(a, n, tg), cp = count_parallel(a, n, tg);
        long fs = first_serial(a, n, tg), fp = first_parallel(a, n, tg);
        long ls = last_serial(a, n, tg),  lp = last_parallel(a, n, tg);
        long *ps = malloc((size_t)(cs + 1) * sizeof(long));
        long *pp = malloc((size_t)(cs + 1) * sizeof(long));
        if (!ps || !pp) { fprintf(stderr, "alloc failed\n"); free(ps); free(pp); free(a); return 1; }
        long ks = positions_serial(a, n, tg, ps), kp = positions_parallel(a, n, tg, pp);
        printf("  target %5d (%-5s): count=%-9ld first=%-9ld last=%-9ld", tg, what[t], cp, fp, lp);
        printf(" -> count %s, first %s, last %s, positions %s\n",
               cs == cp ? "OK" : "WRONG", fs == fp ? "OK" : "WRONG",
               ls == lp ? "OK" : "WRONG", same_positions(ps, ks, pp, kp) ? "OK" : "WRONG");
        free(ps); free(pp);
    }

    /* timing: count/positions with the frequent target; first/last with an
     * ABSENT target so the serial version cannot stop early (full scan). */
    const int many = 42, never = -5;
    long cs = count_serial(a, n, many);
    long *ps = malloc((size_t)(cs + 1) * sizeof(long));
    long *pp = malloc((size_t)(cs + 1) * sizeof(long));
    if (!ps || !pp) { fprintf(stderr, "alloc failed\n"); free(ps); free(pp); free(a); return 1; }
    double t0 = omp_get_wtime(); long c0 = count_serial(a, n, many);  double tsc = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); long f0 = first_serial(a, n, never);        double tsf = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); long l0 = last_serial(a, n, never);         double tsl = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); long k0 = positions_serial(a, n, many, ps); double tsp = omp_get_wtime() - t0;
    printf("\ntiming: count/positions use target %d, first/last use absent target %d\n", many, never);
    printf("serial(s): count %.6f  first %.6f  last %.6f  positions %.6f\n", tsc, tsf, tsl, tsp);
    printf("%8s %10s %6s %10s %6s %10s %6s %10s %6s %6s\n", "threads",
           "count(s)", "sp", "first(s)", "sp", "last(s)", "sp", "allpos(s)", "sp", "check");
    const int threads[] = {1, 2, 4, 8};
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        t0 = omp_get_wtime(); long c = count_parallel(a, n, many);         double tc = omp_get_wtime() - t0;
        t0 = omp_get_wtime(); long f = first_parallel(a, n, never);        double tf = omp_get_wtime() - t0;
        t0 = omp_get_wtime(); long l = last_parallel(a, n, never);         double tl = omp_get_wtime() - t0;
        t0 = omp_get_wtime(); long k = positions_parallel(a, n, many, pp); double tp = omp_get_wtime() - t0;
        int ok = c == c0 && f == f0 && l == l0 && same_positions(ps, k0, pp, k);
        printf("%8d %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %6s\n", threads[ti],
               tc, speedup(tsc, tc), tf, speedup(tsf, tf), tl, speedup(tsl, tl),
               tp, speedup(tsp, tp), ok ? "OK" : "WRONG");
    }
    printf("\n");
    free(ps); free(pp); free(a);
    return 0;
}

int main(int argc, char **argv) {
    long sizes[] = {10000000L, 100000000L};  /* 1e7, 1e8 (1e8 ints = 400 MB) */
    int nsizes = 2;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (sizes[0] < 1) { fprintf(stderr, "usage: %s [N>=1]\n", argv[0]); return 1; }
    for (int s = 0; s < nsizes; s++)
        if (run(sizes[s])) return 1;
    return 0;
}

/* Viva notes:
 * - Race-free: count/first/last each give every thread a PRIVATE copy of the
 *   variable (initialised to 0 / LONG_MAX / LONG_MIN for + / min / max) that is
 *   combined with the original value once at the end. Without the reduction
 *   clause, c++ or first=i on a shared variable is a data race (lost updates).
 * - Sentinels: first starts at n and last at -1, so "not found" falls out of the
 *   min/max naturally. `i < first` keeps the result correct for ANY schedule.
 * - The parallel first/last cannot `break` (not allowed in an omp for), so they
 *   always scan the whole array; the serial first stops at the first hit. That
 *   is why first/last are timed with an absent target.
 * - Positions: writes go to disjoint slices pos[off[t] .. off[t+1]-1], so no two
 *   threads write the same element. The barrier before `single` makes all counts
 *   visible; the implicit barrier after `single` makes all offsets visible.
 *   Sorted output relies on schedule(static) (same contiguous chunk per thread in
 *   both loops); with dynamic/guided the chunks would differ between passes.
 *   Alternative: compute bounds by hand, lo = t*n/nt, hi = (t+1)*n/nt.
 * - Speedup is sub-linear: one compare per 4-byte int is memory-bound, so it is
 *   capped by DRAM bandwidth; positions does two passes plus a serial prefix sum.
 */

/* CS527 Computer Systems — OpenMP practice
 * Q06: Inclusive prefix sum (scan): out[i] = a[0] + a[1] + ... + a[i].
 *      Parallelise it and verify exactly against the serial scan.
 *  - serial running sum
 *  - two-pass blocked scan inside ONE parallel region:
 *      1) each thread scans its own block          -> block total
 *      2) barrier; single thread turns block totals into block offsets
 *      3) barrier; each thread adds its offset to its block
 *  - OpenMP 5.0 `#pragma omp scan` (only compiled on gcc >= 10)
 * Usage: ./q06_prefix_sum [N]   (default N = 1e6, 1e7, 2e7)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 10
#define HAVE_OMP_SCAN 1
#else
#define HAVE_OMP_SCAN 0
#endif

static void scan_serial(const long *a, long *out, long n) {
    long s = 0;
    for (long i = 0; i < n; i++) { s += a[i]; out[i] = s; }
}

/* Returns 0 on success, -1 if the offsets array cannot be allocated. */
static int scan_blocked(const long *a, long *out, long n) {
    long *offset = malloc((size_t)omp_get_max_threads() * sizeof(long));  /* >= team size */
    if (!offset) return -1;
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;    /* my block [lo, hi) */

        long s = 0;                                         /* 1) local scan */
        for (long i = lo; i < hi; i++) { s += a[i]; out[i] = s; }
        offset[tid] = s;                                    /* my block total */

        #pragma omp barrier                                 /* all totals written */
        #pragma omp single
        {                                                   /* 2) exclusive scan of totals */
            long run = 0;
            for (int t = 0; t < nt; t++) { long tot = offset[t]; offset[t] = run; run += tot; }
        }                                                   /* implicit barrier = 2nd barrier */

        long off = offset[tid];                             /* 3) shift my block */
        for (long i = lo; i < hi; i++) out[i] += off;
    }
    free(offset);
    return 0;
}

#if HAVE_OMP_SCAN
/* OpenMP 5.0: the compiler splits the loop at `scan` into the two passes. */
static void scan_omp5(const long *a, long *out, long n) {
    long s = 0;
    #pragma omp parallel for reduction(inscan, +:s)
    for (long i = 0; i < n; i++) {
        s += a[i];
        #pragma omp scan inclusive(s)
        out[i] = s;
    }
}
#endif

static int same(const long *x, const long *y, long n) {
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static void run_size(long n) {
    long *a = malloc((size_t)n * sizeof(long));
    long *ref = malloc((size_t)n * sizeof(long));
    long *out = malloc((size_t)n * sizeof(long));
    if (!a || !ref || !out) { printf("N=%ld skipped (alloc failed)\n", n);
        free(a); free(ref); free(out); return; }
    for (long i = 0; i < n; i++) a[i] = (i * 7 + 3) % 10 - 4;   /* -4 .. 5 */

    scan_blocked(a, out, n);                        /* untimed warm-up runs: also take the */
    scan_serial(a, ref, n);                         /* first-touch page faults of out/ref  */
    double t0 = omp_get_wtime();
    scan_serial(a, ref, n);
    double ts = omp_get_wtime() - t0;
    printf("\nN = %ld   serial time = %.6f s   out[N-1] = %ld\n", n, ts, ref[n - 1]);

    const int threads[] = {1, 2, 4, 8};
    printf("%8s %12s %8s %12s %8s %6s\n", "threads", "blocked(s)", "speedup", "omp scan(s)", "speedup", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        for (long i = 0; i < n; i++) out[i] = 0;
        double b0 = omp_get_wtime();
        int err = scan_blocked(a, out, n);
        double tb = omp_get_wtime() - b0;
        int ok = !err && same(out, ref, n);
        double tk = 0.0;
#if HAVE_OMP_SCAN
        for (long i = 0; i < n; i++) out[i] = 0;
        double k0 = omp_get_wtime();
        scan_omp5(a, out, n);
        tk = omp_get_wtime() - k0;
        ok = ok && same(out, ref, n);
#endif
        printf("%8d %12.6f %8.2f %12.6f %8.2f %6s\n", threads[ti], tb, speedup(ts, tb),
               tk, speedup(ts, tk), ok ? "OK" : "WRONG");
    }
    if (!HAVE_OMP_SCAN) printf("(omp scan columns are 0: needs gcc >= 10)\n");
    free(a); free(ref); free(out);
}

int main(int argc, char **argv) {
    printf("Q06: Inclusive prefix sum  (speedup = T_serial / T_parallel)\n");
    if (argc > 1) {
        long n = (long)atof(argv[1]);               /* accepts 1e7 style too */
        if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
        run_size(n);
    } else {
        const long sizes[] = {1000000L, 10000000L, 20000000L};
        for (int si = 0; si < 3; si++) run_size(sizes[si]);
    }
    return 0;
}

/* Viva notes:
 * - A scan has a loop-carried dependence (out[i] needs out[i-1]), so a plain
 *   `parallel for` is WRONG; reduction does not help either, since every
 *   intermediate sum is needed, not just the final one.
 * - Blocked scan: phase 1 and 3 touch only the thread's own block [lo,hi)
 *   (race-free). The barrier before `single` ensures all block totals are
 *   written before they are combined; the implicit barrier at the end of
 *   `single` ensures offsets are ready before anyone adds them. Adding
 *   `nowait` to single, or dropping the barrier, gives wrong results.
 * - Work is ~2n additions vs n serially, and it streams the array twice, so
 *   the best speedup is about threads/2 and it is memory-bound in practice.
 * - Block bounds n*tid/nt split the array evenly even when n % nt != 0;
 *   omp_get_num_threads() must be read INSIDE the region (it is 1 outside).
 * - Integer data -> the check is exact. With doubles the parallel result
 *   would differ in rounding (different addition order), so use a tolerance.
 * - OpenMP 5.0 alternative: reduction(inscan,+:s) + `#pragma omp scan
 *   inclusive(s)` (exclusive(s) for an exclusive scan). gcc runs the loop
 *   body twice internally, so it is usually slower than the manual version.
 */

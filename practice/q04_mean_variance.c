/* CS527 Computer Systems — OpenMP practice
 * Q04: For an array of doubles compute mean, variance (two-pass), standard
 *      deviation, minimum and maximum in parallel and verify against serial.
 *  - serial: pass 1 sum/min/max, pass 2 sum of squared deviations
 *  - parallel: pass 1 with THREE reductions in one loop
 *      reduction(+:sum) reduction(min:mn) reduction(max:mx)   (OpenMP 3.1+)
 *    pass 2 with reduction(+:sq)
 * Usage: ./q04_mean_variance [N]   (default N = 1e6, 1e7, 3e7)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

typedef struct { double mean, var, sd, mn, mx; } stats_t;

static stats_t stats_serial(const double *a, long n) {
    double sum = 0.0, mn = DBL_MAX, mx = -DBL_MAX;
    for (long i = 0; i < n; i++) {
        sum += a[i];
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }
    double mean = sum / n, sq = 0.0;
    for (long i = 0; i < n; i++) sq += (a[i] - mean) * (a[i] - mean);
    stats_t s = {mean, sq / n, sqrt(sq / n), mn, mx};   /* population variance */
    return s;
}

static stats_t stats_parallel(const double *a, long n) {
    double sum = 0.0, mn = DBL_MAX, mx = -DBL_MAX;
    /* private copies start at 0 / +largest / -largest, merged at the end */
    #pragma omp parallel for reduction(+:sum) reduction(min:mn) reduction(max:mx)
    for (long i = 0; i < n; i++) {
        sum += a[i];
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }
    double mean = sum / n, sq = 0.0;   /* pass 2 needs the final mean -> separate loop */
    #pragma omp parallel for reduction(+:sq)
    for (long i = 0; i < n; i++) sq += (a[i] - mean) * (a[i] - mean);
    stats_t s = {mean, sq / n, sqrt(sq / n), mn, mx};
    return s;
}

static int rel_close(double x, double y) { return fabs(x - y) <= 1e-9 * (1.0 + fabs(y)); }

/* mean/var/sd within FP tolerance; min/max must match exactly */
static int same_stats(stats_t p, stats_t s) {
    return rel_close(p.mean, s.mean) && rel_close(p.var, s.var) &&
           rel_close(p.sd, s.sd) && p.mn == s.mn && p.mx == s.mx;
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static void run_size(long n) {
    double *a = malloc((size_t)n * sizeof(double));
    if (!a) { printf("N=%ld skipped (alloc failed)\n", n); return; }
    unsigned long long st = 2024;                   /* serial LCG fill, values in [0,100) */
    for (long i = 0; i < n; i++) {
        st = st * 6364136223846793005ULL + 1442695040888963407ULL;
        a[i] = 100.0 * (double)(st >> 11) / 9007199254740992.0;
    }
    a[n / 3] = -7.25;                               /* plant a known min and max */
    a[n / 2] = 250.5;

    stats_parallel(a, n);                           /* untimed warm-up */
    double t0 = omp_get_wtime();
    stats_t ref = stats_serial(a, n);
    double ts = omp_get_wtime() - t0;
    printf("\nN = %ld   serial time = %.6f s\n", n, ts);
    printf("serial  : mean=%.6f var=%.6f sd=%.6f min=%.4f max=%.4f\n",
           ref.mean, ref.var, ref.sd, ref.mn, ref.mx);

    const int threads[] = {1, 2, 4, 8};
    stats_t p = ref;
    printf("%8s %12s %8s %6s\n", "threads", "parallel(s)", "speedup", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime();
        p = stats_parallel(a, n);
        double tp = omp_get_wtime() - p0;
        printf("%8d %12.6f %8.2f %6s\n", threads[ti], tp, speedup(ts, tp), same_stats(p, ref) ? "OK" : "WRONG");
    }
    printf("parallel: mean=%.6f var=%.6f sd=%.6f min=%.4f max=%.4f  (8 threads)\n",
           p.mean, p.var, p.sd, p.mn, p.mx);
    free(a);
}

int main(int argc, char **argv) {
    printf("Q04: Mean, variance, std dev, min, max  (speedup = T_serial / T_parallel)\n");
    if (argc > 1) {
        long n = (long)atof(argv[1]);               /* accepts 1e7 style too */
        if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
        run_size(n);
    } else {
        const long sizes[] = {1000000L, 10000000L, 30000000L};
        for (int si = 0; si < 3; si++) run_size(sizes[si]);
    }
    return 0;
}

/* Viva notes:
 * - Several reduction clauses on one loop are allowed; each variable gets its
 *   own private copy initialised to the operator's identity: + -> 0,
 *   min -> largest double, max -> most negative double. The copies are merged
 *   once at the end, so there is no race on sum, mn or mx.
 * - Without the clauses, `if (a[i] < mn) mn = a[i]` is a check-then-act race:
 *   another thread can change mn between the test and the store, losing a
 *   smaller value. min/max reductions need OpenMP 3.1+ in C.
 * - Two passes are needed because pass 2 uses the FINAL mean; the implicit
 *   barrier at the end of the first parallel for guarantees it is ready.
 *   One-pass E[x^2]-E[x]^2 is faster but loses precision (cancellation).
 * - mean/var compare with a tolerance (FP addition order changes), min/max
 *   compare exactly (no arithmetic, just selection).
 * - Speedup is sub-linear: both passes stream the array from memory doing
 *   very little arithmetic per element -> memory-bandwidth bound.
 */

/* CS527 Computer Systems — OpenMP practice
 * Q09: Vector operations. (a) SAXPY: y = a*x + y. (b) Compute the L2 norm
 *      ||x|| = sqrt(sum x[i]^2) and normalise x (x[i] = x[i] / ||x||) using a
 *      SINGLE parallel region.
 *  - saxpy             : parallel for (every y[i] is independent, no reduction)
 *  - normalise (1 reg.): one parallel region: omp for reduction(+:ss), implicit
 *                        barrier, then a second omp for that divides by the norm
 *  - normalise (2 reg.): two separate parallel for loops, for comparison
 * Double precision is used throughout (true SAXPY is single-precision float).
 * Usage: ./q09 [N]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void saxpy_serial(double a, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = a * x[i] + y[i];
}
static void saxpy_parallel(double a, const double *x, double *y, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) y[i] = a * x[i] + y[i];
}

/* returns ||x|| and overwrites x with x/||x||  (x must not be all zeros) */
static double normalise_serial(double *x, long n) {
    double ss = 0.0;
    for (long i = 0; i < n; i++) ss += x[i] * x[i];
    double norm = sqrt(ss);
    for (long i = 0; i < n; i++) x[i] /= norm;
    return norm;
}
/* ONE parallel region = one fork/join. The implicit barrier at the end of the
 * first omp for is what makes it correct: after it, ss holds the FULL sum. */
static double normalise_parallel(double *x, long n) {
    double ss = 0.0;                         /* shared: declared outside the region */
    #pragma omp parallel
    {
        #pragma omp for reduction(+:ss)
        for (long i = 0; i < n; i++) ss += x[i] * x[i];
        /* implicit barrier here -- adding nowait would be a bug (viva notes) */
        double norm = sqrt(ss);              /* private: each thread reads final ss */
        #pragma omp for
        for (long i = 0; i < n; i++) x[i] /= norm;
    }
    return sqrt(ss);
}
/* Same result with two parallel regions (two fork/joins). */
static double normalise_two_regions(double *x, long n) {
    double ss = 0.0;
    #pragma omp parallel for reduction(+:ss)
    for (long i = 0; i < n; i++) ss += x[i] * x[i];
    double norm = sqrt(ss);
    #pragma omp parallel for
    for (long i = 0; i < n; i++) x[i] /= norm;
    return norm;
}

static void init_x(double *x, long n) { for (long i = 0; i < n; i++) x[i] = 1.0 + (i % 100) / 100.0; }
static void init_y(double *y, long n) { for (long i = 0; i < n; i++) y[i] = 2.0 - (i % 37) / 37.0; }

/* relative tolerance: a different summation order changes the last bits */
static int close_val(double a, double b) { return fabs(a - b) <= 1e-9 * (1.0 + fabs(a)); }
static int close_vec(const double *x, const double *y, long n) {
    for (long i = 0; i < n; i++) if (!close_val(x[i], y[i])) return 0;
    return 1;
}
static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static int run(long n) {
    const double alpha = 2.5;
    double *x  = malloc((size_t)n * sizeof(double));   /* original x (read only) */
    double *ys = malloc((size_t)n * sizeof(double));   /* serial SAXPY result    */
    double *yp = malloc((size_t)n * sizeof(double));   /* parallel SAXPY result  */
    double *xs = malloc((size_t)n * sizeof(double));   /* serial normalised x    */
    double *xp = malloc((size_t)n * sizeof(double));   /* parallel normalised x  */
    if (!x || !ys || !yp || !xs || !xp) {
        fprintf(stderr, "alloc failed (N=%ld)\n", n);
        free(x); free(ys); free(yp); free(xs); free(xp); return 1;
    }
    init_x(x, n);
    /* first touch of every buffer, so page faults are not timed later */
    for (long i = 0; i < n; i++) ys[i] = yp[i] = xs[i] = xp[i] = 0.0;

    /* serial references (timed) */
    init_y(ys, n);
    double t0 = omp_get_wtime(); saxpy_serial(alpha, x, ys, n);     double ts_ax = omp_get_wtime() - t0;
    memcpy(xs, x, (size_t)n * sizeof(double));
    t0 = omp_get_wtime(); double norm_s = normalise_serial(xs, n);  double ts_nm = omp_get_wtime() - t0;

    printf("Q09: SAXPY y = %.1f*x + y, then ||x|| and x/||x|| (N = %ld)\n", alpha, n);
    printf("serial(s): saxpy %.6f  normalise %.6f   ||x|| = %.6f\n", ts_ax, ts_nm, norm_s);
    printf("%8s %10s %6s %12s %6s %12s %6s %6s\n", "threads",
           "saxpy(s)", "sp", "norm1reg(s)", "sp", "norm2reg(s)", "sp", "check");
    const int threads[] = {1, 2, 4, 8};
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        init_y(yp, n);                                          /* reset y (untimed) */
        t0 = omp_get_wtime(); saxpy_parallel(alpha, x, yp, n);  double t_ax = omp_get_wtime() - t0;
        int ok = close_vec(ys, yp, n);

        memcpy(xp, x, (size_t)n * sizeof(double));             /* reset x (untimed) */
        t0 = omp_get_wtime(); double n1 = normalise_parallel(xp, n);    double t_n1 = omp_get_wtime() - t0;
        ok = ok && close_val(norm_s, n1) && close_vec(xs, xp, n);

        memcpy(xp, x, (size_t)n * sizeof(double));
        t0 = omp_get_wtime(); double n2 = normalise_two_regions(xp, n); double t_n2 = omp_get_wtime() - t0;
        ok = ok && close_val(norm_s, n2) && close_vec(xs, xp, n);

        printf("%8d %10.6f %6.2f %12.6f %6.2f %12.6f %6.2f %6s\n", threads[ti],
               t_ax, speedup(ts_ax, t_ax), t_n1, speedup(ts_nm, t_n1),
               t_n2, speedup(ts_nm, t_n2), ok ? "OK" : "WRONG");
    }
    printf("\n");
    free(x); free(ys); free(yp); free(xs); free(xp);
    return 0;
}

int main(int argc, char **argv) {
    long sizes[] = {1000000L, 10000000L};    /* 5 arrays x 1e7 doubles = 400 MB */
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
 * - SAXPY is race-free: iteration i reads x[i], y[i] and writes only y[i].
 * - ss is shared in the region; reduction(+:ss) on the omp for gives each thread a
 *   private partial sum (init 0) that is added into ss at the END of the loop.
 * - Why NOT nowait on the first loop: the implicit barrier is the only thing that
 *   guarantees the reduction has finished. With nowait, a fast thread would go on
 *   and compute sqrt(ss) from a partial (or not yet combined) sum -> wrong norm,
 *   and different threads would divide by different values.
 * - The second loop needs no reduction: each thread divides only its own x[i];
 *   norm is a private local, ss is only read after the barrier.
 * - One region vs two: same work, but one fork/join instead of two (team reuse);
 *   the gain is small because libgomp keeps its thread pool alive.
 * - Speedup flattens at large N: ~2 flops per 16-24 bytes moved means these loops
 *   are memory-bandwidth bound; at N=1e6 the data fits in cache and scales better.
 */

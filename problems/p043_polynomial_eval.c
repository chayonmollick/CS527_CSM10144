/* P043: Polynomial evaluation (many points; one huge polynomial)
 * Problem: (a) Evaluate a polynomial of degree D, p(x) = a[0] + a[1]x + ...
 *   + a[D]x^D, at M points x[0..M-1]. (b) Evaluate ONE polynomial of huge
 *   degree (D = 1e7) at a single point x. Parallelise both with OpenMP, verify
 *   against serial Horner within a tolerance and report time and speedup for
 *   1, 2, 4 and 8 threads. Use |x| <= 1 so the powers stay bounded.
 * Solution: (a) Horner for one point is a sequential chain, but different
 *   points are independent -> parallel for over the points (embarrassingly
 *   parallel, each iteration writes only y[j]). (b) only one point, so split
 *   the COEFFICIENTS: p(x) = sum over blocks of x^lo * q_t(x) with
 *   q_t(x) = a[lo] + a[lo+1]x + ... + a[hi-1]x^(hi-1-lo). Each thread runs
 *   Horner on its block [lo, hi), multiplies by x^lo (pow) and the block results
 *   are added with reduction(+:p) -> race-free.
 * Source: lab extension (Horner's rule; reduction with a per-thread prologue).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p043_polynomial_eval.c -o p043 -lm
 * Run:   ./p043 [M] [D] [Dbig] [x]   (defaults M = 1e5, D = 500, Dbig = 1e7, x = -0.9999995)
 */
#include "common.h"

/* Horner: a[0] + x*(a[1] + x*(a[2] + ... + x*a[d])) */
static double horner(const double *a, long d, double x) {
    double p = a[d];
    for (long i = d - 1; i >= 0; i--) p = p * x + a[i];
    return p;
}

/* ---------- (a) many points ---------- */
static void eval_points_serial(const double *a, long d, const double *x, double *y, long m) {
    for (long j = 0; j < m; j++) y[j] = horner(a, d, x[j]);
}
static void eval_points_parallel(const double *a, long d, const double *x, double *y, long m) {
    #pragma omp parallel for schedule(static)
    for (long j = 0; j < m; j++) y[j] = horner(a, d, x[j]);
}

/* ---------- (b) one huge polynomial, one point ---------- */
static double eval_big_parallel(const double *a, long d, double x) {
    long nc = d + 1;                                  /* coefficients a[0..d] */
    double p = 0.0;
    #pragma omp parallel reduction(+:p)
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = nc * tid / nt, hi = nc * (tid + 1) / nt;
        double q = 0.0;                               /* Horner on a[lo..hi) */
        for (long i = hi - 1; i >= lo; i--) q = q * x + a[i];
        p += q * pow(x, (double)lo);                  /* shift the block to x^lo */
    }
    return p;
}

int main(int argc, char *argv[]) {
    long m    = arg_long(argc, argv, 1, 100000L);
    long d    = arg_long(argc, argv, 2, 500L);
    long dbig = arg_long(argc, argv, 3, 10000000L);
    double xb = arg_double(argc, argv, 4, -0.9999995);
    if (m < 1) m = 1;
    if (d < 0) d = 0;
    if (dbig < 0) dbig = 0;
    double *a  = xmalloc((size_t)(d + 1) * sizeof(double));
    double *x  = xmalloc((size_t)m * sizeof(double));
    double *ys = xmalloc((size_t)m * sizeof(double)), *yp = xmalloc((size_t)m * sizeof(double));
    double *ab = xmalloc((size_t)(dbig + 1) * sizeof(double));
    fill_double(a, d + 1, -1.0, 1.0, 43);
    fill_double(x, m, -1.0, 1.0, 4343);
    fill_double(ab, dbig + 1, -1.0, 1.0, 434343);
    omp_warmup();
    double ts, tp;

    BEST_TIME(ts, eval_points_serial(a, d, x, ys, m));
    printf("P043: polynomial evaluation\n");
    printf("\n(a) degree D = %ld at M = %ld points (parallel over points)\n", d, m);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(yp, 0, (size_t)m * sizeof(double));
        BEST_TIME(tp, eval_points_parallel(a, d, x, yp, m));
        table_row(m, THREADS[k], ts, tp, close_doubles(yp, ys, m, 1e-9));
    }

    double ps = 0, pp = 0;
    BEST_TIME(ts, ps = horner(ab, dbig, xb));
    printf("\n(b) one polynomial of degree D = %ld at x = %.7f (blocked Horner + reduction), p(x) = %.10f\n",
           dbig, xb, ps);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, pp = eval_big_parallel(ab, dbig, xb));
        table_row(dbig + 1, THREADS[k], ts, tp, close1(pp, ps, 1e-9));
    }
    free(a); free(x); free(ys); free(yp); free(ab);
    return 0;
}

/* Notes:
 * - Horner (D multiply-adds, no powers) beats summing a[i]*pow(x,i). Its loop
 *   is a dependence chain p = p*x + a[i], so it cannot be split naively.
 * - (a) parallelism is across independent points; best when M >> threads.
 *   If M is tiny and D huge, use the (b) method for each point instead.
 * - (b) is a reduction whose terms need a per-block prologue: x^lo. Any
 *   associative split works: p(x) = sum_t x^lo_t * q_t(x). pow(x, lo) costs
 *   O(1); the block Horner costs O(D/p).
 * - Results differ from serial only by rounding (different order of the
 *   additions) -> compare with a relative tolerance. |x| <= 1 keeps x^lo from
 *   overflowing; for |x| > 1 evaluate in reverse (in 1/x) or scale.
 * - (b) streams 80 MB once with a dependent multiply-add chain per thread, so
 *   it scales until memory bandwidth is reached.
 */

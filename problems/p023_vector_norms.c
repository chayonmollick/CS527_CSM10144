/* P023: Vector norms in one parallel loop
 * Problem: For a vector x[N] of doubles compute the L1 norm sum|x[i]|, the L2 norm
 *   sqrt(sum x[i]^2), the L-infinity norm max|x[i]| and the smallest absolute value
 *   min|x[i]|, all in ONE parallel loop. Verify against a serial loop and report
 *   the speedup.
 * Solution: each quantity is a reduction, so one loop carries several reduction
 *   clauses: reduction(+:l1, sq) reduction(max:mx) reduction(min:mn). Every thread
 *   gets private copies initialised to the identity of the operator (0 for +, the
 *   smallest double for max, the largest for min) and OpenMP combines them at the
 *   end; the square root is taken once, after the loop.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p023_vector_norms.c -o p023 -lm
 * Run:   ./p023 [N]   (default N = 1e7)
 */
#include "common.h"

typedef struct { double l1, l2, linf, minabs; } norms_t;

static norms_t norms_serial(const double *x, long n) {
    double l1 = 0.0, sq = 0.0, mx = 0.0, mn = HUGE_VAL;   /* HUGE_VAL = +infinity */
    for (long i = 0; i < n; i++) {
        double v = fabs(x[i]);
        l1 += v;
        sq += v * v;
        if (v > mx) mx = v;
        if (v < mn) mn = v;
    }
    norms_t r = {l1, sqrt(sq), mx, mn};
    return r;
}

static norms_t norms_parallel(const double *x, long n) {
    double l1 = 0.0, sq = 0.0, mx = 0.0, mn = HUGE_VAL;
    #pragma omp parallel for reduction(+:l1, sq) reduction(max:mx) reduction(min:mn)
    for (long i = 0; i < n; i++) {
        double v = fabs(x[i]);
        l1 += v;
        sq += v * v;
        if (v > mx) mx = v;
        if (v < mn) mn = v;
    }
    norms_t r = {l1, sqrt(sq), mx, mn};    /* sqrt once, after the reduction */
    return r;
}

/* Sums in a different order: tolerance. max and min are exact: must be equal. */
static int same_norms(norms_t a, norms_t b) {
    return close1(a.l1, b.l1, 1e-9) && close1(a.l2, b.l2, 1e-9) && a.linf == b.linf && a.minabs == b.minabs;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    double *x = xmalloc((size_t)n * sizeof(double));
    fill_double(x, n, -10.0, 10.0, 7);
    omp_warmup();

    norms_t rs = {0, 0, 0, 0}, rp = {0, 0, 0, 0};
    double ts, tp;
    BEST_TIME(ts, rs = norms_serial(x, n));
    printf("P023: vector norms, N = %ld\n", n);
    printf("  L1 = %.6f  L2 = %.6f  Linf = %.6f  min|x| = %.3e\n", rs.l1, rs.l2, rs.linf, rs.minabs);
    printf("  sanity Linf <= L2 <= L1: %s\n", ok_str(rs.linf <= rs.l2 && rs.l2 <= rs.l1));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = norms_parallel(x, n));
        table_row(n, THREADS[k], ts, tp, same_norms(rs, rp));
    }
    printf("  parallel: L1 = %.6f  L2 = %.6f  Linf = %.6f  min|x| = %.3e\n", rp.l1, rp.l2, rp.linf, rp.minabs);
    free(x);
    return 0;
}

/* Notes:
 * - Several reduction clauses (and several variables per clause) are allowed on one
 *   loop; one pass over memory instead of four.
 * - reduction(max:) / reduction(min:) exist since OpenMP 3.1 (C/C++). Private copies
 *   start at the identity: -largest value for max, +largest for min. min/max results
 *   are exact (no rounding), so they must match the serial result exactly.
 * - L1 and L2 sums are added in a different order per thread count: compare with a
 *   tolerance. Take sqrt AFTER the reduction (sum of square roots is wrong).
 * - For huge or tiny values x^2 can overflow/underflow; robust codes (BLAS dnrm2)
 *   scale by max|x| first: L2 = m * sqrt(sum (x/m)^2).
 * - Norm identities give a free sanity check: Linf <= L2 <= L1 <= N*Linf.
 */

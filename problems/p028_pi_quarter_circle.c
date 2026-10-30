/* P028: pi from the area of a quarter circle
 * Problem: The quarter circle y = sqrt(1 - x^2), 0 <= x <= 1, has area pi/4.
 *   Approximate it by the left Riemann sum pi/4 ~ sum_{i=0}^{N-1} dx*sqrt(1 - x_i^2)
 *   with x_i = i*dx, dx = 1/N. Parallelise the sum with OpenMP, print the error of
 *   the estimate for N = 1e3 .. 1e8 and the timing table, and verify the parallel
 *   result against the serial one.
 * Solution: the terms are independent; x_i is computed from i (not by x += dx,
 *   which would be a loop-carried dependence and also accumulate rounding error).
 *   Each thread sums its block of terms into a private copy of s via
 *   reduction(+:s); the copies are added at the end. dx is multiplied in once after
 *   the loop.
 * Source: textbook Exercise 19.4.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p028_pi_quarter_circle.c -o p028 -lm
 * Run:   ./p028 [N]   (default N = 1e8 for the timing table)
 */
#include "common.h"

static double pi_serial(long n) {
    double dx = 1.0 / n, s = 0.0;
    for (long i = 0; i < n; i++) {
        double x = i * dx;
        s += sqrt(1.0 - x * x);
    }
    return 4.0 * dx * s;
}

static double pi_parallel(long n) {
    double dx = 1.0 / n, s = 0.0;
    #pragma omp parallel for reduction(+:s)
    for (long i = 0; i < n; i++) {
        double x = i * dx;               /* from i: no dependence between iterations */
        s += sqrt(1.0 - x * x);
    }
    return 4.0 * dx * s;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 100000000L);
    if (n < 1) n = 1;
    omp_warmup();

    printf("P028: pi = 4 * sum dx*sqrt(1 - x_i^2), left Riemann sum\n");
    printf("Error vs N (parallel, %d threads):\n", omp_get_max_threads());
    printf("%12s %20s %12s %10s\n", "N", "estimate", "error", "error*N");
    for (long m = 1000; m <= 100000000L; m *= 10) {
        double p = pi_parallel(m);
        printf("%12ld %20.15f %12.3e %10.4f\n", m, p, p - PI, (p - PI) * m);
    }

    double ps = 0, pp = 0, ts, tp;
    BEST_TIME(ts, ps = pi_serial(n));
    printf("Timing, N = %ld: serial estimate %.15f (error %.3e)\n", n, ps, ps - PI);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, pp = pi_parallel(n));
        table_row(n, THREADS[k], ts, tp, close1(ps, pp, 1e-10));
    }
    return 0;
}

/* Notes:
 * - Error ~ 2/N (error*N -> 2): the left sum of a decreasing f overestimates the
 *   integral by about (f(0) - f(1))*dx/2 = dx/2, so pi is off by 4*dx/2 = 2/N.
 *   A midpoint rule, x_i = (i + 0.5)*dx, is more accurate for the same work.
 * - The reduction makes it race-free; "s += ..." on a shared s would lose updates.
 * - Compute-bound (sqrt + a few flops per term, no array): speedup is close to
 *   linear until the number of physical cores is reached.
 * - The parallel sum groups terms differently, so compare with a tolerance; the
 *   rounding difference (~1e-12) is far below the method's error (~1e-8 at N = 1e8).
 * - More threads make it faster, not more accurate: accuracy needs a larger N or a
 *   better rule (P029: trapezoid/Simpson).
 */

/* P027: Series sums and floating-point summation order
 * Problem: For i = 1..N compute three partial sums with an OpenMP reduction:
 *   the Basel sum S1 = sum 1/i^2 (-> pi^2/6), the Leibniz series
 *   S2 = 4 * sum_{k=0}^{N-1} (-1)^k/(2k+1) (-> pi) and the harmonic number
 *   H_N = sum 1/i (~ ln N + 0.5772...). Show how the summation order changes the
 *   last digits (serial forward, serial backward, parallel with 1/2/4/8 threads),
 *   explain why parallel floating-point sums are compared with a tolerance, verify
 *   within a tolerance and report the speedup.
 * Solution: each term depends only on i, so one loop with
 *   reduction(+:basel, leib, harm) works: each thread adds its own block of terms
 *   into private sums which are then added together. The sign (-1)^k is computed
 *   from i, NOT by flipping a variable each iteration (that would be a loop-carried
 *   dependence). Because floating-point + is not associative, a different grouping
 *   rounds differently: results agree only within the rounding-error bound for
 *   adding N numbers (about N * DBL_EPSILON relative), not bit for bit.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p027_series_sums.c -o p027 -lm
 * Run:   ./p027 [N]   (default N = 1e8)
 */
#include "common.h"
#include <float.h>                    /* DBL_EPSILON = 2.2e-16, gap between 1.0 and the next double */

#define EULER_GAMMA 0.57721566490153286061

typedef struct { double basel, leib, harm; } sums_t;

/* Large terms first (i = 1, 2, ...): small terms are added to a big sum and lose bits. */
static sums_t sums_forward(long n) {
    double basel = 0.0, leib = 0.0, harm = 0.0;
    for (long i = 1; i <= n; i++) {
        double di = (double)i;
        basel += 1.0 / (di * di);
        leib  += (i % 2 == 1 ? 4.0 : -4.0) / (2.0 * di - 1.0);   /* k = i-1: + for even k */
        harm  += 1.0 / di;
    }
    sums_t s = {basel, leib, harm};
    return s;
}

/* Small terms first (i = N down to 1): usually the most accurate order. */
static sums_t sums_backward(long n) {
    double basel = 0.0, leib = 0.0, harm = 0.0;
    for (long i = n; i >= 1; i--) {
        double di = (double)i;
        basel += 1.0 / (di * di);
        leib  += (i % 2 == 1 ? 4.0 : -4.0) / (2.0 * di - 1.0);
        harm  += 1.0 / di;
    }
    sums_t s = {basel, leib, harm};
    return s;
}

static sums_t sums_parallel(long n) {
    double basel = 0.0, leib = 0.0, harm = 0.0;
    #pragma omp parallel for reduction(+:basel, leib, harm)
    for (long i = 1; i <= n; i++) {
        double di = (double)i;
        basel += 1.0 / (di * di);
        leib  += (i % 2 == 1 ? 4.0 : -4.0) / (2.0 * di - 1.0);   /* sign from i: no dependence */
        harm  += 1.0 / di;
    }
    sums_t s = {basel, leib, harm};
    return s;
}

static int close_sums(sums_t a, sums_t b, double tol) {
    return close1(a.basel, b.basel, tol) && close1(a.leib, b.leib, tol) && close1(a.harm, b.harm, tol);
}

static void print_sums(const char *name, sums_t s) {
    printf("  %-22s %.17f  %.17f  %.17f\n", name, s.basel, s.leib, s.harm);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 100000000L);
    double dn = (double)(n > 0 ? n : 1);
    omp_warmup();

    sums_t fwd = {0, 0, 0}, bwd = sums_backward(n), par[NTHREADS];
    double ts, tp[NTHREADS];
    BEST_TIME(ts, fwd = sums_forward(n));
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp[k], par[k] = sums_parallel(n));
    }

    printf("P027: series sums, N = %ld (17 significant digits; watch the last ones)\n", n);
    printf("  %-22s %-19s  %-19s  %-19s\n", "order", "sum 1/i^2", "4 sum (-1)^k/(2k+1)", "H_N = sum 1/i");
    print_sums("serial forward", fwd);
    print_sums("serial backward", bwd);
    for (int k = 0; k < NTHREADS; k++) {
        char name[32];
        snprintf(name, sizeof name, "parallel, %d thread%s", THREADS[k], THREADS[k] > 1 ? "s" : "");
        print_sums(name, par[k]);
    }
    sums_t limit = {PI * PI / 6.0, PI, log(dn) + EULER_GAMMA + 1.0 / (2.0 * dn) - 1.0 / (12.0 * dn * dn)};
    print_sums("limit / asymptotic", limit);
    printf("  (truncation error ~1/N for the first two; the H_N line is ln N + gamma + 1/(2N) - 1/(12N^2))\n");
    printf("  forward - backward:    %.3e  %.3e  %.3e\n", fwd.basel - bwd.basel, fwd.leib - bwd.leib, fwd.harm - bwd.harm);

    /* Adding N numbers in ANY order has relative error <= (N-1)*DBL_EPSILON/2 (for positive
     * terms), so two different orders may differ by up to about N*DBL_EPSILON. */
    double tol = 2.0 * (double)n * DBL_EPSILON;
    printf("Timing (check: parallel within %.1e = 2*N*DBL_EPSILON relative of serial forward):\n", tol);
    table_header();
    for (int k = 0; k < NTHREADS; k++) table_row(n, THREADS[k], ts, tp[k], close_sums(fwd, par[k], tol));
    printf("  serial forward vs backward within the same tolerance: %s\n", ok_str(close_sums(fwd, bwd, tol)));
    return 0;
}

/* Notes:
 * - Floating-point addition is not associative: (a+b)+c != a+(b+c) in the last bit.
 *   A reduction groups the terms by thread, so every thread count gives slightly
 *   different digits. Compare with a relative tolerance, never with ==; here
 *   2*N*DBL_EPSILON, the rounding-error bound for adding N numbers. At N = 1e8 the
 *   forward and backward sums of 1/i^2 differ by ~1e-9: it is not a bug.
 * - Adding small terms first (backward) loses fewer bits than adding them to an
 *   already large sum; Kahan (compensated) summation is the other fix.
 * - Truncation error (stopping at N) is ~1/N, far bigger than rounding error: more
 *   threads do not make the answer more accurate, only faster.
 * - Don't write sign = -sign inside the loop: each iteration would depend on the
 *   previous one. Compute everything from i (i % 2), so iterations are independent.
 * - Use (double)i * i, not i*i: the integer product overflows for i > 3e9 (and for
 *   i > 46340 with 32-bit int).
 */

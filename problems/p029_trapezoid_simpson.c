/* P029: Numerical integration: composite trapezoidal and Simpson's rule
 * Problem: Integrate f(x) = exp(-x^2)*sin(x) + 1 (or any given f) over [a, b] with n
 *   subintervals using the composite trapezoidal rule and Simpson's rule
 *   (weights 1, 4, 2, 4, ..., 2, 4, 1; n even), serially and with OpenMP. Verify the
 *   parallel results against serial ones, check the accuracy on the integral of
 *   sin x over [0, pi] (= 2), and report the speedup.
 * Solution: with h = (b - a)/n and x_i = a + i*h (computed from i), each interior
 *   point contributes w_i * f(x_i) independently, so the sum over i = 1..n-1 is a
 *   reduction(+:s). Trapezoid: T = h*(f(a)/2 + sum f(x_i) + f(b)/2). Simpson:
 *   S = h/3*(f(a) + sum w_i f(x_i) + f(b)) with w_i = 4 for odd i, 2 for even i;
 *   the weight depends only on i, so iterations stay independent.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p029_trapezoid_simpson.c -o p029 -lm
 * Run:   ./p029 [n] [a] [b]   (defaults n = 4e6, a = 0, b = 2; n is made even)
 */
#include "common.h"

static double f(double x) { return exp(-x * x) * sin(x) + 1.0; }   /* change f here */

/* The function being integrated: which = F_PROBLEM -> f, which = F_SIN -> sin (accuracy
 * test, integral over [0, pi] = 2). A selector instead of a function pointer lets gcc
 * inline f into the serial AND the parallel loop, so the timing comparison is fair. */
enum { F_PROBLEM, F_SIN };
static double fun(int which, double x) { return which == F_PROBLEM ? f(x) : sin(x); }

static double trap_serial(int which, double a, double b, long n) {
    double h = (b - a) / n, s = 0.0;
    for (long i = 1; i < n; i++) s += fun(which, a + i * h);
    return h * (0.5 * fun(which, a) + s + 0.5 * fun(which, b));
}

static double trap_parallel(int which, double a, double b, long n) {
    double h = (b - a) / n, s = 0.0;
    #pragma omp parallel for reduction(+:s)
    for (long i = 1; i < n; i++) s += fun(which, a + i * h);
    return h * (0.5 * fun(which, a) + s + 0.5 * fun(which, b));
}

/* n must be even. */
static double simpson_serial(int which, double a, double b, long n) {
    double h = (b - a) / n, s = 0.0;
    for (long i = 1; i < n; i++) s += (i % 2 == 1 ? 4.0 : 2.0) * fun(which, a + i * h);
    return h / 3.0 * (fun(which, a) + s + fun(which, b));
}

static double simpson_parallel(int which, double a, double b, long n) {
    double h = (b - a) / n, s = 0.0;
    #pragma omp parallel for reduction(+:s)
    for (long i = 1; i < n; i++) s += (i % 2 == 1 ? 4.0 : 2.0) * fun(which, a + i * h);   /* weight from i */
    return h / 3.0 * (fun(which, a) + s + fun(which, b));
}

int main(int argc, char *argv[]) {
    long n   = arg_long(argc, argv, 1, 4000000L);
    double a = arg_double(argc, argv, 2, 0.0);
    double b = arg_double(argc, argv, 3, 2.0);
    if (n < 2) n = 2;
    if (n % 2 == 1) n++;                                 /* Simpson needs an even n */
    omp_warmup();

    /* 1. Accuracy on a known integral: errors fall as h^2 (trapezoid) and h^4 (Simpson). */
    printf("P029: accuracy test, integral of sin x over [0, pi] = 2 (parallel)\n");
    printf("%10s %14s %14s\n", "n", "trap error", "simpson error");
    double simp_err = 0.0;
    for (long m = 4; m <= 1024; m *= 4) {
        double et = trap_parallel(F_SIN, 0.0, PI, m) - 2.0;
        double es = simpson_parallel(F_SIN, 0.0, PI, m) - 2.0;
        printf("%10ld %14.3e %14.3e\n", m, et, es);
        simp_err = es;
    }
    printf("  each 4x more points: trapezoid error / 16, Simpson error / 256\n");
    printf("  Simpson with n = 1024 accurate to 1e-10: %s\n", ok_str(fabs(simp_err) < 1e-10));

    /* 2. Timing on f over [a, b]. */
    double ts, tp, rs = 0, rp = 0;
    printf("Trapezoid: integral of exp(-x^2)*sin(x) + 1 over [%g, %g], n = %ld\n", a, b, n);
    BEST_TIME(ts, rs = trap_serial(F_PROBLEM, a, b, n));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = trap_parallel(F_PROBLEM, a, b, n));
        table_row(n, THREADS[k], ts, tp, close1(rs, rp, 1e-10));
    }
    printf("  result %.15f\n", rs);

    double ss = 0, sp = 0;
    printf("Simpson: same integral, n = %ld\n", n);
    BEST_TIME(ts, ss = simpson_serial(F_PROBLEM, a, b, n));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, sp = simpson_parallel(F_PROBLEM, a, b, n));
        table_row(n, THREADS[k], ts, tp, close1(ss, sp, 1e-10));
    }
    printf("  result %.15f (trapezoid - Simpson = %.2e)\n", ss, rs - ss);
    return 0;
}

/* Notes:
 * - Race-free: x_i = a + i*h is computed from i (no x += h carried between
 *   iterations), and the only shared update is the reduction variable s.
 * - Error: trapezoid O(h^2), Simpson O(h^4) for smooth f; Simpson needs n even
 *   (it fits a parabola through each pair of subintervals).
 * - The end points are added once outside the loop; the loop runs i = 1..n-1.
 * - Compute-bound (exp, sin per point, no arrays): near-linear speedup up to the
 *   number of cores. Results differ from serial in the last digits (reordered sum).
 * - To integrate another function just change f(). A function-pointer parameter
 *   would also work, but gcc then inlines f only into the serial loop (it clones it
 *   for the constant pointer), so the serial version looks unfairly fast.
 */

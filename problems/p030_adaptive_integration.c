/* P030: Integration with uneven work per interval (loop schedules)
 * Problem: Integrate f(x) = sqrt(1 - x^2) over [0, 1] (= pi/4) using N coarse
 *   intervals, where each interval is refined into more sub-steps the steeper f is
 *   on it: interval i gets m_i = 1 + K*|slope_i| midpoint sub-steps. f is very steep
 *   near x = 1, so the work per iteration is unpredictable and concentrated at the
 *   end. Parallelise the loop over intervals and compare schedule(static),
 *   schedule(dynamic, c) and schedule(guided); verify all results agree within a
 *   tolerance and explain why dynamic scheduling helps.
 * Solution: iteration i computes the area of interval i on its own (its own m_i and
 *   sub-step loop), and the areas are added with reduction(+:sum), so every schedule
 *   is race-free. static hands each thread one contiguous block of N/T intervals, so
 *   the thread owning the end of [0, 1] gets most of the sub-steps and the others
 *   wait at the barrier. dynamic,c hands out chunks of c intervals on demand and
 *   guided hands out shrinking chunks, so the work is balanced.
 * Source: textbook Exercise 19.9.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p030_adaptive_integration.c -o p030 -lm
 * Run:   ./p030 [N] [K] [chunk]   (defaults N = 2e5 intervals, K = 100, chunk = 64)
 */
#include "common.h"

static double f(double x) { return x < 1.0 ? sqrt(1.0 - x * x) : 0.0; }

/* Sub-steps for coarse interval i: more where f is steep (|slope| -> infinity at x = 1). */
static long substeps(long i, long n, double k) {
    double x0 = (double)i / n, x1 = (double)(i + 1) / n;    /* x1 = 1.0 exactly for i = n-1 */
    double slope = fabs(f(x1) - f(x0)) * n;                 /* |f(x1) - f(x0)| / h */
    return 1 + (long)(k * slope);
}

/* Area of coarse interval i: midpoint rule with its own number of sub-steps. */
static double interval_area(long i, long n, double k) {
    long m = substeps(i, n, k);
    double x0 = (double)i / n, w = 1.0 / n / m, s = 0.0;
    for (long j = 0; j < m; j++) s += f(x0 + (j + 0.5) * w);
    return s * w;
}

static double integrate_serial(long n, double k) {
    double sum = 0.0;
    for (long i = 0; i < n; i++) sum += interval_area(i, n, k);
    return sum;
}

static double integrate_static(long n, double k) {
    double sum = 0.0;
    #pragma omp parallel for schedule(static) reduction(+:sum)
    for (long i = 0; i < n; i++) sum += interval_area(i, n, k);
    return sum;
}

static double integrate_dynamic(long n, double k, int chunk) {
    double sum = 0.0;
    #pragma omp parallel for schedule(dynamic, chunk) reduction(+:sum)
    for (long i = 0; i < n; i++) sum += interval_area(i, n, k);
    return sum;
}

static double integrate_guided(long n, double k) {
    double sum = 0.0;
    #pragma omp parallel for schedule(guided) reduction(+:sum)
    for (long i = 0; i < n; i++) sum += interval_area(i, n, k);
    return sum;
}

int main(int argc, char *argv[]) {
    long n    = arg_long(argc, argv, 1, 200000L);
    double k  = arg_double(argc, argv, 2, 100.0);
    int chunk = (int)arg_long(argc, argv, 3, 64);
    if (n < 1) n = 1;
    if (chunk < 1) chunk = 1;
    omp_warmup();

    /* How uneven is the work? Share of all sub-steps in each quarter of [0, 1]
     * (= what each of 4 threads gets with schedule(static)). */
    double total = 0.0, quarter[4] = {0, 0, 0, 0};
    for (long i = 0; i < n; i++) {
        double m = (double)substeps(i, n, k);
        total += m;
        quarter[i * 4 / n] += m;
    }
    printf("P030: integral of sqrt(1-x^2) on [0,1], N = %ld intervals, K = %g, %.0f sub-steps in total\n",
           n, k, total);
    printf("  schedule(static), 4 threads, share of the work: %.0f%% %.0f%% %.0f%% %.0f%%\n",
           100 * quarter[0] / total, 100 * quarter[1] / total, 100 * quarter[2] / total, 100 * quarter[3] / total);

    double ts, tst, tdy, tgu, r0 = 0, rst = 0, rdy = 0, rgu = 0;
    BEST_TIME(ts, r0 = integrate_serial(n, k));
    printf("  4*integral = %.12f, error vs pi = %.2e\n", 4.0 * r0, 4.0 * r0 - PI);
    printf("%8s %10s %10s %7s %10s %7s %10s %7s %6s\n", "threads", "serial", "static", "speedup",
           "dynamic", "speedup", "guided", "speedup", "check");
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tst, rst = integrate_static(n, k));
        BEST_TIME(tdy, rdy = integrate_dynamic(n, k, chunk));
        BEST_TIME(tgu, rgu = integrate_guided(n, k));
        int ok = close1(r0, rst, 1e-10) && close1(r0, rdy, 1e-10) && close1(r0, rgu, 1e-10);
        printf("%8d %10.6f %10.6f %7.2f %10.6f %7.2f %10.6f %7.2f %6s\n", THREADS[t], ts,
               tst, tst > 0 ? ts / tst : 0.0, tdy, tdy > 0 ? ts / tdy : 0.0, tgu, tgu > 0 ? ts / tgu : 0.0,
               ok_str(ok));
    }
    printf("  (dynamic uses chunk = %d)\n", chunk);
    return 0;
}

/* Notes:
 * - static: iterations split into T equal contiguous blocks before the loop starts;
 *   no runtime cost, but if work per iteration varies, the thread with the expensive
 *   block (here the last one, near x = 1) finishes last and the others idle.
 * - dynamic,c: a thread grabs the next c iterations when it is free -> balanced load,
 *   but every grab is a synchronised operation; c = 1 maximises balance and overhead,
 *   a large c reduces both. guided: chunks start large (remaining/T) and shrink.
 * - Equal iteration COUNTS are not equal WORK: choose dynamic/guided when the cost
 *   of an iteration is unknown or varies (adaptive refinement, triangular loops).
 *   schedule(static, 1) (round-robin) also balances a smoothly increasing workload.
 * - All schedules give the same sum up to rounding (a different addition order),
 *   so they are compared with a tolerance.
 * - schedule(runtime) + OMP_SCHEDULE="dynamic,64" lets you try schedules without
 *   recompiling.
 */

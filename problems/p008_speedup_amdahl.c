/* P008: Measured speedup, efficiency, Karp-Flatt serial fraction and Amdahl prediction
 * Problem: Time a loop with a measurable amount of work (many multiples of the timer
 *   resolution omp_get_wtick()) for 1, 2, ..., 8 threads. Compute speedup S = T1/Tp,
 *   efficiency E = S/p and the experimentally determined serial fraction (Karp-Flatt)
 *   e = (1/S - 1/p) / (1 - 1/p). Use e with Amdahl's law to predict the speedup on 16
 *   and 64 threads, and the maximum possible speedup 1/e.
 * Solution: the work is the midpoint rule for pi = integral of 4/(1+x^2) on [0,1];
 *   iterations are independent, the sum uses reduction(+:sum) so there is no race.
 *   T1 is the serial program (not the parallel one on 1 thread). For small N the
 *   computation is repeated `rounds` times per timing so that every measurement is
 *   many ticks long. Amdahl: S(p) = 1 / (e + (1 - e)/p)  <=  1/e  for every p.
 * Source: textbook Ex 28.1; Amdahl's law and Karp-Flatt metric from the lectures.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p008_speedup_amdahl.c -o p008 -lm
 * Run:   ./p008 [N]          (default N = 2e8 iterations)
 */
#include "common.h"

static double pi_serial(long n) {
    double h = 1.0 / (double)n, sum = 0.0;
    for (long i = 0; i < n; i++) {
        double x = ((double)i + 0.5) * h;
        sum += 4.0 / (1.0 + x * x);
    }
    return sum * h;
}

static double pi_parallel(long n) {
    double h = 1.0 / (double)n, sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < n; i++) {
        double x = ((double)i + 0.5) * h;
        sum += 4.0 / (1.0 + x * x);
    }
    return sum * h;
}

/* One timing = `rounds` repetitions, so even a small N lasts many timer ticks.
 * n_v is volatile: the compiler must re-read it each round and cannot reuse a result. */
static long rounds = 1;
static volatile long n_v;
static double run_serial(void) {
    double r = 0.0;
    for (long k = 0; k < rounds; k++) r = pi_serial(n_v);
    return r;
}
static double run_parallel(void) {
    double r = 0.0;
    for (long k = 0; k < rounds; k++) r = pi_parallel(n_v);
    return r;
}

/* Amdahl's law with serial fraction f on p processors. */
static double amdahl(double f, double p) { return 1.0 / (f + (1.0 - f) / p); }

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 200000000L);
    if (n < 1) n = 1;
    n_v = n;
    rounds = n >= 200000000L ? 1 : 200000000L / n;    /* aim at 2e8 iterations per timing ... */
    if (rounds > 1000) rounds = 1000;                 /* ... but at most 1000 parallel regions */
    omp_set_dynamic(0);
    omp_warmup();

    double tick = omp_get_wtick(), ts, tp, rs = 0, rp = 0;
    BEST_TIME(ts, rs = run_serial());
    printf("P008: pi by the midpoint rule, N = %ld, pi = %.12f (error %.1e)\n", n, rs, fabs(rs - PI));
    printf("each timing = %ld round(s) of N iterations\n", rounds);
    printf("timer resolution omp_get_wtick() = %.1e s; serial time = %.4f s = %.0f ticks%s\n\n",
           tick, ts, ts / tick, ts / tick < 1000 ? "  (short: timings are noisy)" : "");

    printf("%8s %12s %8s %8s %12s %6s\n", "threads", "time(s)", "speedup", "effic.", "Karp-Flatt e", "check");
    int all_ok = ts > 0;
    double e_last = 0.0;
    for (int p = 1; p <= 8; p++) {
        omp_set_num_threads(p);
        BEST_TIME(tp, rp = run_parallel());
        double S = ts / tp, E = S / p;
        int ok = tp > 0 && close1(rs, rp, 1e-9);
        all_ok &= ok;
        if (p == 1) {
            printf("%8d %12.6f %8.2f %8.2f %12s %6s\n", p, tp, S, E, "-", ok_str(ok));
        } else {
            double e = (1.0 / S - 1.0 / p) / (1.0 - 1.0 / p);    /* Karp-Flatt */
            printf("%8d %12.6f %8.2f %8.2f %12.4f %6s\n", p, tp, S, E, e, ok_str(ok));
            e_last = e;
        }
    }

    /* Use the serial fraction measured at the largest p for the prediction. e <= 0
     * (superlinear / timing noise): no measurable serial part. e >= 1: the parallel
     * code is no faster than serial (N too small, overhead dominates). Clamp to [0,1]. */
    double f = e_last;
    const char *why = "";
    if (f < 1e-6) { f = 1e-6; why = "  (<= 0: no measurable serial part; using 1e-6)"; }
    if (f > 1.0)  { f = 1.0;  why = "  (>= 1: parallel is not faster, overhead dominates; using 1)"; }
    printf("\nSerial fraction from p = 8: e = %.4f%s\n", e_last, why);
    double s16 = amdahl(f, 16), s64 = amdahl(f, 64), smax = 1.0 / f;
    printf("Amdahl prediction: S(16) = %.2f, S(64) = %.2f, limit 1/e = %.2f\n", s16, s64, smax);
    int pred_ok = s16 <= smax && s64 <= smax && s16 <= 16 && s64 <= 64 && s16 <= s64;
    all_ok &= pred_ok;
    printf("checks: timings positive, results match serial, predictions <= 1/e: %s\n", ok_str(all_ok));
    return 0;
}

/* Notes:
 * - Amdahl: if a fraction f of the run time is serial, S(p) = 1/(f + (1-f)/p) and
 *   S < 1/f however many threads you use (f = 5% -> at most 20x).
 * - Karp-Flatt e = (1/S - 1/p)/(1 - 1/p) solves Amdahl for f from a MEASURED S.
 *   e constant as p grows: a real serial part. e growing with p: parallel overhead
 *   (fork/join, barriers, load imbalance, memory bandwidth) is the limit.
 * - Timing: use omp_get_wtime() (wall clock), never clock() (CPU time summed over
 *   threads). Measure something many ticks long and take the best of several runs.
 * - Speedup is relative to the best SERIAL program, not the parallel one on 1 thread
 *   (that one includes the OpenMP overhead).
 * - On CPUs with performance + efficiency cores (or hyperthreads, or a busy machine)
 *   speedup is uneven beyond the number of fast cores; static scheduling makes the
 *   fast threads wait for the slow ones.
 */

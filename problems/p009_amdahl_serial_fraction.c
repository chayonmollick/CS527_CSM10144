/* P009: Amdahl's law measured: a program with a tunable serial fraction
 * Problem: Write a program whose work consists of N equal items; a fraction f of the
 *   items must be done by one thread (serial part), the rest in parallel. For
 *   f = 0, 0.05, 0.2, 0.5 measure the speedup on 1, 2, 4, 8 threads and print it next
 *   to Amdahl's prediction 1 / (f + (1 - f)/p). Check that every run computes the same
 *   result as the serial program.
 * Solution: items 0 .. f*N-1 are summed by the initial thread before the parallel
 *   region (the other threads idle, as in Amdahl's model); items f*N .. N-1 are summed
 *   with parallel for reduction(+:sum). The item is an integer hash (unsigned 64-bit
 *   arithmetic wraps, so addition is exactly associative): results must be IDENTICAL.
 * Source: Amdahl's law from the lectures (lab extension).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p009_amdahl_serial_fraction.c -o p009 -lm
 * Run:   ./p009 [N]          (default N = 1.2e7 items)
 */
#include "common.h"

typedef unsigned long long u64;

/* One work item: a fixed amount of pure computation (no memory traffic). */
static u64 work(long i) {
    u64 x = (u64)i + 1;
    for (int r = 0; r < 16; r++) { x ^= x >> 31; x *= 0x9E3779B97F4A7C15ULL; }
    return x;
}

static u64 run_serial(long n) {
    u64 sum = 0;
    for (long i = 0; i < n; i++) sum += work(i);
    return sum;
}

/* Fraction f of the items serial, the rest parallel. */
static u64 run_fraction(long n, double f) {
    long ns = (long)(f * (double)n);         /* number of serial items */
    u64 sum = 0;
    for (long i = 0; i < ns; i++) sum += work(i);         /* serial part: one thread */
    #pragma omp parallel for reduction(+:sum)
    for (long i = ns; i < n; i++) sum += work(i);         /* parallel part */
    return sum;
}

static double amdahl(double f, int p) { return 1.0 / (f + (1.0 - f) / p); }

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 12000000L);
    if (n < 1) n = 1;
    const double F[4] = {0.0, 0.05, 0.2, 0.5};
    omp_set_dynamic(0);
    omp_warmup();

    double ts, tp;
    u64 rs = 0, rp = 0;
    BEST_TIME(ts, rs = run_serial(n));
    printf("P009: Amdahl's law with a serial fraction f, N = %ld items, serial time %.4f s\n", n, ts);
    printf("speedup = serial time / time with f serial and p threads; Amdahl = 1/(f + (1-f)/p)\n\n");
    printf("%6s %8s %12s %10s %10s %10s %6s\n", "f", "threads", "time(s)", "measured", "Amdahl", "meas/Amd", "check");
    int all_ok = 1;
    for (int j = 0; j < 4; j++) {
        for (int k = 0; k < NTHREADS; k++) {
            int p = THREADS[k];
            omp_set_num_threads(p);
            BEST_TIME(tp, rp = run_fraction(n, F[j]));
            double S = ts / tp, A = amdahl(F[j], p);
            int ok = (rp == rs);
            all_ok &= ok;
            printf("%6.2f %8d %12.6f %10.2f %10.2f %10.2f %6s\n", F[j], p, tp, S, A, S / A, ok_str(ok));
        }
        if (F[j] > 0) printf("%6s limit for p -> infinity: 1/f = %.0f\n\n", "", 1.0 / F[j]);
        else          printf("%6s limit for p -> infinity: unbounded (f = 0)\n\n", "");
    }
    printf("All results equal the serial sum: %s\n", ok_str(all_ok));
    return 0;
}

/* Notes:
 * - Amdahl: T(p) = T1 * (f + (1-f)/p), so S(p) = 1/(f + (1-f)/p) and S < 1/f. Even
 *   5% serial code caps the speedup at 20; 50% caps it at 2.
 * - Measured speedup is usually a little BELOW Amdahl: fork/join and reduction
 *   overhead, load imbalance, other processes. meas/Amd near 1 means the model fits.
 * - Gustafson's view: if the problem grows with p (serial part fixed, parallel part
 *   larger), the scaled speedup f + (1-f)*p is not capped. Amdahl = fixed problem size.
 * - Integer (wrap-around) sums are exactly associative, so the parallel result equals
 *   the serial one bit for bit; with doubles you would compare with a tolerance.
 */

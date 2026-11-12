/* 20_timing_speedup.c - time, speedup and efficiency with omp_get_wtime()
 *
 * What: omp_get_wtime() returns wall-clock seconds:  t = omp_get_wtime(); ...; t = omp_get_wtime() - t;
 *         speedup    S(p) = T(serial) / T(p threads)        efficiency E(p) = S(p) / p
 *         Amdahl:    a serial fraction f limits S(p) <= 1 / (f + (1 - f) / p) < 1 / f
 *         Karp-Flatt (serial fraction measured from S): e = (1/S - 1/p) / (1 - 1/p)
 * When: every lab report asks for this table. Time only the computation (not the set-up
 *       or the printing), keep the best of a few runs, and say how many cores the PC has
 *       (nproc): more threads than cores gives no further speedup. The thread counts
 *       are set in the code (num_threads), so OMP_NUM_THREADS does not matter here.
 * Expected output (the times VARY; S grows towards the number of cores, E stays <= ~1;
 * these are from a 10-core Apple M4):
 *   pi = 3.141592653590 by the midpoint rule with 50000000 rectangles
 *   threads   time (s)   speedup   efficiency   serial fraction
 *   serial    0.0412
 *         1    0.0406      1.01       1.01
 *         2    0.0208      1.98       0.99          0.011
 *         4    0.0106      3.89       0.97          0.009
 *         8    0.0056      7.33       0.92          0.013
 *   every thread count gives the serial pi: OK
 */
#include <math.h>
#include <omp.h>
#include <stdio.h>

#define N 50000000L
#define RUNS 3

static double pi_parallel(int p) {          /* pi = integral of 4 / (1 + x^2) over [0, 1] */
    double h = 1.0 / N, s = 0.0;
    #pragma omp parallel for num_threads(p) reduction(+:s)
    for (long i = 0; i < N; i++) {
        double x = (i + 0.5) * h;
        s += 4.0 / (1.0 + x * x);
    }
    return s * h;
}

int main(void) {
    double h = 1.0 / N, t_serial = 1e30, pi_serial = 0.0;
    for (int r = 0; r < RUNS; r++) {        /* serial: no OpenMP at all */
        double t = omp_get_wtime(), s = 0.0;
        for (long i = 0; i < N; i++) { double x = (i + 0.5) * h; s += 4.0 / (1.0 + x * x); }
        t = omp_get_wtime() - t;
        pi_serial = s * h;
        if (t < t_serial) t_serial = t;
    }
    printf("pi = %.12f by the midpoint rule with %ld rectangles\n", pi_serial, N);
    printf("threads   time (s)   speedup   efficiency   serial fraction\n");
    printf("serial    %.4f\n", t_serial);

    int ok = 1, threads[4] = {1, 2, 4, 8};
    for (int k = 0; k < 4; k++) {
        int p = threads[k];
        double best = 1e30, pi = 0.0;
        for (int r = 0; r < RUNS; r++) {
            double t = omp_get_wtime();
            pi = pi_parallel(p);
            t = omp_get_wtime() - t;
            if (t < best) best = t;
        }
        double S = t_serial / best, E = S / p;
        printf("%7d    %.4f     %5.2f      %5.2f", p, best, S, E);
        if (p > 1) printf("         %6.3f", (1.0 / S - 1.0 / p) / (1.0 - 1.0 / p));
        printf("\n");
        ok = ok && fabs(pi - pi_serial) < 1e-9;
    }
    printf("every thread count gives the serial pi: %s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

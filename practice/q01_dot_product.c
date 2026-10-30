/* CS527 Computer Systems — OpenMP practice
 * Q01: Compute the dot product of two double vectors in parallel. Compare
 *      reduction, atomic and critical-section approaches against serial.
 *  - serial loop
 *  - parallel for reduction(+:sum)              (the right way)
 *  - parallel for + atomic on every iteration   (correct but very slow)
 *  - per-thread local sum, combined once with critical (correct and fast)
 * Usage: ./q01_dot_product [N]   (default N = 1e6 and 1e7)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static double dot_serial(const double *a, const double *b, long n) {
    double sum = 0.0;
    for (long i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

/* Each thread gets a private sum (init 0); OpenMP adds them at the end. */
static double dot_reduction(const double *a, const double *b, long n) {
    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

/* Correct but slow: every iteration does an atomic read-modify-write on the
 * same shared variable, so threads fight over one cache line. */
static double dot_atomic(const double *a, const double *b, long n) {
    double sum = 0.0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp atomic
        sum += a[i] * b[i];
    }
    return sum;
}

/* Manual reduction: private partial sum, one critical update per thread. */
static double dot_local_critical(const double *a, const double *b, long n) {
    double sum = 0.0;
    #pragma omp parallel
    {
        double local = 0.0;
        #pragma omp for
        for (long i = 0; i < n; i++) local += a[i] * b[i];
        #pragma omp critical
        sum += local;
    }
    return sum;
}

/* Floating-point sums differ slightly when the order of additions changes. */
static int close_enough(double x, double ref) {
    return fabs(x - ref) <= 1e-9 * (1.0 + fabs(ref));
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static void run_size(long n) {
    double *a = malloc((size_t)n * sizeof(double));
    double *b = malloc((size_t)n * sizeof(double));
    if (!a || !b) { printf("N=%ld skipped (alloc failed)\n", n); free(a); free(b); return; }
    for (long i = 0; i < n; i++) {
        a[i] = (double)(i % 100) * 0.01;          /* 0.00 .. 0.99 */
        b[i] = (double)((i % 7) + 1) * 0.5;       /* 0.5 .. 3.5   */
    }

    dot_reduction(a, b, n);                       /* untimed warm-up: threads, caches, clock */
    double t0 = omp_get_wtime();
    double ref = dot_serial(a, b, n);
    double ts = omp_get_wtime() - t0;
    printf("\nN = %ld   serial dot = %.6f   serial time = %.6f s\n", n, ref, ts);

    const int threads[] = {1, 2, 4, 8};
    printf("%8s %12s %8s %12s %8s %12s %8s %6s\n", "threads", "reduction(s)", "speedup",
           "atomic(s)", "speedup", "local+crit(s)", "speedup", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double r0 = omp_get_wtime(); double dr = dot_reduction(a, b, n);      double tr = omp_get_wtime() - r0;
        double a0 = omp_get_wtime(); double da = dot_atomic(a, b, n);         double ta = omp_get_wtime() - a0;
        double c0 = omp_get_wtime(); double dc = dot_local_critical(a, b, n); double tc = omp_get_wtime() - c0;
        int ok = close_enough(dr, ref) && close_enough(da, ref) && close_enough(dc, ref);
        printf("%8d %12.6f %8.2f %12.6f %8.2f %12.6f %8.2f %6s\n", threads[ti],
               tr, speedup(ts, tr), ta, speedup(ts, ta), tc, speedup(ts, tc), ok ? "OK" : "WRONG");
    }
    free(a); free(b);
}

int main(int argc, char **argv) {
    printf("Q01: Dot product  (speedup = T_serial / T_parallel)\n");
    if (argc > 1) {
        long n = (long)atof(argv[1]);                 /* accepts 1e7 style too */
        if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
        run_size(n);
    } else {
        const long sizes[] = {1000000L, 10000000L};
        for (int si = 0; si < 2; si++) run_size(sizes[si]);
    }
    return 0;
}

/* Viva notes:
 * - reduction(+:sum): each thread accumulates into its own private copy
 *   (initialised to 0, the identity of +); copies are combined into the shared
 *   sum once at the end -> no data race and no per-iteration synchronisation.
 * - Without reduction/atomic/critical, `sum += a[i]*b[i]` is a read-modify-write
 *   race: two threads read the same old value and one update is lost.
 * - atomic is correct but serialises n updates on one cache line that bounces
 *   between cores, so it gets SLOWER as threads increase. critical per
 *   iteration would be even worse (a lock, not a hardware atomic).
 * - local sum + one critical per thread = what reduction does internally:
 *   only nthreads synchronised updates instead of n.
 * - Speedup is sub-linear: a dot product does 2 flops per 16 bytes loaded, so
 *   it is memory-bandwidth bound; extra threads stop helping once the memory
 *   bus is saturated. Small N is dominated by fork/join overhead.
 * - Results differ from serial in the last digits because FP addition is not
 *   associative (different summation order), hence the tolerance check.
 */

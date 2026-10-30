/* CS527 Computer Systems — OpenMP practice
 * Q02: Compute pi = integral_0^1 4/(1+x^2) dx with the midpoint rule over
 *      num_steps rectangles, serially and in parallel; compare the times and
 *      the error |pi - PI| of each method.
 *  - serial loop
 *  - parallel for reduction(+:sum)
 *  - SPMD: shared partial[nthreads] array indexed by thread id (FALSE SHARING)
 *  - SPMD padded: each thread's slot on its own 64-byte cache line
 * Usage: ./q02_pi_integration [num_steps]   (default 1e8)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI_REF      3.14159265358979323846
#define MAX_THREADS 64
#define CACHE_LINE  64                              /* bytes; 128 on Apple M-series */
#define PAD         (CACHE_LINE / sizeof(double))   /* doubles per cache line = 8 */

static double pi_serial(long num_steps) {
    double step = 1.0 / (double)num_steps, sum = 0.0;
    for (long i = 0; i < num_steps; i++) {
        double x = (i + 0.5) * step;                /* midpoint of rectangle i */
        sum += 4.0 / (1.0 + x * x);
    }
    return step * sum;
}

/* x is declared inside the loop -> private; sum is a reduction variable. */
static double pi_reduction(long num_steps) {
    double step = 1.0 / (double)num_steps, sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < num_steps; i++) {
        double x = (i + 0.5) * step;
        sum += 4.0 / (1.0 + x * x);
    }
    return step * sum;
}

/* SPMD style: each thread takes iterations id, id+nt, id+2nt, ... and adds
 * into partial[id]. Correct (no two threads write the same element) but the
 * 8 doubles share one cache line, which ping-pongs between cores.
 * volatile forces a real store every iteration, as the classic -O0 version
 * does; without it gcc -O2 may keep partial[id] in a register and hide the
 * effect. */
static double pi_spmd_shared(long num_steps) {
    double step = 1.0 / (double)num_steps;
    volatile double partial[MAX_THREADS];
    int nthreads = 1;
    #pragma omp parallel
    {
        int id = omp_get_thread_num(), nt = omp_get_num_threads();
        if (id == 0) nthreads = nt;                 /* we may get fewer threads */
        partial[id] = 0.0;
        for (long i = id; i < num_steps; i += nt) {
            double x = (i + 0.5) * step;
            partial[id] += 4.0 / (1.0 + x * x);
        }
    }
    double sum = 0.0;
    for (int t = 0; t < nthreads; t++) sum += partial[t];
    return step * sum;
}

/* Same algorithm, but partial[id][0] is CACHE_LINE bytes away from the next
 * thread's slot, so every thread writes to its own cache line. */
static double pi_spmd_padded(long num_steps) {
    double step = 1.0 / (double)num_steps;
    volatile double partial[MAX_THREADS][PAD];
    int nthreads = 1;
    #pragma omp parallel
    {
        int id = omp_get_thread_num(), nt = omp_get_num_threads();
        if (id == 0) nthreads = nt;
        partial[id][0] = 0.0;
        for (long i = id; i < num_steps; i += nt) {
            double x = (i + 0.5) * step;
            partial[id][0] += 4.0 / (1.0 + x * x);
        }
    }
    double sum = 0.0;
    for (int t = 0; t < nthreads; t++) sum += partial[t][0];
    return step * sum;
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static int pi_ok(double pi) { return fabs(pi - PI_REF) < 1e-6; }

int main(int argc, char **argv) {
    long num_steps = 100000000L;                    /* 1e8 */
    if (argc > 1) num_steps = (long)atof(argv[1]);  /* accepts 1e8 style too */
    if (num_steps <= 0) { fprintf(stderr, "usage: %s [num_steps]\n", argv[0]); return 1; }

    printf("Q02: pi by midpoint-rule integration  (num_steps = %ld)\n", num_steps);
    pi_reduction(num_steps / 4);                    /* untimed warm-up: thread pool + CPU clock */
    double t0 = omp_get_wtime();
    double ref = pi_serial(num_steps);
    double ts = omp_get_wtime() - t0;
    printf("serial: pi = %.15f  |pi-PI| = %.3e  time = %.4f s\n\n", ref, fabs(ref - PI_REF), ts);

    const int threads[] = {1, 2, 4, 8};
    printf("%8s %10s %7s %12s %7s %12s %7s %6s\n", "threads", "reduce(s)", "spdup",
           "falseshr(s)", "spdup", "padded(s)", "spdup", "check");
    double pr = 0, pf = 0, pp = 0;
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double r0 = omp_get_wtime(); pr = pi_reduction(num_steps);   double tr = omp_get_wtime() - r0;
        double f0 = omp_get_wtime(); pf = pi_spmd_shared(num_steps); double tf = omp_get_wtime() - f0;
        double p0 = omp_get_wtime(); pp = pi_spmd_padded(num_steps); double tp = omp_get_wtime() - p0;
        int ok = pi_ok(pr) && pi_ok(pf) && pi_ok(pp);
        printf("%8d %10.4f %7.2f %12.4f %7.2f %12.4f %7.2f %6s\n", threads[ti],
               tr, speedup(ts, tr), tf, speedup(ts, tf), tp, speedup(ts, tp), ok ? "OK" : "WRONG");
    }

    printf("\nresults with %d threads:\n", threads[3]);
    printf("  %-22s pi = %.15f  |pi-PI| = %.3e  %s\n", "serial", ref, fabs(ref - PI_REF), pi_ok(ref) ? "OK" : "WRONG");
    printf("  %-22s pi = %.15f  |pi-PI| = %.3e  %s\n", "reduction", pr, fabs(pr - PI_REF), pi_ok(pr) ? "OK" : "WRONG");
    printf("  %-22s pi = %.15f  |pi-PI| = %.3e  %s\n", "spmd (false sharing)", pf, fabs(pf - PI_REF), pi_ok(pf) ? "OK" : "WRONG");
    printf("  %-22s pi = %.15f  |pi-PI| = %.3e  %s\n", "spmd (padded)", pp, fabs(pp - PI_REF), pi_ok(pp) ? "OK" : "WRONG");
    return 0;
}

/* Viva notes:
 * - reduction(+:sum) is race-free: each thread sums into a private copy and
 *   the copies are added once at the end. x must be private (declared inside
 *   the loop); a shared x would be a race giving a wrong pi.
 * - False sharing: partial[0..7] are different variables but sit in the SAME
 *   cache line. Each write invalidates the line in the other cores' caches
 *   (MESI coherence), so the line bounces between cores and the "parallel"
 *   loop can be slower than serial. Padding each slot to its own line (or a
 *   private local sum written once at the end) removes it.
 * - Compare false-sharing vs padded columns with each other: both do a
 *   load+store per step (volatile), so even at 1 thread they are slower than
 *   the register-only serial loop. Without volatile, gcc -O2 keeps
 *   partial[id] in a register and the false sharing vanishes (the textbook
 *   demo uses -O0, which stores every time). On x86 every store must own the
 *   line, so the gap is large; on ARM/Apple M stores coalesce, gap is small.
 * - The integrand is compute-bound (a division per step, no array reads), so
 *   the reduction version scales nearly linearly with physical cores.
 * - Results differ from serial in ~1e-12 because FP addition order changes;
 *   the midpoint-rule error itself is O(h^2), negligible at 1e8 steps.
 * - Alternatives: #pragma omp atomic / critical on one shared sum after a
 *   private local loop; or schedule(static) block split instead of cyclic.
 */

/* CS527 Computer Systems — OpenMP practice
 * Q03: Estimate pi by Monte Carlo: throw N random points into the unit
 *      square and count those inside the quarter circle (pi ~ 4*hits/N).
 *  - serial loop with one RNG
 *  - parallel: per-thread RNG state seeded from the thread id, reduction(+:hits)
 * Usage: ./q03_monte_carlo_pi [N]   (default N = 1e7 and 1e8)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI_REF 3.14159265358979323846

/* Different, well-spread starting state for each thread id. */
static unsigned long long seed_for(int tid) {
    return 0x9E3779B97F4A7C15ULL * (unsigned long long)(tid + 1) + 12345ULL;
}

/* 64-bit LCG (Knuth's MMIX constants); the state lives in the caller, so
 * each thread owns its own generator -> thread-safe, no locks. */
static double rand01(unsigned long long *state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(*state >> 11) * (1.0 / 9007199254740992.0);  /* top 53 bits -> [0,1) */
}

static long mc_serial(long n) {
    unsigned long long s = seed_for(0);
    long hits = 0;
    for (long i = 0; i < n; i++) {
        double x = rand01(&s), y = rand01(&s);
        if (x * x + y * y <= 1.0) hits++;
    }
    return hits;
}

/* hits is a reduction variable of the parallel region: inside, every thread
 * increments its private copy; copies are summed at the end of the region. */
static long mc_parallel(long n) {
    long hits = 0;
    #pragma omp parallel reduction(+:hits)
    {
        unsigned long long s = seed_for(omp_get_thread_num());   /* private RNG */
        #pragma omp for schedule(static)
        for (long i = 0; i < n; i++) {
            double x = rand01(&s), y = rand01(&s);
            if (x * x + y * y <= 1.0) hits++;
        }
    }
    return hits;
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static void run_size(long n) {
    /* std. dev. of the estimate is about 1.64/sqrt(N); 0.01 is safe for N >= 1e6 */
    double tol = (n >= 1000000L) ? 0.01 : 10.0 / sqrt((double)n);

    double t0 = omp_get_wtime();
    long ref_hits = mc_serial(n);
    double ts = omp_get_wtime() - t0;
    double est_s = 4.0 * (double)ref_hits / (double)n;
    printf("\nN = %ld  serial estimate = %.6f  |err| = %.2e  time = %.4f s  (tolerance %.4f)\n",
           n, est_s, fabs(est_s - PI_REF), ts, tol);

    const int threads[] = {1, 2, 4, 8};
    printf("%8s %12s %8s %12s %10s %6s\n", "threads", "parallel(s)", "speedup", "estimate", "|err|", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime();
        long hits = mc_parallel(n);
        double tp = omp_get_wtime() - p0;
        double est = 4.0 * (double)hits / (double)n;
        int ok = fabs(est - PI_REF) < tol;
        if (threads[ti] == 1) ok = ok && (hits == ref_hits);   /* same seed -> identical */
        printf("%8d %12.4f %8.2f %12.6f %10.2e %6s\n", threads[ti], tp, speedup(ts, tp), est,
               fabs(est - PI_REF), ok ? "OK" : "WRONG");
    }
}

int main(int argc, char **argv) {
    printf("Q03: Monte Carlo pi  (speedup = T_serial / T_parallel)\n");
    mc_parallel(10000000L);                         /* untimed warm-up: thread pool + CPU clock */
    if (argc > 1) {
        long n = (long)atof(argv[1]);               /* accepts 1e8 style too */
        if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
        run_size(n);
    } else {
        const long sizes[] = {10000000L, 100000000L};
        for (int si = 0; si < 2; si++) run_size(sizes[si]);
    }
    return 0;
}

/* Viva notes:
 * - Why not rand(): it keeps ONE hidden global state. Called from many threads
 *   it is a data race (corrupted/repeated numbers, UB), or, where libc locks
 *   it (glibc), every call serialises on that lock and the "parallel" code is
 *   slower than serial. rand_r(&seed) or a per-thread generator like the LCG
 *   here fixes both: state is private, no sharing, no locks.
 * - Seeds must differ per thread (here from the thread id); with the same
 *   seed every thread would generate the same points -> no new information.
 * - The estimate changes with thread count because each thread count splits
 *   the N samples among different random streams, so a different set of
 *   points is drawn. It is still reproducible for a fixed thread count
 *   (schedule(static) gives each thread a fixed iteration range), and with
 *   1 thread it equals the serial result exactly (same seed).
 * - Error shrinks only as 1/sqrt(N) (statistics), so the check is a tolerance
 *   (|est-PI| < 0.01), not equality. More threads do not improve accuracy.
 * - Speedup is close to linear: work is pure computation in registers, no
 *   shared memory traffic; only one reduction at the end.
 */

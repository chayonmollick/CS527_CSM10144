/* CS527 Computer Systems — OpenMP practice
 * Q07: Count the primes <= N by trial division. The cost of testing i grows
 *      with i, so the work per iteration is uneven: compare loop schedules.
 *  - serial loop
 *  - parallel for reduction(+:count) with
 *      schedule(static)          big contiguous blocks -> last thread gets the
 *                                most expensive numbers (load imbalance)
 *      schedule(static,1)        round-robin, cheap and well balanced here
 *      schedule(dynamic,CHUNK)   threads grab CHUNK iterations when free
 *      schedule(guided)          dynamic with shrinking chunk sizes
 * Usage: ./q07_count_primes [N]   (default N = 2e6)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define CHUNK 1000

/* Trial division by odd d up to sqrt(x): cost ~ sqrt(x) for primes. */
static int is_prime(long x) {
    if (x < 2) return 0;
    if (x % 2 == 0) return x == 2;
    for (long d = 3; d * d <= x; d += 2)
        if (x % d == 0) return 0;
    return 1;
}

static long count_serial(long n) {
    long count = 0;
    for (long i = 2; i <= n; i++) count += is_prime(i);
    return count;
}

static long count_static(long n) {
    long count = 0;
    #pragma omp parallel for reduction(+:count) schedule(static)
    for (long i = 2; i <= n; i++) count += is_prime(i);
    return count;
}

static long count_static1(long n) {
    long count = 0;
    #pragma omp parallel for reduction(+:count) schedule(static, 1)
    for (long i = 2; i <= n; i++) count += is_prime(i);
    return count;
}

static long count_dynamic(long n) {
    long count = 0;
    #pragma omp parallel for reduction(+:count) schedule(dynamic, CHUNK)
    for (long i = 2; i <= n; i++) count += is_prime(i);
    return count;
}

static long count_guided(long n) {
    long count = 0;
    #pragma omp parallel for reduction(+:count) schedule(guided)
    for (long i = 2; i <= n; i++) count += is_prime(i);
    return count;
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

int main(int argc, char **argv) {
    long n = 2000000L;                              /* 2e6 */
    if (argc > 1) n = (long)atof(argv[1]);          /* accepts 2e6 style too */
    if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }

    printf("Q07: Count primes <= %ld by trial division  (speedup = T_serial / T_parallel)\n", n);
    count_dynamic(n / 4);                           /* untimed warm-up */
    double t0 = omp_get_wtime();
    long ref = count_serial(n);
    double ts = omp_get_wtime() - t0;
    printf("serial: count = %ld   time = %.4f s\n\n", ref, ts);

    const int threads[] = {1, 2, 4, 8};
    printf("%8s %10s %6s %10s %6s %11s %6s %10s %6s %6s\n", "threads",
           "static(s)", "sp", "static1(s)", "sp", "dyn1000(s)", "sp", "guided(s)", "sp", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double a0 = omp_get_wtime(); long ca = count_static(n);  double ta = omp_get_wtime() - a0;
        double b0 = omp_get_wtime(); long cb = count_static1(n); double tb = omp_get_wtime() - b0;
        double c0 = omp_get_wtime(); long cc = count_dynamic(n); double tc = omp_get_wtime() - c0;
        double d0 = omp_get_wtime(); long cd = count_guided(n);  double td = omp_get_wtime() - d0;
        int ok = (ca == ref) && (cb == ref) && (cc == ref) && (cd == ref);
        printf("%8d %10.4f %6.2f %10.4f %6.2f %11.4f %6.2f %10.4f %6.2f %6s\n", threads[ti],
               ta, speedup(ts, ta), tb, speedup(ts, tb), tc, speedup(ts, tc), td, speedup(ts, td),
               ok ? "OK" : "WRONG");
    }
    return 0;
}

/* Viva notes:
 * - Race-free: is_prime() only reads its argument; the shared counter is a
 *   reduction(+:count), so each thread counts privately and the totals are
 *   added once. Plain `count++` from all threads would lose updates.
 * - schedule(static) gives thread t the t-th contiguous block; numbers near N
 *   are the costliest (primes need ~sqrt(N)/2 divisions), so the last thread
 *   finishes late while others idle -> worst speedup (load imbalance).
 * - static,1 deals iterations round-robin, so every thread gets a mix of
 *   cheap and expensive numbers; dynamic/guided balance at run time at the
 *   cost of some scheduling overhead (a shared counter per chunk grab).
 * - dynamic with chunk 1 would be correct but slow (one grab per number);
 *   a chunk of ~1000 keeps overhead low and balance good. guided starts with
 *   big chunks (n/threads) and shrinks them, so the expensive tail is split.
 * - To try schedules without recompiling: schedule(runtime) and set
 *   OMP_SCHEDULE="dynamic,1000" (or call omp_set_schedule()).
 */

/* P018: Parallel random numbers that do not depend on the thread count
 * Problem: Fill an array with N pseudo-random numbers in [0,1) in parallel so that the
 *   array is identical for 1, 2, 4 and 8 threads. Contrast with per-thread seeded
 *   generators and explain why rand() must not be used. Use the generator to estimate
 *   pi by Monte Carlo with M points. Check that the arrays are bitwise identical for all
 *   thread counts and that the estimate is within 0.01 of pi.
 * Solution: a counter-based generator: x_i = hash(seed, i) (the splitmix64 mixer applied
 *   to seed + (i+1)*golden). Element i depends only on i, never on which thread computes
 *   it or what was computed before, so any schedule and any thread count give the same
 *   bits, and there is no shared generator state (no race). Per-thread LCGs (seed + id,
 *   one block each) are also race-free and fast, but the numbers change with the thread
 *   count. Pi: point j = (x_2j, x_2j+1), count points with x^2 + y^2 <= 1 using
 *   reduction(+:inside); the integer count, hence the estimate, is identical for all p.
 * Source: lab extension (Monte Carlo pi, random numbers in parallel).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p018_parallel_random.c -o p018 -lm
 * Run:   ./p018 [N] [M]      (defaults N = 1e7 numbers, M = 2e7 points)
 */
#include "common.h"

typedef unsigned long long u64;
#define GOLDEN 0x9E3779B97F4A7C15ULL

/* splitmix64 finaliser: scrambles all 64 bits */
static inline u64 mix64(u64 z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* The i-th random number of stream `seed`, uniform in [0,1): a pure function of (seed, i). */
static inline double rand_at(u64 seed, long i) {
    u64 z = mix64(seed + ((u64)i + 1) * GOLDEN);
    return (double)(z >> 11) * (1.0 / 9007199254740992.0);    /* top 53 bits / 2^53 */
}

static void fill_serial(double *x, long n, u64 seed) {
    for (long i = 0; i < n; i++) x[i] = rand_at(seed, i);
}

static void fill_parallel(double *x, long n, u64 seed) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) x[i] = rand_at(seed, i);     /* any schedule gives the same x */
}

/* Per-thread LCG: thread id seeds its own state and fills one contiguous block.
 * Race-free and fast, but block boundaries and seeds depend on the thread count. */
static void fill_per_thread_lcg(double *x, long n, u64 seed) {
    #pragma omp parallel
    {
        int id = omp_get_thread_num(), nt = omp_get_num_threads();
        unsigned long long state = mix64(seed + (u64)(id + 1) * GOLDEN);
        long lo = n * id / nt, hi = n * (id + 1) / nt;
        for (long i = lo; i < hi; i++) x[i] = (double)lcg_next(&state) / 2147483648.0;
    }
}

/* Monte Carlo pi: point j = (x_{2j}, x_{2j+1}) of the counter-based stream. */
static long pi_count(long m, u64 seed) {
    long inside = 0;
    #pragma omp parallel for reduction(+:inside)
    for (long j = 0; j < m; j++) {
        double x = rand_at(seed, 2 * j), y = rand_at(seed, 2 * j + 1);
        if (x * x + y * y <= 1.0) inside++;
    }
    return inside;
}

/* Same with per-thread LCGs: a valid estimate, but it changes with the thread count. */
static long pi_count_lcg(long m, u64 seed) {
    long inside = 0;
    #pragma omp parallel reduction(+:inside)
    {
        int id = omp_get_thread_num(), nt = omp_get_num_threads();
        unsigned long long state = mix64(seed + (u64)(id + 1) * GOLDEN);
        long lo = m * id / nt, hi = m * (id + 1) / nt;
        for (long j = lo; j < hi; j++) {
            double x = (double)lcg_next(&state) / 2147483648.0;
            double y = (double)lcg_next(&state) / 2147483648.0;
            if (x * x + y * y <= 1.0) inside++;
        }
    }
    return inside;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    long m = arg_long(argc, argv, 2, 20000000L);
    if (n < 1) n = 1;
    if (m < 1) m = 1;
    const u64 seed = 2026;
    double *ref = xmalloc((size_t)n * sizeof(double));
    double *x = xmalloc((size_t)n * sizeof(double));
    double *lcg1 = xmalloc((size_t)n * sizeof(double));
    omp_set_dynamic(0);
    omp_warmup();

    double tfs, tfp, tps, tpp;
    long in_s = 0, in_p = 0;
    BEST_TIME(tfs, fill_serial(ref, n, seed));
    omp_set_num_threads(1);
    BEST_TIME(tps, in_s = pi_count(m, seed));            /* 1-thread pi = serial reference */
    fill_per_thread_lcg(lcg1, n, seed);                  /* per-thread LCG with 1 thread */
    double pi_est = 4.0 * (double)in_s / (double)m;

    printf("P018: counter-based random numbers, N = %ld numbers, M = %ld pi points\n", n, m);
    printf("serial fill %.4f s, pi on 1 thread %.4f s, pi estimate %.6f (error %.1e)\n\n",
           tfs, tps, pi_est, fabs(pi_est - PI));
    printf("%8s %10s %8s %10s %8s %16s %18s %14s %6s\n", "threads", "fill(s)", "speedup", "pi(s)",
           "speedup", "array == serial", "pi count == p=1", "per-thread LCG", "check");
    int all_ok = 1;
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(x, 0, (size_t)n * sizeof(double));
        BEST_TIME(tfp, fill_parallel(x, n, seed));
        BEST_TIME(tpp, in_p = pi_count(m, seed));
        int same = memcmp(x, ref, (size_t)n * sizeof(double)) == 0;   /* bitwise identical */
        int same_pi = (in_p == in_s);
        fill_per_thread_lcg(x, n, seed);
        int lcg_same = memcmp(x, lcg1, (size_t)n * sizeof(double)) == 0;
        int ok = same && same_pi;
        all_ok &= ok;
        printf("%8d %10.4f %8.2f %10.4f %8.2f %16s %18s %14s %6s\n", THREADS[k], tfp, tfs / tfp,
               tpp, tps / tpp, same ? "identical" : "DIFFERENT", same_pi ? "identical" : "DIFFERENT",
               lcg_same ? "same as p=1" : "differs", ok_str(ok));
    }

    printf("\nper-thread LCG pi estimates (valid, but not reproducible across thread counts):");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        printf("  p=%d: %.6f", THREADS[k], 4.0 * (double)pi_count_lcg(m, seed) / (double)m);
    }
    int pi_ok = fabs(pi_est - PI) < 0.01;
    if (m < 1000000) { pi_ok = 1; printf("\n(M < 1e6: too few points for a 0.01 accuracy check, skipped)"); }
    all_ok &= pi_ok;
    printf("\npi estimate %.6f within 0.01 of pi: %s\n", pi_est, ok_str(pi_ok));
    printf("Counter-based arrays and pi counts identical for every thread count, pi accurate: %s\n",
           ok_str(all_ok));
    free(ref); free(x); free(lcg1);
    return 0;
}

/* Notes:
 * - rand() keeps ONE hidden global state: calling it from several threads is a data
 *   race (glibc adds a lock, so it is also slow), and which thread gets which number
 *   depends on timing: results are not reproducible. rand_r is POSIX, not C99/OpenMP.
 * - Counter-based (x_i = hash(seed, i)): no state at all, any thread computes any i,
 *   bitwise reproducible for every thread count and schedule. Cost: one hash per number.
 * - Per-thread generators (seed + thread id): no race, fast, but the stream changes
 *   when the number of threads changes; streams with nearby seeds must be decorrelated
 *   (here the seed itself is hashed).
 * - Monte Carlo pi: P(x^2 + y^2 <= 1) = pi/4 for (x, y) uniform in the unit square;
 *   the error falls like 1/sqrt(M) (about 1.6/sqrt(M) for one standard deviation).
 * - Use a long/integer reduction for the count: integer sums are order-independent, so
 *   the parallel count equals the serial one exactly.
 * - A fill "speedup" above 1 on ONE thread is not parallelism: gcc compiles the outlined
 *   OpenMP loop slightly differently from the serial one (and the fill is memory-bound).
 */

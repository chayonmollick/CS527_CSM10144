/* P087: Smallest factor of N with OpenMP tasks (early stop)
 * Problem: (Textbook Ex 24.1.) Find the smallest factor f >= 2 of a number N by
 *   trial division (f = 2, 3, ... up to sqrt(N); if none divides, N is prime and
 *   the answer is N), e.g. N = 2999 * 3001. Create one task per trial factor
 *   (or per block of factors) from a single thread, record the smallest factor
 *   found in a shared variable, and stop creating tasks once a factor is known.
 *   Compare with a "parallel for" + reduction(min:). Test several numbers,
 *   including a prime, and verify against the serial loop.
 * Solution: one thread (single) walks the candidates in blocks of B and makes
 *   a task per block; the other threads of the team execute the tasks. A task
 *   scans its block upward and, at its first divisor f, does
 *   "factor = min(factor, f)" inside a critical section (read-compare-write
 *   must be one atomic step: OpenMP 4.5 has no atomic min). Before creating
 *   the next block the generator reads factor (also in the critical section);
 *   if factor <= start of the block, every remaining candidate is larger, so it
 *   stops. Tasks that start after a smaller factor is known skip their block.
 *   Correct because the block holding the true smallest factor always starts
 *   below any factor found, so it is always created and scanned.
 *   The parallel for version cannot leave the loop early (no break in an omp
 *   for), so it always tests every candidate up to sqrt(N); reduction(min:best)
 *   gives each thread a private best = N and takes the minimum at the end.
 * Source: textbook Ex 24.1 (tasks).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p087_smallest_factor_tasks.c -o p087 -lm
 * Run:   ./p087 [P] [B]      (defaults P = 1e7: timing uses N = p*q with p, q the
 *                             first primes >= P; block B = 10000)
 */
#include "common.h"

/* floor(sqrt(n)), exact for 64-bit n. */
static long isqrt(long n) {
    long r = (long)sqrt((double)n);
    while (r > 0 && r * r > n) r--;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

static long factor_serial(long n) {
    for (long f = 2; f * f <= n; f++)
        if (n % f == 0) return f;
    return n;                                   /* no factor up to sqrt(n): prime */
}

static long factor_tasks(long n, long block) {
    long limit = isqrt(n), factor = n;           /* factor: shared, smallest found so far */
    #pragma omp parallel
    #pragma omp single                           /* one thread creates all tasks */
    {
        for (long lo = 2; lo <= limit; lo += block) {
            long known;
            #pragma omp critical
            known = factor;
            if (known <= lo) break;              /* smaller factor known: stop creating tasks */
            #pragma omp task firstprivate(lo) shared(factor)
            {
                long hi = lo + block - 1 < limit ? lo + block - 1 : limit, now;
                #pragma omp critical
                now = factor;
                if (now > lo)                    /* skip the block if it cannot win */
                    for (long f = lo; f <= hi; f++)
                        if (n % f == 0) {
                            #pragma omp critical
                            { if (f < factor) factor = f; }
                            break;               /* first divisor in this block */
                        }
            }
        }
    }   /* implicit barrier: all tasks have finished */
    return factor;
}

static long factor_for_min(long n) {
    long limit = isqrt(n), best = n;
    #pragma omp parallel for reduction(min:best) schedule(static)
    for (long f = 2; f <= limit; f++)
        if (n % f == 0 && f < best) best = f;
    return best;
}

static long next_prime(long x) {
    while (factor_serial(x) != x) x++;
    return x;
}

int main(int argc, char *argv[]) {
    long P = arg_long(argc, argv, 1, 10000000);
    long B = arg_long(argc, argv, 2, 10000);
    if (P < 2) P = 2;
    if (P > 1000000000) P = 1000000000;          /* keep p*q inside 64 bits */
    if (B < 1) B = 1;
    long p = next_prime(P), q = next_prime(p + 1), big = p * q;

    printf("P087: smallest factor by trial division, tasks vs parallel for\n");
    omp_warmup();
    long tests[5] = {2999L * 3001L, 1000000007L, 2L * 1000000007L, big, 2L * big};
    const char *what[5] = {"2999 * 3001", "prime", "even: factor 2", "p * q", "2 * p * q"};
    long expect[5] = {2999, 1000000007L, 2, p, 2};
    printf("%d threads; times in seconds; B = block of candidates per task\n", omp_get_max_threads());
    printf("%20s %-15s %10s %9s %9s %9s %9s %6s\n", "N", "", "factor", "serial",
           "tasks B=1", "tasks B", "for+min", "check");
    for (int t = 0; t < 5; t++) {
        long n = tests[t], fs = 0, f1 = -1, fb = 0, fm = 0;
        double t_s, t_1 = -1.0, t_b, t_m;
        BEST_TIME(t_s, fs = factor_serial(n));
        if (isqrt(n) <= 100000) BEST_TIME(t_1, f1 = factor_tasks(n, 1));   /* one task per factor */
        BEST_TIME(t_b, fb = factor_tasks(n, B));
        BEST_TIME(t_m, fm = factor_for_min(n));
        int ok = fs == expect[t] && fb == fs && fm == fs && (t_1 < 0.0 || f1 == fs);
        printf("%20ld %-15s %10ld %9.5f ", n, what[t], fs, t_s);
        if (t_1 < 0.0) printf("%9s ", "skipped");
        else           printf("%9.5f ", t_1);
        printf("%9.5f %9.5f %6s\n", t_b, t_m, ok_str(ok));
    }

    double ts, tp;
    long fs = 0, fp = 0;
    BEST_TIME(ts, fs = factor_serial(big));
    printf("\nN = %ld * %ld = %ld, smallest factor %ld\n", p, q, big, fs);
    printf("(column N below = %ld candidates tested by the serial loop)\n", fs - 1);
    printf("Tasks, blocks of %ld candidates (check: factor equal to serial)\n", B);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, fp = factor_tasks(big, B));
        table_row(fs - 1, THREADS[k], ts, tp, fp == fs);
    }
    printf("parallel for + reduction(min:) (check: factor equal to serial)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, fp = factor_for_min(big));
        table_row(fs - 1, THREADS[k], ts, tp, fp == fs);
    }
    return 0;
}

/* Notes:
 * - Tasks = units of work put in a queue by one thread (single) and run by any
 *   thread of the team; good for irregular work and early termination. The
 *   implicit barrier at the end of the parallel region waits for all tasks.
 * - "factor = min(factor, f)" is read-compare-write: it needs critical (an
 *   atomic write alone could overwrite a smaller value). Reads of factor are
 *   also done in the critical section so they never see a torn value.
 * - parallel for cannot break: for N = 2 * p * q it tests all ~sqrt(N)
 *   candidates although 2 divides N (compare the times in the first table).
 *   OpenMP 4.0 "#pragma omp cancel for" can stop it, but only when the
 *   environment variable OMP_CANCELLATION=true is set.
 * - Task granularity: one task per candidate (B = 1) costs ~1 microsecond of
 *   overhead per division; blocks of 1e3-1e4 candidates amortise it.
 * - The answer of the task version does not depend on timing: the block with
 *   the true smallest factor is always created and scanned.
 */

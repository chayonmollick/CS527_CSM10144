/* P100: Longest Collatz chain (max-location reduction)
 * Problem: The Collatz sequence of n repeats n -> n/2 (n even) or n -> 3n+1
 *   (n odd) until it reaches 1. Its length is the number of terms including n
 *   and 1 (length(13) = 10: 13 40 20 10 5 16 8 4 2 1). Find the starting number
 *   below N with the longest chain (ties -> the smaller start) using OpenMP. The
 *   work per number is very uneven. Verify against the serial result (below 1e6:
 *   837799, length 525) and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: the chain lengths are independent, so the loop over starts is a
 *   parallel for; schedule(dynamic, 1000) because chain lengths vary wildly.
 *   The answer is a (length, start) pair - a max-location, which the built-in
 *   reduction(max:) cannot do. Two ways:
 *   (a) every thread keeps its own best pair; at the end each thread merges it
 *       into the shared best inside a critical section (once per thread).
 *   (b) #pragma omp declare reduction(maxloc : Best : ...) with a combiner that
 *       keeps the better pair and an initializer {0, 0}; then
 *       reduction(maxloc:best) on the parallel for.
 *   Both use the same better() test: longer wins, equal length -> smaller start,
 *   so the answer does not depend on how the work was split.
 * Source: Project Euler 14 style load-balancing exercise (lab extension).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p100_collatz_longest.c -o p100 -lm
 * Run:   ./p100 [N]          (default N = 1e6)
 */
#include "common.h"

typedef struct { long len; long start; } Best;

/* Number of terms from n down to 1. Values exceed 2^32 for n < 1e6, so 64-bit. */
static long collatz_len(long n) {
    unsigned long long x = (unsigned long long)n;
    long len = 1;
    while (x != 1) {
        x = (x & 1) ? 3 * x + 1 : x >> 1;
        len++;
    }
    return len;
}

/* Is a strictly better than b? Longer chain, or same length and smaller start. */
static int better(Best a, Best b) {
    return a.len > b.len || (a.len == b.len && a.start < b.start);
}
static Best best_of(Best a, Best b) { return better(a, b) ? a : b; }
static Best no_best(void) { Best z = {0, 0}; return z; }   /* identity: loses to all */

/* (b) a user-defined reduction on the struct type Best. */
#pragma omp declare reduction(maxloc : Best : omp_out = best_of(omp_out, omp_in)) \
    initializer(omp_priv = no_best())

static Best longest_serial(long n) {
    Best best = no_best();
    for (long k = 1; k < n; k++) {
        Best cand = {collatz_len(k), k};
        if (better(cand, best)) best = cand;
    }
    return best;
}

/* (a) per-thread best, merged in a critical section. */
static Best longest_critical(long n) {
    Best best = no_best();
    #pragma omp parallel
    {
        Best mine = no_best();
        #pragma omp for schedule(dynamic, 1000) nowait
        for (long k = 1; k < n; k++) {
            Best cand = {collatz_len(k), k};
            if (better(cand, mine)) mine = cand;
        }
        #pragma omp critical
        if (better(mine, best)) best = mine;
    }
    return best;
}

/* (b) declare reduction. */
static Best longest_reduction(long n) {
    Best best = no_best();
    #pragma omp parallel for schedule(dynamic, 1000) reduction(maxloc:best)
    for (long k = 1; k < n; k++) {
        Best cand = {collatz_len(k), k};
        if (better(cand, best)) best = cand;
    }
    return best;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000000);
    omp_warmup();

    Best rs = no_best(), rp = no_best();
    double ts, tp;
    BEST_TIME(ts, rs = longest_serial(n));
    printf("P100: longest Collatz chain below N = %ld: start %ld, length %ld",
           n, rs.start, rs.len);
    /* known answers below 10^e, e = 1..7: {start, length} */
    const long known[8][2] = {{0, 0}, {9, 20}, {97, 119}, {871, 179}, {6171, 262},
                              {77031, 351}, {837799, 525}, {8400511, 686}};
    long p10 = 1;
    for (int e = 1; e <= 7; e++) {
        p10 *= 10;
        if (n == p10)
            printf(" (known %ld, %ld: %s)", known[e][0], known[e][1],
                   ok_str(rs.start == known[e][0] && rs.len == known[e][1]));
    }
    printf("\n");

    printf("(a) per-thread best + critical, schedule(dynamic, 1000)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = longest_critical(n));
        table_row(n, THREADS[k], ts, tp, rp.len == rs.len && rp.start == rs.start);
    }
    printf("(b) declare reduction(maxloc : Best), schedule(dynamic, 1000)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = longest_reduction(n));
        table_row(n, THREADS[k], ts, tp, rp.len == rs.len && rp.start == rs.start);
    }
    return 0;
}

/* Notes:
 * - Why not reduction(max:len)? It gives the longest length but loses WHICH
 *   start produced it; a max-location needs the pair to travel together.
 * - Tie rule: equal lengths -> smaller start. better() is used everywhere, so
 *   merging partial results in any order gives the same answer as serial.
 * - Critical once per thread (after the loop, nowait) is cheap; a critical per
 *   iteration would serialise the loop.
 * - declare reduction: omp_in / omp_out are the two partial results being
 *   combined, omp_priv is each thread's private copy (initialised by the
 *   initializer clause to a value that loses to everything).
 * - Chain lengths vary a lot (1 to 525 terms), so schedule(dynamic, chunk);
 *   the chunk of 1000 keeps scheduling overhead low.
 */

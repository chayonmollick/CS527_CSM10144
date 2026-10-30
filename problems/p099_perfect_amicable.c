/* P099: Perfect numbers and amicable pairs (load balancing)
 * Problem: Let s(k) be the sum of the proper divisors of k (divisors < k), found
 *   by trial division up to sqrt(k). Find all perfect numbers below N
 *   (s(k) = k) and all amicable pairs a < b below N (s(a) = b and s(b) = a).
 *   The cost of s(k) grows like sqrt(k), so the work per number is uneven:
 *   compare schedule(static) with schedule(dynamic). Verify against the serial
 *   code (perfect numbers below 1e5: 6, 28, 496, 8128) and report time and
 *   speedup for 1, 2, 4 and 8 threads.
 * Solution: step 1 computes the table s[k] for all k < N: iteration k writes only
 *   s[k], so a parallel for has no race. Only the schedule matters:
 *   (a) schedule(static): thread t gets one contiguous block; the last block
 *       holds the largest (most expensive) numbers, so the others wait for it.
 *   (b) schedule(static, 1): cyclic, thread t gets k = t, t+T, ... - every thread
 *       gets a mix of small and large k (good here because cost grows smoothly).
 *   (c) schedule(dynamic, 256): idle threads grab the next chunk of 256 numbers.
 *   Step 2 (cheap, O(N)) collects the results with one ordered serial pass over
 *   s[], so the lists come out sorted with no shared list to protect.
 * Source: number-theory load-balancing exercise (lab extension).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p099_perfect_amicable.c -o p099 -lm
 * Run:   ./p099 [N]          (default N = 2e5)
 */
#include "common.h"

/* Sum of proper divisors of k by trial division: d and k/d for every d <= sqrt(k). */
static long sum_proper_divisors(long k) {
    if (k < 2) return 0;
    long s = 1;                                   /* 1 divides every k >= 2 */
    for (long d = 2; d * d <= k; d++)
        if (k % d == 0) {
            s += d;
            if (d != k / d) s += k / d;           /* do not count sqrt(k) twice */
        }
    return s;
}

static void sums_serial(long n, long *s) {
    for (long k = 0; k < n; k++) s[k] = sum_proper_divisors(k);
}
static void sums_static(long n, long *s) {
    #pragma omp parallel for schedule(static)
    for (long k = 0; k < n; k++) s[k] = sum_proper_divisors(k);
}
static void sums_cyclic(long n, long *s) {
    #pragma omp parallel for schedule(static, 1)
    for (long k = 0; k < n; k++) s[k] = sum_proper_divisors(k);
}
static void sums_dynamic(long n, long *s) {
    #pragma omp parallel for schedule(dynamic, 256)
    for (long k = 0; k < n; k++) s[k] = sum_proper_divisors(k);
}

/* Step 2: one ordered pass over the table; prints and returns the counts. */
static void report(long n, const long *s, long *nperfect, long *npairs) {
    *nperfect = 0; *npairs = 0;
    printf("perfect numbers below %ld:", n);
    for (long k = 2; k < n; k++)
        if (s[k] == k) { printf(" %ld", k); (*nperfect)++; }
    printf("\namicable pairs below %ld:", n);
    for (long a = 2; a < n; a++) {
        long b = s[a];
        if (b > a && b < n && s[b] == a) { printf(" (%ld,%ld)", a, b); (*npairs)++; }
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 200000);
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    long *ss = xmalloc((size_t)n * sizeof(long)), *sp = xmalloc((size_t)n * sizeof(long));
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, sums_serial(n, ss));
    printf("P099: perfect numbers and amicable pairs below N = %ld\n", n);
    long nperf, npairs;
    report(n, ss, &nperf, &npairs);
    const long perfect[5] = {6, 28, 496, 8128, 33550336};    /* all perfect numbers < 1e9 */
    long expect = 0;
    for (int i = 0; i < 5; i++) expect += perfect[i] < n;
    printf("%ld perfect numbers (known: %ld, %s), %ld amicable pairs\n",
           nperf, expect, ok_str(nperf == expect), npairs);

    void (*version[3])(long, long *) = {sums_static, sums_cyclic, sums_dynamic};
    const char *label[3] = {"(a) schedule(static)    - contiguous blocks",
                            "(b) schedule(static, 1) - cyclic",
                            "(c) schedule(dynamic, 256)"};
    for (int v = 0; v < 3; v++) {
        printf("%s   [check: s[] equals serial]\n", label[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            BEST_TIME(tp, version[v](n, sp));
            table_row(n, THREADS[k], ts, tp, same_longs(ss, sp, n));
        }
    }
    free(ss); free(sp);
    return 0;
}

/* Notes:
 * - Load imbalance: cost(k) ~ sqrt(k), so with schedule(static) the thread with
 *   the last block does the most work (with 2 threads it has ~65% of it).
 * - static,1 balances smoothly growing costs with no runtime overhead; dynamic
 *   adapts to ANY uneven cost (e.g. random), at the price of a shared counter
 *   per chunk - keep chunks big enough (256 here).
 * - Collecting results: appending to one shared list from inside the loop would
 *   need a critical section and gives a random order. Here the loop only fills
 *   s[k] (own slot) and a cheap serial pass reads s[] in order. Alternative:
 *   per-thread lists, merged and sorted at the end.
 * - The amicable test needs s[b] for b = s[a], i.e. another number's result -
 *   that is why the table is built completely (barrier) before step 2.
 */

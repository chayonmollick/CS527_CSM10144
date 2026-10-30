/* P025: Count positives, negatives, zeros and values in a range
 * Problem: Given an integer array a[N] and a range [lo, hi], count in ONE parallel
 *   loop how many elements are positive, negative and zero, and how many lie in
 *   [lo, hi]. Check that pos + neg + zero == N and verify against serial code.
 * Solution: four independent counters, each a sum, so one clause
 *   reduction(+:pos, neg, zero, inrange) gives every thread private counters starting
 *   at 0 and adds them up at the end. Without it, cnt++ on shared counters is a
 *   read-modify-write race and counts are lost.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p025_count_categories.c -o p025 -lm
 * Run:   ./p025 [N] [lo] [hi]   (defaults N = 2e7, lo = -10, hi = 20; data in [-50, 50])
 */
#include "common.h"

typedef struct { long pos, neg, zero, inrange; } counts_t;

static counts_t count_serial(const int *a, long n, int lo, int hi) {
    long pos = 0, neg = 0, zero = 0, inrange = 0;
    for (long i = 0; i < n; i++) {
        if (a[i] > 0) pos++;
        else if (a[i] < 0) neg++;
        else zero++;
        if (a[i] >= lo && a[i] <= hi) inrange++;
    }
    counts_t c = {pos, neg, zero, inrange};
    return c;
}

static counts_t count_parallel(const int *a, long n, int lo, int hi) {
    long pos = 0, neg = 0, zero = 0, inrange = 0;
    #pragma omp parallel for reduction(+:pos, neg, zero, inrange)
    for (long i = 0; i < n; i++) {
        if (a[i] > 0) pos++;
        else if (a[i] < 0) neg++;
        else zero++;
        if (a[i] >= lo && a[i] <= hi) inrange++;
    }
    counts_t c = {pos, neg, zero, inrange};
    return c;
}

static int same_counts(counts_t x, counts_t y) {
    return x.pos == y.pos && x.neg == y.neg && x.zero == y.zero && x.inrange == y.inrange;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    int lo = (int)arg_long(argc, argv, 2, -10);
    int hi = (int)arg_long(argc, argv, 3, 20);
    int *a = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, -50, 50, 3);
    omp_warmup();

    counts_t cs = {0, 0, 0, 0}, cp = {0, 0, 0, 0};
    double ts, tp;
    BEST_TIME(ts, cs = count_serial(a, n, lo, hi));
    printf("P025: N = %ld, data in [-50, 50], range [%d, %d]\n", n, lo, hi);
    printf("  positive %ld, negative %ld, zero %ld, in range %ld\n", cs.pos, cs.neg, cs.zero, cs.inrange);
    printf("  pos + neg + zero == N: %s\n", ok_str(cs.pos + cs.neg + cs.zero == n));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, cp = count_parallel(a, n, lo, hi));
        table_row(n, THREADS[k], ts, tp, same_counts(cs, cp) && cp.pos + cp.neg + cp.zero == n);
    }
    free(a);
    return 0;
}

/* Notes:
 * - One reduction clause can list several variables; each gets its own private copy.
 * - Integer counts are exact: the parallel result must equal the serial one exactly
 *   (no tolerance), and pos + neg + zero == N is a free consistency check.
 * - Alternatives: "#pragma omp atomic" on every increment (correct but very slow:
 *   every update contends for the same cache line), or per-thread counters in an
 *   array indexed by thread id (works, but beware false sharing).
 * - The branches are data dependent; gcc usually turns them into branch-free
 *   compare-and-add code, which is why the loop runs at memory speed.
 */

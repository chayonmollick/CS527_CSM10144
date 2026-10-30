/* P090: Recursive divide-and-conquer sum and max with tasks (cutoff)
 * Problem: Compute the sum and the maximum of an array a[N] of doubles by
 *   recursive divide and conquer: split the range in two halves, solve each
 *   half (as an OpenMP task), wait for both, combine (sum = left + right,
 *   max = larger of the two). Below a cutoff length solve the range with a
 *   plain loop. Compare with "parallel for reduction(+:sum) reduction(max:mx)"
 *   and the serial loop, verify, and show how the cutoff affects the time.
 * Solution: one thread (single) calls the recursive function; at each level it
 *   creates a task for the left half and one for the right half, then
 *   "taskwait" waits for these two children before combining. Each task
 *   writes its result into a variable of the parent declared shared(...)
 *   (task variables are firstprivate by default, so without shared the
 *   result would go into the task's own copy and be lost); the parent's
 *   variables stay alive until the taskwait. Idle threads of the team pick up
 *   the queued tasks. The cutoff controls task granularity: too small = huge
 *   numbers of tiny tasks (overhead), too large = too few tasks to keep all
 *   threads busy. Max is exact in any order; the sum is added in a different
 *   order, so compare it with a tolerance.
 * Source: lab extension (tasks, recursive parallelism).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p090_recursive_sum_tasks.c -o p090 -lm
 * Run:   ./p090 [N] [cutoff]   (defaults N = 2e7, cutoff = 1e5)
 */
#include "common.h"

typedef struct { double sum, max; } summax_t;

static summax_t leaf(const double *a, long lo, long hi) {      /* range [lo, hi), not empty */
    summax_t r = {0.0, a[lo]};
    for (long i = lo; i < hi; i++) {
        r.sum += a[i];
        if (a[i] > r.max) r.max = a[i];
    }
    return r;
}

static summax_t combine(summax_t l, summax_t r) {
    summax_t c = {l.sum + r.sum, l.max > r.max ? l.max : r.max};
    return c;
}

static summax_t summax_serial(const double *a, long n) { return leaf(a, 0, n); }

static summax_t rec(const double *a, long lo, long hi, long cutoff) {
    if (hi - lo <= cutoff) return leaf(a, lo, hi);
    long mid = lo + (hi - lo) / 2;
    summax_t left, right;
    #pragma omp task shared(left)                 /* result must reach the parent */
    left = rec(a, lo, mid, cutoff);
    #pragma omp task shared(right)
    right = rec(a, mid, hi, cutoff);
    #pragma omp taskwait                          /* wait for both children */
    return combine(left, right);
}

static summax_t summax_tasks(const double *a, long n, long cutoff) {
    summax_t r;
    #pragma omp parallel
    #pragma omp single                            /* one thread starts the recursion */
    r = rec(a, 0, n, cutoff);
    return r;
}

static summax_t summax_reduction(const double *a, long n) {
    double s = 0.0, m = a[0];
    #pragma omp parallel for reduction(+:s) reduction(max:m)
    for (long i = 0; i < n; i++) {
        s += a[i];
        if (a[i] > m) m = a[i];
    }
    summax_t r = {s, m};
    return r;
}

/* Number of leaves of rec's recursion tree (same split as rec). */
static long count_leaves(long len, long cutoff) {
    return len <= cutoff ? 1 : count_leaves(len / 2, cutoff) + count_leaves(len - len / 2, cutoff);
}

static int same_result(summax_t x, summax_t y) { return close1(x.sum, y.sum, 1e-9) && x.max == y.max; }

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000);
    long cutoff = arg_long(argc, argv, 2, 100000);
    if (n < 1) n = 1;
    if (cutoff < 1) cutoff = 1;

    /* Worked example: 8 values, cutoff 2 -> tree of 7 calls, 4 leaves. */
    double ex[8] = {3, 1, 4, 1, 5, 9, 2, 6};
    summax_t e = summax_tasks(ex, 8, 2);
    printf("P090: recursive sum and max with tasks\n");
    printf("example [3 1 4 1 5 9 2 6], cutoff 2: leaves [3 1] [4 1] [5 9] [2 6]\n"
           "  sums 4 5 14 8 -> 9 22 -> 31, max 9; tasks give sum = %g, max = %g  %s\n\n",
           e.sum, e.max, ok_str(e.sum == 31 && e.max == 9));

    double *a = xmalloc((size_t)n * sizeof(double));
    fill_double(a, n, 0.0, 1000.0, 90);
    omp_warmup();

    double ts, tp;
    summax_t rs = {0, 0}, rp = {0, 0};
    BEST_TIME(ts, rs = summax_serial(a, n));
    printf("N = %ld: serial sum = %.6f, max = %.6f\n", n, rs.sum, rs.max);

    printf("\nTasks, cutoff = %ld (check: sum within 1e-9, max exact)\n", cutoff);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = summax_tasks(a, n, cutoff));
        table_row(n, THREADS[k], ts, tp, same_result(rs, rp));
    }
    printf("parallel for reduction(+:sum) reduction(max:mx)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = summax_reduction(a, n));
        table_row(n, THREADS[k], ts, tp, same_result(rs, rp));
    }

    int t_max = THREADS[NTHREADS - 1];
    omp_set_num_threads(t_max);
    printf("\nEffect of the cutoff, %d threads (tasks created = 2 * (leaves - 1))\n", t_max);
    printf("%12s %10s %12s %12s %8s %6s\n", "cutoff", "leaves", "tasks", "time(s)", "speedup", "check");
    long cutoffs[6] = {100, 1000, 10000, 100000, 1000000, 10000000};
    for (int c = 0; c < 6; c++) {
        long leaves = count_leaves(n, cutoffs[c]);
        BEST_TIME(tp, rp = summax_tasks(a, n, cutoffs[c]));
        printf("%12ld %10ld %12ld %12.6f %8.2f %6s\n", cutoffs[c], leaves, 2 * (leaves - 1),
               tp, ts / tp, ok_str(same_result(rs, rp)));
    }
    free(a);
    return 0;
}

/* Notes:
 * - shared(left): variables used in a task are firstprivate by default (when
 *   they are private in the enclosing code, as locals of rec are), so the
 *   child would write its own copy. taskwait keeps the parent's locals alive
 *   until both children have written them.
 * - taskwait waits only for the CHILDREN of the current task (not
 *   grandchildren; those are waited for inside the children).
 * - Cutoff = granularity: each task costs about a microsecond; with cutoff
 *   100 there are ~400k tasks (overhead dominates), with a huge cutoff only a
 *   few leaves (not enough parallelism). Aim for leaves >> threads.
 * - Reduction is simpler and faster for a flat loop; the task pattern is for
 *   recursive problems (quicksort, mergesort, tree traversal) where no loop
 *   exists. Common tweak: run the right half in the current task instead of
 *   creating a second task.
 */

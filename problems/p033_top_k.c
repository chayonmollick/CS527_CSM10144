/* P033: Top 3 largest elements with their indices
 * Problem: Find the 3 largest elements of an integer array a[N] together with their
 *   indices (largest first; equal values: smaller index first) using OpenMP. Verify
 *   against a serial scan and against sorting a copy of the array, and time it.
 * Solution: each thread scans its part of the array ("omp for nowait") keeping a
 *   private sorted top-3 list (insertion into 3 slots). The per-thread lists are then
 *   merged into the global list, either (A) inside a critical section (one thread at
 *   a time) or (B) by storing each list at lists[thread id] and merging serially after
 *   the region. Because the order "larger value, then smaller index" is a strict
 *   total order, the merged answer does not depend on the merge order.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p033_top_k.c -o p033 -lm
 * Run:   ./p033 [N]   (default N = 1e7, values in [0, 999999] so ties occur)
 */
#include "common.h"

#define K 3                                       /* how many largest elements */

typedef struct { int cnt; int val[K]; long idx[K]; } topk_t;   /* sorted, best first */

/* Is (v, i) better than (w, j)? Larger value wins; on a tie the smaller index wins. */
static int better(int v, long i, int w, long j) { return v > w || (v == w && i < j); }

/* Insert (v, i) into the sorted list t if it belongs in the top K. */
static void topk_insert(topk_t *t, int v, long i) {
    if (t->cnt == K && !better(v, i, t->val[K - 1], t->idx[K - 1])) return;   /* not good enough */
    int p;                                        /* slot to fill */
    if (t->cnt < K) p = t->cnt++;                 /* list not full: next free slot */
    else p = K - 1;                               /* full: overwrite the worst entry */
    while (p > 0 && better(v, i, t->val[p - 1], t->idx[p - 1])) {           /* shift worse ones down */
        t->val[p] = t->val[p - 1];
        t->idx[p] = t->idx[p - 1];
        p--;
    }
    t->val[p] = v;
    t->idx[p] = i;
}

static topk_t topk_serial(const int *a, long n) {
    topk_t t = {0, {0}, {0}};
    for (long i = 0; i < n; i++) topk_insert(&t, a[i], i);
    return t;
}

/* (A) private lists merged in a critical section. */
static topk_t topk_critical(const int *a, long n) {
    topk_t best = {0, {0}, {0}};                  /* shared */
    #pragma omp parallel
    {
        topk_t mine = {0, {0}, {0}};              /* private */
        #pragma omp for nowait
        for (long i = 0; i < n; i++) topk_insert(&mine, a[i], i);
        #pragma omp critical
        for (int k = 0; k < mine.cnt; k++) topk_insert(&best, mine.val[k], mine.idx[k]);
    }
    return best;
}

/* (B) one list per thread in a shared array, merged serially afterwards. */
static topk_t topk_serial_merge(const int *a, long n) {
    int maxt = omp_get_max_threads();
    topk_t *lists = xmalloc((size_t)maxt * sizeof(topk_t));
    int nt = 1;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        #pragma omp single
        nt = omp_get_num_threads();
        topk_t mine = {0, {0}, {0}};
        #pragma omp for nowait
        for (long i = 0; i < n; i++) topk_insert(&mine, a[i], i);
        lists[id] = mine;                         /* each thread writes only its own slot */
    }
    topk_t best = {0, {0}, {0}};
    for (int t = 0; t < nt; t++)
        for (int k = 0; k < lists[t].cnt; k++) topk_insert(&best, lists[t].val[k], lists[t].idx[k]);
    free(lists);
    return best;
}

static int same_topk(const topk_t *x, const topk_t *y) {
    if (x->cnt != y->cnt) return 0;
    for (int k = 0; k < x->cnt; k++)
        if (x->val[k] != y->val[k] || x->idx[k] != y->idx[k]) return 0;
    return 1;
}

static void print_topk(const char *name, const topk_t *t) {
    printf("  %-22s", name);
    for (int k = 0; k < t->cnt; k++) printf("  a[%ld] = %d", t->idx[k], t->val[k]);
    printf("\n");
}

/* Independent check: sort (value, index) pairs by the same order and take the first K. */
typedef struct { int val; long idx; } pair_t;
static int cmp_pair(const void *p, const void *q) {
    const pair_t *x = p, *y = q;
    if (better(x->val, x->idx, y->val, y->idx)) return -1;
    if (better(y->val, y->idx, x->val, x->idx)) return 1;
    return 0;
}
static topk_t topk_by_sorting(const int *a, long n) {
    pair_t *p = xmalloc((size_t)n * sizeof(pair_t));
    for (long i = 0; i < n; i++) { p[i].val = a[i]; p[i].idx = i; }
    qsort(p, (size_t)n, sizeof(pair_t), cmp_pair);
    topk_t t = {0, {0}, {0}};
    for (long i = 0; i < n && i < K; i++) { t.val[t.cnt] = p[i].val; t.idx[t.cnt] = p[i].idx; t.cnt++; }
    free(p);
    return t;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    if (n < 0) n = 0;
    int *a = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, 0, 999999, 13);
    omp_warmup();

    /* small hand-made test with ties: expected a[1] = 9, a[3] = 9, a[5] = 9 */
    int small[7] = {5, 9, 1, 9, 7, 9, 9};
    topk_t hs = topk_serial(small, 7), ha = topk_critical(small, 7), hb = topk_serial_merge(small, 7);
    int hand_ok = hs.cnt == 3 && hs.idx[0] == 1 && hs.idx[1] == 3 && hs.idx[2] == 5 && hs.val[2] == 9 &&
                  same_topk(&hs, &ha) && same_topk(&hs, &hb);
    printf("P033: top %d of {5, 9, 1, 9, 7, 9, 9}: a[%ld] a[%ld] a[%ld]  %s\n", K, hs.idx[0], hs.idx[1],
           hs.idx[2], ok_str(hand_ok));

    topk_t rs = {0, {0}, {0}}, ra, rb;
    double ts, tA, tB;
    BEST_TIME(ts, rs = topk_serial(a, n));
    topk_t sorted = topk_by_sorting(a, n);
    printf("P033: N = %ld\n", n);
    print_topk("serial", &rs);
    print_topk("by sorting a copy", &sorted);
    printf("  serial == sorting: %s\n", ok_str(same_topk(&rs, &sorted)));

    printf("A = critical merge, B = per-thread array + serial merge\n");
    printf("%8s %12s %12s %8s %12s %8s %6s\n", "threads", "serial(s)", "A(s)", "speedA", "B(s)", "speedB", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tA, ra = topk_critical(a, n));
        BEST_TIME(tB, rb = topk_serial_merge(a, n));
        int ok = same_topk(&ra, &sorted) && same_topk(&rb, &sorted);
        printf("%8d %12.6f %12.6f %8.2f %12.6f %8.2f %6s\n", THREADS[k], ts, tA,
               tA > 0 ? ts / tA : 0.0, tB, tB > 0 ? ts / tB : 0.0, ok_str(ok));
    }
    free(a);
    return 0;
}

/* Notes:
 * - No built-in reduction returns "top 3 with indices", so this is a manual reduction:
 *   private partial result per thread + a combine step (critical or serial merge).
 * - The critical runs once per thread (T times, K inserts each), not once per element,
 *   so it costs nothing; a critical inside the loop would serialise everything.
 * - Ties: comparing (value, index) gives a strict total order, so any merge order
 *   gives the same, deterministic answer as the serial scan.
 * - Per-thread scan is O(N) with K compares at most per element (usually 1: most
 *   elements are below the current K-th best). Sorting is O(N log N): only a check.
 * - In version B the array must be sized by omp_get_max_threads() before the region,
 *   and each thread writes only lists[its id], so there is no race.
 */

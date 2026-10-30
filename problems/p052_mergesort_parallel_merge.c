/* P052: Merge sort with tasks where the MERGE is parallel too (Lab 9 extension)
 * Problem: Lab 9 sorted the two halves of merge sort as OpenMP tasks but merged
 *   them serially. Make the merge step parallel as well: recursive parallel
 *   merge (take the middle element of the larger input, binary-search its
 *   position in the other input, merge the two resulting pairs as tasks), with
 *   cutoffs for both the sort and the merge. Compare serial merge sort, the
 *   Lab 9 version (parallel recursion, serial merge) and the new version for
 *   1, 2, 4, 8 threads, verify against qsort, and explain why the serial merge
 *   limited the speedup to about log n.
 * Solution: sort: task for each half, taskwait, then merge a[0..h) and a[h..n)
 *   into tmp and copy back. Parallel merge of A (na) and B (nb) into C: let A
 *   be the larger, r = na/2, s = number of B elements < A[r] (lower bound);
 *   then A[r] lands at C[r+s], everything left of it is merge(A[0..r), B[0..s))
 *   and everything right is merge(A[r+1..), B[s..)) -> two independent merges
 *   on disjoint input and output ranges (one as a task, one by the current
 *   thread, then taskwait). If B is larger, split B the same way using
 *   upper bound (A elements <= B[r] go first: ties from A first -> stable).
 *   The copy back is split into tasks too. Recursion stops at the cutoffs.
 *   Tasks are started from `parallel` + `single`; every task owns disjoint
 *   ranges of a and tmp -> race-free.
 * Source: Lab 9 Task 1 extension (session3/task1_mergesort.c).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p052_mergesort_parallel_merge.c -o p052 -lm
 * Run:   ./p052 [N]          (default N = 2e6)
 */
#include "common.h"

#define SORT_CUTOFF  2048       /* shorter subarrays: serial merge sort, no tasks (as Lab 9) */
#define MERGE_CUTOFF 8192       /* shorter merges / copies: serial, no tasks */

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}
static int is_sorted_int(const int *a, long n) {
    for (long i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}

/* Stable serial merge of A[0..na) and B[0..nb) into C (ties: A first). */
static void merge_serial(const int *A, long na, const int *B, long nb, int *C) {
    long i = 0, j = 0, k = 0;
    while (i < na && j < nb) C[k++] = (A[i] <= B[j]) ? A[i++] : B[j++];
    while (i < na) C[k++] = A[i++];
    while (j < nb) C[k++] = B[j++];
}

/* ---------------- serial merge sort (baseline) ---------------- */
static void msort_serial(int *a, int *tmp, long n) {
    if (n < 2) return;
    long h = n / 2;
    msort_serial(a, tmp, h);
    msort_serial(a + h, tmp + h, n - h);
    merge_serial(a, h, a + h, n - h, tmp);
    memcpy(a, tmp, (size_t)n * sizeof(int));
}

/* ---------------- Lab 9: parallel recursion, SERIAL merge ---------------- */
static void msort_lab9(int *a, int *tmp, long n) {
    if (n < SORT_CUTOFF) { msort_serial(a, tmp, n); return; }
    long h = n / 2;
    #pragma omp task
    msort_lab9(a, tmp, h);
    #pragma omp task
    msort_lab9(a + h, tmp + h, n - h);
    #pragma omp taskwait                               /* both halves sorted */
    merge_serial(a, h, a + h, n - h, tmp);             /* O(n) on ONE thread */
    memcpy(a, tmp, (size_t)n * sizeof(int));           /* O(n) on ONE thread */
}

/* ---------------- parallel merge and parallel copy ---------------- */
static long lower_bound(const int *x, long n, int v) { /* first index with x[i] >= v */
    long lo = 0, hi = n;
    while (lo < hi) { long mid = lo + (hi - lo) / 2; if (x[mid] < v) lo = mid + 1; else hi = mid; }
    return lo;
}
static long upper_bound(const int *x, long n, int v) { /* first index with x[i] > v */
    long lo = 0, hi = n;
    while (lo < hi) { long mid = lo + (hi - lo) / 2; if (x[mid] <= v) lo = mid + 1; else hi = mid; }
    return lo;
}

static void merge_parallel(const int *A, long na, const int *B, long nb, int *C) {
    if (na + nb < MERGE_CUTOFF) { merge_serial(A, na, B, nb, C); return; }
    if (na >= nb) {                                    /* split the larger input, A */
        long r = na / 2;
        long s = lower_bound(B, nb, A[r]);             /* B[0..s) < A[r] */
        C[r + s] = A[r];
        #pragma omp task
        merge_parallel(A, r, B, s, C);                                    /* left part  */
        merge_parallel(A + r + 1, na - r - 1, B + s, nb - s, C + r + s + 1); /* right part */
        #pragma omp taskwait
    } else {                                           /* split the larger input, B */
        long r = nb / 2;
        long s = upper_bound(A, na, B[r]);             /* A[0..s) <= B[r] (ties: A first) */
        C[s + r] = B[r];
        #pragma omp task
        merge_parallel(A, s, B, r, C);
        merge_parallel(A + s, na - s, B + r + 1, nb - r - 1, C + s + r + 1);
        #pragma omp taskwait
    }
}

static void copy_parallel(int *dst, const int *src, long n) {
    if (n < MERGE_CUTOFF) { memcpy(dst, src, (size_t)n * sizeof(int)); return; }
    long h = n / 2;
    #pragma omp task
    copy_parallel(dst, src, h);
    copy_parallel(dst + h, src + h, n - h);
    #pragma omp taskwait
}

/* ---------------- parallel recursion AND parallel merge ---------------- */
static void msort_pmerge(int *a, int *tmp, long n) {
    if (n < SORT_CUTOFF) { msort_serial(a, tmp, n); return; }
    long h = n / 2;
    #pragma omp task
    msort_pmerge(a, tmp, h);
    #pragma omp task
    msort_pmerge(a + h, tmp + h, n - h);
    #pragma omp taskwait
    merge_parallel(a, h, a + h, n - h, tmp);           /* split into tasks */
    copy_parallel(a, tmp, n);
}

/* Start the recursion: one thread creates the tasks, the whole team runs them. */
static void sort_lab9(int *a, int *tmp, long n) {
    #pragma omp parallel
    {
        #pragma omp single
        msort_lab9(a, tmp, n);
    }
}
static void sort_pmerge(int *a, int *tmp, long n) {
    #pragma omp parallel
    {
        #pragma omp single
        msort_pmerge(a, tmp, n);
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 2000000L);
    if (n < 1) n = 1;
    int *in  = xmalloc((size_t)n * sizeof(int)), *ref = xmalloc((size_t)n * sizeof(int));
    int *w   = xmalloc((size_t)n * sizeof(int)), *tmp = xmalloc((size_t)n * sizeof(int));
    fill_int(in, n, -1000000000, 1000000000, 52);
    omp_warmup();
    memcpy(ref, in, (size_t)n * sizeof(int));
    qsort(ref, (size_t)n, sizeof(int), cmp_int);
    size_t bytes = (size_t)n * sizeof(int);

    /* Each timed run restores the unsorted input first (memcpy included, ~1% of the time). */
    double ts, tp;
    BEST_TIME(ts, { memcpy(w, in, bytes); msort_serial(w, tmp, n); });
    int ok_serial = is_sorted_int(w, n) && memcmp(w, ref, bytes) == 0;
    printf("P052: merge sort with tasks, N = %ld, cutoffs: sort %d, merge %d; serial %s\n",
           n, SORT_CUTOFF, MERGE_CUTOFF, ok_str(ok_serial));
    printf("Lab 9 limit (serial merges on the critical path): work/span ~ log2(N)/2 = %.1f\n",
           log2((double)n) / 2.0);

    printf("\n(a) Lab 9: parallel recursion, serial merge\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(w, in, bytes); sort_lab9(w, tmp, n); });
        table_row(n, THREADS[k], ts, tp, is_sorted_int(w, n) && memcmp(w, ref, bytes) == 0);
    }

    printf("\n(b) parallel recursion + parallel merge (+ parallel copy back)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(w, in, bytes); sort_pmerge(w, tmp, n); });
        table_row(n, THREADS[k], ts, tp, is_sorted_int(w, n) && memcmp(w, ref, bytes) == 0);
    }
    free(in); free(ref); free(w); free(tmp);
    return 0;
}

/* Notes:
 * - Why Lab 9 stalls: work T1 ~ N log2 N, but the top merge (N elements) runs
 *   on one thread, the next level has only 2 merges of N/2, ... The critical
 *   path (span) is N + N/2 + N/4 + ... ~ 2N, so speedup <= T1 / span ~
 *   log2(N)/2 no matter how many threads (Amdahl: the first log2(p) levels
 *   have fewer merges than threads).
 * - Parallel merge: each split halves the larger input, so the recursion depth
 *   is O(log N) and each level costs one binary search: span of a merge
 *   O(log^2 N), of the whole sort O(log^3 N) -> parallelism ~ N / log^2 N.
 * - Placing A[r] at C[r+s] is exact because r elements of A and s elements of B
 *   are smaller (or tie-broken before it). lower_bound vs upper_bound keeps ties
 *   ordered A before B, i.e. the merge stays stable.
 * - Cutoffs matter: tasks cost ~1 us, so tiny merges/sorts run serially. The
 *   current thread does one half itself (only one task per split).
 * - taskwait inside merge_parallel is required: the caller's taskwait waits only
 *   for its own child tasks, not for grandchildren.
 */

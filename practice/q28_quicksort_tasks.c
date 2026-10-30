/* CS527 Computer Systems — OpenMP practice
 * Q28: Parallelise quicksort with OpenMP tasks. After partitioning, sort the
 *      two sides as independent tasks; below a cutoff sort serially. Verify
 *      against qsort() and report speedup for N = 1e5, 1e6, 1e7.
 *  - serial quicksort: Hoare partition, median-of-3 pivot, insertion sort for tiny ranges
 *  - task quicksort: partition, then two tasks (left / right), serial below CUTOFF
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CUTOFF  10000   /* ranges smaller than this are sorted serially */
#define SMALL   16      /* ranges smaller than this use insertion sort  */

static unsigned long long rng = 42;            /* deterministic, serial only */
static unsigned next_rand(void) {
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng >> 33);
}

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}
static void swap_int(int *x, int *y) { int t = *x; *x = *y; *y = t; }

static void insertion_sort(int *a, long lo, long hi) {     /* inclusive range */
    for (long i = lo + 1; i <= hi; i++) {
        int v = a[i]; long j = i - 1;
        while (j >= lo && a[j] > v) { a[j+1] = a[j]; j--; }
        a[j+1] = v;
    }
}

/* Hoare partition of a[lo..hi]; returns p with a[lo..p] <= a[p+1..hi]. */
static long partition(int *a, long lo, long hi) {
    long mid = lo + (hi - lo) / 2;
    if (a[mid] < a[lo]) swap_int(&a[mid], &a[lo]);    /* median of three */
    if (a[hi]  < a[lo]) swap_int(&a[hi],  &a[lo]);
    if (a[hi]  < a[mid]) swap_int(&a[hi], &a[mid]);
    int pivot = a[mid];
    long i = lo - 1, j = hi + 1;
    for (;;) {
        do i++; while (a[i] < pivot);
        do j--; while (a[j] > pivot);
        if (i >= j) return j;
        swap_int(&a[i], &a[j]);
    }
}

static void quicksort_serial(int *a, long lo, long hi) {
    if (hi - lo + 1 < SMALL) { insertion_sort(a, lo, hi); return; }
    long p = partition(a, lo, hi);
    quicksort_serial(a, lo, p);
    quicksort_serial(a, p + 1, hi);
}

/* The two halves are disjoint parts of the array -> no race between tasks. */
static void quicksort_task(int *a, long lo, long hi) {
    if (hi - lo + 1 < CUTOFF) { quicksort_serial(a, lo, hi); return; }
    long p = partition(a, lo, hi);       /* partition itself is serial */
    #pragma omp task                      /* a, lo, p are firstprivate: fine */
    quicksort_task(a, lo, p);
    #pragma omp task
    quicksort_task(a, p + 1, hi);
    #pragma omp taskwait                  /* nothing to combine, but keeps the
                                             call synchronous for the caller */
}

static void quicksort_parallel(int *a, long n) {
    #pragma omp parallel
    #pragma omp single
    quicksort_task(a, 0, n - 1);
}

static int is_sorted(const int *a, long n) {
    for (long i = 0; i + 1 < n; i++) if (a[i] > a[i+1]) return 0;
    return 1;
}

int main(int argc, char **argv) {
    long sizes[3] = {100000L, 1000000L, 10000000L}; int nsizes = 3;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    const int threads[] = {1, 2, 4, 8};
    printf("Q28: Quicksort with OpenMP tasks (cutoff = %d)\n", CUTOFF);

    for (int si = 0; si < nsizes; si++) {
        long n = sizes[si];
        if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
        int *orig = malloc((size_t)n * sizeof(int));
        int *ref  = malloc((size_t)n * sizeof(int));
        int *w    = malloc((size_t)n * sizeof(int));
        if (!orig || !ref || !w) { fprintf(stderr, "alloc failed\n"); return 1; }
        for (long i = 0; i < n; i++) orig[i] = (int)(next_rand() % 1000000000u);

        memcpy(ref, orig, (size_t)n * sizeof(int));          /* reference */
        double q0 = omp_get_wtime(); qsort(ref, n, sizeof(int), cmp_int);
        double tq = omp_get_wtime() - q0;

        memcpy(w, orig, (size_t)n * sizeof(int));            /* serial baseline */
        double t0 = omp_get_wtime(); quicksort_serial(w, 0, n - 1);
        double ts = omp_get_wtime() - t0;
        int ok_s = memcmp(w, ref, (size_t)n * sizeof(int)) == 0;

        printf("\nN = %ld   qsort %.6f s, serial quicksort %.6f s (%s)\n",
               n, tq, ts, ok_s ? "OK" : "WRONG");
        printf("%8s %12s %12s %9s %11s %7s\n",
               "threads", "serial(s)", "parallel(s)", "speedup", "efficiency", "check");
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            memcpy(w, orig, (size_t)n * sizeof(int));
            double p0 = omp_get_wtime(); quicksort_parallel(w, n);
            double tp = omp_get_wtime() - p0;
            int ok = is_sorted(w, n) && memcmp(w, ref, (size_t)n * sizeof(int)) == 0;
            double sp = tp > 0 ? ts / tp : 0.0;
            printf("%8d %12.6f %12.6f %9.2f %11.2f %7s\n",
                   threads[ti], ts, tp, sp, sp / threads[ti], ok ? "OK" : "WRONG");
        }
        free(orig); free(ref); free(w);
    }
    return 0;
}

/* Viva notes:
 * - Divide and conquer -> tasks: after partition the two sides are disjoint,
 *   independent sub-problems, so each becomes a task; no locks needed.
 * - The first partition (whole array) is serial O(n) work, the next level has
 *   only 2 tasks, etc. -- this serial top of the tree limits speedup (Amdahl).
 *   Merge sort has the same issue at the final merge.
 * - Cutoff (10000): below it task overhead > work, so sort serially. Too small
 *   a cutoff = millions of tiny tasks; too large = too few tasks to balance.
 * - Unbalanced partitions (bad pivots) give unequal tasks; tasks still balance
 *   because idle threads steal whatever task is queued (a parallel for with
 *   static chunks could not). Median-of-3 avoids O(n^2) on sorted input.
 * - taskwait waits for the two child tasks only; the implicit barrier at the
 *   end of single/parallel waits for all tasks, so sorting is complete there.
 */

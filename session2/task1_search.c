/* CS527 Computer Systems — OpenMP Lab 8
 * Task 1: Array search. Determine whether a target exists and, if so, its
 * index. The parallel version returns the smallest matching index using a
 * min-reduction, so the result is deterministic and matches serial search.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

static long search_serial(const int *a, long n, int target) {
    for (long i = 0; i < n; i++)
        if (a[i] == target) return i;
    return -1;
}

/* First (smallest) index where a[i]==target, or -1. No race: each thread
 * lowers a private copy of `found`, combined by reduction(min:). */
static long search_parallel(const int *a, long n, int target) {
    long found = n;                      /* sentinel: n means "not found" */
    #pragma omp parallel for reduction(min:found)
    for (long i = 0; i < n; i++)
        if (a[i] == target && i < found) found = i;
    return (found == n) ? -1 : found;
}

int main(void) {
    const long n = 100000000L;           /* 1e8 */
    int *a = malloc((size_t)n * sizeof(int));
    if (!a) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < n; i++) a[i] = (int)(i % 1000);  /* values 0..999 */
    a[n/2] = 12345;                      /* a unique element to find */

    /* correctness: present (unique), present (duplicated), absent */
    printf("Task 1: Array Search (N = %ld)\n", n);
    int targets[] = {12345, 500, 99999};
    for (int t = 0; t < 3; t++) {
        long s = search_serial(a, n, targets[t]);
        long p = search_parallel(a, n, targets[t]);
        printf("  target %-6d : serial idx=%ld, parallel idx=%ld  (%s)\n",
               targets[t], s, p, s == p ? "OK" : "MISMATCH");
    }

    /* timing: search for an ABSENT value so the whole array is scanned */
    const int absent = 99999;
    const int threads[] = {1, 2, 4, 8};
    double t0 = omp_get_wtime();
    volatile long sref = search_serial(a, n, absent); (void)sref;
    double ts = omp_get_wtime() - t0;
    printf("\n%8s %12s %12s %9s\n", "threads", "serial(s)", "parallel(s)", "speedup");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime();
        volatile long pr = search_parallel(a, n, absent); (void)pr;
        double tp = omp_get_wtime() - p0;
        printf("%8d %12.6f %12.6f %9.2f\n", threads[ti], ts, tp, tp > 0 ? ts/tp : 0.0);
    }
    free(a);
    return 0;
}

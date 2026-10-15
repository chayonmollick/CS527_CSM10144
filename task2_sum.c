/* CS527 Computer Systems — OpenMP Lab
 * Task 2: Sum of array elements.
 * Shows (1) the naive unsynchronised parallel sum is wrong/inconsistent,
 *       (2) reduction(+:sum) is correct every run,
 *       (3) a critical-section sum is correct but far slower.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

static long sum_serial(const long *a, long n) {
    long s = 0;
    for (long i = 0; i < n; i++) s += a[i];
    return s;
}

/* Intentionally broken: unsynchronised `sum += a[i]` is a data race. */
static long sum_naive_broken(const long *a, long n) {
    long sum = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        sum += a[i];
    return sum;
}

static long sum_reduction(const long *a, long n) {
    long sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < n; i++)
        sum += a[i];
    return sum;
}

static long sum_critical(const long *a, long n) {
    long sum = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp critical
        sum += a[i];
    }
    return sum;
}

int main(void) {
    const long n = 100000000L;                 /* 1e8 */
    long *a = malloc((size_t)n * sizeof(long));
    if (!a) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < n; i++) a[i] = (i % 10) + 1;

    long expected = sum_serial(a, n);
    printf("Task 2: Sum of Array Elements  (N = %ld)\n", n);
    printf("Serial reference sum = %ld\n\n", expected);

    omp_set_num_threads(8);
    printf("[Step 1] Naive parallel sum (BROKEN, 8 threads), 5 runs:\n");
    for (int r = 0; r < 5; r++) {
        long s = sum_naive_broken(a, n);
        printf("   run %d: %-12ld off by %ld\n", r + 1, s, expected - s);
    }

    printf("\n[Step 2+3] reduction(+:sum), 5 runs:\n");
    for (int r = 0; r < 5; r++) {
        long s = sum_reduction(a, n);
        printf("   run %d: %-12ld %s\n", r + 1, s, s == expected ? "OK" : "WRONG");
    }

    printf("\n[Step 4] reduction timing:\n");
    const long sizes[]  = {10000000L, 100000000L};
    const int  threads[] = {1, 2, 4, 8};
    printf("%12s %8s %12s %12s %9s\n", "N", "threads", "serial(s)", "reduce(s)", "speedup");
    for (int si = 0; si < 2; si++) {
        long m = sizes[si];
        double t0 = omp_get_wtime();
        volatile long ref = sum_serial(a, m); (void)ref;
        double ts = omp_get_wtime() - t0;
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double p0 = omp_get_wtime();
            volatile long s = sum_reduction(a, m); (void)s;
            double tp = omp_get_wtime() - p0;
            printf("%12ld %8d %12.6f %12.6f %9.2f\n",
                   m, threads[ti], ts, tp, tp > 0 ? ts / tp : 0.0);
        }
    }

    /* critical is correct but serialises every iteration: small N only. */
    const long small = 1000000L;
    omp_set_num_threads(8);
    double c0 = omp_get_wtime();
    long cres = sum_critical(a, small);
    double tc = omp_get_wtime() - c0;
    double r0 = omp_get_wtime();
    long rres = sum_reduction(a, small);
    double tr = omp_get_wtime() - r0;
    printf("\n[critical vs reduction] N=%ld, 8 threads: "
           "critical %.6fs (%s), reduction %.6fs, critical ~%.0fx slower\n",
           small, tc, cres == sum_serial(a, small) ? "correct" : "wrong",
           tr, tr > 0 ? tc / tr : 0.0);
    (void)rres;

    free(a);
    return 0;
}

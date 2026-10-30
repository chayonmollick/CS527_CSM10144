/* P024: Count elements above the average in one parallel region
 * Problem: Given an integer array a[N], compute the average of its elements and then
 *   count how many elements are strictly greater than the average. Do both steps
 *   inside ONE parallel region, explain why a barrier is needed between them, and
 *   verify against serial code.
 * Solution: one "#pragma omp parallel" region holding two worksharing loops.
 *   Loop 1: "#pragma omp for reduction(+:sum)". Its implicit barrier guarantees the
 *   combined sum is complete before anyone uses it. Then "#pragma omp single"
 *   computes avg once (its own implicit barrier makes avg visible to all threads);
 *   variant B lets every thread compute its own private avg instead (no single).
 *   Loop 2: "#pragma omp for reduction(+:count)". sum, avg and count are shared
 *   (declared before the region); the loop indices are private.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p024_above_average.c -o p024 -lm
 * Run:   ./p024 [N]   (default N = 1e7)
 */
#include "common.h"

static long above_serial(const int *a, long n, double *avg_out) {
    long sum = 0, count = 0;
    for (long i = 0; i < n; i++) sum += a[i];
    double avg = n > 0 ? (double)sum / n : 0.0;
    for (long i = 0; i < n; i++) if (a[i] > avg) count++;
    *avg_out = avg;
    return count;
}

/* Variant A: one thread computes avg in a single construct. */
static long above_single(const int *a, long n, double *avg_out) {
    long sum = 0, count = 0;
    double avg = 0.0;                         /* shared: declared outside the region */
    #pragma omp parallel
    {
        #pragma omp for reduction(+:sum)
        for (long i = 0; i < n; i++) sum += a[i];
        /* implicit barrier: sum is complete only after ALL threads added their part */

        #pragma omp single
        avg = n > 0 ? (double)sum / n : 0.0;
        /* implicit barrier at the end of single: no thread reads avg before it is set */

        #pragma omp for reduction(+:count)
        for (long i = 0; i < n; i++) if (a[i] > avg) count++;
    }
    *avg_out = avg;
    return count;
}

/* Variant B: every thread computes its own copy of avg (no single needed). */
static long above_private_avg(const int *a, long n, double *avg_out) {
    long sum = 0, count = 0;
    #pragma omp parallel
    {
        #pragma omp for reduction(+:sum)
        for (long i = 0; i < n; i++) sum += a[i];
        /* the barrier is still required: each thread must see the FINAL sum */

        double my_avg = n > 0 ? (double)sum / n : 0.0;   /* private: declared inside */

        #pragma omp for reduction(+:count)
        for (long i = 0; i < n; i++) if (a[i] > my_avg) count++;

        #pragma omp single
        *avg_out = my_avg;
    }
    return count;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    int *a = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, 0, 1000, 11);
    omp_warmup();

    double avg0 = 0, avgA = 0, avgB = 0, ts, tA, tB;
    long c0 = 0, cA = 0, cB = 0;
    BEST_TIME(ts, c0 = above_serial(a, n, &avg0));
    printf("P024: above-average count, N = %ld: average = %.6f, %ld elements above it\n", n, avg0, c0);
    printf("  A = omp single computes avg, B = every thread computes its own avg\n");
    printf("%8s %12s %12s %8s %12s %8s %6s\n", "threads", "serial(s)", "A(s)", "speedA", "B(s)", "speedB", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tA, cA = above_single(a, n, &avgA));
        BEST_TIME(tB, cB = above_private_avg(a, n, &avgB));
        int ok = cA == c0 && cB == c0 && avgA == avg0 && avgB == avg0;   /* integer sum: exact */
        printf("%8d %12.6f %12.6f %8.2f %12.6f %8.2f %6s\n", THREADS[k], ts, tA,
               tA > 0 ? ts / tA : 0.0, tB, tB > 0 ? ts / tB : 0.0, ok_str(ok));
    }
    free(a);
    return 0;
}

/* Notes:
 * - Why the barrier: a reduction's combined value exists only after every thread
 *   has added its private copy, which happens at the end of the loop. With
 *   "omp for nowait" on loop 1 a fast thread would read a partial sum: wrong avg.
 * - single has an implicit barrier at its end, so avg is written before loop 2.
 *   "single nowait" or "master" (no barrier!) would need an explicit
 *   "#pragma omp barrier" after it.
 * - The integer sum is exact (long), so avg and the count must match the serial
 *   result exactly; with doubles a value equal to the average could flip.
 * - One region instead of two parallel fors saves one fork/join; the barriers
 *   inside the region are cheaper than creating the team again.
 * - Shared vs private: sum, avg, count are declared before the region (shared);
 *   my_avg and the loop variables are declared inside (private).
 */

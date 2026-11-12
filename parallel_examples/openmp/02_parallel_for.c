/* 02_parallel_for.c - parallel for: share the iterations of a loop among the threads
 *
 * What: "#pragma omp parallel for" = a parallel region + a worksharing loop: the
 *       iterations are divided among the threads (schedule(static), the usual default:
 *       one contiguous block per thread). The loop variable is private, and all threads
 *       wait at the end of the loop (implicit barrier).
 * When: the iterations are independent - each one writes only its own element, like
 *       c[i] = a[i] + b[i]. NOT when iteration i uses the result of iteration i-1
 *       (a[i] = a[i-1] + x is a loop-carried dependence).
 * Expected output (OMP_NUM_THREADS=4; only the time varies):
 *   thread that ran each of 16 iterations: 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3
 *   c[i] = a[i] + b[i] for 4000000 elements in 0.0021 s; c[3999999] = 11999997
 *   every c[i] equals the serial answer 3i: OK
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 4000000

int main(void) {
    int owner[16];
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < 16; i++)
        owner[i] = omp_get_thread_num();    /* each i writes its own owner[i] */
    printf("thread that ran each of 16 iterations:");
    for (int i = 0; i < 16; i++) printf(" %d", owner[i]);
    printf("\n");

    double *a = malloc(N * sizeof *a), *b = malloc(N * sizeof *b), *c = malloc(N * sizeof *c);
    if (!a || !b || !c) { printf("out of memory\n"); return 1; }
    #pragma omp parallel for
    for (int i = 0; i < N; i++) { a[i] = i; b[i] = 2.0 * i; }

    double t = omp_get_wtime();
    #pragma omp parallel for
    for (int i = 0; i < N; i++)
        c[i] = a[i] + b[i];                 /* independent iterations: no race */
    t = omp_get_wtime() - t;

    long wrong = 0;
    for (int i = 0; i < N; i++)             /* serial check: c[i] = i + 2i */
        if (c[i] != 3.0 * i) wrong++;
    printf("c[i] = a[i] + b[i] for %d elements in %.4f s; c[%d] = %.0f\n", N, t, N - 1, c[N - 1]);
    printf("every c[i] equals the serial answer 3i: %s\n", wrong == 0 ? "OK" : "WRONG");
    free(a); free(b); free(c);
    return wrong == 0 ? 0 : 1;
}

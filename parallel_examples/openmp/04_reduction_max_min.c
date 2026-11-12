/* 04_reduction_max_min.c - reduction(max:) and reduction(min:), and the max WITH its index
 *
 * What: reduction(max:mx) gives each thread a private mx that starts at the smallest
 *       possible value; at the end the largest copy (and the old mx) wins. min likewise.
 *       (OpenMP 3.1, gcc >= 4.7.) It returns the VALUE only. For "the max and where it
 *       is", each thread keeps its own (value, index) and they are merged once per
 *       thread in a critical section; ties go to the smaller index (Lab 7 task 3).
 * When: largest / smallest element, range = max - min, argmax / argmin.
 *       Not reduction(max:index): that gives the largest INDEX, not where the max is.
 * Expected output (the same every run):
 *   parallel: max = 1000000 at index 246168, min = -1000000 (reduction(max:) = 1000000)
 *   serial  : max = 1000000 at index 246168, min = -1000000
 *   all equal the serial loop: OK
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define N 5000000

int main(void) {
    int *a = malloc(N * sizeof *a);
    if (!a) return 1;
    for (int i = 0; i < N; i++)             /* fixed pseudo-random values in -1000000..1000000 */
        a[i] = (int)(((unsigned)i * 2654435761u) % 2000001u) - 1000000;

    int mx = INT_MIN, mn = INT_MAX;
    #pragma omp parallel for reduction(max:mx) reduction(min:mn)
    for (int i = 0; i < N; i++) {
        if (a[i] > mx) mx = a[i];
        if (a[i] < mn) mn = a[i];
    }

    int best = a[0];                        /* max with its index */
    long where = 0;
    #pragma omp parallel
    {
        int my_best = a[0];                 /* this thread's best (value, index) */
        long my_where = 0;
        #pragma omp for nowait
        for (int i = 0; i < N; i++)
            if (a[i] > my_best) { my_best = a[i]; my_where = i; }
        #pragma omp critical                /* once per thread, not once per element */
        if (my_best > best || (my_best == best && my_where < where)) { best = my_best; where = my_where; }
    }

    int s_mx = a[0], s_mn = a[0];           /* serial check */
    long s_where = 0;
    for (int i = 1; i < N; i++) {
        if (a[i] > s_mx) { s_mx = a[i]; s_where = i; }
        if (a[i] < s_mn) s_mn = a[i];
    }
    printf("parallel: max = %d at index %ld, min = %d (reduction(max:) = %d)\n", best, where, mn, mx);
    printf("serial  : max = %d at index %ld, min = %d\n", s_mx, s_where, s_mn);
    int ok = mx == s_mx && mn == s_mn && best == s_mx && where == s_where;
    printf("all equal the serial loop: %s\n", ok ? "OK" : "WRONG");
    free(a);
    return ok ? 0 : 1;
}

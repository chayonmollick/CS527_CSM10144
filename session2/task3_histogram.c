/* CS527 Computer Systems — OpenMP Lab 8
 * Task 3: Histogram of integers in [0,9]. Shows the data race in a naive
 * shared-bin version, then two correct, race-free parallel methods.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#define BINS 10

static void hist_serial(const int *a, long n, long *h) {
    for (int b = 0; b < BINS; b++) h[b] = 0;
    for (long i = 0; i < n; i++) h[a[i]]++;
}

/* BROKEN: all threads increment the same shared bins -> data race. */
static void hist_broken(const int *a, long n, long *h) {
    for (int b = 0; b < BINS; b++) h[b] = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) h[a[i]]++;
}

/* Correct method A: per-thread private histogram, combined once at the end. */
static void hist_local(const int *a, long n, long *h) {
    for (int b = 0; b < BINS; b++) h[b] = 0;
    #pragma omp parallel
    {
        long local[BINS] = {0};
        #pragma omp for
        for (long i = 0; i < n; i++) local[a[i]]++;
        #pragma omp critical
        for (int b = 0; b < BINS; b++) h[b] += local[b];
    }
}

/* Correct method B: array-section reduction (OpenMP 4.5+). */
static void hist_reduction(const int *a, long n, long *h) {
    for (int b = 0; b < BINS; b++) h[b] = 0;
    #pragma omp parallel for reduction(+:h[:BINS])
    for (long i = 0; i < n; i++) h[a[i]]++;
}

static int equal_hist(const long *x, const long *y) {
    for (int b = 0; b < BINS; b++) if (x[b] != y[b]) return 0;
    return 1;
}

int main(void) {
    const long n = 100000000L;           /* 1e8 */
    int *a = malloc((size_t)n * sizeof(int));
    if (!a) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < n; i++) a[i] = (int)((i * 1103515245L + 12345L) % BINS);

    long ref[BINS], hb[BINS], hl[BINS], hr[BINS];
    hist_serial(a, n, ref);
    printf("Task 3: Histogram of values 0..9 (N = %ld)\n", n);
    printf("serial counts: ");
    for (int b = 0; b < BINS; b++) printf("%ld ", ref[b]);
    printf("\n\n");

    omp_set_num_threads(8);
    printf("Naive shared-bin version (BROKEN, 8 threads), 5 runs:\n");
    for (int r = 0; r < 5; r++) {
        hist_broken(a, n, hb);
        long tot = 0; for (int b = 0; b < BINS; b++) tot += hb[b];
        printf("   run %d: total counted = %ld (should be %ld) -> %s\n",
               r+1, tot, n, tot == n ? "OK" : "LOST UPDATES");
    }

    hist_local(a, n, hl);
    hist_reduction(a, n, hr);
    printf("\nper-thread-local method : %s\n", equal_hist(ref, hl) ? "OK" : "WRONG");
    printf("array-reduction method  : %s\n", equal_hist(ref, hr) ? "OK" : "WRONG");

    const int threads[] = {1, 2, 4, 8};
    printf("\n%8s %14s %14s\n", "threads", "local(s)", "reduction(s)");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double l0 = omp_get_wtime(); hist_local(a, n, hl);     double tl = omp_get_wtime()-l0;
        double r0 = omp_get_wtime(); hist_reduction(a, n, hr); double tr = omp_get_wtime()-r0;
        printf("%8d %14.6f %14.6f\n", threads[ti], tl, tr);
    }
    free(a);
    return 0;
}

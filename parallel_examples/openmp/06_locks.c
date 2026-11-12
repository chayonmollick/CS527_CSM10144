/* 06_locks.c - omp_lock_t: locks you set and unset yourself, here one per bin
 *
 * What: omp_init_lock(&l) once; omp_set_lock(&l) waits until l is free and takes it;
 *       omp_unset_lock(&l) releases it; omp_destroy_lock(&l) at the end.
 *       omp_test_lock(&l) takes l only if it is free (returns 0 instead of waiting).
 *       Like critical, but a lock is a variable: with one lock PER BIN, threads that
 *       update different bins never wait for each other (critical = one lock for all).
 * When: fine-grained protection: one lock per bin / bank account / list / tree node.
 *       Always unset what you set. If a thread needs two locks, take them in a fixed
 *       order (e.g. the smaller index first), or two threads can deadlock.
 * Expected output (the same every run):
 *   bin:          0       1       2       3       4       5       6       7
 *   count:   249998  249996  250003  249999  249998  250004  249999  250003
 *   total 2000000; every bin equals the serial histogram: OK
 */
#include <omp.h>
#include <stdio.h>

#define N 2000000
#define BINS 8

static int bin_of(int i) {                  /* a fixed pseudo-random bin 0..7 for value i */
    return (int)((((unsigned)i * 2654435761u) >> 16) % BINS);
}

int main(void) {
    long hist[BINS] = {0}, serial[BINS] = {0};
    omp_lock_t lock[BINS];
    for (int b = 0; b < BINS; b++) omp_init_lock(&lock[b]);

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        int b = bin_of(i);
        omp_set_lock(&lock[b]);             /* only threads that hit the SAME bin wait */
        hist[b]++;
        omp_unset_lock(&lock[b]);
    }
    for (int b = 0; b < BINS; b++) omp_destroy_lock(&lock[b]);

    for (int i = 0; i < N; i++) serial[bin_of(i)]++;    /* serial histogram */
    long total = 0;
    int ok = 1;
    printf("bin:   ");
    for (int b = 0; b < BINS; b++) printf("%8d", b);
    printf("\ncount: ");
    for (int b = 0; b < BINS; b++) {
        printf("%8ld", hist[b]);
        total += hist[b];
        ok = ok && hist[b] == serial[b];
    }
    printf("\ntotal %ld; every bin equals the serial histogram: %s\n", total, ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

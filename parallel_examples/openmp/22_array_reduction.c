/* 22_array_reduction.c - reduction over a whole array: reduction(+:hist[:B])  (OpenMP 4.5)
 *
 * What: every thread gets a private, zeroed copy of the elements hist[0..B-1]; at the end
 *       of the loop the copies are added element by element into hist. The array
 *       section is name[start:length], and [:B] means [0:B]. For a malloc'ed pointer the
 *       section is required: reduction(+:cs[0:COLS]). A real array may also be written
 *       reduction(+:hist).
 * When: histograms, counts per category, column sums, y = A^T x: any loop in which many
 *       iterations add into a small array. Needs gcc >= 6. It costs B elements per
 *       thread, so for a huge B use atomic updates or a lock per bin (06) instead.
 * Expected output (the same every run):
 *   histogram of 1000000 values into 10 bins: 100017 100002 99995 100023 100001 100009 99991 99989 99995 99978
 *   column sums of a 100000 x 8 matrix: 4950000 4900000 4950000 4800000 4750000 4900000 4950000 4800000
 *   both equal the serial loops: OK
 */
#include <stdio.h>
#include <stdlib.h>

#define N 1000000
#define B 10
#define ROWS 100000
#define COLS 8

static int value(int i) { return (int)((((unsigned)i * 2654435761u) >> 16) % B); }

int main(void) {
    long hist[B] = {0}, s_hist[B] = {0};
    #pragma omp parallel for reduction(+:hist[:B])  /* a private hist[0..B-1] per thread */
    for (int i = 0; i < N; i++)
        hist[value(i)]++;
    for (int i = 0; i < N; i++) s_hist[value(i)]++;

    long *cs = calloc(COLS, sizeof *cs), s_cs[COLS] = {0};
    if (!cs) return 1;
    #pragma omp parallel for reduction(+:cs[0:COLS])  /* pointer: the section is required */
    for (int r = 0; r < ROWS; r++)          /* every row adds into all COLS sums */
        for (int c = 0; c < COLS; c++)
            cs[c] += (r * (c + 1)) % 100;   /* the matrix element A[r][c] */
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) s_cs[c] += (r * (c + 1)) % 100;

    int ok = 1;
    printf("histogram of %d values into %d bins:", N, B);
    for (int b = 0; b < B; b++) { printf(" %ld", hist[b]); ok = ok && hist[b] == s_hist[b]; }
    printf("\ncolumn sums of a %d x %d matrix:", ROWS, COLS);
    for (int c = 0; c < COLS; c++) { printf(" %ld", cs[c]); ok = ok && cs[c] == s_cs[c]; }
    printf("\nboth equal the serial loops: %s\n", ok ? "OK" : "WRONG");
    free(cs);
    return ok ? 0 : 1;
}

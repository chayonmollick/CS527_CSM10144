/* 13_collapse.c - collapse(2): share two nested loops as one
 *
 * What: collapse(2) turns "for i < n { for j < m }" into ONE loop of n*m iterations,
 *       and that bigger loop is what gets divided among the threads.
 * When: the outer loop alone has too few iterations for the threads (here 3 rows for
 *       4 threads: one thread gets nothing) or they do not divide evenly.
 *       Rules: perfectly nested loops (no code between the two for lines), rectangular
 *       (m must not depend on i). With n much larger than the thread count it changes
 *       nothing. Never collapse a loop that must stay serial (the k of a matmul sum).
 * Expected output (OMP_NUM_THREADS=4; the same every run):
 *   outer loop only: 3 of 4 threads got work, the busiest did 1000 iterations
 *   collapse(2)    : 4 of 4 threads got work, the busiest did  750 iterations
 *   C = A + B is correct both ways: OK
 */
#include <omp.h>
#include <stdio.h>

#define ROWS 3
#define COLS 1000
#define MAXT 256

static double A[ROWS][COLS], B[ROWS][COLS], C[ROWS][COLS];
static int owner[ROWS][COLS];               /* which thread computed C[i][j] */

static int report(const char *how) {        /* prints the work per thread; returns 1 if C is right */
    int per[MAXT] = {0}, used = 0, most = 0, right = 1;
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++) {
            if (owner[i][j] < MAXT) per[owner[i][j]]++;
            right = right && C[i][j] == A[i][j] + B[i][j];
        }
    for (int t = 0; t < MAXT; t++) {
        if (per[t] > 0) used++;
        if (per[t] > most) most = per[t];
    }
    printf("%-15s: %d of %d threads got work, the busiest did %4d iterations\n",
           how, used, omp_get_max_threads(), most);
    return right;
}

int main(void) {
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++) { A[i][j] = i + j; B[i][j] = i * j; }

    #pragma omp parallel for                /* only ROWS = 3 iterations to share */
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++) {
            C[i][j] = A[i][j] + B[i][j];
            owner[i][j] = omp_get_thread_num();
        }
    int ok = report("outer loop only");

    #pragma omp parallel for collapse(2)    /* ROWS * COLS = 3000 iterations to share */
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++) {
            C[i][j] = A[i][j] + B[i][j];
            owner[i][j] = omp_get_thread_num();
        }
    ok = report("collapse(2)") && ok;

    printf("C = A + B is correct both ways: %s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

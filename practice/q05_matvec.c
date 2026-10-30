/* CS527 Computer Systems — OpenMP practice
 * Q05: Matrix-vector product y = A*x (A is N x N, flattened row-major).
 *      Parallelise it row-wise and column-wise; verify both and compare times.
 *  - serial row-oriented (i outer, dot product of row i with x)
 *  - parallel row-oriented: parallel for over i, each thread owns y[i]
 *  - serial column-oriented (j outer: y += x[j] * column j)
 *  - parallel column-oriented: parallel for over j with array reduction
 *      reduction(+:y[:n])   (OpenMP 4.5+)
 * Usage: ./q05_matvec [N]   (default N = 2000 and 8000; 8000 needs 512 MB)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void mv_row_serial(const double *A, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j] * x[j];
        y[i] = s;
    }
}

/* Race-free: iteration i writes only y[i]; s and j are private (loop-local). */
static void mv_row_parallel(const double *A, const double *x, double *y, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j] * x[j];
        y[i] = s;
    }
}

static void mv_col_serial(const double *A, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = 0.0;
    for (long j = 0; j < n; j++)
        for (long i = 0; i < n; i++) y[i] += A[i*n + j] * x[j];   /* stride-n access */
}

/* Different j's add into the SAME y[i] -> race without the reduction.
 * reduction(+:y[:n]) gives each thread a private zeroed copy of y[0..n-1],
 * and adds all copies into y at the end. */
static void mv_col_parallel(const double *A, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = 0.0;
    #pragma omp parallel for reduction(+:y[:n])
    for (long j = 0; j < n; j++)
        for (long i = 0; i < n; i++) y[i] += A[i*n + j] * x[j];
}

static int close_enough(const double *y, const double *ref, long n) {
    for (long i = 0; i < n; i++)
        if (fabs(y[i] - ref[i]) > 1e-9 * (1.0 + fabs(ref[i]))) return 0;
    return 1;
}

static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static void run_size(long n) {
    double *A = malloc((size_t)(n * n) * sizeof(double));
    double *x = malloc((size_t)n * sizeof(double));
    double *ref = malloc((size_t)n * sizeof(double));
    double *y = malloc((size_t)n * sizeof(double));
    if (!A || !x || !ref || !y) { printf("N=%ld skipped (alloc failed)\n", n);
        free(A); free(x); free(ref); free(y); return; }
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) A[i*n + j] = (double)((i + 2*j) % 11) - 5.0;
    for (long j = 0; j < n; j++) x[j] = 1.0 + (double)(j % 5) * 0.25;

    mv_row_parallel(A, x, y, n);                    /* untimed warm-up runs */
    mv_row_serial(A, x, ref, n);
    double t0 = omp_get_wtime(); mv_row_serial(A, x, ref, n); double ts_row = omp_get_wtime() - t0;
    double c0 = omp_get_wtime(); mv_col_serial(A, x, y, n);   double ts_col = omp_get_wtime() - c0;
    printf("\nN = %ld   serial row = %.4f s   serial col = %.4f s  (col check %s)\n",
           n, ts_row, ts_col, close_enough(y, ref, n) ? "OK" : "WRONG");

    const int threads[] = {1, 2, 4, 8};
    printf("%8s %10s %12s %10s %12s %6s\n", "threads", "row(s)", "sp vs row", "col(s)", "sp vs col", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double r0 = omp_get_wtime(); mv_row_parallel(A, x, y, n); double tr = omp_get_wtime() - r0;
        int ok = close_enough(y, ref, n);
        double k0 = omp_get_wtime(); mv_col_parallel(A, x, y, n); double tk = omp_get_wtime() - k0;
        ok = ok && close_enough(y, ref, n);
        printf("%8d %10.4f %12.2f %10.4f %12.2f %6s\n", threads[ti],
               tr, speedup(ts_row, tr), tk, speedup(ts_col, tk), ok ? "OK" : "WRONG");
    }
    free(A); free(x); free(ref); free(y);
}

int main(int argc, char **argv) {
    printf("Q05: Matrix-vector product y = A*x  (speedup = T_serial / T_parallel)\n");
    if (argc > 1) {
        long n = (long)atof(argv[1]);
        if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
        run_size(n);
    } else {
        const long sizes[] = {2000L, 8000L};
        for (int si = 0; si < 2; si++) run_size(sizes[si]);
    }
    return 0;
}

/* Viva notes:
 * - Row version is race-free with no clause: each i is done by exactly one
 *   thread and writes only y[i]; the inner sum s is a loop-local (private).
 *   Parallelise the OUTER loop: one fork/join, n/threads rows each.
 * - Column version: every j-iteration updates ALL y[i], so threads with
 *   different j collide on y[i] -> lost updates without reduction(+:y[:n]).
 *   Each thread then holds a private n-element y (extra memory n*threads),
 *   merged at the end. Older alternative: per-thread local array + critical,
 *   or atomic on each y[i] (very slow).
 * - For large N the column version is slower even serially: A is row-major,
 *   so walking down a column jumps n*8 bytes each step -> cache and TLB
 *   misses. Row version reads A contiguously (prefetch-friendly). At small N
 *   (2000) the n lines touched stay in L2 for the next j, and the column
 *   version can win because its y[i] updates are independent, while the row
 *   version's `s +=` is one serial chain of dependent FP adds.
 * - Speedup is limited: mat-vec does 2 flops per 8-byte element of A, read
 *   once -> memory-bandwidth bound; extra threads stop helping once DRAM
 *   bandwidth is saturated.
 * - Results differ from serial only by FP rounding (summation order).
 */

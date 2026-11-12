/* M02: Matrix norms: 1-norm, infinity-norm and max-abs norm
 * Problem: For an N x N matrix A of doubles compute, serially and with OpenMP,
 *     ||A||_1   = max over columns j of sum_i |A[i][j]|   (largest column sum),
 *     ||A||_inf = max over rows i of sum_j |A[i][j]|      (largest row sum),
 *     ||A||_max = max over all (i, j) of |A[i][j]|.
 *   Verify against serial (and on a 3 x 3 example by hand) and report the time,
 *   speedup and efficiency for 1, 2, 4 and 8 threads.
 * Solution: infinity-norm: parallel for over rows; the row sum s is private
 *   (declared inside the loop) and the maximum over the rows is reduction(max:).
 *   1-norm: (a) parallel for over COLUMNS: each iteration sums its own column
 *   (stride-N reads, cache-unfriendly) and the maximum is reduction(max:);
 *   (b) parallel for over ROWS with the array reduction reduction(+:cs[:n])
 *   (OpenMP 4.5): each thread adds its rows into a private, zeroed copy of the
 *   column-sum vector, the copies are added at the end, and a short serial loop
 *   takes the maximum. max-abs: collapse(2) reduction(max:m) over all (i, j).
 *   Each row (or column) sum is added in the serial order except in (b), whose
 *   column sums add the thread blocks in another order: it is compared with a
 *   relative tolerance, the others are bitwise equal.
 * Source: lab extension (MATRIX_RULES.md section 3: norms; the Frobenius norm is
 *   in practice/q10_matrix_stats.c).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m02_matrix_norms.c -o m02 -lm
 * Run:   ./m02 [N]   (default N = 3000)
 */
#include "../../problems/common.h"

/* ---------- serial reference ---------- */
static double norm_inf_serial(const double *A, long n) {
    double best = 0.0;
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += fabs(A[i*n + j]);
        if (s > best) best = s;
    }
    return best;
}

static double norm_1_serial(const double *A, long n, double *cs) {
    for (long j = 0; j < n; j++) cs[j] = 0.0;
    for (long i = 0; i < n; i++)                       /* row by row: contiguous */
        for (long j = 0; j < n; j++) cs[j] += fabs(A[i*n + j]);
    double best = 0.0;
    for (long j = 0; j < n; j++) if (cs[j] > best) best = cs[j];
    return best;
}

static double norm_max_serial(const double *A, long n) {
    double best = 0.0;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (fabs(A[i*n + j]) > best) best = fabs(A[i*n + j]);
    return best;
}

/* ---------- parallel versions ---------- */
static double norm_inf_parallel(const double *A, long n) {
    double best = 0.0;
    #pragma omp parallel for reduction(max:best)
    for (long i = 0; i < n; i++) {
        double s = 0.0;                                 /* private: one row */
        for (long j = 0; j < n; j++) s += fabs(A[i*n + j]);
        if (s > best) best = s;
    }
    return best;
}

/* (a) one column per iteration: no shared writes, but stride-N reads. */
static double norm_1_by_columns(const double *A, long n) {
    double best = 0.0;
    #pragma omp parallel for reduction(max:best)
    for (long j = 0; j < n; j++) {
        double s = 0.0;
        for (long i = 0; i < n; i++) s += fabs(A[i*n + j]);
        if (s > best) best = s;
    }
    return best;
}

/* (b) rows in parallel; all rows add into the same cs[] -> array reduction. */
static double norm_1_by_rows(const double *A, long n, double *cs) {
    for (long j = 0; j < n; j++) cs[j] = 0.0;
    #pragma omp parallel for reduction(+:cs[:n])
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) cs[j] += fabs(A[i*n + j]);
    double best = 0.0;                                  /* n values: serial is fine */
    for (long j = 0; j < n; j++) if (cs[j] > best) best = cs[j];
    return best;
}

static double norm_max_parallel(const double *A, long n) {
    double best = 0.0;
    #pragma omp parallel for collapse(2) reduction(max:best)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (fabs(A[i*n + j]) > best) best = fabs(A[i*n + j]);
    return best;
}

static void hand_example(void) {
    const double A[9] = { 1, -2,  3,
                         -4,  5, -6,
                          7, -8,  9};
    double cs[3];
    double n1a = norm_1_by_columns(A, 3), n1b = norm_1_by_rows(A, 3, cs);
    double ninf = norm_inf_parallel(A, 3), nmax = norm_max_parallel(A, 3);
    printf("Hand example: A = [1 -2 3; -4 5 -6; 7 -8 9]\n");
    printf("  column sums 12 15 18 -> ||A||_1 = %g and %g (expected 18): %s\n",
           n1a, n1b, ok_str(n1a == 18.0 && n1b == 18.0));
    printf("  row sums 6 15 24 -> ||A||_inf = %g (expected 24): %s\n", ninf, ok_str(ninf == 24.0));
    printf("  largest |a_ij| -> ||A||_max = %g (expected 9): %s\n\n", nmax, ok_str(nmax == 9.0));
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 3000);
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    double *A = xmalloc((size_t)n * n * sizeof(double));
    double *cs = xmalloc((size_t)n * sizeof(double));
    fill_double(A, n * n, -1.0, 1.0, 5);
    omp_warmup();

    hand_example();
    double rinf = 0, r1 = 0, rmax = 0, p = 0, ts, tp;
    BEST_TIME(ts, rinf = norm_inf_serial(A, n));
    printf("M02: N = %ld, entries uniform in [-1, 1)\n", n);
    printf("serial: ||A||_inf = %.6f\n", rinf);
    printf("infinity-norm: parallel for over rows, reduction(max:)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, p = norm_inf_parallel(A, n));
        table_row(n, THREADS[k], ts, tp, p == rinf);
    }

    BEST_TIME(ts, r1 = norm_1_serial(A, n, cs));
    printf("\nserial: ||A||_1 = %.6f (column sums added row by row)\n", r1);
    printf("1-norm (a): parallel for over columns, reduction(max:), stride-N reads\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, p = norm_1_by_columns(A, n));
        table_row(n, THREADS[k], ts, tp, p == r1);
    }
    printf("1-norm (b): parallel for over rows, reduction(+:cs[:n]), then a serial max\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, p = norm_1_by_rows(A, n, cs));
        table_row(n, THREADS[k], ts, tp, close1(r1, p, 1e-12));
    }

    BEST_TIME(ts, rmax = norm_max_serial(A, n));
    printf("\nserial: ||A||_max = %.6f\n", rmax);
    printf("max-abs norm: collapse(2) reduction(max:)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, p = norm_max_parallel(A, n));
        table_row(n, THREADS[k], ts, tp, p == rmax);
    }
    free(A); free(cs);
    return 0;
}

/* Notes:
 * - Which is which: 1-norm = largest COLUMN sum of |a_ij|, infinity-norm = largest
 *   ROW sum (the "1" is a vertical stroke: columns). ||A||_1 = ||A^T||_inf.
 *   Frobenius = sqrt(sum a_ij^2) (practice/q10). For any norm here, ||kA|| = |k| ||A||.
 * - reduction(max:best) (OpenMP 3.1, C/C++): every thread starts from the most
 *   negative double, keeps its own maximum, and the maxima are combined at the end.
 *   The starting value 0.0 of best is also included, which is fine: norms are >= 0.
 * - The row sum s must be private: declared inside the loop it is. A shared s
 *   would mix the rows of different threads (race).
 * - Column sums: parallel over columns has no race but reads A with stride N
 *   (one cache line per element); parallel over rows reads contiguously but all
 *   threads add into the same cs[], so it needs reduction(+:cs[:n]) (or per-thread
 *   arrays + critical). The private copies of an array reduction live on each
 *   thread's stack in gcc: fine for N doubles, not for huge arrays.
 * - The max over the n column sums is O(N) after O(N^2) work: leave it serial.
 * - All three norms are O(N^2) reads with one add each: memory-bound, so the
 *   speedup levels off when the memory bandwidth is used up.
 */

/* P070: LU decomposition without pivoting (Doolittle, in place)
 * Problem: Factor an N x N strictly diagonally dominant matrix A as A = L*U,
 *   with L unit lower triangular and U upper triangular, in place (U on and above
 *   the diagonal, the multipliers of L below it), without pivoting, serially and
 *   with OpenMP. Explain which loops can run in parallel, verify that L*U
 *   reproduces A and that the parallel factors match serial, and report timings.
 * Solution: for k = 0..N-2: for every row i > k, l = A[i][k] / A[k][k] is stored
 *   in A[i][k] and row i is updated: A[i][j] -= l * A[k][j] for j > k. The k loop
 *   is SEQUENTIAL: step k uses the values produced by step k-1. For a fixed k the
 *   rows i > k are independent: each reads the pivot row k (not changed in this
 *   step) and writes only its own row. (a) one parallel region around the k
 *   loop; every thread runs the k loop and "#pragma omp for" shares the rows i;
 *   its implicit barrier finishes step k before step k+1 begins (k is private:
 *   declared in the loop). (b) the same with two omp for loops per step: first
 *   the multipliers (column k), then the trailing update over (i, j) with
 *   collapse(2) (rectangular for fixed k). Diagonal dominance guarantees nonzero
 *   pivots, so no pivoting is needed. Matrices are flattened row-major:
 *   A[i][j] = A[i*N + j].
 * Source: textbook (Eijkhout, Parallel Programming) Ex 19.10.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p070_lu_decomposition.c -o p070 -lm
 * Run:   ./p070 [N]   (default N = 1000)
 */
#include "common.h"

static void lu_serial(double *A, long n) {
    for (long k = 0; k < n - 1; k++)
        for (long i = k + 1; i < n; i++) {
            double l = A[i*n + k] / A[k*n + k];
            A[i*n + k] = l;                               /* L below the diagonal */
            for (long j = k + 1; j < n; j++) A[i*n + j] -= l * A[k*n + j];
        }
}

/* (a) rows of each step shared by omp for; one parallel region for all steps */
static void lu_par_rows(double *A, long n) {
    #pragma omp parallel
    for (long k = 0; k < n - 1; k++) {                   /* every thread runs k */
        #pragma omp for
        for (long i = k + 1; i < n; i++) {
            double l = A[i*n + k] / A[k*n + k];
            A[i*n + k] = l;
            for (long j = k + 1; j < n; j++) A[i*n + j] -= l * A[k*n + j];
        }
        /* implicit barrier: step k complete before any thread starts step k+1 */
    }
}

/* (b) multipliers first, then the trailing (i, j) update with collapse(2) */
static void lu_par_collapse(double *A, long n) {
    #pragma omp parallel
    for (long k = 0; k < n - 1; k++) {
        #pragma omp for
        for (long i = k + 1; i < n; i++) A[i*n + k] /= A[k*n + k];
        /* implicit barrier: all multipliers of column k are ready */
        #pragma omp for collapse(2)
        for (long i = k + 1; i < n; i++)
            for (long j = k + 1; j < n; j++)
                A[i*n + j] -= A[i*n + k] * A[k*n + j];
    }
}

/* max |(L*U)[i][j] - A0[i][j]|, L unit lower and U upper, both stored in LU. */
static double lu_residual(const double *LU, const double *A0, long n) {
    double maxerr = 0.0;
    #pragma omp parallel reduction(max:maxerr)
    {
        double *r = xmalloc((size_t)n * sizeof(double)); /* private row buffer */
        #pragma omp for
        for (long i = 0; i < n; i++) {
            for (long j = 0; j < n; j++) r[j] = 0.0;
            for (long k = 0; k <= i; k++) {               /* row i of L*U, row-wise */
                double l = (k < i) ? LU[i*n + k] : 1.0;   /* L[i][i] = 1 */
                for (long j = k; j < n; j++) r[j] += l * LU[k*n + j];   /* U[k][j], j >= k */
            }
            for (long j = 0; j < n; j++)
                if (fabs(r[j] - A0[i*n + j]) > maxerr) maxerr = fabs(r[j] - A0[i*n + j]);
        }
        free(r);
    }
    return maxerr;
}

/* Strictly diagonally dominant: off-diagonal in [-1, 1), diagonal = row sum + 1. */
static void make_dominant(double *A, long n) {
    fill_double(A, n * n, -1.0, 1.0, 70);
    for (long i = 0; i < n; i++) {
        double off = 0.0;
        for (long j = 0; j < n; j++) if (j != i) off += fabs(A[i*n + j]);
        A[i*n + i] = off + 1.0;
    }
}

static void tiny_example(void) {
    double A[9] = {2, 1, 1,
                   4, 3, 3,
                   8, 7, 9};
    omp_set_num_threads(4);
    lu_par_rows(A, 3);
    printf("Worked example: A = [2 1 1; 4 3 3; 8 7 9]\n");
    printf("  L = [1 0 0; %g 1 0; %g %g 1]   (expected [1 0 0; 2 1 0; 4 3 1])\n", A[3], A[6], A[7]);
    printf("  U = [%g %g %g; 0 %g %g; 0 0 %g]   (expected [2 1 1; 0 1 1; 0 0 2])\n",
           A[0], A[1], A[2], A[4], A[5], A[8]);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000);
    long nn = n * n;
    size_t bytes = (size_t)nn * sizeof(double);
    double *A0 = xmalloc(bytes), *LUs = xmalloc(bytes), *W = xmalloc(bytes);
    make_dominant(A0, n);
    omp_warmup();

    tiny_example();

    double maxa = 0.0;
    for (long t = 0; t < nn; t++) if (fabs(A0[t]) > maxa) maxa = fabs(A0[t]);
    double ts, tp;
    /* LU works in place, so every timed run first copies A0 (O(N^2), small). */
    BEST_TIME(ts, memcpy(LUs, A0, bytes); lu_serial(LUs, n));
    omp_set_num_threads(4);
    double res = lu_residual(LUs, A0, n) / maxa;
    printf("\nP070: LU of a strictly diagonally dominant N x N matrix, N = %ld\n", n);
    printf("serial: max |L*U - A| / max|A| = %.2e  -> %s\n", res, ok_str(res < 1e-12));

    printf("\n(a) k loop sequential; rows i shared by omp for (one parallel region)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, memcpy(W, A0, bytes); lu_par_rows(W, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(LUs, W, nn, 1e-10));
    }
    omp_set_num_threads(4);
    res = lu_residual(W, A0, n) / maxa;
    printf("    parallel factors: max |L*U - A| / max|A| = %.2e -> %s\n", res, ok_str(res < 1e-12));

    printf("(b) k loop sequential; multipliers, barrier, then collapse(2) over (i, j)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, memcpy(W, A0, bytes); lu_par_collapse(W, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(LUs, W, nn, 1e-10));
    }
    omp_set_num_threads(4);
    res = lu_residual(W, A0, n) / maxa;
    printf("    parallel factors: max |L*U - A| / max|A| = %.2e -> %s\n", res, ok_str(res < 1e-12));

    free(A0); free(LUs); free(W);
    return 0;
}

/* Notes:
 * - k carries a dependence (step k reads what step k-1 wrote), so it stays
 *   serial; i (and j) inside one step are independent -> parallel.
 * - Parallelising the k loop would let step k+1 read a pivot row that step k has
 *   not finished updating: wrong factors.
 * - One parallel region outside k avoids N fork/joins; every thread must meet the
 *   same sequence of omp for constructs (k is private and identical in all
 *   threads). Never add nowait to these loops: the barrier is what orders the steps.
 * - Work shrinks every step ((N-k)^2 updates), so late steps have too little work
 *   for many threads; the total is about N^3/3 multiply-adds. Each step streams
 *   the trailing matrix through memory (memory-bound), plus N barriers: expect
 *   modest speedups (blocked LU is the fix).
 * - collapse(2) is legal in (b) because for a fixed k the (i, j) ranges are
 *   rectangular; it only pays when N-k is small compared with the thread count.
 * - No pivoting is safe only for matrices like this one (strictly diagonally
 *   dominant, or symmetric positive definite); in general use partial pivoting (p071).
 */

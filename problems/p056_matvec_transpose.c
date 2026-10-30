/* P056: Transposed matrix-vector product y = A^T x
 * Problem: Given an M x N matrix A and a vector x of length M, compute the
 *   length-N vector y = A^T x, i.e. y[j] = sum_i A[i][j]*x[i], without forming
 *   A^T, serially and with OpenMP in three ways: (a) parallel over the output
 *   index j, each thread walking down column j of A; (b) parallel over the rows i
 *   with the array reduction reduction(+:y[:n]); (c) form T = A^T explicitly
 *   (in parallel) and then do an ordinary row-wise matvec y = T x. Verify all
 *   three, compare their times and explain the cache behaviour.
 * Solution: (a) y[j] is owned by one thread, which sums into a private s and
 *   writes y[j] once: race-free, but A is read with stride N (one useful double
 *   per cache line per row). (b) row i adds x[i]*A[i][0..N-1] into ALL of y, so
 *   threads would race on y; reduction(+:y[:n]) gives each thread a private,
 *   zeroed copy of y and adds the copies into y at the end (OpenMP 4.5 array
 *   section reduction). A is read contiguously. (c) the transpose is parallel
 *   over the rows of A (each T[j][i] is written exactly once), then
 *   y[j] = dot(row j of T, x) is parallel over j with contiguous reads; it costs
 *   an extra pass over memory and M*N more doubles. Matrices are flattened
 *   row-major: A[i][j] = A[i*N + j], T[j][i] = T[j*M + i].
 * Source: lab extension of Lab 7 matrix-vector product (Eijkhout Ch. 19 matvec).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p056_matvec_transpose.c -o p056 -lm
 * Run:   ./p056 [M] [N]   (defaults M = N = 3000)
 */
#include "common.h"

/* Serial reference, cache-friendly order: row i of A scaled by x[i] is added to y. */
static void atx_serial(const double *A, const double *x, double *y, long m, long n) {
    for (long j = 0; j < n; j++) y[j] = 0.0;
    for (long i = 0; i < m; i++) {
        double xi = x[i];
        for (long j = 0; j < n; j++) y[j] += A[i*n + j] * xi;
    }
}

/* (a) parallel over output j: thread owns y[j]; reads column j (stride n). */
static void atx_by_column(const double *A, const double *x, double *y, long m, long n) {
    #pragma omp parallel for
    for (long j = 0; j < n; j++) {
        double s = 0.0;                         /* private partial sum */
        for (long i = 0; i < m; i++) s += A[i*n + j] * x[i];
        y[j] = s;                               /* one write per j: no race */
    }
}

/* (b) parallel over rows i: every row updates all of y -> array reduction. */
static void atx_by_row_reduction(const double *A, const double *x, double *y, long m, long n) {
    for (long j = 0; j < n; j++) y[j] = 0.0;   /* reduction adds into these */
    #pragma omp parallel for reduction(+:y[:n])
    for (long i = 0; i < m; i++) {
        double xi = x[i];
        for (long j = 0; j < n; j++) y[j] += A[i*n + j] * xi;  /* private copy of y */
    }
}

/* (c) phase 1: T = A^T (N x M); phase 2: y = T x, row-wise. */
static void transpose_par(const double *A, double *T, long m, long n) {
    #pragma omp parallel for
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            T[j*m + i] = A[i*n + j];           /* each T element written once */
}
static void matvec_rows_par(const double *T, const double *x, double *y, long n, long m) {
    #pragma omp parallel for
    for (long j = 0; j < n; j++) {
        double s = 0.0;
        for (long i = 0; i < m; i++) s += T[j*m + i] * x[i];   /* contiguous */
        y[j] = s;
    }
}
static void atx_transpose_first(const double *A, double *T, const double *x, double *y,
                                long m, long n) {
    transpose_par(A, T, m, n);
    matvec_rows_par(T, x, y, n, m);
}

static void tiny_example(void) {
    /* A is 2 x 3, x has length 2, y = A^T x has length 3 */
    double A[6] = {1, 2, 3,
                   4, 5, 6};
    double x[2] = {1, 10};
    double y[3];
    atx_by_row_reduction(A, x, y, 2, 3);
    printf("Worked example: A = [1 2 3; 4 5 6] (2x3), x = [1 10]\n");
    printf("  y = A^T x = [%g %g %g]   (expected [41 52 63])\n", y[0], y[1], y[2]);
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 3000);
    long n = arg_long(argc, argv, 2, m);
    double *A = xmalloc((size_t)m * n * sizeof(double));
    double *T = xmalloc((size_t)m * n * sizeof(double));
    double *x = xmalloc((size_t)m * sizeof(double));
    double *ys = xmalloc((size_t)n * sizeof(double));
    double *yp = xmalloc((size_t)n * sizeof(double));
    fill_double(A, m * n, -1.0, 1.0, 5);
    fill_double(x, m, -1.0, 1.0, 6);
    omp_warmup();

    tiny_example();
    printf("\nP056: y = A^T x, A is %ld x %ld (the tables' N column is the length of y)\n", m, n);
    printf("serial reference = row-order loop (y += x[i]*row i), used for every table\n");

    double ts, tp;
    BEST_TIME(ts, atx_serial(A, x, ys, m, n));

    printf("\n(a) parallel over j, column access A[i*n + j] (stride n)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(yp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, atx_by_column(A, x, yp, m, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(ys, yp, n, 1e-9));
    }

    printf("(b) parallel over i, reduction(+:y[:n]) (contiguous rows)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(yp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, atx_by_row_reduction(A, x, yp, m, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(ys, yp, n, 1e-9));
    }

    printf("(c) explicit transpose T = A^T, then row-wise y = T x (time includes the transpose)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(yp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, atx_transpose_first(A, T, x, yp, m, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(ys, yp, n, 1e-9));
    }
    double t_tr, t_mv;
    omp_set_num_threads(4);
    BEST_TIME(t_tr, transpose_par(A, T, m, n));
    BEST_TIME(t_mv, matvec_rows_par(T, x, yp, n, m));
    printf("    with 4 threads: transpose %.6f s + matvec %.6f s"
           " (if T is reused, only the matvec is paid again)\n", t_tr, t_mv);

    free(A); free(T); free(x); free(ys); free(yp);
    return 0;
}

/* Notes:
 * - (a) is the "obvious" parallel loop (output index j), race-free because each
 *   thread writes its own y[j]; but A[i*n + j] for fixed j jumps n doubles each
 *   step, so every access touches a new cache line. It survives only while the
 *   lines of a column block stay in cache (M lines x 64 B).
 * - Parallelising over i WITHOUT a reduction races: two rows add into the same
 *   y[j] (lost updates). reduction(+:y[:n]) (OpenMP 4.5) fixes it at the cost of
 *   one private y per thread (n doubles each, often on the thread's stack: raise
 *   OMP_STACKSIZE for very large n) plus a final combine of threads*n adds.
 * - (c) makes both phases contiguous in the inner loop but writes T with stride
 *   M during the transpose; worth it only if T is reused for many products.
 *   Its dot-product inner loop (s += T*x) is also not vectorised at -O2 (the
 *   compiler may not reorder a floating-point sum), while the axpy loop of (b)
 *   (y[j] += xi*A[i][j]) is.
 * - Results differ from serial in the last bits (different summation order), so
 *   the check uses a relative tolerance.
 * - All versions are memory-bound (2 flops per 8-byte element of A), so speedup is
 *   limited by memory bandwidth.
 */

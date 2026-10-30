/* P076: Sparse matrix-vector product (CSR) with very uneven rows
 * Problem: Multiply an N x N sparse matrix stored in CSR format by a vector,
 *   y = A x. CSR: row_ptr[N+1], col_idx[nnz], val[nnz]; the nonzeros of row i
 *   are val[k], at column col_idx[k], for k = row_ptr[i] .. row_ptr[i+1]-1.
 *   The rows have very different lengths: in the first quarter of the matrix
 *   every 100th row is 1000x longer than a normal row. Build the CSR matrix
 *   serially from a deterministic generator, parallelise the product with
 *   OpenMP and compare schedule(static), schedule(dynamic, c) and
 *   schedule(guided). Verify against the serial product (and against a dense
 *   product for a small matrix).
 * Solution: y[i] depends only on row i, and only one thread writes y[i], so
 *   "parallel for" over rows is race-free. The work of row i is its length
 *   row_ptr[i+1] - row_ptr[i], which varies by 1000x and is concentrated at
 *   the start of the matrix: schedule(static) gives thread 0 a contiguous block
 *   that holds almost all the long rows (load imbalance). dynamic hands out
 *   chunks of c rows on demand, so threads that get light rows simply take
 *   more chunks. guided hands out big chunks first (about remaining/threads)
 *   and smaller later; here the heavy rows are at the start, so its first big
 *   chunk is also unbalanced. Each row is summed in the same order as the
 *   serial code, so the results are bitwise equal.
 * Source: lab extension (loop scheduling).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p076_sparse_csr_spmv.c -o p076 -lm
 * Run:   ./p076 [N] [chunk]  (defaults N = 400000 rows, chunk = 64)
 */
#include "common.h"

#define LIGHT_LEN 4          /* nonzeros in a normal row */
#define HEAVY_LEN 4000       /* nonzeros in a heavy row (1000x longer) */
#define HEAVY_EVERY 100      /* in the first quarter, every 100th row is heavy */

typedef struct {
    long n, nnz;
    long *row_ptr, *col_idx;
    double *val;
} csr_t;

static long row_length(long i, long n) {
    long len = (i < n / 4 && i % HEAVY_EVERY == 0) ? HEAVY_LEN : LIGHT_LEN;
    return len < n ? len : n;               /* a row cannot have more than n entries */
}

/* Build the CSR matrix serially: pass 1 counts row lengths (prefix sum gives
 * row_ptr), pass 2 fills the columns and values. Column k of row i is a random
 * position inside bucket k = [start + k*stride, start + (k+1)*stride) (mod n),
 * so the columns of a row are distinct and spread over the whole row. */
static csr_t build_csr(long n) {
    csr_t A;
    A.n = n;
    A.row_ptr = xmalloc((size_t)(n + 1) * sizeof(long));
    A.row_ptr[0] = 0;
    for (long i = 0; i < n; i++) A.row_ptr[i + 1] = A.row_ptr[i] + row_length(i, n);
    A.nnz = A.row_ptr[n];
    A.col_idx = xmalloc((size_t)A.nnz * sizeof(long));
    A.val = xmalloc((size_t)A.nnz * sizeof(double));
    unsigned long long s = 76;
    for (long i = 0; i < n; i++) {
        long len = A.row_ptr[i + 1] - A.row_ptr[i], stride = n / len;
        long start = (long)(lcg_next(&s) % (unsigned long)n);
        for (long k = 0; k < len; k++) {
            long idx = A.row_ptr[i] + k;
            long jitter = (long)(lcg_next(&s) % (unsigned long)stride);
            A.col_idx[idx] = (start + k * stride + jitter) % n;
            A.val[idx] = -1.0 + 2.0 * (double)lcg_next(&s) / 2147483648.0;
        }
    }
    return A;
}

static void free_csr(csr_t *A) { free(A->row_ptr); free(A->col_idx); free(A->val); }

/* y[i] = sum of val[k] * x[col_idx[k]] over the nonzeros of row i. */
static inline double row_times_x(const csr_t *A, const double *x, long i) {
    double s = 0.0;
    for (long k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++) s += A->val[k] * x[A->col_idx[k]];
    return s;
}

static void spmv_serial(const csr_t *A, const double *x, double *y) {
    for (long i = 0; i < A->n; i++) y[i] = row_times_x(A, x, i);
}
static void spmv_static(const csr_t *A, const double *x, double *y) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < A->n; i++) y[i] = row_times_x(A, x, i);
}
static void spmv_dynamic(const csr_t *A, const double *x, double *y, long chunk) {
    #pragma omp parallel for schedule(dynamic, chunk)
    for (long i = 0; i < A->n; i++) y[i] = row_times_x(A, x, i);
}
static void spmv_guided(const csr_t *A, const double *x, double *y) {
    #pragma omp parallel for schedule(guided)
    for (long i = 0; i < A->n; i++) y[i] = row_times_x(A, x, i);
}

/* Worked example: a 5 x 5 CSR matrix written out by hand. */
static void tiny_example(void) {
    /*  A = [10 0 0 0 2 ; 0 3 0 0 0 ; 1 1 1 1 1 ; 0 0 0 4 0 ; 0 5 0 0 6] */
    long row_ptr[6] = {0, 2, 3, 8, 9, 11};
    long col_idx[11] = {0, 4,   1,   0, 1, 2, 3, 4,   3,   1, 4};
    double val[11]   = {10, 2,  3,   1, 1, 1, 1, 1,   4,   5, 6};
    csr_t A = {5, 11, row_ptr, col_idx, val};
    double x[5] = {1, 2, 3, 4, 5}, y[5];
    const double want[5] = {20, 6, 15, 16, 40};
    spmv_dynamic(&A, x, y, 1);
    printf("5x5 example: row_ptr = {0,2,3,8,9,11}, col_idx = {0,4, 1, 0,1,2,3,4, 3, 1,4}\n"
           "             val = {10,2, 3, 1,1,1,1,1, 4, 5,6}, x = [1 2 3 4 5]\n");
    printf("             y = [%g %g %g %g %g] (expected [20 6 15 16 40])  %s\n",
           y[0], y[1], y[2], y[3], y[4], ok_str(close_doubles(y, want, 5, 0.0)));
}

/* Small generated matrix: expand the CSR to a dense matrix and compare products. */
static int dense_check(long n) {
    csr_t A = build_csr(n);
    double *D = xmalloc((size_t)n * n * sizeof(double));
    double *x = xmalloc((size_t)n * sizeof(double));
    double *y1 = xmalloc((size_t)n * sizeof(double)), *y2 = xmalloc((size_t)n * sizeof(double));
    for (long k = 0; k < n * n; k++) D[k] = 0.0;
    for (long i = 0; i < n; i++)
        for (long k = A.row_ptr[i]; k < A.row_ptr[i + 1]; k++) D[i * n + A.col_idx[k]] += A.val[k];
    fill_double(x, n, -1.0, 1.0, 7);
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += D[i * n + j] * x[j];
        y1[i] = s;
    }
    spmv_dynamic(&A, x, y2, 4);
    int ok = close_doubles(y1, y2, n, 1e-12);
    free_csr(&A); free(D); free(x); free(y1); free(y2);
    return ok;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 400000);
    long chunk = arg_long(argc, argv, 2, 64);
    if (n < 1) n = 1;
    if (chunk < 1) chunk = 1;

    printf("P076: CSR sparse matrix-vector product with uneven rows\n");
    tiny_example();
    printf("dense check (N = 300 generated matrix, CSR product vs dense product): %s\n\n",
           ok_str(dense_check(300)));

    csr_t A = build_csr(n);
    double *x  = xmalloc((size_t)n * sizeof(double));
    double *ys = xmalloc((size_t)n * sizeof(double));
    double *yp = xmalloc((size_t)n * sizeof(double));
    fill_double(x, n, -1.0, 1.0, 77);

    /* How much work does thread 0 get under schedule(static) with 4 threads? */
    long first_block = A.row_ptr[(n + 3) / 4];
    printf("N = %ld rows, nnz = %ld, row lengths %d (normal) and %d (heavy)\n",
           n, A.nnz, LIGHT_LEN, HEAVY_LEN);
    printf("static, 4 threads: thread 0 gets rows 0..%ld = %.0f%% of the nonzeros (ideal 25%%)\n",
           (n + 3) / 4 - 1, 100.0 * (double)first_block / (double)A.nnz);
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, spmv_serial(&A, x, ys));
    const char *label[3] = {"schedule(static)", "schedule(dynamic, chunk)", "schedule(guided)"};
    for (int v = 0; v < 3; v++) {
        if (v == 1) printf("\n%s with chunk = %ld rows\n", label[v], chunk);
        else        printf("\n%s\n", label[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            for (long i = 0; i < n; i++) yp[i] = 0.0;
            if (v == 0)      BEST_TIME(tp, spmv_static(&A, x, yp));
            else if (v == 1) BEST_TIME(tp, spmv_dynamic(&A, x, yp, chunk));
            else             BEST_TIME(tp, spmv_guided(&A, x, yp));
            table_row(n, THREADS[k], ts, tp, close_doubles(ys, yp, n, 0.0));   /* bitwise equal */
        }
    }
    free_csr(&A); free(x); free(ys); free(yp);
    return 0;
}

/* Notes:
 * - CSR: row_ptr has N+1 entries; row i is val/col_idx[row_ptr[i] .. row_ptr[i+1]-1].
 *   Memory is 2*nnz + N + 1 numbers instead of N*N.
 * - Race-free: one thread per row writes y[i]; x and the matrix are read-only.
 *   (Parallelising over the nonzeros of a row instead would need a reduction.)
 * - static splits ROWS evenly, not WORK: with skewed rows one thread does most
 *   of the nonzeros. dynamic balances at run time (cost: one shared counter
 *   update per chunk; chunk too small = overhead, too large = imbalance).
 * - guided is good when early iterations are not heavier than late ones; here
 *   the heavy rows are first, so its first large chunks are unbalanced.
 * - Another fix: split the rows so each thread gets about nnz/threads
 *   nonzeros (binary search in row_ptr), then use those static blocks.
 * - SpMV is memory-bound (2 loads + 1 irregular load of x per multiply-add).
 */

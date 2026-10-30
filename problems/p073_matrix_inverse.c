/* P073: Matrix inverse by Gauss-Jordan elimination (partial pivoting)
 * Problem: Given a nonsingular N x N matrix A of doubles (flattened row-major,
 *   A[(long)i*N + j]), compute inv(A) by Gauss-Jordan elimination on the
 *   augmented matrix [A | I] with partial pivoting, serially and with OpenMP.
 *   Verify that A * inv(A) is (close to) the identity and that the parallel
 *   inverse matches the serial one, and report the time and speedup for 1, 2,
 *   4 and 8 threads.
 * Solution: the pivot columns k = 0..N-1 are processed in order (step k+1 needs
 *   the matrix left by step k), so the k loop stays serial. In step k the pivot
 *   row (largest |M[i][k]|, i >= k) is swapped into row k and divided by the
 *   pivot; this is O(N) work and stays serial. Then every other row i does
 *   row_i -= M[i][k] * row_k. These N-1 row updates are independent: a thread
 *   writes only its own rows i and only reads row k, which nobody writes during
 *   the update, so "#pragma omp parallel for" over i is race-free. All rows do
 *   the same work, so schedule(static) is fine. The augmented matrix M is
 *   N x 2N, row-major M[(long)i*2N + j]; its right half ends up as inv(A).
 * Source: lab extension (linear algebra kernels).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p073_matrix_inverse.c -o p073 -lm
 * Run:   ./p073 [N]          (default N = 600)
 */
#include "common.h"

/* M = [A | I], an n x 2n row-major matrix. */
static void build_augmented(const double *A, double *M, long n) {
    long w = 2 * n;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) {
            M[i * w + j]     = A[i * n + j];
            M[i * w + n + j] = (i == j) ? 1.0 : 0.0;
        }
}

/* Copy the right half of M (which now holds inv(A)) into X. */
static void extract_inverse(const double *M, double *X, long n) {
    long w = 2 * n;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) X[i * n + j] = M[i * w + n + j];
}

/* Step k, serial O(n) part: find the pivot (largest |M[i][k]| for i >= k), swap
 * that row into row k, divide row k by the pivot so M[k][k] = 1.
 * Returns 0 if the matrix is (numerically) singular. */
static int pivot_and_scale(double *M, long n, long k) {
    long w = 2 * n, p = k;
    for (long i = k + 1; i < n; i++)
        if (fabs(M[i * w + k]) > fabs(M[p * w + k])) p = i;
    if (fabs(M[p * w + k]) < 1e-14) return 0;
    if (p != k)
        for (long j = 0; j < w; j++) {
            double t = M[k * w + j]; M[k * w + j] = M[p * w + j]; M[p * w + j] = t;
        }
    double inv_pivot = 1.0 / M[k * w + k];
    for (long j = k; j < w; j++) M[k * w + j] *= inv_pivot;
    return 1;
}

/* Step k, O(n) per row: row_i -= M[i][k] * row_k (makes M[i][k] = 0).
 * Columns < k of row k are already 0, so the loop can start at column k. */
static void eliminate_row(double *M, long n, long k, long i) {
    long w = 2 * n;
    double f = M[i * w + k];                 /* read once, before the row changes */
    if (f == 0.0) return;
    for (long j = k; j < w; j++) M[i * w + j] -= f * M[k * w + j];
}

static int inverse_serial(const double *A, double *X, long n) {
    double *M = xmalloc((size_t)n * 2 * (size_t)n * sizeof(double));
    build_augmented(A, M, n);
    for (long k = 0; k < n; k++) {
        if (!pivot_and_scale(M, n, k)) { free(M); return 0; }
        for (long i = 0; i < n; i++)
            if (i != k) eliminate_row(M, n, k, i);
    }
    extract_inverse(M, X, n);
    free(M);
    return 1;
}

static int inverse_parallel(const double *A, double *X, long n) {
    double *M = xmalloc((size_t)n * 2 * (size_t)n * sizeof(double));
    build_augmented(A, M, n);
    for (long k = 0; k < n; k++) {                       /* steps: sequential */
        if (!pivot_and_scale(M, n, k)) { free(M); return 0; }   /* O(n): serial */
        #pragma omp parallel for schedule(static)        /* O(n^2): rows in parallel */
        for (long i = 0; i < n; i++)
            if (i != k) eliminate_row(M, n, k, i);       /* reads row k, writes row i */
    }                                                    /* implicit barrier per step */
    extract_inverse(M, X, n);
    free(M);
    return 1;
}

/* max |(A*X - I)[i][j]|, rows of A*X computed in parallel (i-k-j order, one
 * private row buffer per thread). */
static double identity_error(const double *A, const double *X, long n) {
    double err = 0.0;
    #pragma omp parallel reduction(max:err)
    {
        double *row = xmalloc((size_t)n * sizeof(double));
        #pragma omp for
        for (long i = 0; i < n; i++) {
            for (long j = 0; j < n; j++) row[j] = 0.0;
            for (long k = 0; k < n; k++) {
                double a = A[i * n + k];
                for (long j = 0; j < n; j++) row[j] += a * X[k * n + j];
            }
            for (long j = 0; j < n; j++) {
                double e = fabs(row[j] - (i == j ? 1.0 : 0.0));
                if (e > err) err = e;
            }
        }
        free(row);
    }
    return err;
}

static void print_matrix(const char *name, const double *A, long n) {
    printf("%s =\n", name);
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) printf(" %8.3f", A[i * n + j]);
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 600);
    if (n < 1) n = 1;

    /* Worked example: A[0][0] = 0, so without pivoting the first step would divide by 0.
     * Exact inverse: [-4.5 7 -1.5; -2 4 -1; 1.5 -2 0.5]. */
    double A3[9] = {0, 1, 2,   1, 0, 3,   4, -3, 8}, X3[9];
    const double want3[9] = {-4.5, 7, -1.5,   -2, 4, -1,   1.5, -2, 0.5};
    int ok3 = inverse_parallel(A3, X3, 3);
    printf("P073: Gauss-Jordan inverse with partial pivoting\n");
    print_matrix("A (3x3 example)", A3, 3);
    print_matrix("inv(A)", X3, 3);
    printf("3x3 example vs exact inverse: %s\n\n", ok_str(ok3 && close_doubles(X3, want3, 9, 1e-12)));

    double *A  = xmalloc((size_t)n * n * sizeof(double));
    double *Xs = xmalloc((size_t)n * n * sizeof(double));
    double *Xp = xmalloc((size_t)n * n * sizeof(double));
    fill_double(A, n * n, -1.0, 1.0, 73);
    omp_warmup();

    double ts, tp;
    int oks = 0, okp = 0;
    BEST_TIME(ts, oks = inverse_serial(A, Xs, n));
    if (!oks) { printf("matrix is singular\n"); return 1; }
    double err_s = identity_error(A, Xs, n);
    printf("N = %ld, serial: max |A*inv(A) - I| = %.2e  %s\n", n, err_s, ok_str(err_s < 1e-8));

    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, okp = inverse_parallel(A, Xp, n));
        table_row(n, THREADS[k], ts, tp, okp && close_doubles(Xs, Xp, n * n, 1e-9));
    }
    double err_p = identity_error(A, Xp, n);
    printf("parallel: max |A*inv(A) - I| = %.2e  %s\n", err_p, ok_str(err_p < 1e-8));

    free(A); free(Xs); free(Xp);
    return 0;
}

/* Notes:
 * - Why not parallelise over k? Step k uses the matrix produced by step k-1
 *   (true dependence), so only the work INSIDE a step is parallel.
 * - Race-free: in step k, thread T writes rows i != k only and reads row k,
 *   which no one writes; f = M[i][k] is saved before row i is changed.
 * - Partial pivoting (largest |pivot|) avoids division by 0 (see A[0][0] = 0
 *   in the example) and keeps rounding errors small. The pivot search is O(N),
 *   so leaving it serial costs little (Amdahl: O(N^2) of O(N^3) work).
 * - One parallel for per step = N fork/joins; with gcc's thread pool each is a
 *   few microseconds, small next to the O(N^2) update. Alternative: one
 *   parallel region around the k loop, "single" for the pivot, "for" for rows.
 * - Cost about N^3 multiply-adds over the 2N-wide augmented rows; check with
 *   A*inv(A) = I rather than exact equality (rounding).
 */

/* M04: Cholesky factorisation A = L L^T
 * Problem: A is an N x N symmetric positive definite (SPD) matrix. Compute the
 *   lower-triangular L (positive diagonal) with A = L L^T, in place in the lower
 *   triangle of A, serially and with OpenMP (right-looking algorithm):
 *     for k = 0 .. N-1:
 *       L[k][k] = sqrt(A[k][k])
 *       L[i][k] = A[i][k] / L[k][k]                       for i > k   (column k)
 *       A[i][j] = A[i][j] - L[i][k] * L[j][k]    for k < j <= i, i > k (update)
 *   Verify L L^T = A (residual), parallel = serial, the 3 x 3 textbook example,
 *   and that a matrix that is not SPD is rejected. Report the timings.
 * Solution: the k loop is SEQUENTIAL: step k uses the matrix left by step k-1
 *   (its pivot A[k][k] is only final after k updates). Inside step k:
 *   (1) the pivot d = sqrt(A[k][k]): every thread computes it itself (one
 *       square root is cheaper than a barrier); one thread stores it later;
 *   (2) column k: parallel for over i > k, iteration i writes only L[i][k];
 *   (3) trailing update: parallel for over rows i > k. Row i writes only its own
 *       elements A[i][k+1..i] and reads column k, so the rows are independent;
 *       but (3) needs ALL of column k, hence the implicit barrier after (2).
 *       Row i has i - k elements (triangular) -> schedule(dynamic, 8).
 *   One parallel region surrounds the k loop, so the threads are created once;
 *   the implicit barrier at the end of each omp for keeps the steps in order
 *   (two barriers per step). Column k is also copied into a contiguous vector
 *   col[], so the update's inner loop reads col[j] with stride 1, not stride N.
 * Source: lab extension (like problems/p070 LU; MATRIX_RULES.md section 7 rule 5).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m04_cholesky.c -o m04 -lm
 * Run:   ./m04 [N]   (default N = 1000)
 */
#include "../../problems/common.h"

/* Serial. Returns 1 on success, 0 if a pivot is <= 0 (A is not SPD). */
static int chol_serial(double *A, long n, double *col) {
    for (long k = 0; k < n; k++) {
        double d = A[k*n + k];
        if (!(d > 0.0)) return 0;
        d = sqrt(d);
        A[k*n + k] = d;
        for (long i = k + 1; i < n; i++) {
            double l = A[i*n + k] / d;
            A[i*n + k] = l;
            col[i] = l;
        }
        for (long i = k + 1; i < n; i++) {
            double lik = col[i], *ai = A + i*n;
            for (long j = k + 1; j <= i; j++) ai[j] -= lik * col[j];
        }
    }
    return 1;
}

/* Parallel: one region, k loop run by every thread, work inside each step shared. */
static int chol_parallel(double *A, long n, double *col) {
    int ok = 1;
    #pragma omp parallel
    {
        for (long k = 0; k < n; k++) {
            double akk = A[k*n + k];           /* final: the last barrier came after its update */
            if (!(akk > 0.0)) {                /* same value -> same decision in every thread */
                if (omp_get_thread_num() == 0) ok = 0;
                break;
            }
            double d = sqrt(akk);              /* every thread computes the pivot itself */
            #pragma omp for schedule(static)
            for (long i = k + 1; i < n; i++) {
                double l = A[i*n + k] / d;
                A[i*n + k] = l;
                col[i] = l;
            }                                  /* barrier: column k complete, A[k][k] read by all */
            #pragma omp single nowait
            A[k*n + k] = d;                    /* L[k][k]; nobody reads it during the update */
            #pragma omp for schedule(dynamic, 8)
            for (long i = k + 1; i < n; i++) {
                double lik = col[i], *ai = A + i*n;
                for (long j = k + 1; j <= i; j++) ai[j] -= lik * col[j];
            }                                  /* barrier: step k done */
        }
    }
    return ok;
}

/* Timed helpers: start every repetition from the original matrix. */
static int run_serial(double *W, const double *A0, long n, double *col) {
    memcpy(W, A0, (size_t)n * n * sizeof(double));
    return chol_serial(W, n, col);
}
static int run_parallel(double *W, const double *A0, long n, double *col) {
    memcpy(W, A0, (size_t)n * n * sizeof(double));
    return chol_parallel(W, n, col);
}

/* max over i >= j of |(L L^T)[i][j] - A0[i][j]|, relative to max |A0|. L = lower part of W. */
static double residual(const double *W, const double *A0, long n) {
    double err = 0.0, amax = 0.0;
    #pragma omp parallel for schedule(dynamic, 8) reduction(max:err, amax)
    for (long i = 0; i < n; i++)
        for (long j = 0; j <= i; j++) {
            double s = 0.0;
            for (long k = 0; k <= j; k++) s += W[i*n + k] * W[j*n + k];   /* rows i, j of L */
            double e = fabs(s - A0[i*n + j]);
            if (e > err) err = e;
            if (fabs(A0[i*n + j]) > amax) amax = fabs(A0[i*n + j]);
        }
    return amax > 0 ? err / amax : err;
}

/* Symmetric, strictly diagonally dominant, positive diagonal -> SPD. */
static void make_spd(double *A, long n) {
    fill_double(A, n * n, -1.0, 1.0, 9);
    for (long i = 0; i < n; i++)
        for (long j = 0; j < i; j++) A[j*n + i] = A[i*n + j];
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) if (j != i) s += fabs(A[i*n + j]);
        A[i*n + i] = s + 1.0;
    }
}

static void small_checks(void) {
    double A[9] = {  4,  12, -16,
                    12,  37, -43,
                   -16, -43,  98};
    const double L[9] = { 2, 0, 0,
                          6, 1, 0,
                         -8, 5, 3};
    double col[3];
    int ok = chol_parallel(A, 3, col);
    A[1] = A[2] = A[5] = 0.0;                     /* clear the untouched upper triangle */
    ok = ok && close_doubles(A, L, 9, 1e-12);
    printf("Hand example: A = [4 12 -16; 12 37 -43; -16 -43 98]\n");
    printf("  L = [%g 0 0; %g %g 0; %g %g %g] (expected [2 0 0; 6 1 0; -8 5 3]): %s\n",
           A[0], A[3], A[4], A[6], A[7], A[8], ok_str(ok));
    double B[4] = {1, 2,
                   2, 1};                         /* eigenvalues 3 and -1: not SPD */
    int rejected = !chol_parallel(B, 2, col);
    printf("  A = [1 2; 2 1] is not SPD (pivot 1 - 2*2 = -3 at k = 1) and is rejected: %s\n\n",
           ok_str(rejected));
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000);
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    double *A0 = xmalloc((size_t)n * n * sizeof(double));
    double *Ws = xmalloc((size_t)n * n * sizeof(double));
    double *Wp = xmalloc((size_t)n * n * sizeof(double));
    double *col = xmalloc((size_t)n * sizeof(double));
    make_spd(A0, n);
    omp_warmup();

    small_checks();
    int oks = 0, okp = 0;
    double ts, tp;
    BEST_TIME(ts, oks = run_serial(Ws, A0, n, col));
    double res = residual(Ws, A0, n);
    printf("M04: N = %ld, symmetric diagonally dominant SPD matrix, N^3/3 = %.0f Mflop\n",
           n, (double)n * n * n / 3e6);
    printf("serial: factorised = %s, max |L L^T - A| / max |A| = %.2e: %s\n",
           oks ? "yes" : "no", res, ok_str(oks && res < 1e-10));
    printf("parallel: one region, k loop serial, omp for (column) + omp for dynamic (update)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, okp = run_parallel(Wp, A0, n, col));
        table_row(n, THREADS[k], ts, tp, okp && close_doubles(Ws, Wp, n * n, 1e-9));
    }
    double resp = residual(Wp, A0, n);
    printf("parallel result (%d threads): max |L L^T - A| / max |A| = %.2e: %s\n",
           THREADS[NTHREADS - 1], resp, ok_str(okp && resp < 1e-10));
    free(A0); free(Ws); free(Wp); free(col);
    return 0;
}

/* Notes:
 * - Cholesky exists (with a positive diagonal) exactly when A is symmetric positive
 *   definite. A pivot <= 0 during the factorisation proves A is not SPD: stop.
 *   No pivoting is needed for SPD matrices (unlike LU / Gaussian elimination).
 * - Cost: N^3/3 flops, half of LU (2N^3/3), because only the lower triangle is
 *   updated. Uses: solving A x = b as L y = b (forward, m05) then L^T x = y
 *   (backward); least squares via A^T A (m03); sampling correlated random numbers.
 * - Why k is serial: the pivot A[k][k] and column k are final only after the
 *   updates of steps 0..k-1 (a loop-carried flow dependence). Only the work
 *   inside one step is parallel.
 * - Why two loops with a barrier between them: the update of row i reads L[j][k]
 *   for every j <= i, which other threads compute in the column loop. Fusing them
 *   into one loop would read unfinished values (race).
 * - The pivot: a "single" that computes sqrt and stores it would also be correct,
 *   but it costs a third barrier per step. Here every thread reads the same
 *   A[k][k] (made final by the previous barrier) and computes the same d; the
 *   store is a "single nowait", placed after the column loop's barrier so that no
 *   thread can still be reading the old value. Since every thread sees the same
 *   A[k][k], they all leave the k loop at the same step on a non-SPD matrix (a
 *   thread must not skip a worksharing construct that the others reach); only
 *   thread 0 writes the shared ok flag.
 * - Load balance: rows near the bottom have the longest updates and the active
 *   part shrinks as k grows, so dynamic beats static. The parallel work per step
 *   is (N-k)^2/2: tiny for the last steps, where the 2N barriers dominate. Each
 *   barrier costs microseconds (more on macOS, where libgomp's barrier puts the
 *   threads to sleep), so small N gains little; the speedup grows with N.
 * - Same operations in the same order as serial, so the results agree to the
 *   last bits (compared with a tolerance anyway).
 */

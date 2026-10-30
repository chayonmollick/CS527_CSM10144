/* P071: Solve Ax = b by Gaussian elimination with partial pivoting
 * Problem: Solve the N x N linear system A x = b by Gaussian elimination with
 *   partial pivoting followed by back substitution, serially and with OpenMP.
 *   For each column k: find the pivot row (largest |A[i][k]|, i >= k), swap it
 *   into row k, and eliminate column k from the rows below. Verify that the
 *   residual ||Ax - b|| is tiny, that x matches the serial solution and the
 *   known true solution, and report the timings.
 * Solution: the k loop is sequential (step k works on the matrix left by step
 *   k-1). Inside step k: (1) pivot search = parallel argmax of |A[i][k]|: each
 *   thread keeps a private (value, row) best, merged in a critical section (ties
 *   -> smaller row, so the choice equals serial); (2) the row swap is O(N), done
 *   serially; (3) elimination: rows i > k are independent (each reads pivot row
 *   k, writes only row i and b[i]) -> parallel for over i. Back substitution:
 *   x[i] = (b[i] - sum_{j>i} A[i][j] x[j]) / A[i][i] runs i = N-1 down to 0
 *   sequentially (x[i] needs all later x[j]); the inner dot product is a
 *   reduction(+:s). Matrices are flattened row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (Eijkhout Ch. 19 exercises on Gaussian elimination).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p071_gaussian_elimination.c -o p071 -lm
 * Run:   ./p071 [N]   (default N = 1000)
 */
#include "common.h"

/* ---------- pivot search: row p >= k with the largest |A[p][k]| ---------- */
static long pivot_serial(const double *A, long n, long k) {
    long best = k;
    double bv = fabs(A[k*n + k]);
    for (long i = k + 1; i < n; i++)
        if (fabs(A[i*n + k]) > bv) { bv = fabs(A[i*n + k]); best = i; }  /* first max */
    return best;
}

static long pivot_par(const double *A, long n, long k) {
    double bv = -1.0;
    long best = k;                                       /* shared result */
    #pragma omp parallel
    {
        double mv = -1.0;
        long mi = k;                                     /* private best */
        #pragma omp for nowait
        for (long i = k; i < n; i++) {
            double v = fabs(A[i*n + k]);
            if (v > mv || (v == mv && i < mi)) { mv = v; mi = i; }
        }
        #pragma omp critical
        if (mv > bv || (mv == bv && mi < best)) { bv = mv; best = mi; }
    }
    return best;
}

static void swap_rows(double *A, double *b, long n, long k, long p) {
    if (p == k) return;
    for (long j = k; j < n; j++) {                       /* columns < k are 0 in both rows */
        double t = A[k*n + j]; A[k*n + j] = A[p*n + j]; A[p*n + j] = t;
    }
    double t = b[k]; b[k] = b[p]; b[p] = t;
}

/* ---------- solve (A and b are overwritten). Returns 0 if A is singular. ---------- */
static int gauss_serial(double *A, double *b, double *x, long n) {
    for (long k = 0; k < n; k++) {
        long p = pivot_serial(A, n, k);
        if (A[p*n + k] == 0.0) return 0;                 /* whole column is zero */
        swap_rows(A, b, n, k, p);
        for (long i = k + 1; i < n; i++) {
            double l = A[i*n + k] / A[k*n + k];
            A[i*n + k] = 0.0;
            for (long j = k + 1; j < n; j++) A[i*n + j] -= l * A[k*n + j];
            b[i] -= l * b[k];
        }
    }
    for (long i = n - 1; i >= 0; i--) {                  /* back substitution */
        double s = 0.0;
        for (long j = i + 1; j < n; j++) s += A[i*n + j] * x[j];
        x[i] = (b[i] - s) / A[i*n + i];
    }
    return 1;
}

static int gauss_par(double *A, double *b, double *x, long n) {
    for (long k = 0; k < n; k++) {                       /* sequential steps */
        long p = pivot_par(A, n, k);                     /* parallel argmax */
        if (A[p*n + k] == 0.0) return 0;
        swap_rows(A, b, n, k, p);                        /* O(n), serial */
        #pragma omp parallel for
        for (long i = k + 1; i < n; i++) {               /* rows independent */
            double l = A[i*n + k] / A[k*n + k];
            A[i*n + k] = 0.0;
            for (long j = k + 1; j < n; j++) A[i*n + j] -= l * A[k*n + j];
            b[i] -= l * b[k];                            /* own b[i] */
        }
    }
    for (long i = n - 1; i >= 0; i--) {                  /* sequential over i */
        double s = 0.0;
        #pragma omp parallel for reduction(+:s)
        for (long j = i + 1; j < n; j++) s += A[i*n + j] * x[j];
        x[i] = (b[i] - s) / A[i*n + i];
    }
    return 1;
}

/* Relative residual ||A x - b||_inf / (||A||_inf ||x||_inf + ||b||_inf). */
static double rel_residual(const double *A, const double *x, const double *b, long n) {
    double rmax = 0.0, amax = 0.0, xmax = 0.0, bmax = 0.0;
    #pragma omp parallel for reduction(max:rmax, amax)
    for (long i = 0; i < n; i++) {
        double s = 0.0, rowabs = 0.0;
        for (long j = 0; j < n; j++) { s += A[i*n + j] * x[j]; rowabs += fabs(A[i*n + j]); }
        if (fabs(s - b[i]) > rmax) rmax = fabs(s - b[i]);
        if (rowabs > amax) amax = rowabs;
    }
    for (long i = 0; i < n; i++) {
        if (fabs(x[i]) > xmax) xmax = fabs(x[i]);
        if (fabs(b[i]) > bmax) bmax = fabs(b[i]);
    }
    return rmax / (amax * xmax + bmax);
}

static void tiny_example(void) {
    /* A[0][0] = 0: elimination without pivoting would divide by zero. */
    double A[9] = { 0,  2,  1,
                    1, -2, -3,
                   -1,  1,  2};
    double b[3] = {7, -12, 7}, x[3];
    omp_set_num_threads(4);
    int ok = gauss_par(A, b, x, 3);
    printf("Worked example: A = [0 2 1; 1 -2 -3; -1 1 2], b = [7 -12 7] (needs a row swap)\n");
    printf("  x = [%g %g %g]   (expected [1 2 3]) %s\n", x[0], x[1], x[2], ok ? "" : "SINGULAR");
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000);
    long nn = n * n;
    size_t bytes = (size_t)nn * sizeof(double), vbytes = (size_t)n * sizeof(double);
    double *A0 = xmalloc(bytes), *W = xmalloc(bytes);
    double *b0 = xmalloc(vbytes), *bw = xmalloc(vbytes), *xt = xmalloc(vbytes);
    double *xs = xmalloc(vbytes), *xp = xmalloc(vbytes);
    fill_double(A0, nn, -1.0, 1.0, 71);                  /* general matrix: pivoting needed */
    for (long i = 0; i < n; i++) xt[i] = 1.0 + (double)(i % 7);   /* known solution */
    for (long i = 0; i < n; i++) {                       /* b = A * x_true */
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A0[i*n + j] * xt[j];
        b0[i] = s;
    }
    omp_warmup();

    tiny_example();

    double ts, tp;
    int oks = 0, okp = 0;
    /* A and b are overwritten, so each timed run starts from fresh copies (O(N^2)). */
    BEST_TIME(ts, memcpy(W, A0, bytes); memcpy(bw, b0, vbytes); oks = gauss_serial(W, bw, xs, n));
    double err = 0.0;
    for (long i = 0; i < n; i++) if (fabs(xs[i] - xt[i]) > err) err = fabs(xs[i] - xt[i]);
    printf("\nP071: Gaussian elimination with partial pivoting, N = %ld\n", n);
    printf("serial: %s, relative residual = %.2e, max |x - x_true| = %.2e\n",
           oks ? "solved" : "SINGULAR", rel_residual(A0, xs, b0, n), err);

    printf("\nelimination parallel over rows i, pivot search = parallel argmax,"
           " back substitution with reduction\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(xp, 0, vbytes);
        BEST_TIME(tp, memcpy(W, A0, bytes); memcpy(bw, b0, vbytes); okp = gauss_par(W, bw, xp, n));
        double res = rel_residual(A0, xp, b0, n);
        table_row(n, THREADS[k], ts, tp,
                  okp && oks && res < 1e-12 && close_doubles(xs, xp, n, 1e-8));
    }
    double res = rel_residual(A0, xp, b0, n);
    err = 0.0;
    for (long i = 0; i < n; i++) if (fabs(xp[i] - xt[i]) > err) err = fabs(xp[i] - xt[i]);
    printf("parallel (8 threads): relative residual = %.2e -> %s, max |x - x_true| = %.2e -> %s\n",
           res, ok_str(res < 1e-12), err, ok_str(err < 1e-6));

    free(A0); free(W); free(b0); free(bw); free(xt); free(xs); free(xp);
    return 0;
}

/* Notes:
 * - Sequential: the k loop (each step needs the previous one) and the back
 *   substitution over i (x[i] needs x[i+1..N-1]). Parallel: the rows of one
 *   elimination step, the pivot search, and the dot product in back substitution.
 * - Partial pivoting (largest |A[i][k]|) avoids division by zero (tiny example)
 *   and keeps the multipliers |l| <= 1, which keeps rounding errors small.
 * - Parallel argmax: reduction(max:) gives only the value, not the row; use a
 *   private (value, row) per thread + one critical merge per thread.
 * - The check is the residual, not exact equality: the reduction in back
 *   substitution sums in a different order, so x differs from serial in the last
 *   bits. A small relative residual (~1e-16 to 1e-13) means a backward-stable solve.
 * - Overheads: 2N parallel regions for pivot search and back substitution do
 *   only O(N) work each; for small N they are slower than serial (an if(n - k >
 *   1000) clause would switch them off). The O(N^3/3) elimination dominates.
 * - Each step is a rank-1 update that streams the whole trailing matrix through
 *   memory (2 flops per element loaded): memory-bound, so the speedup saturates
 *   early; blocked (BLAS-3) elimination is what real libraries use.
 */

/* P072: Determinant by Gaussian elimination
 * Problem: Compute the determinant of an N x N matrix by Gaussian elimination with
 *   partial pivoting: det(A) = (product of the pivots) * (-1)^(number of row
 *   swaps). Do it serially and with OpenMP (parallel elimination as in P071).
 *   Test on matrices with known determinants: small 3x3 matrices worked by hand,
 *   a triangular matrix (det = product of the diagonal), the same with its rows
 *   reversed, a permutation matrix, a matrix with two equal rows (det = 0) and
 *   det(2A) = 2^N det(A). Verify
 *   parallel against serial and against the known values, and time it.
 * Solution: the k loop is sequential; in step k the pivot search is a parallel
 *   argmax (private (value, row) per thread + critical, ties -> smaller row),
 *   the swap is serial and flips the sign, and the rows below are eliminated with
 *   parallel for over i (each row written by one thread, pivot row only read).
 *   Row operations "row_i -= l * row_k" do not change the determinant, so det is
 *   the product of the diagonal of the final upper-triangular matrix times the
 *   swap sign. The product of N pivots overflows a double for large N (|det| of a
 *   random 1000 x 1000 matrix is ~1e1000), so the result is kept as
 *   sign * exp(logabs) with logabs = sum of log|pivot|; a zero pivot column means
 *   det = 0 (sign 0). Matrices are flattened row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (MATRIX_RULES.md section 3: determinant rules).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p072_determinant.c -o p072 -lm
 * Run:   ./p072 [N]   (default N = 1000)
 */
#include "common.h"

typedef struct { int sign; double logabs; } Det;     /* det = sign * exp(logabs) */

static long pivot_serial(const double *A, long n, long k) {
    long best = k;
    double bv = fabs(A[k*n + k]);
    for (long i = k + 1; i < n; i++)
        if (fabs(A[i*n + k]) > bv) { bv = fabs(A[i*n + k]); best = i; }
    return best;
}

static long pivot_par(const double *A, long n, long k) {
    double bv = -1.0;
    long best = k;
    #pragma omp parallel
    {
        double mv = -1.0;
        long mi = k;
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

static void swap_rows(double *A, long n, long k, long p) {
    for (long j = k; j < n; j++) {                       /* columns < k are 0 in both */
        double t = A[k*n + j]; A[k*n + j] = A[p*n + j]; A[p*n + j] = t;
    }
}

/* Both versions overwrite A with its upper-triangular form. */
static Det det_serial(double *A, long n) {
    Det d = {1, 0.0};
    for (long k = 0; k < n; k++) {
        long p = pivot_serial(A, n, k);
        if (A[p*n + k] == 0.0) { d.sign = 0; d.logabs = 0.0; return d; }   /* det = 0 */
        if (p != k) { swap_rows(A, n, k, p); d.sign = -d.sign; }           /* swap: -1 */
        double piv = A[k*n + k];
        if (piv < 0.0) d.sign = -d.sign;
        d.logabs += log(fabs(piv));
        for (long i = k + 1; i < n; i++) {
            double l = A[i*n + k] / piv;
            A[i*n + k] = 0.0;
            for (long j = k + 1; j < n; j++) A[i*n + j] -= l * A[k*n + j];
        }
    }
    return d;
}

static Det det_par(double *A, long n) {
    Det d = {1, 0.0};
    for (long k = 0; k < n; k++) {                       /* sequential steps */
        long p = pivot_par(A, n, k);
        if (A[p*n + k] == 0.0) { d.sign = 0; d.logabs = 0.0; return d; }
        if (p != k) { swap_rows(A, n, k, p); d.sign = -d.sign; }
        double piv = A[k*n + k];
        if (piv < 0.0) d.sign = -d.sign;
        d.logabs += log(fabs(piv));
        #pragma omp parallel for
        for (long i = k + 1; i < n; i++) {               /* rows independent */
            double l = A[i*n + k] / piv;
            A[i*n + k] = 0.0;
            for (long j = k + 1; j < n; j++) A[i*n + j] -= l * A[k*n + j];
        }
    }
    return d;
}

/* "4", "-3", "0" for small values; "+e^1234.56" when exp would overflow. */
static void det_str(Det d, char *buf, size_t size) {
    if (d.sign == 0) snprintf(buf, size, "0");
    else if (d.logabs < 700.0) snprintf(buf, size, "%.10g", d.sign * exp(d.logabs));
    else snprintf(buf, size, "%se^%.6f", d.sign > 0 ? "+" : "-", d.logabs);
}
static int det_close(Det a, Det b) {
    return a.sign == b.sign && (a.sign == 0 || fabs(a.logabs - b.logabs) <= 1e-9 * (1.0 + fabs(b.logabs)));
}

/* Run serial and parallel (4 threads) on copies of A, compare with the expected value. */
static int run_test(const char *name, const double *A, long n, Det expect, double *W) {
    size_t bytes = (size_t)n * n * sizeof(double);
    memcpy(W, A, bytes);
    Det s = det_serial(W, n);
    memcpy(W, A, bytes);
    omp_set_num_threads(4);
    Det p = det_par(W, n);
    char bs[64], bp[64], be[64];
    det_str(s, bs, sizeof bs); det_str(p, bp, sizeof bp); det_str(expect, be, sizeof be);
    int ok = det_close(s, expect) && det_close(p, expect);
    printf("  %-40s expected %-18s serial %-18s parallel %-18s %s\n", name, be, bs, bp, ok_str(ok));
    return ok;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000);
    long nn = n * n;
    size_t bytes = (size_t)(nn > 9 ? nn : 9) * sizeof(double);
    double *A = xmalloc(bytes), *W = xmalloc(bytes), *Ds = xmalloc(bytes);
    omp_warmup();

    printf("P072: determinant by elimination with partial pivoting, N = %ld\n", n);
    printf("known-value tests (4 threads):\n");

    double h1[9] = {2, -1, 0, -1, 2, -1, 0, -1, 2};      /* by hand: 4 */
    double h2[9] = {1, 2, 3, 4, 5, 6, 7, 8, 10};         /* by hand: -3 (needs swaps) */
    double h3[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};          /* rows dependent: 0 */
    run_test("3x3 [2 -1 0; -1 2 -1; 0 -1 2]", h1, 3, (Det){1, log(4.0)}, W);
    run_test("3x3 [1 2 3; 4 5 6; 7 8 10]", h2, 3, (Det){-1, log(3.0)}, W);
    /* [1 2 3; 4 5 6; 7 8 9] is singular in exact arithmetic, but rounding leaves a
       ~1e-16 pivot: elimination cannot give an exact 0 for it. */
    {
        memcpy(W, h3, sizeof h3);
        Det d = det_serial(W, 3);
        double v = d.sign == 0 ? 0.0 : d.sign * exp(d.logabs);
        printf("  %-40s exact 0; computed %.3g (rounding error, |det| < 1e-12: %s)\n",
               "3x3 [1 2 3; 4 5 6; 7 8 9]", v, ok_str(fabs(v) < 1e-12));
    }

    /* Upper triangular: det = product of the diagonal (|d| in [0.5, 1], some < 0). */
    fill_double(A, nn, -1.0, 1.0, 72);
    Det expect = {1, 0.0};
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < i; j++) A[i*n + j] = 0.0;
        double dii = (i % 3 == 0 ? -1.0 : 1.0) * (0.5 + 0.5 * fabs(A[i*n + i]));
        A[i*n + i] = dii;
        if (dii < 0) expect.sign = -expect.sign;
        expect.logabs += log(fabs(dii));
    }
    run_test("upper triangular: product of diagonal", A, n, expect, W);

    /* Same matrix with the row order reversed: N/2 row swaps undo the reversal,
       so det = (-1)^(N/2) * product of the diagonal (tests the swap sign). */
    for (long i = 0; i < n / 2; i++)
        for (long j = 0; j < n; j++) {
            double t = A[i*n + j]; A[i*n + j] = A[(n - 1 - i)*n + j]; A[(n - 1 - i)*n + j] = t;
        }
    if ((n / 2) % 2) expect.sign = -expect.sign;
    run_test("upper triangular, rows reversed", A, n, expect, W);

    /* Anti-identity (reverses the rows): det = (-1)^(N/2). */
    memset(A, 0, (size_t)nn * sizeof(double));
    for (long i = 0; i < n; i++) A[i*n + (n - 1 - i)] = 1.0;
    run_test("anti-identity: (-1)^(N/2)", A, n, (Det){(n / 2) % 2 ? -1 : 1, 0.0}, W);

    /* Two equal rows: det = 0 (elimination produces an exactly zero row). */
    fill_double(A, nn, -1.0, 1.0, 73);
    if (n >= 2) {
        memcpy(&A[(n - 1)*n], &A[0], (size_t)n * sizeof(double));
        run_test("random with row N-1 = row 0: 0", A, n, (Det){0, 0.0}, W);
    }

    /* Random matrix: det(2A) = 2^N det(A) (scaling by 2 is exact in floating point). */
    fill_double(A, nn, -1.0, 1.0, 74);
    memcpy(W, A, (size_t)nn * sizeof(double));
    Det dA = det_serial(W, n);
    for (long t = 0; t < nn; t++) Ds[t] = 2.0 * A[t];
    Det d2 = dA;
    d2.logabs += (double)n * log(2.0);
    run_test("random 2A: 2^N * det(A)", Ds, n, d2, W);

    /* Timing on the random matrix. */
    double ts, tp;
    Det rs = dA, rp = dA;
    BEST_TIME(ts, memcpy(W, A, (size_t)nn * sizeof(double)); rs = det_serial(W, n));
    char b[64];
    det_str(rs, b, sizeof b);
    printf("\nrandom N x N matrix, entries in [-1, 1): det = %s\n", b);
    printf("elimination parallel over rows, pivot search = parallel argmax\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        rp.sign = 2;                                     /* invalid until computed */
        BEST_TIME(tp, memcpy(W, A, (size_t)nn * sizeof(double)); rp = det_par(W, n));
        table_row(n, THREADS[k], ts, tp, det_close(rp, rs));
    }

    free(A); free(W); free(Ds);
    return 0;
}

/* Notes:
 * - det rules used: row swap flips the sign; adding a multiple of one row to
 *   another changes nothing; triangular -> product of the diagonal; two equal
 *   rows -> 0; det(cA) = c^N det(A).
 * - Sequential: the k loop. Parallel: the elimination of rows below the pivot
 *   (and the pivot search). The determinant itself is accumulated by one thread
 *   (outside the parallel loops), so it needs no reduction.
 * - Logs avoid overflow/underflow: a product of 1000 pivots easily exceeds 1e308.
 * - Floating point: a matrix that is singular only in exact arithmetic (1..9)
 *   gives a tiny nonzero det; an exactly zero pivot column (equal rows) gives 0.
 *   Real code tests |pivot| < eps * ||A|| instead of == 0. Ill-conditioned
 *   matrices (e.g. random LOWER triangular ones, which pivoting scrambles) can
 *   give a det that is wrong by orders of magnitude: test with well-conditioned
 *   or exactly-handled cases, as here.
 * - Each step does O((N-k)^2) work after O(N-k) pivot search; parallel regions
 *   per step cost microseconds, so speedup needs N in the hundreds or more.
 */

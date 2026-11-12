/* M05: Forward substitution: solve L x = b for a lower-triangular L
 * Problem: L is an N x N lower-triangular matrix with a nonzero diagonal. Solve
 *   L x = b by forward substitution,
 *       x[i] = (b[i] - sum_{j < i} L[i][j] * x[j]) / L[i][i],   i = 0, 1, ..., N-1,
 *   serially and with OpenMP, (a) row-oriented, (b) column-oriented, (c) blocked,
 *   and (d) for many right-hand sides at once (X = L^-1 B, k columns). Verify
 *   against serial, a known solution and a 3 x 3 example, and report the timings.
 * Solution: the loop over rows i is SEQUENTIAL: x[i] needs every earlier x[j]
 *   (a loop-carried flow dependence), so a parallel for over i would read x[j]
 *   that another thread has not computed yet. Only the work inside a row (or
 *   column) step is parallel:
 *   (a) row-oriented: for each i (serial), the dot product sum_{j<i} L[i][j] x[j]
 *       is a parallel for with reduction(+:s); one fork/join per row.
 *   (b) column-oriented: r = copy of b; for each j (serial) one thread computes
 *       x[j] = r[j] / L[j][j] ("single", whose barrier publishes x[j]), then
 *       "omp for" over i > j does r[i] -= L[i][j] * x[j] (each r[i] is owned by
 *       one thread: no race). One parallel region around the j loop; the implicit
 *       barrier after the omp for makes r[j+1] final before the next step.
 *   (c) blocked (b): columns in blocks of nb (128). One thread solves the small
 *       triangle of the block; then all rows below it subtract their row piece
 *       L[i][block] . x[block] in parallel (contiguous reads, one owner per r[i]).
 *       2 barriers per block instead of 2 per column.
 *   (d) k independent systems L x_c = b_c: parallel for over the right-hand sides,
 *       each thread runs the serial algorithm: no synchronisation at all.
 * Source: lab extension (MATRIX_RULES.md section 7 rule 5; back substitution is in
 *   problems/p071).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m05_forward_substitution.c -o m05 -lm
 * Run:   ./m05 [N] [k] [nb]   (defaults N = 2000, k = 32 right-hand sides, nb = 128)
 */
#include "../../problems/common.h"

static void fwd_serial(const double *L, const double *b, double *x, long n) {
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < i; j++) s += L[i*n + j] * x[j];
        x[i] = (b[i] - s) / L[i*n + i];
    }
}

/* (a) rows in order; the dot product of row i is a parallel reduction. */
static void fwd_rows(const double *L, const double *b, double *x, long n) {
    for (long i = 0; i < n; i++) {                /* serial: x[i] needs x[0..i-1] */
        double s = 0.0;
        #pragma omp parallel for reduction(+:s)
        for (long j = 0; j < i; j++) s += L[i*n + j] * x[j];
        x[i] = (b[i] - s) / L[i*n + i];
    }
}

/* (b) columns in order; after x[j] is known, update the rest of r in parallel. */
static void fwd_cols(const double *L, const double *b, double *x, double *r, long n) {
    memcpy(r, b, (size_t)n * sizeof(double));      /* keep b, update a copy */
    #pragma omp parallel
    for (long j = 0; j < n; j++) {                 /* every thread runs the j loop */
        #pragma omp single
        x[j] = r[j] / L[j*n + j];                  /* barrier after single: x[j] visible */
        #pragma omp for schedule(static)
        for (long i = j + 1; i < n; i++)
            r[i] -= L[i*n + j] * x[j];             /* r[i] written by one thread only */
    }                                              /* barrier after for: r[j+1] final */
}

/* (c) blocked column-oriented: nb columns per step, so far fewer barriers. */
static void fwd_blocked(const double *L, const double *b, double *x, double *r, long n, long nb) {
    memcpy(r, b, (size_t)n * sizeof(double));
    #pragma omp parallel
    for (long j0 = 0; j0 < n; j0 += nb) {          /* every thread runs the block loop */
        long j1 = j0 + nb < n ? j0 + nb : n;
        #pragma omp single
        for (long i = j0; i < j1; i++) {           /* small triangle: serial */
            double s = r[i];
            for (long j = j0; j < i; j++) s -= L[i*n + j] * x[j];
            x[i] = s / L[i*n + i];
        }                                          /* barrier: x[j0..j1) visible */
        #pragma omp for schedule(static)
        for (long i = j1; i < n; i++) {            /* rows below the block */
            double s = 0.0;
            for (long j = j0; j < j1; j++) s += L[i*n + j] * x[j];
            r[i] -= s;                             /* r[i] written by one thread only */
        }                                          /* barrier: r final for the next block */
    }
}

/* (d) k right-hand sides; B and X hold one system per row: b_c = B + c*n. */
static void fwd_multi_serial(const double *L, const double *B, double *X, long n, long k) {
    for (long c = 0; c < k; c++) fwd_serial(L, B + c*n, X + c*n, n);
}
static void fwd_multi_parallel(const double *L, const double *B, double *X, long n, long k) {
    #pragma omp parallel for schedule(static)
    for (long c = 0; c < k; c++) fwd_serial(L, B + c*n, X + c*n, n);
}

/* Lower triangle in [-1, 1), diagonal 1 + sum |row| (strictly diagonally dominant). */
static void make_lower(double *L, long n) {
    fill_double(L, n * n, -1.0, 1.0, 21);
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < i; j++) s += fabs(L[i*n + j]);
        L[i*n + i] = 1.0 + s;
        for (long j = i + 1; j < n; j++) L[i*n + j] = 0.0;
    }
}

/* b = L x (to build a system with a known solution). */
static void lower_times(const double *L, const double *x, double *b, long n) {
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j <= i; j++) s += L[i*n + j] * x[j];
        b[i] = s;
    }
}

static double max_abs_diff(const double *x, const double *y, long n) {
    double m = 0.0;
    for (long i = 0; i < n; i++) if (fabs(x[i] - y[i]) > m) m = fabs(x[i] - y[i]);
    return m;
}

static void hand_example(void) {
    const double L[9] = {2,  0, 0,
                         3,  1, 0,
                         1, -2, 4};
    const double b[3] = {4, 7, 12}, expect[3] = {2, 1, 3};
    double xa[3], xb[3], xc[3], r[3];
    fwd_rows(L, b, xa, 3);
    fwd_cols(L, b, xb, r, 3);
    fwd_blocked(L, b, xc, r, 3, 2);                /* blocks of 2: {0, 1} and {2} */
    printf("Hand example: L = [2 0 0; 3 1 0; 1 -2 4], b = [4 7 12]\n");
    printf("  x0 = 4/2 = 2, x1 = 7 - 3*2 = 1, x2 = (12 - 2 + 2)/4 = 3 -> x = [%g %g %g]: %s\n\n",
           xa[0], xa[1], xa[2],
           ok_str(close_doubles(xa, expect, 3, 1e-12) && close_doubles(xb, expect, 3, 1e-12)
                  && close_doubles(xc, expect, 3, 1e-12)));
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 2000), k = arg_long(argc, argv, 2, 32);
    long nb = arg_long(argc, argv, 3, 128);
    if (n < 1 || k < 1 || nb < 1) { fprintf(stderr, "N, k and nb must be >= 1\n"); return 1; }
    double *L = xmalloc((size_t)n * n * sizeof(double));
    double *xt = xmalloc((size_t)n * sizeof(double)), *b = xmalloc((size_t)n * sizeof(double));
    double *xs = xmalloc((size_t)n * sizeof(double)), *xp = xmalloc((size_t)n * sizeof(double));
    double *r = xmalloc((size_t)n * sizeof(double));
    size_t kn = (size_t)k * n * sizeof(double);             /* k systems of size n */
    double *Xt = xmalloc(kn), *B = xmalloc(kn), *Xs = xmalloc(kn), *Xp = xmalloc(kn);
    make_lower(L, n);
    fill_double(xt, n, -1.0, 1.0, 22);
    lower_times(L, xt, b, n);
    fill_double(Xt, k * n, -1.0, 1.0, 23);
    for (long c = 0; c < k; c++) lower_times(L, Xt + c*n, B + c*n, n);
    omp_warmup();

    hand_example();
    double ts, tp;
    BEST_TIME(ts, fwd_serial(L, b, xs, n));
    double err = max_abs_diff(xs, xt, n);
    printf("M05: N = %ld, diagonally dominant L, b = L * x_true\n", n);
    printf("serial: max |x - x_true| = %.2e: %s\n", err, ok_str(err < 1e-10));

    printf("(a) row-oriented: serial i loop, parallel for reduction(+:s) per row\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, fwd_rows(L, b, xp, n));
        table_row(n, THREADS[t], ts, tp, close_doubles(xs, xp, n, 1e-9));
    }
    printf("(b) column-oriented: one region, single x[j] + omp for update of r[i > j]\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, fwd_cols(L, b, xp, r, n));
        table_row(n, THREADS[t], ts, tp, close_doubles(xs, xp, n, 1e-9));
    }
    printf("(c) blocked, nb = %ld columns per step: single small solve + omp for update\n", nb);
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, fwd_blocked(L, b, xp, r, n, nb));
        table_row(n, THREADS[t], ts, tp, close_doubles(xs, xp, n, 1e-9));
    }

    BEST_TIME(ts, fwd_multi_serial(L, B, Xs, n, k));
    err = max_abs_diff(Xs, Xt, k * n);
    printf("\n(d) %ld right-hand sides: serial max |X - X_true| = %.2e: %s\n",
           k, err, ok_str(err < 1e-10));
    printf("    parallel for over the right-hand sides (each solve is serial)\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, fwd_multi_parallel(L, B, Xp, n, k));
        table_row(n, THREADS[t], ts, tp, close_doubles(Xs, Xp, k * n, 1e-12));
    }
    free(L); free(xt); free(b); free(xs); free(xp); free(r);
    free(Xt); free(B); free(Xs); free(Xp);
    return 0;
}

/* Notes:
 * - The race in "#pragma omp parallel for" over i: iteration i reads x[0..i-1],
 *   which other threads may not have written yet (they still hold old values),
 *   so the answer is wrong and changes from run to run. Rows are sequential.
 * - (a) and (b) have only N^2/2 multiply-adds in total, but need N fork/joins or
 *   2N barriers: the synchronisation (microseconds each, more on macOS where
 *   libgomp's barrier puts threads to sleep) costs more than the work of a row,
 *   so they are SLOWER than serial. That is the expected answer, not a bug.
 * - (a) reads row i of L contiguously; (b) reads column j (stride N), which wastes
 *   cache lines, but needs no reduction (each r[i] has one owner).
 * - (c) is the fix: more work per synchronisation. With blocks of nb columns
 *   there are 2N/nb barriers, and each parallel step is a matrix-vector product
 *   over contiguous row pieces. The serial part (the small triangles) is only
 *   N*nb/2 multiply-adds. Bigger nb: fewer barriers but a bigger serial part.
 *   (b) could also drop the single: every thread computes x[j] = r[j]/L[j][j]
 *   itself (as m04 does with its pivot), one barrier per column.
 * - (d) is embarrassingly parallel: the k systems are independent. This is how
 *   L^-1 (k = N columns of the identity) or many solves with the same factor
 *   (after Cholesky m04 / LU p070) are parallelised in practice.
 * - Back substitution (upper triangular U x = y) is the same with i running from
 *   N-1 down to 0 (problems/p071). Cholesky solve: L y = b, then L^T x = y.
 * - The sums are added in a different order in (a), (b) and (c), so compare with
 *   a relative tolerance. A strictly diagonally dominant L keeps the errors small.
 */

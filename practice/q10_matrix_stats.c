/* CS527 Computer Systems — OpenMP practice
 * Q10: For an N x N matrix compute in parallel: all row sums, all column sums,
 *      the trace, the Frobenius norm sqrt(sum a_ij^2), and the maximum element
 *      together with its (i,j) position.
 *  - row sums          : parallel for over rows (each thread owns rs[i])
 *  - column sums (a)   : parallel for over columns (owns cs[j], stride-N reads)
 *  - column sums (b)   : parallel for over ROWS with array reduction
 *                        reduction(+:cs[:n]) (row-wise, cache-friendly reads)
 *  - trace             : reduction(+:t)
 *  - Frobenius norm    : collapse(2) reduction(+:ss)
 *  - max + position    : per-thread local best, merged in a critical section
 * Matrices are flattened row-major: A[i][j] = A[(long)i*n + j].
 * Usage: ./q10 [N]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ---------- serial reference versions ---------- */
static void row_sums_serial(const double *A, long n, double *rs) {
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j];
        rs[i] = s;
    }
}
static void col_sums_serial(const double *A, long n, double *cs) {
    for (long j = 0; j < n; j++) cs[j] = 0.0;
    for (long i = 0; i < n; i++)                 /* row-wise: cache friendly */
        for (long j = 0; j < n; j++) cs[j] += A[i*n + j];
}
static double trace_serial(const double *A, long n) {
    double t = 0.0;
    for (long i = 0; i < n; i++) t += A[i*n + i];
    return t;
}
static double frobenius_serial(const double *A, long n) {
    double ss = 0.0;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) ss += A[i*n + j] * A[i*n + j];
    return sqrt(ss);
}
/* strict '>' keeps the FIRST occurrence (row-major order) of the maximum */
static double max_serial(const double *A, long n, long *bi, long *bj) {
    double best = A[0]; *bi = 0; *bj = 0;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (A[i*n + j] > best) { best = A[i*n + j]; *bi = i; *bj = j; }
    return best;
}

/* ---------- parallel versions ---------- */
static void row_sums_parallel(const double *A, long n, double *rs) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j];
        rs[i] = s;
    }
}
/* (a) each thread owns whole columns: no race, but A is read with stride n */
static void col_sums_parallel_cols(const double *A, long n, double *cs) {
    #pragma omp parallel for
    for (long j = 0; j < n; j++) {
        double s = 0.0;
        for (long i = 0; i < n; i++) s += A[i*n + j];
        cs[j] = s;
    }
}
/* (b) rows split across threads; every thread adds into its own private copy
 * of cs[0..n-1] (zeroed by OpenMP), the copies are summed into cs at the end. */
static void col_sums_parallel_reduce(const double *A, long n, double *cs) {
    for (long j = 0; j < n; j++) cs[j] = 0.0;
    #pragma omp parallel for reduction(+:cs[:n])
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) cs[j] += A[i*n + j];
}
static double trace_parallel(const double *A, long n) {
    double t = 0.0;
    #pragma omp parallel for reduction(+:t)
    for (long i = 0; i < n; i++) t += A[i*n + i];
    return t;
}
static double frobenius_parallel(const double *A, long n) {
    double ss = 0.0;
    #pragma omp parallel for collapse(2) reduction(+:ss)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) ss += A[i*n + j] * A[i*n + j];
    return sqrt(ss);
}
/* reduction(max:) would give the value but not WHERE it is, so each thread keeps
 * its own (value,i,j) and merges it once. Ties go to the smaller (i,j), so the
 * answer matches serial no matter which thread enters the critical first. */
static double max_parallel(const double *A, long n, long *bi, long *bj) {
    double best = A[0]; long gi = 0, gj = 0;
    #pragma omp parallel
    {
        double lb = A[0]; long li = 0, lj = 0;      /* thread-local best */
        #pragma omp for
        for (long i = 0; i < n; i++)
            for (long j = 0; j < n; j++)
                if (A[i*n + j] > lb) { lb = A[i*n + j]; li = i; lj = j; }
        #pragma omp critical
        {
            if (lb > best || (lb == best && (li < gi || (li == gi && lj < gj)))) {
                best = lb; gi = li; gj = lj;
            }
        }
    }
    *bi = gi; *bj = gj;
    return best;
}

/* relative tolerance: a different summation order changes the last bits */
static int close_val(double a, double b) { return fabs(a - b) <= 1e-9 * (1.0 + fabs(a)); }
static int close_vec(const double *x, const double *y, long n) {
    for (long i = 0; i < n; i++) if (!close_val(x[i], y[i])) return 0;
    return 1;
}
static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static int run(long n) {
    long n2 = n * n;
    double *A   = malloc((size_t)n2 * sizeof(double));
    double *rsS = malloc((size_t)n * sizeof(double)), *rsP = malloc((size_t)n * sizeof(double));
    double *csS = malloc((size_t)n * sizeof(double)), *csP = malloc((size_t)n * sizeof(double));
    if (!A || !rsS || !rsP || !csS || !csP) {
        fprintf(stderr, "alloc failed (N=%ld)\n", n);
        free(A); free(rsS); free(rsP); free(csS); free(csP); return 1;
    }
    /* values in [-6.25, 6.25]; the max 6.25 occurs many times (tests tie-break) */
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            A[i*n + j] = (double)((i * 31 + j * 17) % 101 - 50) / 8.0;

    /* serial references (timed) */
    long si, sj, pi, pj;
    row_sums_serial(A, n, rsS);              /* untimed warm-up: 1st sweep of new memory is slow */
    double t0 = omp_get_wtime(); row_sums_serial(A, n, rsS);     double ts_row = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); col_sums_serial(A, n, csS);            double ts_col = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); double trS = trace_serial(A, n);       double ts_tr  = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); double frS = frobenius_serial(A, n);   double ts_fr  = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); double mxS = max_serial(A, n, &si, &sj); double ts_mx = omp_get_wtime() - t0;

    printf("Q10: matrix statistics (N = %ld)\n", n);
    printf("serial: trace = %.4f  frobenius = %.4f  max = %.4f at (%ld,%ld)\n", trS, frS, mxS, si, sj);
    printf("serial(s): rows %.6f  cols %.6f  trace %.6f  frob %.6f  max %.6f\n",
           ts_row, ts_col, ts_tr, ts_fr, ts_mx);

    /* correctness of every version at 8 threads */
    omp_set_num_threads(8);
    row_sums_parallel(A, n, rsP);
    printf("row sums           : %s\n", close_vec(rsS, rsP, n) ? "OK" : "WRONG");
    col_sums_parallel_cols(A, n, csP);
    printf("col sums (by cols) : %s\n", close_vec(csS, csP, n) ? "OK" : "WRONG");
    col_sums_parallel_reduce(A, n, csP);
    printf("col sums (reduce)  : %s\n", close_vec(csS, csP, n) ? "OK" : "WRONG");
    printf("trace              : %s\n", close_val(trS, trace_parallel(A, n)) ? "OK" : "WRONG");
    printf("frobenius          : %s\n", close_val(frS, frobenius_parallel(A, n)) ? "OK" : "WRONG");
    double mxP = max_parallel(A, n, &pi, &pj);
    printf("max + (i,j)        : %s  (%.4f at (%ld,%ld))\n",
           (mxP == mxS && pi == si && pj == sj) ? "OK" : "WRONG", mxP, pi, pj);

    /* trace is only N additions: too small to time meaningfully, so not in the table */
    printf("%8s %10s %6s %10s %6s %10s %6s %10s %6s %10s %6s %6s\n", "threads",
           "rows(s)", "sp", "colsJ(s)", "sp", "colsRed(s)", "sp",
           "frob(s)", "sp", "max(s)", "sp", "check");
    const int threads[] = {1, 2, 4, 8};
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        t0 = omp_get_wtime(); row_sums_parallel(A, n, rsP);          double tr = omp_get_wtime() - t0;
        int ok = close_vec(rsS, rsP, n);
        t0 = omp_get_wtime(); col_sums_parallel_cols(A, n, csP);     double tcj = omp_get_wtime() - t0;
        ok = ok && close_vec(csS, csP, n);
        t0 = omp_get_wtime(); col_sums_parallel_reduce(A, n, csP);   double tcr = omp_get_wtime() - t0;
        ok = ok && close_vec(csS, csP, n);
        t0 = omp_get_wtime(); double fr = frobenius_parallel(A, n);  double tf = omp_get_wtime() - t0;
        ok = ok && close_val(frS, fr);
        t0 = omp_get_wtime(); double mx = max_parallel(A, n, &pi, &pj); double tm = omp_get_wtime() - t0;
        ok = ok && mx == mxS && pi == si && pj == sj;
        printf("%8d %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %6s\n",
               threads[ti], tr, speedup(ts_row, tr), tcj, speedup(ts_col, tcj),
               tcr, speedup(ts_col, tcr), tf, speedup(ts_fr, tf), tm, speedup(ts_mx, tm),
               ok ? "OK" : "WRONG");
    }
    printf("\n");
    free(A); free(rsS); free(rsP); free(csS); free(csP);
    return 0;
}

int main(int argc, char **argv) {
    long sizes[] = {1024, 4096};             /* 4096^2 doubles = 128 MB */
    int nsizes = 2;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (sizes[0] < 1) { fprintf(stderr, "usage: %s [N>=1]\n", argv[0]); return 1; }
    for (int s = 0; s < nsizes; s++)
        if (run(sizes[s])) return 1;
    return 0;
}

/* Viva notes:
 * - Row sums / column sums by column: each iteration owns one output element
 *   (rs[i] or cs[j]) and sums into a local `s`, so there is nothing to reduce.
 * - Column sums by column read A with stride N (one cache line per element), so
 *   even 8 threads lose to the row-wise serial loop at large N (speedup < 1).
 *   The array reduction reads A row-wise. Its cost: every thread
 *   gets a private n-double copy of cs (on the thread stack in GCC -> very large n
 *   may need OMP_STACKSIZE) plus a final merge. Parallel over rows WITHOUT the
 *   reduction would be a race on cs[j].
 * - collapse(2) merges the i and j loops into one N*N iteration space, which
 *   helps load balance when N is small compared to the thread count.
 * - Max + position: reduction(max:) only returns the value. Options: local best +
 *   critical (here, one critical entry per thread, so cheap), or a user-defined
 *   reduction with `#pragma omp declare reduction` on a struct {val,i,j}.
 *   Doing `critical` per element would serialise the whole loop.
 * - Speedup is limited because these are memory-bound: 1-2 flops per 8-byte load.
 *   ==/> on doubles is exact; sums are checked with a relative tolerance.
 */

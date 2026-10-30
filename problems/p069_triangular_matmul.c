/* P069: Product of two upper-triangular matrices (load balancing)
 * Problem: A and B are N x N upper-triangular matrices (zero below the diagonal).
 *   Their product C = A*B is upper triangular, with
 *   C[i][j] = sum_{k=i..j} A[i][k]*B[k][j] for j >= i and C[i][j] = 0 for j < i.
 *   Compute it serially and with OpenMP using only these terms, compare
 *   schedule(static) with schedule(dynamic) (and static,1), verify against an
 *   ordinary full matrix multiplication and report the timings.
 * Solution: row-wise (ikj) form: for k = i..N-1, row i of C (columns j = k..N-1)
 *   += A[i][k] * row k of B (only j >= k is nonzero). Row i of C is written only
 *   by iteration i, so parallel for over i is race-free. Row i costs
 *   (N-i)(N-i+1)/2 multiply-adds: the first rows are far more expensive than the
 *   last, so schedule(static), which gives each thread one contiguous block of
 *   rows, is badly unbalanced (with 4 threads, thread 0 gets ~58% of the work).
 *   schedule(dynamic, 4) hands out small chunks to whichever thread is free;
 *   schedule(static, 1) deals the rows round robin. Matrices are flattened
 *   row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (Eijkhout Ch. 19 schedules; triangular loops).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p069_triangular_matmul.c -o p069 -lm
 * Run:   ./p069 [N]   (default N = 1000)
 */
#include "common.h"

/* Row i of C = sum over k in [i, n) of A[i][k] * (row k of B, columns k..n-1). */
static void tri_row(const double *A, const double *B, double *C, long n, long i) {
    for (long j = 0; j < n; j++) C[i*n + j] = 0.0;       /* includes the zero part j < i */
    for (long k = i; k < n; k++) {
        double a = A[i*n + k];
        for (long j = k; j < n; j++) C[i*n + j] += a * B[k*n + j];
    }
}

static void trimul_serial(const double *A, const double *B, double *C, long n) {
    for (long i = 0; i < n; i++) tri_row(A, B, C, n, i);
}
static void trimul_static(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) tri_row(A, B, C, n, i);
}
static void trimul_static1(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for schedule(static, 1)
    for (long i = 0; i < n; i++) tri_row(A, B, C, n, i);
}
static void trimul_dynamic(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for schedule(dynamic, 4)
    for (long i = 0; i < n; i++) tri_row(A, B, C, n, i);
}

/* Reference: ordinary full product (zeros included), serial ikj. */
static void full_matmul(const double *A, const double *B, double *C, long n) {
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) C[i*n + j] = 0.0;
        for (long k = 0; k < n; k++) {
            double a = A[i*n + k];
            for (long j = 0; j < n; j++) C[i*n + j] += a * B[k*n + j];
        }
    }
}

static void make_upper(double *A, long n, unsigned long long seed) {
    fill_double(A, n * n, -1.0, 1.0, seed);
    for (long i = 0; i < n; i++)
        for (long j = 0; j < i; j++) A[i*n + j] = 0.0;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000);
    long nn = n * n;
    size_t bytes = (size_t)nn * sizeof(double);
    double *A = xmalloc(bytes), *B = xmalloc(bytes);
    double *Cf = xmalloc(bytes), *Cs = xmalloc(bytes), *Cp = xmalloc(bytes);
    make_upper(A, n, 691);
    make_upper(B, n, 692);
    omp_warmup();

    double a3[9] = {1, 2, 3, 0, 4, 5, 0, 0, 6}, b3[9] = {1, 1, 1, 0, 1, 1, 0, 0, 1}, c3[9];
    trimul_dynamic(a3, b3, c3, 3);
    printf("Worked example: A = [1 2 3; 0 4 5; 0 0 6], B = [1 1 1; 0 1 1; 0 0 1]\n");
    printf("  C = A*B = [%g %g %g; %g %g %g; %g %g %g]   (expected [1 3 6; 0 4 9; 0 0 6])\n",
           c3[0], c3[1], c3[2], c3[3], c3[4], c3[5], c3[6], c3[7], c3[8]);

    full_matmul(A, B, Cf, n);
    double ts, tp;
    BEST_TIME(ts, trimul_serial(A, B, Cs, n));
    printf("\nP069: upper x upper, N = %ld: %ld multiply-adds (full product: %ld)\n",
           n, n * (n + 1) * (n + 2) / 6, n * n * n);
    printf("serial triangular product = full product: %s\n", ok_str(close_doubles(Cf, Cs, nn, 1e-10)));

    /* Work share of thread 0 under schedule(static) with 4 threads. */
    double total = 0.0, first = 0.0;
    for (long i = 0; i < n; i++) {
        double w = (double)(n - i) * (double)(n - i + 1) / 2.0;
        total += w;
        if (i < (n + 3) / 4) first += w;
    }
    printf("schedule(static), 4 threads: thread 0's rows hold %.0f%% of the work (ideal 25%%)\n",
           total > 0 ? 100.0 * first / total : 0.0);

    const char *title[3] = {
        "(1) schedule(static): contiguous row blocks, unbalanced",
        "(2) schedule(static, 1): rows dealt round robin",
        "(3) schedule(dynamic, 4): chunks of 4 rows to any free thread"};
    void (*fn[3])(const double *, const double *, double *, long) = {
        trimul_static, trimul_static1, trimul_dynamic};
    for (int v = 0; v < 3; v++) {
        printf("\n%s\n", title[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            memset(Cp, 0, bytes);
            BEST_TIME(tp, fn[v](A, B, Cp, n));
            table_row(n, THREADS[k], ts, tp, close_doubles(Cf, Cp, nn, 1e-10));
        }
    }

    free(A); free(B); free(Cf); free(Cs); free(Cp);
    return 0;
}

/* Notes:
 * - Upper x upper = upper: A[i][k] = 0 for k < i and B[k][j] = 0 for k > j, so
 *   only k in [i, j] contributes, and C[i][j] = 0 for j < i. This skips 5/6 of
 *   the full product's work (N^3/6 instead of N^3 multiply-adds).
 * - Work per row falls like (N-i)^2/2, so equal-sized row blocks (static) are
 *   unequal work: with 4 threads, thread 0 does ~58% (1 - (3/4)^3), capping the
 *   speedup near 1.7. dynamic/static,1 fix it; guided would be poor (its first,
 *   largest chunks are the most expensive rows).
 * - collapse(2) cannot be used: the k and j bounds depend on i (non-rectangular,
 *   not allowed in OpenMP 4.5), and the k loop accumulates into C[i][j] anyway.
 * - Parallelising k or j instead of i: k races (all k add into the same row of
 *   C); j is legal but gives tiny, unbalanced work per thread.
 */

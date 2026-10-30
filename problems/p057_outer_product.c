/* P057: Outer product and rank-1 update
 * Problem: Given vectors x (length M) and y (length N), form the M x N outer
 *   product A = x y^T (A[i][j] = x[i]*y[j]) and apply the rank-1 update
 *   B = B + alpha * x y^T to an existing M x N matrix B, serially and with OpenMP
 *   using collapse(2). Verify both and report time, speedup and efficiency for
 *   1, 2, 4 and 8 threads. Also show why collapse(2) matters when M is small.
 * Solution: iteration (i, j) writes only its own element A[i][j] (or B[i][j]) and
 *   only reads x[i], y[j]: all M*N iterations are independent, so there is no
 *   race and no reduction. collapse(2) turns the perfectly nested, rectangular
 *   i/j loops into one M*N loop that is split evenly among the threads. With a
 *   tiny M (e.g. 3 rows) "parallel for" on i alone can keep only M threads busy,
 *   while collapse(2) uses them all. Matrices are flattened row-major:
 *   A[i][j] = A[i*N + j].
 * Source: lab extension (BLAS-2 operations dger / outer product).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p057_outer_product.c -o p057 -lm
 * Run:   ./p057 [M] [N] [alpha]   (defaults M = N = 3000, alpha = 0.5)
 */
#include "common.h"

/* ---------- A = x y^T ---------- */
static void outer_serial(const double *x, const double *y, double *A, long m, long n) {
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            A[i*n + j] = x[i] * y[j];
}

static void outer_collapse(const double *x, const double *y, double *A, long m, long n) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            A[i*n + j] = x[i] * y[j];          /* own element: no race */
}

/* Outer loop only: at most m threads get work. */
static void outer_rows(const double *x, const double *y, double *A, long m, long n) {
    #pragma omp parallel for
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            A[i*n + j] = x[i] * y[j];
}

/* ---------- B += alpha x y^T (rank-1 update) ---------- */
static void rank1_serial(double *B, const double *x, const double *y, long m, long n,
                         double alpha) {
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            B[i*n + j] += alpha * x[i] * y[j];
}

static void rank1_collapse(double *B, const double *x, const double *y, long m, long n,
                           double alpha) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            B[i*n + j] += alpha * x[i] * y[j];  /* read-modify-write of own element */
}

/* How many threads of the current team get at least one iteration? */
static int busy_threads(int use_collapse, long m, long n) {
    int busy[256] = {0};                  /* busy[t] = 1 if thread t got work */
    if (use_collapse) {
        #pragma omp parallel for collapse(2)
        for (long i = 0; i < m; i++)
            for (long j = 0; j < n; j++) busy[omp_get_thread_num() % 256] = 1;
    } else {
        #pragma omp parallel for
        for (long i = 0; i < m; i++)
            for (long j = 0; j < n; j++) busy[omp_get_thread_num() % 256] = 1;
    }
    int count = 0;
    for (int t = 0; t < 256; t++) count += busy[t];
    return count;
}

static void print_mat(const char *name, const double *M, long r, long c) {
    printf("  %s =\n", name);
    for (long i = 0; i < r; i++) {
        printf("   ");
        for (long j = 0; j < c; j++) printf(" %6g", M[i*c + j]);
        printf("\n");
    }
}

static void tiny_example(void) {
    double x[3] = {1, 2, 3}, y[4] = {1, 0, -1, 2};
    double A[12], B[12];
    outer_collapse(x, y, A, 3, 4);
    for (int k = 0; k < 12; k++) B[k] = 1.0;           /* B = all ones */
    rank1_collapse(B, x, y, 3, 4, 2.0);
    printf("Worked example: x = [1 2 3], y = [1 0 -1 2]\n");
    print_mat("A = x y^T (3x4)", A, 3, 4);
    print_mat("B = ones + 2 x y^T", B, 3, 4);
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 3000);
    long n = arg_long(argc, argv, 2, m);
    double alpha = arg_double(argc, argv, 3, 0.5);
    long mn = m * n;
    size_t bytes = (size_t)mn * sizeof(double);
    long cap = mn > 3 ? mn : 3;                     /* As, Ap are reused by Part 3 */
    double *x = xmalloc((size_t)m * sizeof(double)), *y = xmalloc((size_t)n * sizeof(double));
    double *As = xmalloc((size_t)cap * sizeof(double)), *Ap = xmalloc((size_t)cap * sizeof(double));
    double *B0 = xmalloc(bytes);
    fill_double(x, m, -1.0, 1.0, 1);
    fill_double(y, n, -1.0, 1.0, 2);
    fill_double(B0, mn, -1.0, 1.0, 3);
    omp_warmup();

    tiny_example();
    printf("\nP057: x has %ld, y has %ld entries, A and B are %ld x %ld, alpha = %g\n",
           m, n, m, n, alpha);

    double ts, tp;
    /* Part 1: A = x y^T */
    BEST_TIME(ts, outer_serial(x, y, As, m, n));
    printf("\nPart 1: A = x y^T, collapse(2)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Ap, 0, bytes);
        BEST_TIME(tp, outer_collapse(x, y, Ap, m, n));
        table_row(mn, THREADS[k], ts, tp, close_doubles(As, Ap, mn, 1e-12));
    }

    /* Part 2: B += alpha x y^T. The update changes B, so time it on a scratch copy
       and verify ONE update applied to a fresh copy of B0. */
    memcpy(As, B0, bytes);
    rank1_serial(As, x, y, m, n, alpha);              /* reference: B0 + alpha x y^T */
    memcpy(Ap, B0, bytes);
    BEST_TIME(ts, rank1_serial(Ap, x, y, m, n, alpha));
    printf("\nPart 2: B += alpha x y^T (rank-1 update), collapse(2)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rank1_collapse(Ap, x, y, m, n, alpha));   /* timing only */
        memcpy(Ap, B0, bytes);
        rank1_collapse(Ap, x, y, m, n, alpha);                  /* one update to check */
        table_row(mn, THREADS[k], ts, tp, close_doubles(As, Ap, mn, 1e-12));
    }

    /* Part 3: same number of elements but only 3 rows: rows-only vs collapse(2). */
    long m3 = 3, n3 = mn / 3 > 0 ? mn / 3 : 1;      /* m3*n3 <= cap */
    double *x3 = xmalloc((size_t)m3 * sizeof(double)), *y3 = xmalloc((size_t)n3 * sizeof(double));
    fill_double(x3, m3, -1.0, 1.0, 4);
    fill_double(y3, n3, -1.0, 1.0, 5);
    BEST_TIME(ts, outer_serial(x3, y3, As, m3, n3));
    printf("\nPart 3: A = x y^T with only %ld rows (%ld x %ld)\n", m3, m3, n3);
    printf("(a) parallel for on i only: at most %ld threads have work\n", m3);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Ap, 0, (size_t)(m3 * n3) * sizeof(double));
        BEST_TIME(tp, outer_rows(x3, y3, Ap, m3, n3));
        table_row(m3 * n3, THREADS[k], ts, tp, close_doubles(As, Ap, m3 * n3, 1e-12));
    }
    printf("(b) collapse(2): all %ld (i, j) pairs shared among all threads\n", m3 * n3);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Ap, 0, (size_t)(m3 * n3) * sizeof(double));
        BEST_TIME(tp, outer_collapse(x3, y3, Ap, m3, n3));
        table_row(m3 * n3, THREADS[k], ts, tp, close_doubles(As, Ap, m3 * n3, 1e-12));
    }
    omp_set_num_threads(8);
    printf("Threads that got work in a team of 8: parallel for on i = %d, collapse(2) = %d\n",
           busy_threads(0, m3, n3), busy_threads(1, m3, n3));

    free(x); free(y); free(As); free(Ap); free(B0); free(x3); free(y3);
    return 0;
}

/* Notes:
 * - The table's N column is the number of matrix elements M*N.
 * - Every (i, j) owns A[i][j], so collapse(2) is safe; collapse is only legal for
 *   perfectly nested rectangular loops (OpenMP 4.5) and only CORRECT when no two
 *   (i, j) iterations write the same location (compare y[i] += A[i][j]*x[j], p066).
 * - Part 3: with 3 rows and 8 threads, parallel for on i leaves 5 threads idle
 *   (the program counts them); collapse(2) splits all 3*N iterations evenly. The
 *   time gap is small here only because the loop is memory-bound; with more work
 *   per element it approaches 8/3. With M >> threads both are equal.
 * - The update B += ... reads and writes each element once and does 2-3 flops per
 *   16 bytes moved: memory-bound, so speedup stops at memory bandwidth.
 * - Timing a function that modifies its input (rank-1 update) needs care: time it
 *   on scratch data, then verify a single call on a fresh copy.
 */

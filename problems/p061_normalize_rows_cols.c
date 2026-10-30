/* P061: Row normalisation and column standardisation
 * Problem: Given an M x N matrix A of positive doubles, (1) row-normalise it:
 *   divide every row by its sum so each row sums to 1; (2) column-standardise
 *   it: replace A[i][j] by (A[i][j] - mean_j) / sd_j, where mean_j and sd_j are
 *   the mean and (population) standard deviation of column j, so each column has
 *   mean 0 and standard deviation 1. Do both serially and with OpenMP: rows
 *   parallel over rows; columns (a) parallel over columns and (b) with column
 *   sums computed by an array reduction over rows. Verify and time all versions.
 * Solution: (1) row i is read and written only by the thread that owns iteration
 *   i; its sum is a private local, so parallel for over i is race-free and reads
 *   contiguously. (2a) parallel for over j: column j (its mean, sd and update) is
 *   owned by one thread, race-free, but walks memory with stride N. (2b) three
 *   row-parallel passes: column sums with reduction(+:sum[:n]), squared
 *   deviations with reduction(+:sq[:n]) (rows share the per-column totals, hence
 *   the array reduction), then an element-wise update parallel over rows; all
 *   reads are contiguous. Two-pass variance (mean first) avoids the cancellation
 *   of E[x^2] - mean^2. Matrices are flattened row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (data preprocessing: normalise / standardise).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p061_normalize_rows_cols.c -o p061 -lm
 * Run:   ./p061 [M] [N]   (defaults M = 3000 rows, N = 2000 columns)
 */
#include "common.h"

/* ---------- (1) row normalisation ---------- */
static void rownorm_serial(double *A, long m, long n) {
    for (long i = 0; i < m; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j];
        if (s != 0.0)
            for (long j = 0; j < n; j++) A[i*n + j] /= s;
    }
}

static void rownorm_par(double *A, long m, long n) {
    #pragma omp parallel for
    for (long i = 0; i < m; i++) {
        double s = 0.0;                          /* private: declared in the loop */
        for (long j = 0; j < n; j++) s += A[i*n + j];
        if (s != 0.0)
            for (long j = 0; j < n; j++) A[i*n + j] /= s;
    }
}

/* ---------- (2) column standardisation ---------- */
/* Serial reference: row-order passes (cache-friendly). */
static void colstd_serial(double *A, long m, long n) {
    double *mean = xmalloc((size_t)n * sizeof(double)), *sq = xmalloc((size_t)n * sizeof(double));
    for (long j = 0; j < n; j++) mean[j] = sq[j] = 0.0;
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) mean[j] += A[i*n + j];
    for (long j = 0; j < n; j++) mean[j] /= (double)m;
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) { double d = A[i*n + j] - mean[j]; sq[j] += d * d; }
    for (long j = 0; j < n; j++) {                       /* sq[j] becomes 1/sd */
        double sd = sqrt(sq[j] / (double)m);
        sq[j] = sd > 0.0 ? 1.0 / sd : 0.0;               /* constant column -> 0 */
    }
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) A[i*n + j] = (A[i*n + j] - mean[j]) * sq[j];
    free(mean); free(sq);
}

/* (2a) parallel over columns: everything for column j done by one thread. */
static void colstd_by_column(double *A, long m, long n) {
    #pragma omp parallel for
    for (long j = 0; j < n; j++) {
        double mean = 0.0, sq = 0.0;
        for (long i = 0; i < m; i++) mean += A[i*n + j];          /* stride n */
        mean /= (double)m;
        for (long i = 0; i < m; i++) { double d = A[i*n + j] - mean; sq += d * d; }
        double sd = sqrt(sq / (double)m);
        double inv = sd > 0.0 ? 1.0 / sd : 0.0;
        for (long i = 0; i < m; i++) A[i*n + j] = (A[i*n + j] - mean) * inv;
    }
}

/* (2b) parallel over rows with array reductions for the column totals. */
static void colstd_reduction(double *A, long m, long n) {
    double *mean = xmalloc((size_t)n * sizeof(double)), *sq = xmalloc((size_t)n * sizeof(double));
    for (long j = 0; j < n; j++) mean[j] = sq[j] = 0.0;

    #pragma omp parallel for reduction(+:mean[:n])
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) mean[j] += A[i*n + j];       /* private copy */
    for (long j = 0; j < n; j++) mean[j] /= (double)m;

    #pragma omp parallel for reduction(+:sq[:n])
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) { double d = A[i*n + j] - mean[j]; sq[j] += d * d; }
    for (long j = 0; j < n; j++) {
        double sd = sqrt(sq[j] / (double)m);
        sq[j] = sd > 0.0 ? 1.0 / sd : 0.0;
    }

    #pragma omp parallel for                                      /* element-wise */
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) A[i*n + j] = (A[i*n + j] - mean[j]) * sq[j];
    free(mean); free(sq);
}

/* ---------- property checks ---------- */
static int rows_sum_to_one(const double *A, long m, long n) {
    for (long i = 0; i < m; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j];
        if (!close1(s, 1.0, 1e-9)) return 0;
    }
    return 1;
}
static int cols_standardised(const double *A, long m, long n) {
    for (long j = 0; j < n; j++) {
        double mean = 0.0, sq = 0.0;
        for (long i = 0; i < m; i++) mean += A[i*n + j];
        mean /= (double)m;
        for (long i = 0; i < m; i++) sq += (A[i*n + j] - mean) * (A[i*n + j] - mean);
        if (fabs(mean) > 1e-9 || !close1(sqrt(sq / (double)m), 1.0, 1e-9)) return 0;
    }
    return 1;
}

static void print_mat(const char *name, const double *A, long m, long n) {
    printf("  %s =\n", name);
    for (long i = 0; i < m; i++) {
        printf("   ");
        for (long j = 0; j < n; j++) printf(" %8.4f", A[i*n + j]);
        printf("\n");
    }
}

static void tiny_example(void) {
    double A0[9] = {1, 2, 1,  2, 2, 4,  3, 6, 9}, R[9], C[9];
    memcpy(R, A0, sizeof A0);
    memcpy(C, A0, sizeof A0);
    rownorm_par(R, 3, 3);
    colstd_reduction(C, 3, 3);
    printf("Worked example (3x3):\n");
    print_mat("A", A0, 3, 3);
    print_mat("rows normalised (each row sums to 1)", R, 3, 3);
    print_mat("columns standardised (col 0 = 1,2,3 -> -1.2247 0 1.2247)", C, 3, 3);
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 3000);
    long n = arg_long(argc, argv, 2, 2000);
    long mn = m * n;
    size_t bytes = (size_t)mn * sizeof(double);
    double *A0 = xmalloc(bytes), *Ref = xmalloc(bytes), *W = xmalloc(bytes);
    fill_double(A0, mn, 1.0, 10.0, 77);                  /* positive: row sums > 0 */
    omp_warmup();

    tiny_example();
    printf("\nP061: A is %ld x %ld (M rows, N columns; the tables' N column shows M)\n", m, n);

    /* Each version works in place, so: time it on the scratch copy W (repeated
       calls do the same work), then verify ONE call on a fresh copy of A0. */
    double ts, tp;

    /* Part 1: row normalisation */
    memcpy(Ref, A0, bytes);
    rownorm_serial(Ref, m, n);
    memcpy(W, A0, bytes);
    BEST_TIME(ts, rownorm_serial(W, m, n));
    printf("\nPart 1: row normalisation, parallel over rows (rows sum to 1: %s)\n",
           ok_str(rows_sum_to_one(Ref, m, n)));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rownorm_par(W, m, n));
        memcpy(W, A0, bytes);
        rownorm_par(W, m, n);
        table_row(m, THREADS[k], ts, tp, close_doubles(Ref, W, mn, 1e-12));
    }

    /* Part 2: column standardisation */
    memcpy(Ref, A0, bytes);
    colstd_serial(Ref, m, n);
    memcpy(W, A0, bytes);
    BEST_TIME(ts, colstd_serial(W, m, n));
    if (m >= 2)
        printf("\nPart 2: column standardisation (columns have mean 0, sd 1: %s)\n",
               ok_str(cols_standardised(Ref, m, n)));
    else
        printf("\nPart 2: column standardisation (M = 1: every column is constant -> 0)\n");
    printf("(a) parallel over columns j (stride-N access)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, colstd_by_column(W, m, n));
        memcpy(W, A0, bytes);
        colstd_by_column(W, m, n);
        table_row(m, THREADS[k], ts, tp, close_doubles(Ref, W, mn, 1e-9));
    }
    printf("(b) parallel over rows, reduction(+:mean[:n]) and reduction(+:sq[:n])\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, colstd_reduction(W, m, n));
        memcpy(W, A0, bytes);
        colstd_reduction(W, m, n);
        table_row(m, THREADS[k], ts, tp, close_doubles(Ref, W, mn, 1e-9));
    }

    free(A0); free(Ref); free(W);
    return 0;
}

/* Notes:
 * - Rows: the natural parallel loop is the row loop; each row's sum is private,
 *   reads are contiguous, no synchronisation at all.
 * - Columns, (a): parallel over j is race-free (one thread per column) but each
 *   access jumps N doubles, and neighbouring columns of different threads share
 *   cache lines (false sharing on the writes at block edges).
 * - Columns, (b): parallelising the row loop of "sum[j] += A[i][j]" WITHOUT a
 *   reduction races on sum[j] (every row adds to every column). reduction(+:a[:n])
 *   (OpenMP 4.5) gives each thread a private copy of n doubles, combined at the
 *   end of the loop; the implicit barrier after each loop separates the passes.
 * - Different summation order -> compare with a tolerance; population sd divides
 *   by M (use M-1 for the sample sd). Guard sd == 0 (constant column) and a zero
 *   row sum to avoid dividing by zero.
 * - Timing in-place transforms: time on scratch data, verify one call on a fresh copy.
 */

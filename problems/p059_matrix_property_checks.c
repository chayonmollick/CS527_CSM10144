/* P059: Matrix property checks with reduction(&&:)
 * Problem: Write serial and OpenMP functions that test whether an N x N matrix A
 *   is (1) upper triangular (A[i][j] == 0 for all j < i), (2) lower triangular
 *   (A[i][j] == 0 for all j > i), (3) diagonal (A[i][j] == 0 for all i != j) and
 *   (4) strictly diagonally dominant (|A[i][i]| > sum_{j != i} |A[i][j]| for every
 *   row i). Test them on matrices that have each property and on matrices that
 *   fail it by a single element, verify the parallel answers against serial and
 *   the expected answers, and time each check.
 * Solution: each check is "AND over many independent conditions", so
 *   reduction(&&:ok): every thread starts with a private ok = 1, clears it when it
 *   finds a violation, and the copies are AND-ed at the end (no race on ok).
 *   (1)/(2) parallel for over rows i; the inner j loop covers only the triangle,
 *   so rows have unequal work -> schedule(dynamic, 32) (collapse is not allowed:
 *   the j bound depends on i). (3) all (i, j) pairs: rectangular, so collapse(2).
 *   (4) parallel over rows; the off-diagonal row sum is a private local of the
 *   iteration. A parallel loop cannot "break" early, so a failing matrix is still
 *   scanned to the end by the parallel version. Matrices are flattened row-major:
 *   A[i][j] = A[i*N + j].
 * Source: lab extension (MATRIX_RULES.md section 4: special matrices).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p059_matrix_property_checks.c -o p059 -lm
 * Run:   ./p059 [N]   (default N = 3000)
 */
#include "common.h"

/* ---------- serial checks (may stop at the first violation) ---------- */
static int is_upper_serial(const double *A, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < i; j++)                  /* below the diagonal */
            if (A[i*n + j] != 0.0) return 0;
    return 1;
}
static int is_lower_serial(const double *A, long n) {
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++)              /* above the diagonal */
            if (A[i*n + j] != 0.0) return 0;
    return 1;
}
static int is_diag_serial(const double *A, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (i != j && A[i*n + j] != 0.0) return 0;
    return 1;
}
static int is_sdd_serial(const double *A, long n) {
    for (long i = 0; i < n; i++) {
        double off = 0.0;
        for (long j = 0; j < n; j++)
            if (j != i) off += fabs(A[i*n + j]);
        if (!(fabs(A[i*n + i]) > off)) return 0;
    }
    return 1;
}

/* ---------- parallel checks: reduction(&&:ok) ---------- */
static int is_upper_par(const double *A, long n) {
    int ok = 1;
    #pragma omp parallel for reduction(&&:ok) schedule(dynamic, 32)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < i; j++)
            if (A[i*n + j] != 0.0) ok = 0;            /* private ok of this thread */
    return ok;
}
static int is_lower_par(const double *A, long n) {
    int ok = 1;
    #pragma omp parallel for reduction(&&:ok) schedule(dynamic, 32)
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++)
            if (A[i*n + j] != 0.0) ok = 0;
    return ok;
}
static int is_diag_par(const double *A, long n) {
    int ok = 1;
    #pragma omp parallel for collapse(2) reduction(&&:ok)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (i != j && A[i*n + j] != 0.0) ok = 0;
    return ok;
}
static int is_sdd_par(const double *A, long n) {
    int ok = 1;
    #pragma omp parallel for reduction(&&:ok)
    for (long i = 0; i < n; i++) {
        double off = 0.0;                             /* private row sum */
        for (long j = 0; j < n; j++)
            if (j != i) off += fabs(A[i*n + j]);
        ok = ok && (fabs(A[i*n + i]) > off);
    }
    return ok;
}

/* ---------- test matrices ---------- */
enum { DENSE, UPPER, LOWER, DIAG };

/* Entries in [1, 2) (never 0), then zero the part outside the shape. */
static void make_shape(double *A, long n, int shape, unsigned long long seed) {
    fill_double(A, n * n, 1.0, 2.0, seed);
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) {
            int keep = shape == DENSE || (shape == UPPER && j >= i) ||
                       (shape == LOWER && j <= i) || (shape == DIAG && j == i);
            if (!keep) A[i*n + j] = 0.0;
        }
}
/* Off-diagonal |row sum| of row i, summed in the same order as the checks. */
static double off_sum(const double *A, long n, long i) {
    double off = 0.0;
    for (long j = 0; j < n; j++)
        if (j != i) off += fabs(A[i*n + j]);
    return off;
}
/* Set every diagonal entry to (off-diagonal row sum + 1): strictly dominant. */
static void make_dominant(double *A, long n) {
    for (long i = 0; i < n; i++) A[i*n + i] = off_sum(A, n, i) + 1.0;
}

/* The one-element change applied after building the shape. */
enum { NONE, AT_LAST_ROW_FIRST_COL, AT_FIRST_ROW_LAST_COL, AT_LAST_ROW_PREV_COL, EQUAL_DIAG };

typedef struct {
    const char *name;
    int shape;            /* DENSE, UPPER, LOWER, DIAG */
    int change;           /* one of the enum above */
    int exp[4];           /* expected upper, lower, diag, SDD; -1 = "true only if N == 1" */
} TestCase;

static int run_case(double *A, long n, const TestCase *tc, int seed) {
    int need2 = tc->change != NONE && tc->change != EQUAL_DIAG;
    if (need2 && n < 2) { printf("  %-44s skipped (needs N >= 2)\n", tc->name); return 1; }
    make_shape(A, n, tc->shape, (unsigned long long)seed);
    switch (tc->change) {                       /* one nonzero where a zero should be */
    case AT_LAST_ROW_FIRST_COL: A[(n-1)*n + 0]     = 0.5; break;   /* below diagonal */
    case AT_FIRST_ROW_LAST_COL: A[0*n + (n-1)]     = 0.5; break;   /* above diagonal */
    case AT_LAST_ROW_PREV_COL:  A[(n-1)*n + (n-2)] = 0.5; break;   /* just below it  */
    default: break;
    }
    make_dominant(A, n);
    if (tc->change == EQUAL_DIAG)               /* row n/2: |a_ii| == sum, not strict */
        A[(n/2)*n + n/2] = off_sum(A, n, n/2);

    int s[4] = { is_upper_serial(A, n), is_lower_serial(A, n), is_diag_serial(A, n), is_sdd_serial(A, n) };
    int p[4] = { is_upper_par(A, n), is_lower_par(A, n), is_diag_par(A, n), is_sdd_par(A, n) };
    int ok = 1, e[4];
    for (int k = 0; k < 4; k++) {
        e[k] = tc->exp[k] == -1 ? (n == 1) : tc->exp[k];
        ok = ok && s[k] == p[k] && s[k] == e[k];
    }
    printf("  %-44s %d %d %d %d    %d %d %d %d    %d %d %d %d   %s\n", tc->name,
           s[0], s[1], s[2], s[3], p[0], p[1], p[2], p[3], e[0], e[1], e[2], e[3], ok_str(ok));
    return ok;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 3000);
    double *A = xmalloc((size_t)n * n * sizeof(double));
    omp_warmup();
    omp_set_num_threads(4);

    const TestCase cases[] = {
        {"upper triangular",                       UPPER, NONE,                  {1, -1, -1, 1}},
        {"upper + one nonzero at (N-1, 0)",        UPPER, AT_LAST_ROW_FIRST_COL, {0, 0, 0, 1}},
        {"lower triangular",                       LOWER, NONE,                  {-1, 1, -1, 1}},
        {"lower + one nonzero at (0, N-1)",        LOWER, AT_FIRST_ROW_LAST_COL, {0, 0, 0, 1}},
        {"diagonal",                               DIAG,  NONE,                  {1, 1, 1, 1}},
        {"diagonal + one nonzero at (N-1, N-2)",   DIAG,  AT_LAST_ROW_PREV_COL,  {0, 1, 0, 1}},
        {"dense, strictly diagonally dominant",    DENSE, NONE,                  {-1, -1, -1, 1}},
        {"dense, row N/2: |a_ii| == off-diag sum", DENSE, EQUAL_DIAG,            {-1, -1, -1, 0}},
    };
    int ncases = (int)(sizeof cases / sizeof cases[0]);

    printf("P059: property checks on N x N matrices, N = %ld (cases run with 4 threads)\n", n);
    printf("  %-44s serial     parallel   expected\n", "case");
    printf("  %-44s U L D S    U L D S    U L D S\n", "");
    int all = 1;
    for (int c = 0; c < ncases; c++) all = run_case(A, n, &cases[c], 100 + c) && all;
    printf("  all cases: %s   (U upper, L lower, D diagonal, S strictly diag. dominant)\n",
           ok_str(all));

    /* Timing on a diagonal, dominant matrix: it passes all four checks, so every
       check must scan its whole region (worst case). */
    make_shape(A, n, DIAG, 1);
    make_dominant(A, n);
    double ts, tp;
    int rs, rp;
    const char *title[4] = {
        "(1) upper triangular: rows i, j < i, schedule(dynamic, 32)",
        "(2) lower triangular: rows i, j > i, schedule(dynamic, 32)",
        "(3) diagonal: collapse(2) over all (i, j)",
        "(4) strictly diagonally dominant: rows i, private off-diagonal sum"};
    int (*serial_fn[4])(const double *, long) = {is_upper_serial, is_lower_serial,
                                                 is_diag_serial, is_sdd_serial};
    int (*par_fn[4])(const double *, long) = {is_upper_par, is_lower_par, is_diag_par, is_sdd_par};
    for (int c = 0; c < 4; c++) {
        BEST_TIME(ts, rs = serial_fn[c](A, n));
        printf("\n%s   (answer %d)\n", title[c], rs);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            BEST_TIME(tp, rp = par_fn[c](A, n));
            table_row(n, THREADS[k], ts, tp, rs == rp);
        }
    }
    free(A);
    return 0;
}

/* Notes:
 * - reduction(&&:ok): private ok starts at 1 (the identity of &&); a violation sets
 *   the private copy to 0; the final AND is 0 if any thread saw a violation.
 *   Writing ok = 0 into a SHARED ok would also give the right answer here (all
 *   writers write 0) but is formally a data race; the reduction is the clean way.
 * - No early exit: "break" is not allowed in an omp for loop (OpenMP 4.0 has
 *   "#pragma omp cancel for", but it needs OMP_CANCELLATION=true). So the parallel
 *   check of a failing matrix costs the full scan, while serial stops early.
 * - Triangular regions: row i of the strict lower triangle has i elements, so a
 *   static schedule overloads the last thread; dynamic (or static,1) balances it.
 *   collapse(2) is illegal there in OpenMP 4.5 (non-rectangular), but legal for
 *   the full (i, j) square used by the diagonal check.
 * - "Strictly": |a_ii| > off-diagonal sum; equality fails (last test case).
 * - Diagonal == upper AND lower; the table confirms the three answers agree.
 */

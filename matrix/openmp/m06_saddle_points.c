/* M06: Saddle points of a matrix
 * Problem: A saddle point of an N x N integer matrix A is an element A[i][j] that
 *   is the smallest element of its row AND the largest element of its column.
 *   Find, serially and with OpenMP, how many saddle points A has and the first
 *   one in row-major order (smallest i, then smallest j). Verify the parallel
 *   answer against serial on a matrix with planted (tied) saddle points and on a
 *   random matrix, check a 3 x 3 example by hand, and report the timings.
 * Solution: three passes over A, each a parallel loop (pass 3 needs 1 and 2):
 *   (1) row minima rmin[i]: parallel for over rows, iteration i writes rmin[i];
 *   (2) column maxima cmax[j]: (a) parallel for over COLUMNS, iteration j writes
 *       cmax[j] (stride-N reads); or (b) parallel for over ROWS with the array
 *       reduction reduction(max:cmax[:n]) (OpenMP 4.5): every thread keeps a
 *       private cmax (started at INT_MIN), merged with max at the end;
 *   (3) A[i][j] is a saddle point iff A[i][j] == rmin[i] && A[i][j] == cmax[j]:
 *       collapse(2) over (i, j) with reduction(+:count) and reduction(min:first),
 *       first = i*N + j, so the first saddle point does not depend on which
 *       thread finds it.
 *   All values are integers, so the parallel answer must equal the serial one.
 * Source: lab extension (classic matrix exercise; MATRIX_RULES.md section 7 rules 6, 8).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m06_saddle_points.c -o m06 -lm
 * Run:   ./m06 [N]   (default N = 3000, N >= 10)
 */
#include "../../problems/common.h"

/* ---------- serial ---------- */
static long saddle_serial(const int *A, long n, int *rmin, int *cmax, long *first) {
    for (long j = 0; j < n; j++) cmax[j] = INT_MIN;
    for (long i = 0; i < n; i++) {
        int mn = A[i*n];
        for (long j = 0; j < n; j++) {
            int a = A[i*n + j];
            if (a < mn) mn = a;
            if (a > cmax[j]) cmax[j] = a;
        }
        rmin[i] = mn;
    }
    long count = 0, f = LONG_MAX;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (A[i*n + j] == rmin[i] && A[i*n + j] == cmax[j]) {
                if (count == 0) f = i*n + j;
                count++;
            }
    *first = f;
    return count;
}

/* ---------- parallel passes ---------- */
static void row_min_parallel(const int *A, long n, int *rmin) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        int mn = A[i*n];
        for (long j = 1; j < n; j++) if (A[i*n + j] < mn) mn = A[i*n + j];
        rmin[i] = mn;                                  /* one writer per rmin[i] */
    }
}

/* (a) one column per iteration (stride-N reads, no shared writes). */
static void col_max_by_columns(const int *A, long n, int *cmax) {
    #pragma omp parallel for
    for (long j = 0; j < n; j++) {
        int mx = A[j];
        for (long i = 1; i < n; i++) if (A[i*n + j] > mx) mx = A[i*n + j];
        cmax[j] = mx;
    }
}

/* (b) rows in parallel (contiguous reads); all rows update every cmax[j]. */
static void col_max_by_rows(const int *A, long n, int *cmax) {
    for (long j = 0; j < n; j++) cmax[j] = INT_MIN;
    #pragma omp parallel for reduction(max:cmax[:n])
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) if (A[i*n + j] > cmax[j]) cmax[j] = A[i*n + j];
}

static long count_parallel(const int *A, long n, const int *rmin, const int *cmax, long *first) {
    long count = 0, f = LONG_MAX;
    #pragma omp parallel for collapse(2) reduction(+:count) reduction(min:f)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (A[i*n + j] == rmin[i] && A[i*n + j] == cmax[j]) {
                count++;
                if (i*n + j < f) f = i*n + j;
            }
    *first = f;
    return count;
}

static long saddle_parallel(const int *A, long n, int *rmin, int *cmax, long *first, int by_rows) {
    row_min_parallel(A, n, rmin);
    if (by_rows) col_max_by_rows(A, n, cmax); else col_max_by_columns(A, n, cmax);
    return count_parallel(A, n, rmin, cmax, first);
}

/* Value 0 at rows R x columns C; the other entries of those rows are >= 1 and the
 * other entries of those columns are <= -1, so there are exactly 3 * 2 = 6 saddle
 * points (all with value 0, tied in their rows and columns), the first at (R[0], C[0]). */
static void make_planted(int *A, long n, const long R[3], const long C[2]) {
    char *inC = xmalloc((size_t)n);
    memset(inC, 0, (size_t)n);
    inC[C[0]] = inC[C[1]] = 1;
    fill_int(A, n * n, -100, 100, 31);
    for (long i = 0; i < n; i++) {
        int inR = (i == R[0] || i == R[1] || i == R[2]);
        for (long j = 0; j < n; j++) {
            int *a = &A[i*n + j];
            if (inR && inC[j]) *a = 0;
            else if (inR) *a = abs(*a) + 1;            /* row of a saddle point: >= 1 */
            else if (inC[j]) *a = -abs(*a) - 1;        /* column of a saddle point: <= -1 */
        }
    }
    free(inC);
}

static void small_checks(void) {
    const int A[9] = {1, 2, 3,
                      4, 5, 6,
                      7, 8, 9};
    int rmin[4], cmax[4], C4[16];
    long f1, f2;
    long c1 = saddle_parallel(A, 3, rmin, cmax, &f1, 0);
    printf("Hand example: A = [1 2 3; 4 5 6; 7 8 9]: row minima 1 4 7, column maxima 7 8 9\n");
    printf("  saddle points: %ld, first = %d at (%ld, %ld) (expected one: 7 at (2, 0)): %s\n",
           c1, c1 ? A[f1] : 0, f1 / 3, f1 % 3, ok_str(c1 == 1 && f1 == 6));
    for (int k = 0; k < 16; k++) C4[k] = 5;
    long c2 = saddle_parallel(C4, 4, rmin, cmax, &f2, 1);
    printf("  constant 4 x 4 matrix: every element is a saddle point: %ld (expected 16): %s\n\n",
           c2, ok_str(c2 == 16 && f2 == 0));
}

static void run_case(const char *name, const int *A, long n, int *rmin, int *cmax) {
    long fs = 0, fp = 0, cs = 0, cp = 0;
    double ts, tp;
    BEST_TIME(ts, cs = saddle_serial(A, n, rmin, cmax, &fs));
    if (cs > 0)
        printf("%s: serial: %ld saddle point(s), first = %d at (%ld, %ld)\n",
               name, cs, A[fs], fs / n, fs % n);
    else
        printf("%s: serial: no saddle point\n", name);
    for (int v = 0; v < 2; v++) {
        printf("  (%c) row minima + %s + collapse(2) count\n", v ? 'b' : 'a',
               v ? "column maxima by rows, reduction(max:cmax[:n])"
                 : "column maxima by columns (stride N)");
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            BEST_TIME(tp, cp = saddle_parallel(A, n, rmin, cmax, &fp, v));
            table_row(n, THREADS[k], ts, tp, cp == cs && fp == fs);
        }
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 3000);
    if (n < 10) { fprintf(stderr, "N must be >= 10\n"); return 1; }
    int *A = xmalloc((size_t)n * n * sizeof(int));
    int *rmin = xmalloc((size_t)n * sizeof(int)), *cmax = xmalloc((size_t)n * sizeof(int));
    const long R[3] = {n / 5, n / 2, 4 * n / 5}, C[2] = {n / 3, 2 * n / 3};
    omp_warmup();

    small_checks();
    printf("M06: N = %ld\n", n);
    make_planted(A, n, R, C);
    long f = 0, c = saddle_parallel(A, n, rmin, cmax, &f, 1);
    printf("planted: 6 saddle points expected, first at (%ld, %ld); parallel found %ld, "
           "first at (%ld, %ld): %s\n", R[0], C[0], c, f / n, f % n,
           ok_str(c == 6 && f == R[0] * n + C[0]));
    run_case("planted", A, n, rmin, cmax);
    fill_int(A, n * n, -1000000, 1000000, 32);
    printf("\n");
    run_case("random", A, n, rmin, cmax);
    free(A); free(rmin); free(cmax);
    return 0;
}

/* Notes:
 * - Definition used: minimum of its row and maximum of its column. The other
 *   convention (maximum of its row, minimum of its column) is the same program
 *   with min and max swapped (or run this one on -A).
 * - All saddle points of a matrix have the same value: if (i, j) and (k, l) are
 *   saddle points, A[i][j] <= A[i][l] <= A[k][l] <= A[k][j] <= A[i][j]. So with
 *   distinct values there is at most one; ties (e.g. a constant matrix) give many.
 *   This is how the test matrix plants exactly 6.
 * - Why separate passes: whether A[i][j] is a saddle point depends on its whole
 *   row and its whole column, which other threads are scanning. Computing rmin
 *   and cmax first (implicit barrier after each parallel for) removes the
 *   dependence; the final test is then independent for every (i, j).
 * - Column maxima: parallel over columns has one writer per cmax[j] but reads with
 *   stride N; parallel over rows reads contiguously but every row updates every
 *   cmax[j], a race without reduction(max:cmax[:n]) (or per-thread arrays merged
 *   in a critical section).
 * - "first" with reduction(min:) on the linear index i*N + j gives the same answer
 *   as the serial scan whatever the schedule. A shared "found" flag written by
 *   whichever thread gets there first would not.
 * - collapse(2) is legal (perfectly nested, rectangular) and correct (only the
 *   reduction variables are shared). With just the outer loop parallel, the same
 *   reductions work too.
 * - Every pass is O(N^2) integer compares: memory-bound, modest speedups.
 */

/* P065: Rectangular matrix product C(m x n) = A(m x k) * B(k x n)
 * Problem: Multiply a rectangular m x k matrix A by a k x n matrix B, with m, k
 *   and n all different (e.g. 600 x 800 times 800 x 700), serially and with
 *   OpenMP: (1) the ijk loop order with collapse(2) on the (i, j) loops and
 *   (2) the ikj (row-wise) order, parallel over i. Check the dimension rule in
 *   code (columns of A must equal rows of B) and print an error otherwise.
 *   Verify both versions and report the timings.
 * Solution: C[i][j] = sum_p A[i][p]*B[p][j] (row i of A times column j of B), so
 *   every (i, j) is computed by one iteration that owns C[i][j]. (1) collapse(2)
 *   merges the rectangular, perfectly nested i and j loops (m*n iterations); the
 *   dot-product sum s is private (declared inside). The p loop must NOT be
 *   parallelised/collapsed: all p add into the same C[i][j]. (2) ikj: for each i
 *   and p, C[i][:] += A[i][p]*B[p][:] streams rows of B and C contiguously; row i
 *   of C belongs to one thread, so parallel for over i is race-free.
 *   Matrices are flattened row-major: A[i][p] = A[i*k + p], B[p][j] = B[p*n + j],
 *   C[i][j] = C[i*n + j].
 * Source: lab extension of Lab 7 Task 5 (matrix multiplication) to rectangular sizes.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p065_rect_matmul.c -o p065 -lm
 * Run:   ./p065 [m] [k] [n]   (defaults m = 400, k = 600, n = 500)
 */
#include "common.h"

/* Dimension rule: (ar x ac) * (br x bc) exists only if ac == br; result ar x bc. */
static int check_dims(long ar, long ac, long br, long bc) {
    if (ar <= 0 || ac <= 0 || br <= 0 || bc <= 0) {
        printf("  dimension error: A is %ld x %ld, B is %ld x %ld: sizes must be positive\n",
               ar, ac, br, bc);
        return 0;
    }
    if (ac != br) {
        printf("  dimension error: A is %ld x %ld, B is %ld x %ld: columns of A (%ld) "
               "must equal rows of B (%ld)\n", ar, ac, br, bc, ac, br);
        return 0;
    }
    return 1;
}

/* ---------- (1) ijk: C[i][j] = dot(row i of A, column j of B) ---------- */
static void matmul_ijk_serial(const double *A, const double *B, double *C,
                              long m, long k, long n) {
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) {
            double s = 0.0;
            for (long p = 0; p < k; p++) s += A[i*k + p] * B[p*n + j];
            C[i*n + j] = s;
        }
}
static void matmul_ijk_collapse(const double *A, const double *B, double *C,
                                long m, long k, long n) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) {
            double s = 0.0;                              /* private */
            for (long p = 0; p < k; p++) s += A[i*k + p] * B[p*n + j];
            C[i*n + j] = s;                              /* own element */
        }
}

/* ---------- (2) ikj: row i of C += A[i][p] * row p of B ---------- */
static void matmul_ikj_serial(const double *A, const double *B, double *C,
                              long m, long k, long n) {
    for (long i = 0; i < m; i++) {
        for (long j = 0; j < n; j++) C[i*n + j] = 0.0;
        for (long p = 0; p < k; p++) {
            double a = A[i*k + p];
            for (long j = 0; j < n; j++) C[i*n + j] += a * B[p*n + j];
        }
    }
}
static void matmul_ikj_par(const double *A, const double *B, double *C,
                           long m, long k, long n) {
    #pragma omp parallel for
    for (long i = 0; i < m; i++) {                       /* thread owns row i of C */
        for (long j = 0; j < n; j++) C[i*n + j] = 0.0;
        for (long p = 0; p < k; p++) {
            double a = A[i*k + p];
            for (long j = 0; j < n; j++) C[i*n + j] += a * B[p*n + j];
        }
    }
}

static void tiny_example(void) {
    double A[6] = {1, 2, 3,
                   4, 5, 6};                             /* 2 x 3 */
    double B[6] = {7, 8,
                   9, 10,
                   11, 12};                              /* 3 x 2 */
    double C[4];
    printf("Worked example: A (2x3) = [1 2 3; 4 5 6], B (3x2) = [7 8; 9 10; 11 12]\n");
    if (check_dims(2, 3, 3, 2)) {
        matmul_ikj_par(A, B, C, 2, 3, 2);
        printf("  C = A*B (2x2) = [%g %g; %g %g]   (expected [58 64; 139 154])\n",
               C[0], C[1], C[2], C[3]);
    }
    printf("  B*B: (3x2)*(3x2) ->\n");
    int ok = check_dims(3, 2, 3, 2);
    printf("  rejected as expected: %s\n", ok_str(!ok));
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 400);
    long k = arg_long(argc, argv, 2, 600);
    long n = arg_long(argc, argv, 3, 500);
    tiny_example();

    printf("\nP065: C (%ld x %ld) = A (%ld x %ld) * B (%ld x %ld)\n", m, n, m, k, k, n);
    if (!check_dims(m, k, k, n)) return 1;               /* assert-like guard */

    double *A = xmalloc((size_t)m * k * sizeof(double));
    double *B = xmalloc((size_t)k * n * sizeof(double));
    double *Cs = xmalloc((size_t)m * n * sizeof(double));
    double *Cp = xmalloc((size_t)m * n * sizeof(double));
    fill_double(A, m * k, -1.0, 1.0, 1);
    fill_double(B, k * n, -1.0, 1.0, 2);
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, matmul_ijk_serial(A, B, Cs, m, k, n));
    printf("(the tables' N column shows m)\n");
    printf("\n(1) ijk order, collapse(2) over (i, j): %ld iterations\n", m * n);
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        memset(Cp, 0, (size_t)m * n * sizeof(double));
        BEST_TIME(tp, matmul_ijk_collapse(A, B, Cp, m, k, n));
        table_row(m, THREADS[t], ts, tp, close_doubles(Cs, Cp, m * n, 1e-10));
    }

    double ts2;
    BEST_TIME(ts2, matmul_ikj_serial(A, B, Cp, m, k, n));
    printf("(2) ikj (row-wise) order, parallel over i   [serial ikj is %.1fx faster than serial ijk]\n",
           ts / ts2);
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        memset(Cp, 0, (size_t)m * n * sizeof(double));
        BEST_TIME(tp, matmul_ikj_par(A, B, Cp, m, k, n));
        table_row(m, THREADS[t], ts2, tp, close_doubles(Cs, Cp, m * n, 1e-10));
    }

    free(A); free(B); free(Cs); free(Cp);
    return 0;
}

/* Notes:
 * - Dimension rule: (m x k)(k x n) = (m x n). The loop bounds are m (rows of A),
 *   n (columns of B) and k (the shared inner size); mixing them up is the usual
 *   bug, which square test matrices hide. Index A with k, B and C with n.
 * - Parallel loops: i and j (each writes its own C[i][j]). The p loop is a sum
 *   into one element: parallelising it needs a reduction and is slower.
 * - collapse(2) on (i, j) is legal (perfectly nested, rectangular) and gives m*n
 *   iterations to share, useful when m is small; collapse(3) would race on C[i][j].
 * - ijk reads B down a column (stride n): one new cache line per multiply. ikj
 *   reads A[i][p] once and streams rows of B and C contiguously, which also
 *   vectorises; typically several times faster.
 * - ikj accumulates in the same p order as ijk, so results agree to rounding.
 */

/* P068: Recursive (divide-and-conquer) matrix multiplication with tasks
 * Problem: Multiply two N x N matrices, C = A*B, by recursive divide and conquer:
 *   split A, B and C into 2 x 2 blocks (quadrants) and use
 *     C11 = A11 B11 + A12 B21     C12 = A11 B12 + A12 B22
 *     C21 = A21 B11 + A22 B21     C22 = A21 B12 + A22 B22,
 *   computing the four quadrants of C as independent OpenMP tasks, and switch to
 *   a serial ikj kernel below a cutoff block size. Handle any N. Verify against
 *   the serial product and report the timings.
 * Solution: the four quadrants of C are disjoint, so the four tasks never write
 *   the same element. Inside one task the two products for the same quadrant
 *   (e.g. A11 B11 then A12 B21, both adding into C11) run one after the other,
 *   so there is no race on C11. Every call ends with "#pragma omp taskwait", so
 *   when a recursive call returns, all of its sub-tasks have finished; the next
 *   call into the same quadrant starts only then. The top call is made by one
 *   thread (parallel + single); the other threads execute the tasks. General N:
 *   halves are m/2 and m - m/2 (may differ by one), so no padding is needed; the
 *   blocks are views into the full matrices with row stride ld. Matrices are
 *   flattened row-major: A[i][j] = A[i*N + j]; a block starting at P with row
 *   stride ld has element (i, j) at P[i*ld + j].
 * Source: lab extension (task parallelism, recursive algorithms; cf. Strassen).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p068_block_matmul_tasks.c -o p068 -lm
 * Run:   ./p068 [N] [cutoff]   (defaults N = 700, cutoff = 128)
 */
#include "common.h"

static long cutoff = 128;     /* recurse while all block sides are > cutoff */

/* C (m x n) += A (m x k) * B (k x n); row strides lda, ldb, ldc; ikj order. */
static void kernel(const double *A, const double *B, double *C, long m, long n, long k,
                   long lda, long ldb, long ldc) {
    for (long i = 0; i < m; i++)
        for (long p = 0; p < k; p++) {
            double a = A[i*lda + p];
            for (long j = 0; j < n; j++) C[i*ldc + j] += a * B[p*ldb + j];
        }
}

/* C += A*B by recursion on quadrants; each C quadrant is one task. */
static void mm_rec(const double *A, const double *B, double *C, long m, long n, long k,
                   long lda, long ldb, long ldc) {
    if (m <= cutoff || n <= cutoff || k <= cutoff) {
        kernel(A, B, C, m, n, k, lda, ldb, ldc);
        return;
    }
    long m1 = m / 2, n1 = n / 2, k1 = k / 2;             /* first halves */
    long m2 = m - m1, n2 = n - n1, k2 = k - k1;          /* second halves (>= first) */
    const double *A11 = A,           *A12 = A + k1;
    const double *A21 = A + m1*lda,  *A22 = A21 + k1;
    const double *B11 = B,           *B12 = B + n1;
    const double *B21 = B + k1*ldb,  *B22 = B21 + n1;
    double *C11 = C,                 *C12 = C + n1;
    double *C21 = C + m1*ldc,        *C22 = C21 + n1;
    /* The pointers and sizes are locals of this call: firstprivate in the tasks. */

    #pragma omp task                                     /* C11 += A11 B11 + A12 B21 */
    { mm_rec(A11, B11, C11, m1, n1, k1, lda, ldb, ldc);
      mm_rec(A12, B21, C11, m1, n1, k2, lda, ldb, ldc); }
    #pragma omp task                                     /* C12 += A11 B12 + A12 B22 */
    { mm_rec(A11, B12, C12, m1, n2, k1, lda, ldb, ldc);
      mm_rec(A12, B22, C12, m1, n2, k2, lda, ldb, ldc); }
    #pragma omp task                                     /* C21 += A21 B11 + A22 B21 */
    { mm_rec(A21, B11, C21, m2, n1, k1, lda, ldb, ldc);
      mm_rec(A22, B21, C21, m2, n1, k2, lda, ldb, ldc); }
    #pragma omp task                                     /* C22 += A21 B12 + A22 B22 */
    { mm_rec(A21, B12, C22, m2, n2, k1, lda, ldb, ldc);
      mm_rec(A22, B22, C22, m2, n2, k2, lda, ldb, ldc); }
    #pragma omp taskwait                                 /* all four quadrants done */
}

static void matmul_tasks(const double *A, const double *B, double *C, long n) {
    memset(C, 0, (size_t)n * n * sizeof(double));       /* the recursion adds into C */
    #pragma omp parallel
    #pragma omp single                                   /* one thread starts the recursion */
    mm_rec(A, B, C, n, n, n, n, n, n);
}

static void matmul_serial(const double *A, const double *B, double *C, long n) {
    memset(C, 0, (size_t)n * n * sizeof(double));
    kernel(A, B, C, n, n, n, n, n, n);
}

/* For comparison: plain loop parallelism over the rows of C. */
static void matmul_rows(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) C[i*n + j] = 0.0;
        kernel(&A[i*n], B, &C[i*n], 1, n, n, n, n, n);   /* row i only */
    }
}

static void tiny_example(void) {
    double A[9] = {1, 2, 3, 4, 5, 6, 7, 8, 10};
    double B[9] = {1, 0, 2, 0, 1, 0, 3, 0, 1};
    double C[9];
    long saved = cutoff;
    cutoff = 1;                                          /* force recursion on 3x3 */
    omp_set_num_threads(4);
    matmul_tasks(A, B, C, 3);
    cutoff = saved;
    printf("Worked example (cutoff 1, so the 3x3 is split into 1+2 halves):\n");
    printf("  A = [1 2 3; 4 5 6; 7 8 10], B = [1 0 2; 0 1 0; 3 0 1]\n");
    printf("  C = A*B = [%g %g %g; %g %g %g; %g %g %g]\n", C[0], C[1], C[2], C[3], C[4],
           C[5], C[6], C[7], C[8]);
    printf("  expected  [10 2 5; 22 5 14; 37 8 24]\n");
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 700);
    cutoff = arg_long(argc, argv, 2, 128);
    if (cutoff < 1) cutoff = 1;
    long nn = n * n;
    size_t bytes = (size_t)nn * sizeof(double);
    double *A = xmalloc(bytes), *B = xmalloc(bytes), *Cs = xmalloc(bytes), *Cp = xmalloc(bytes);
    fill_double(A, nn, -1.0, 1.0, 68);
    fill_double(B, nn, -1.0, 1.0, 69);
    omp_warmup();

    tiny_example();
    printf("\nP068: C = A*B, N = %ld, cutoff = %ld (serial ikj kernel below it)\n", n, cutoff);

    double ts, tp;
    BEST_TIME(ts, matmul_serial(A, B, Cs, n));
    printf("\n(a) recursive quadrants, 4 tasks per level + taskwait\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Cp, 0, bytes);
        BEST_TIME(tp, matmul_tasks(A, B, Cp, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(Cs, Cp, nn, 1e-10));
    }
    printf("(b) for comparison: parallel for over rows (ikj)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Cp, 0, bytes);
        BEST_TIME(tp, matmul_rows(A, B, Cp, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(Cs, Cp, nn, 1e-10));
    }

    free(A); free(B); free(Cs); free(Cp);
    return 0;
}

/* Notes:
 * - Why no race on C: the 4 tasks write 4 disjoint quadrants; the 2 products of
 *   one quadrant run sequentially inside the same task. Making all 8 products
 *   separate tasks WOULD race (two tasks adding into the same quadrant) unless
 *   each pair is ordered (taskwait or depend clauses) or uses temporaries.
 * - taskwait waits for the child tasks of the current task only; because every
 *   mm_rec ends with one, a returned call means its whole subtree is finished.
 * - parallel + single: the team is created once and one thread spawns the tree;
 *   idle threads pick up tasks. Without single, every thread would run the
 *   whole recursion (N times the work, and races).
 * - Cutoff: tasks cost microseconds each, so stop splitting at blocks of ~100;
 *   the leaves are cache-sized blocks, which is why the recursive version can
 *   beat the plain loop (blocking for cache).
 * - Results differ from the serial ikj only by rounding (the k sum is split into
 *   halves), so compare with a tolerance.
 */

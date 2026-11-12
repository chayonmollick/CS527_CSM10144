/* M01: Kronecker product K = A (x) B
 * Problem: A is an m x n and B a p x q integer matrix. Their Kronecker product
 *   K = A (x) B is the (m p) x (n q) block matrix whose block (i, j) is A[i][j] * B:
 *       K[i*p + r][j*q + s] = A[i][j] * B[r][s].
 *   Compute K serially and with OpenMP, verify it, and report the time, speedup
 *   and efficiency for 1, 2, 4 and 8 threads. Check a 2 x 2 (x) 2 x 2 example by
 *   hand and the rule trace(A (x) B) = trace(A) * trace(B).
 * Solution: every element of K is written exactly once and depends on one element
 *   of A and one of B, so there is no race and no reduction.
 *   (a) parallel for over the m*p output rows I: row I lies in block row
 *       i = I / p and uses row r = I % p of B; the thread writes the whole row
 *       (n blocks of q elements, contiguous).
 *   (b) collapse(2) over the m x n blocks (i, j): iteration (i, j) writes its own
 *       p x q block. Legal (perfectly nested, rectangular loops) and correct (the
 *       blocks are disjoint). Useful when m is small but n is not.
 *   K is long (products of ints) and is compared exactly.
 * Source: lab extension (matrix products; MATRIX_RULES.md section 2).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m01_kronecker.c -o m01 -lm
 * Run:   ./m01 [m] [n] [p] [q]   (defaults 60 50 40 50: K is 2400 x 2500)
 */
#include "../../problems/common.h"

/* K has m*p rows and n*q columns: K[I][J] is K[I*(n*q) + J]. */
static void kron_serial(const int *A, long m, long n, const int *B, long p, long q, long *K) {
    long cols = n * q;
    for (long i = 0; i < m; i++)
        for (long r = 0; r < p; r++) {
            long *row = K + (i*p + r) * cols;         /* output row I = i*p + r */
            for (long j = 0; j < n; j++) {
                long a = A[i*n + j];
                for (long s = 0; s < q; s++)
                    row[j*q + s] = a * B[r*q + s];
            }
        }
}

/* (a) one output row per iteration: i = I / p (block row), r = I % p (row of B). */
static void kron_rows(const int *A, long m, long n, const int *B, long p, long q, long *K) {
    long cols = n * q;
    #pragma omp parallel for schedule(static)
    for (long I = 0; I < m * p; I++) {
        long i = I / p, r = I % p;
        long *row = K + I * cols;
        for (long j = 0; j < n; j++) {
            long a = A[i*n + j];
            for (long s = 0; s < q; s++)
                row[j*q + s] = a * B[r*q + s];
        }
    }
}

/* (b) one p x q block per iteration (i, j); different blocks never overlap. */
static void kron_blocks(const int *A, long m, long n, const int *B, long p, long q, long *K) {
    long cols = n * q;
    #pragma omp parallel for collapse(2) schedule(static)
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) {
            long a = A[i*n + j];
            for (long r = 0; r < p; r++)
                for (long s = 0; s < q; s++)
                    K[(i*p + r)*cols + j*q + s] = a * B[r*q + s];
        }
}

/* Hand example (the textbook one) and the trace rule on a square case. */
static int small_checks(void) {
    const int A[4] = {1, 2,
                      3, 4};
    const int B[4] = {0, 5,
                      6, 7};
    const long expect[16] = { 0,  5,  0, 10,
                              6,  7, 12, 14,
                              0, 15,  0, 20,
                             18, 21, 24, 28};
    long Ka[16], Kb[16];
    kron_rows(A, 2, 2, B, 2, 2, Ka);
    kron_blocks(A, 2, 2, B, 2, 2, Kb);
    int ok1 = same_longs(Ka, expect, 16) && same_longs(Kb, expect, 16);
    printf("Hand example: A = [1 2; 3 4], B = [0 5; 6 7]\n");
    for (int I = 0; I < 4; I++)
        printf("  %4ld %4ld %4ld %4ld\n", Ka[I*4], Ka[I*4 + 1], Ka[I*4 + 2], Ka[I*4 + 3]);
    printf("  expected [0 5 0 10; 6 7 12 14; 0 15 0 20; 18 21 24 28]: %s\n", ok_str(ok1));

    /* trace(A (x) B) = trace(A) * trace(B) for square A (7 x 7) and B (5 x 5) */
    int A2[49], B2[25];
    long K2[35 * 35], trA = 0, trB = 0, trK = 0;
    fill_int(A2, 49, -9, 9, 11);
    fill_int(B2, 25, -9, 9, 12);
    kron_blocks(A2, 7, 7, B2, 5, 5, K2);
    for (int i = 0; i < 7; i++) trA += A2[i*7 + i];
    for (int i = 0; i < 5; i++) trB += B2[i*5 + i];
    for (int I = 0; I < 35; I++) trK += K2[I*35 + I];
    int ok2 = (trK == trA * trB);
    printf("Trace rule (7x7 (x) 5x5): trace(K) = %ld, trace(A)*trace(B) = %ld*%ld = %ld: %s\n\n",
           trK, trA, trB, trA * trB, ok_str(ok2));
    return ok1 && ok2;
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 60), n = arg_long(argc, argv, 2, 50);
    long p = arg_long(argc, argv, 3, 40), q = arg_long(argc, argv, 4, 50);
    if (m < 1 || n < 1 || p < 1 || q < 1) { fprintf(stderr, "all sizes must be >= 1\n"); return 1; }
    long rows = m * p, cols = n * q, total = rows * cols;
    int *A = xmalloc((size_t)(m * n) * sizeof(int));
    int *B = xmalloc((size_t)(p * q) * sizeof(int));
    long *Ks = xmalloc((size_t)total * sizeof(long));
    long *Kp = xmalloc((size_t)total * sizeof(long));
    fill_int(A, m * n, -50, 50, 1);
    fill_int(B, p * q, -50, 50, 2);
    omp_warmup();

    small_checks();
    printf("M01: A is %ld x %ld, B is %ld x %ld, K = A (x) B is %ld x %ld (N = %ld elements)\n",
           m, n, p, q, rows, cols, total);
    double ts, tp;
    BEST_TIME(ts, kron_serial(A, m, n, B, p, q, Ks));

    printf("(a) parallel for over the %ld output rows\n", rows);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Kp, 0, (size_t)total * sizeof(long));
        BEST_TIME(tp, kron_rows(A, m, n, B, p, q, Kp));
        table_row(total, THREADS[k], ts, tp, same_longs(Ks, Kp, total));
    }
    printf("(b) collapse(2) over the %ld x %ld blocks\n", m, n);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Kp, 0, (size_t)total * sizeof(long));
        BEST_TIME(tp, kron_blocks(A, m, n, B, p, q, Kp));
        table_row(total, THREADS[k], ts, tp, same_longs(Ks, Kp, total));
    }
    free(A); free(B); free(Ks); free(Kp);
    return 0;
}

/* Notes:
 * - Index map: element (I, J) of K comes from block (i, j) = (I / p, J / q) and
 *   position (r, s) = (I % p, J % q) inside it: K[I][J] = A[I/p][J/q] * B[I%p][J%q].
 * - Sizes: (m x n) (x) (p x q) is (mp) x (nq); unlike A*B there is no condition on
 *   the sizes. (A (x) B)^T = A^T (x) B^T, (A (x) B)(C (x) D) = (AC) (x) (BD),
 *   det(A (x) B) = det(A)^p * det(B)^n for square A (n x n) and B (p x p).
 * - No race in either version: each element of K is written by exactly one
 *   iteration and A, B are only read. No reduction is needed.
 * - (a) writes whole rows (contiguous) and has m*p iterations; (b) writes q
 *   contiguous elements per block row, so it jumps n*q elements between them.
 *   collapse(2) in (b) is legal because nothing sits between the i and j loops
 *   and the bounds are rectangular; without it only m iterations would be
 *   shared out (bad when m < number of threads).
 * - Work: one multiply per element of K, i.e. m*n*p*q; the output is far larger
 *   than the inputs, so the program is limited by memory bandwidth (writing K)
 *   and the speedup flattens at a few threads.
 * - Integer results are exact in any order, so the check is exact (same_longs).
 */

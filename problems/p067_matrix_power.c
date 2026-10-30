/* P067: Matrix power A^p by repeated squaring
 * Problem: Compute A^p for an N x N matrix A and an integer p >= 0 using repeated
 *   squaring (O(log p) matrix multiplications instead of p-1), serially and with
 *   OpenMP. To keep the numbers bounded, work with integers modulo the prime
 *   P = 1000003. Verify the parallel result against serial, against naive
 *   repeated multiplication (small p) and on a 2x2 Fibonacci example, and report
 *   the timings.
 * Solution: binary exponentiation: R = I, B = A; for each bit of p (lowest first):
 *   if the bit is 1, R = R*B; then B = B*B. This exponent loop is SEQUENTIAL:
 *   each step needs the previous R and B. The parallelism is inside each matrix
 *   multiply: C = X*Y (mod P) with parallel for over the rows i of C (ikj order:
 *   row i of C += X[i][k] * row k of Y). Row i of C is written by one thread and
 *   X, Y are only read, so there is no race. The product goes to a separate
 *   buffer (never C = C*Y in place). Values are < P < 2^20, so a product is
 *   < 2^40 and a row sum of N products fits in 64 bits for N < 9e6; reduce mod P
 *   once per element. long is 64-bit on Linux/macOS (LP64). Matrices are
 *   flattened row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (fast exponentiation; Fibonacci / graph path counting).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p067_matrix_power.c -o p067 -lm
 * Run:   ./p067 [N] [p]   (defaults N = 250, p = 1000)
 */
#include "common.h"

#define MODP 1000003L                                    /* a prime just above 10^6 */

typedef void (*mul_fn)(const long *, const long *, long *, long);

/* C = X*Y mod P (C must not alias X or Y). */
static void matmul_mod_serial(const long *X, const long *Y, long *C, long n) {
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) C[i*n + j] = 0;
        for (long k = 0; k < n; k++) {
            long a = X[i*n + k];
            for (long j = 0; j < n; j++) C[i*n + j] += a * Y[k*n + j];
        }
        for (long j = 0; j < n; j++) C[i*n + j] %= MODP;
    }
}

static void matmul_mod_par(const long *X, const long *Y, long *C, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {                       /* row i of C: one thread */
        for (long j = 0; j < n; j++) C[i*n + j] = 0;
        for (long k = 0; k < n; k++) {
            long a = X[i*n + k];
            for (long j = 0; j < n; j++) C[i*n + j] += a * Y[k*n + j];
        }
        for (long j = 0; j < n; j++) C[i*n + j] %= MODP;
    }
}

static void set_identity(long *R, long n) {
    for (long t = 0; t < n * n; t++) R[t] = 0;
    for (long i = 0; i < n; i++) R[i*n + i] = 1 % MODP;
}

/* R = A^p mod P by repeated squaring; returns the number of multiplications. */
static int matpow(const long *A, long *R, long n, long p, mul_fn mul) {
    size_t bytes = (size_t)n * n * sizeof(long);
    long *B = xmalloc(bytes), *T = xmalloc(bytes);
    int mults = 0;
    memcpy(B, A, bytes);                                 /* B = A^(2^bit) */
    set_identity(R, n);
    while (p > 0) {                                      /* sequential: bit by bit */
        if (p & 1) { mul(R, B, T, n); memcpy(R, T, bytes); mults++; }   /* R = R*B */
        p >>= 1;
        if (p > 0) { mul(B, B, T, n); memcpy(B, T, bytes); mults++; }  /* B = B*B */
    }
    free(B); free(T);
    return mults;
}

/* Naive: R = A*A*...*A (p - 1 multiplications), for checking. */
static void matpow_naive(const long *A, long *R, long n, long p, mul_fn mul) {
    size_t bytes = (size_t)n * n * sizeof(long);
    long *T = xmalloc(bytes);
    if (p == 0) { set_identity(R, n); free(T); return; }
    memcpy(R, A, bytes);
    for (long e = 1; e < p; e++) { mul(R, A, T, n); memcpy(R, T, bytes); }
    free(T);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 250);
    long p = arg_long(argc, argv, 2, 1000);
    if (p < 0) { printf("p must be >= 0\n"); return 1; }
    size_t bytes = (size_t)n * n * sizeof(long);
    long *A = xmalloc(bytes), *Rs = xmalloc(bytes), *Rp = xmalloc(bytes), *Rn = xmalloc(bytes);
    int *tmp = xmalloc((size_t)n * n * sizeof(int));
    fill_int(tmp, n * n, 0, (int)(MODP - 1), 67);
    for (long t = 0; t < n * n; t++) A[t] = tmp[t];
    free(tmp);
    omp_warmup();

    /* Worked example: Fibonacci matrix [[1,1],[1,0]]^p = [[F(p+1), F(p)], [F(p), F(p-1)]] */
    long F[4] = {1, 1, 1, 0}, F10[4], F0[4];
    int m10 = matpow(F, F10, 2, 10, matmul_mod_par);
    matpow(F, F0, 2, 0, matmul_mod_par);
    printf("Worked example: [[1,1],[1,0]]^10 = [[%ld,%ld],[%ld,%ld]] using %d multiplications"
           " (expected [[89,55],[55,34]], 5)\n", F10[0], F10[1], F10[2], F10[3], m10);
    printf("                [[1,1],[1,0]]^0  = [[%ld,%ld],[%ld,%ld]] (identity)\n",
           F0[0], F0[1], F0[2], F0[3]);

    printf("\nP067: A^p mod %ld, N = %ld, p = %ld\n", MODP, n, p);

    /* Check repeated squaring against naive multiplication for a small exponent. */
    long ps = p < 13 ? p : 13;
    omp_set_num_threads(4);
    matpow(A, Rp, n, ps, matmul_mod_par);
    matpow_naive(A, Rn, n, ps, matmul_mod_serial);
    printf("check: squaring vs naive (%ld multiplications) for p = %ld: %s\n",
           ps > 0 ? ps - 1 : 0, ps, ok_str(same_longs(Rp, Rn, n * n)));

    double ts, tp;
    int mults = 0;
    BEST_TIME(ts, mults = matpow(A, Rs, n, p, matmul_mod_serial));
    printf("repeated squaring uses %d multiplications (naive would use %ld)\n",
           mults, p > 0 ? p - 1 : 0);
    if (p >= 1) {                                        /* A^p = A^(p-1) * A */
        matpow(A, Rn, n, p - 1, matmul_mod_par);
        matmul_mod_par(Rn, A, Rp, n);
        printf("check: A^p == A^(p-1) * A: %s\n", ok_str(same_longs(Rs, Rp, n * n)));
    }

    printf("\nA^p, exponent loop serial, each multiply parallel over rows\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Rp, 0, bytes);
        BEST_TIME(tp, matpow(A, Rp, n, p, matmul_mod_par));
        table_row(n, THREADS[k], ts, tp, same_longs(Rs, Rp, n * n));
    }

    free(A); free(Rs); free(Rp); free(Rn);
    return 0;
}

/* Notes:
 * - The exponent loop cannot be parallelised: R and B at step t depend on step
 *   t-1 (a loop-carried dependence). Only the O(N^3) work inside each multiply is.
 * - Each multiply writes into a separate buffer: computing C = C*Y in place would
 *   overwrite entries of C that other rows (threads) still have to read.
 * - p = 13 = 1101b: R picks up B for bits 0, 2, 3; B is squared 3 times -> 6
 *   multiplications (in general floor(log2 p) squarings + one per 1-bit, at most
 *   2*floor(log2 p) + 1), against 12 for the naive loop.
 * - Integer arithmetic mod a prime keeps values bounded and makes the check exact.
 *   With doubles, use a row-stochastic matrix (rows sum to 1, so A^p stays
 *   bounded) and compare with a tolerance.
 * - Parallel region per multiply: about 2*log2(p) fork/joins, negligible next to
 *   N^3 work; the integer kernel is not vectorised (no 64-bit vector multiply on
 *   many CPUs), so it is compute-bound and scales well.
 */

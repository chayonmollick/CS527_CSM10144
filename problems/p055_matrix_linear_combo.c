/* P055: Linear combination and Hadamard product of matrices
 * Problem: Given two N x N matrices A and B of doubles and two scalars alpha and
 *   beta, compute C = alpha*A + beta*B and the element-wise (Hadamard) product
 *   H = A o B, i.e. H[i][j] = A[i][j]*B[i][j], serially and with OpenMP.
 *   Parallelise (1) only the outer (row) loop and (2) both loops with collapse(2).
 *   Verify every version and report time, speedup and efficiency for 1, 2, 4
 *   and 8 threads.
 * Solution: iteration (i, j) reads only A[i][j], B[i][j] and writes only its own
 *   C[i][j], so all N*N iterations are independent: no race, no reduction.
 *   (1) parallel for on the i loop: each thread gets a block of whole rows; j is
 *   private because it is declared inside the loop. (2) collapse(2) merges the i
 *   and j loops into one N*N iteration space that is split among the threads;
 *   legal because the loops are perfectly nested and rectangular. Matrices are
 *   flattened row-major: A[i][j] = A[i*N + j] (i, j are long, no int overflow).
 * Source: lab extension of Lab 7 matrix addition (element-wise operations).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p055_matrix_linear_combo.c -o p055 -lm
 * Run:   ./p055 [N] [alpha] [beta]   (defaults N = 2500, alpha = 2, beta = -3)
 */
#include "common.h"

/* ---------- C = alpha*A + beta*B ---------- */
static void lincomb_serial(const double *A, const double *B, double *C, long n,
                           double alpha, double beta) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            C[i*n + j] = alpha * A[i*n + j] + beta * B[i*n + j];
}

/* (1) outer loop parallel: each thread gets whole rows */
static void lincomb_outer(const double *A, const double *B, double *C, long n,
                          double alpha, double beta) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            C[i*n + j] = alpha * A[i*n + j] + beta * B[i*n + j];
}

/* (2) collapse(2): the N*N (i, j) pairs are shared out as one big loop */
static void lincomb_collapse(const double *A, const double *B, double *C, long n,
                             double alpha, double beta) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            C[i*n + j] = alpha * A[i*n + j] + beta * B[i*n + j];
}

/* ---------- H = A o B (Hadamard, element-wise product) ---------- */
static void hadamard_serial(const double *A, const double *B, double *H, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            H[i*n + j] = A[i*n + j] * B[i*n + j];
}

static void hadamard_outer(const double *A, const double *B, double *H, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            H[i*n + j] = A[i*n + j] * B[i*n + j];
}

static void hadamard_collapse(const double *A, const double *B, double *H, long n) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            H[i*n + j] = A[i*n + j] * B[i*n + j];
}

static void print_mat(const char *name, const double *M, long n) {
    printf("  %s =\n", name);
    for (long i = 0; i < n; i++) {
        printf("   ");
        for (long j = 0; j < n; j++) printf(" %6g", M[i*n + j]);
        printf("\n");
    }
}

/* Tiny worked example: 3x3, alpha = 2, beta = -1. */
static void tiny_example(void) {
    double A[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    double B[9] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
    double C[9], H[9];
    lincomb_collapse(A, B, C, 3, 2.0, -1.0);
    hadamard_collapse(A, B, H, 3);
    printf("Worked example (3x3, alpha = 2, beta = -1):\n");
    print_mat("A", A, 3);
    print_mat("B", B, 3);
    print_mat("C = 2A - B", C, 3);
    print_mat("H = A o B", H, 3);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 2500);
    double alpha = arg_double(argc, argv, 2, 2.0);
    double beta  = arg_double(argc, argv, 3, -3.0);
    long nn = n * n;
    size_t bytes = (size_t)nn * sizeof(double);
    double *A = xmalloc(bytes), *B = xmalloc(bytes);
    double *Cs = xmalloc(bytes), *Cp = xmalloc(bytes);
    fill_double(A, nn, -1.0, 1.0, 11);
    fill_double(B, nn, -1.0, 1.0, 22);
    omp_warmup();

    tiny_example();
    printf("\nP055: N = %ld (N x N matrices), alpha = %g, beta = %g\n", n, alpha, beta);

    double ts, tp;
    /* Part 1: linear combination. Cp is cleared before each version so a version
       that forgot some elements cannot pass on the previous version's values. */
    BEST_TIME(ts, lincomb_serial(A, B, Cs, n, alpha, beta));
    printf("\nPart 1 (a): C = alpha*A + beta*B, outer loop parallel (parallel for on i)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Cp, 0, bytes);
        BEST_TIME(tp, lincomb_outer(A, B, Cp, n, alpha, beta));
        table_row(n, THREADS[k], ts, tp, close_doubles(Cs, Cp, nn, 1e-12));
    }
    printf("Part 1 (b): C = alpha*A + beta*B, collapse(2) over (i, j)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Cp, 0, bytes);
        BEST_TIME(tp, lincomb_collapse(A, B, Cp, n, alpha, beta));
        table_row(n, THREADS[k], ts, tp, close_doubles(Cs, Cp, nn, 1e-12));
    }

    /* Part 2: Hadamard product (Cs/Cp reused as H). */
    BEST_TIME(ts, hadamard_serial(A, B, Cs, n));
    printf("\nPart 2 (a): H = A o B, outer loop parallel\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Cp, 0, bytes);
        BEST_TIME(tp, hadamard_outer(A, B, Cp, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(Cs, Cp, nn, 1e-12));
    }
    printf("Part 2 (b): H = A o B, collapse(2) over (i, j)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Cp, 0, bytes);
        BEST_TIME(tp, hadamard_collapse(A, B, Cp, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(Cs, Cp, nn, 1e-12));
    }

    free(A); free(B); free(Cs); free(Cp);
    return 0;
}

/* Notes:
 * - Both loops are parallelisable: every (i, j) writes a different C[i][j]. With
 *   only the outer loop parallel, the inner j loop runs serially inside each
 *   thread (and is vectorised); this is the usual choice when N >> threads.
 * - collapse(2) is legal only for perfectly nested, rectangular loops (nothing
 *   between "for i" and "for j", j bounds not depending on i in OpenMP 4.5). It
 *   helps when the outer loop is short (e.g. 3 rows, 8 threads). Here N is large,
 *   so it gains nothing and can even be slower: the compiler recovers i and j from
 *   the merged index, which can stop the inner loop from being vectorised.
 * - Keep j as the inner loop: row-major storage makes A[i*n + j] contiguous in j.
 *   Swapping the loops (j outer) strides by N doubles and is several times slower.
 * - Element-wise operations are memory-bound (1-2 flops per 24 bytes moved), so the
 *   speedup flattens at memory bandwidth, not at the core count.
 * - Results are bitwise identical to serial in practice (same operations per
 *   element); a tiny tolerance only guards against FMA contraction differences.
 */

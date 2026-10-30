/* CS527 Computer Systems — OpenMP practice
 * Q14: Transpose a square N x N matrix IN PLACE in parallel (no second matrix),
 *      and compare loop schedules; also compare a naive and a blocked (tiled)
 *      out-of-place transpose B = A^T.
 *  - in place: swap A[i][j] <-> A[j][i] for j > i, parallel over rows i. Row i
 *    does n-i-1 swaps (triangular workload), so compare
 *    schedule(static) vs schedule(dynamic,16) vs schedule(guided)
 *  - out of place, naive  : parallel for over i, B[j][i] = A[i][j]
 *  - out of place, blocked: 32x32 tiles, collapse(2) over the tile grid
 * Matrices are flattened row-major: A[i][j] = A[(long)i*n + j].
 * Usage: ./q14 [N]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS 32                                /* tile size for the blocked transpose */

/* ---------- in place ---------- */
static void transpose_inplace_serial(double *A, long n) {
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++) {
            double t = A[i*n + j]; A[i*n + j] = A[j*n + i]; A[j*n + i] = t;
        }
}
/* Iteration i swaps only the pairs (i,j)/(j,i) with j > i. Every unordered pair
 * {(i,j),(j,i)} belongs to exactly one i (the smaller index) -> no race.
 * static: thread 0 gets the first rows, which are the LONGEST -> imbalance. */
static void transpose_inplace_static(double *A, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++) {
            double t = A[i*n + j]; A[i*n + j] = A[j*n + i]; A[j*n + i] = t;
        }
}
/* dynamic,16: free threads grab the next 16 rows -> balances the triangle */
static void transpose_inplace_dynamic(double *A, long n) {
    #pragma omp parallel for schedule(dynamic, 16)
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++) {
            double t = A[i*n + j]; A[i*n + j] = A[j*n + i]; A[j*n + i] = t;
        }
}
/* guided: big chunks first, shrinking towards the end (fewer scheduling calls) */
static void transpose_inplace_guided(double *A, long n) {
    #pragma omp parallel for schedule(guided)
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++) {
            double t = A[i*n + j]; A[i*n + j] = A[j*n + i]; A[j*n + i] = t;
        }
}

/* ---------- out of place ---------- */
static void transpose_naive_serial(const double *A, double *B, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) B[j*n + i] = A[i*n + j];
}
/* reads A along a row, writes B down a column (stride n) */
static void transpose_naive_parallel(const double *A, double *B, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) B[j*n + i] = A[i*n + j];
}
/* One TSxTS tile of A and of B fit in cache together, so the strided writes hit
 * cache lines that are reused before eviction. Each tile is one iteration. */
static void transpose_blocked_parallel(const double *A, double *B, long n) {
    #pragma omp parallel for collapse(2)
    for (long ii = 0; ii < n; ii += TS)
        for (long jj = 0; jj < n; jj += TS) {
            long iend = ii + TS < n ? ii + TS : n;          /* last tile may be partial */
            long jend = jj + TS < n ? jj + TS : n;
            for (long i = ii; i < iend; i++)
                for (long j = jj; j < jend; j++) B[j*n + i] = A[i*n + j];
        }
}

/* M must be the transpose of A (exact copy, so == is fine) */
static int is_transpose(const double *A, const double *M, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (M[j*n + i] != A[i*n + j]) return 0;
    return 1;
}
static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

static int run(long n) {
    long n2 = n * n;
    size_t bytes = (size_t)n2 * sizeof(double);
    double *A = malloc(bytes);               /* original, never modified */
    double *M = malloc(bytes);               /* in-place work copy       */
    double *B = malloc(bytes);               /* out-of-place result      */
    if (!A || !M || !B) {
        fprintf(stderr, "alloc failed (N=%ld)\n", n);
        free(A); free(M); free(B); return 1;
    }
    for (long i = 0; i < n2; i++) { A[i] = (double)i; M[i] = 0.0; B[i] = 0.0; }  /* first touch */

    memcpy(M, A, bytes);
    double t0 = omp_get_wtime(); transpose_inplace_serial(M, n);   double ts_in  = omp_get_wtime() - t0;
    int ok_in = is_transpose(A, M, n);
    t0 = omp_get_wtime(); transpose_naive_serial(A, B, n);          double ts_out = omp_get_wtime() - t0;
    int ok_out = is_transpose(A, B, n);
    printf("Q14: in-place transpose schedules + out-of-place naive vs blocked (N = %ld)\n", n);
    printf("serial(s): in-place %.6f (%s)  out-of-place naive %.6f (%s)\n",
           ts_in, ok_in ? "OK" : "WRONG", ts_out, ok_out ? "OK" : "WRONG");
    printf("%8s %10s %6s %10s %6s %10s %6s %10s %6s %10s %6s %6s\n", "threads",
           "static(s)", "sp", "dyn16(s)", "sp", "guided(s)", "sp",
           "naive(s)", "sp", "tiled(s)", "sp", "check");
    const int threads[] = {1, 2, 4, 8};
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        memcpy(M, A, bytes);                                  /* reset (untimed) */
        t0 = omp_get_wtime(); transpose_inplace_static(M, n);       double tst = omp_get_wtime() - t0;
        int ok = is_transpose(A, M, n);
        memcpy(M, A, bytes);
        t0 = omp_get_wtime(); transpose_inplace_dynamic(M, n);      double tdy = omp_get_wtime() - t0;
        ok = ok && is_transpose(A, M, n);
        memcpy(M, A, bytes);
        t0 = omp_get_wtime(); transpose_inplace_guided(M, n);       double tgu = omp_get_wtime() - t0;
        ok = ok && is_transpose(A, M, n);
        memset(B, 0, bytes);
        t0 = omp_get_wtime(); transpose_naive_parallel(A, B, n);    double tna = omp_get_wtime() - t0;
        ok = ok && is_transpose(A, B, n);
        memset(B, 0, bytes);
        t0 = omp_get_wtime(); transpose_blocked_parallel(A, B, n);  double tbl = omp_get_wtime() - t0;
        ok = ok && is_transpose(A, B, n);
        printf("%8d %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %6s\n",
               threads[ti], tst, speedup(ts_in, tst), tdy, speedup(ts_in, tdy), tgu, speedup(ts_in, tgu),
               tna, speedup(ts_out, tna), tbl, speedup(ts_out, tbl), ok ? "OK" : "WRONG");
    }
    printf("(in-place speedups vs serial in-place; out-of-place speedups vs serial naive)\n\n");
    free(A); free(M); free(B);
    return 0;
}

int main(int argc, char **argv) {
    long sizes[] = {1000, 4096};             /* 1000 is not a multiple of 32 (partial tiles) */
    int nsizes = 2;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (sizes[0] < 1) { fprintf(stderr, "usage: %s [N>=1]\n", argv[0]); return 1; }
    for (int s = 0; s < nsizes; s++)
        if (run(sizes[s])) return 1;
    return 0;
}

/* Viva notes:
 * - In place is race-free because the loop runs over j > i only: pair (i,j)/(j,i)
 *   is swapped by iteration min(i,j) and nobody else. Looping j over 0..n-1 would
 *   swap every pair twice (giving back A) and two threads would race on it.
 * - Load balance: row i has n-i-1 swaps, so the work is a triangle. With
 *   schedule(static) the thread holding the first rows does ~(2T-1)x the work of
 *   the last one, which caps static near T/2 speedup. dynamic,16 hands out small
 *   chunks on demand; guided starts big and shrinks. schedule(static,1)
 *   (round-robin rows) also balances it. schedule(runtime) plus
 *   OMP_SCHEDULE="dynamic,16" picks the schedule without recompiling.
 * - Why chunk 16 and not 1: each grab of the shared counter costs time, and
 *   neighbouring rows share cache lines in the column being swapped.
 * - Transpose is pure data movement and one side is always strided (column
 *   access), so it is memory/cache bound; speedups are modest. Tiling (32x32
 *   doubles = 8 KB per tile, two tiles fit in L1) makes the strided side reuse
 *   cache lines, so at large N tiled beats naive even on one thread. N = 4096
 *   (a power of two) makes naive extra slow: a column maps to few cache sets.
 * - Out of place needs no special care: A is read-only and every B element is
 *   written by exactly one (i,j) iteration / one tile.
 */

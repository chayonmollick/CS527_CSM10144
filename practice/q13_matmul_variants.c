/* CS527 Computer Systems — OpenMP practice
 * Q13: Matrix multiplication C = A x B (N x N). Parallelise it and compare loop
 *      orders / memory-access optimisations. Report time, speedup and GFLOP/s.
 *  - serial ijk        : baseline (inner k loop reads B down a column, stride N)
 *  - parallel ijk      : parallel for on the outer i loop
 *  - parallel ikj      : i,k,j order -> inner loop walks B and C along rows
 *  - parallel B^T      : transpose B first, then C[i][j] = row i of A . row j of B^T
 *                        (inner dot product marked `omp simd reduction(+:s)`)
 *  - parallel blocked  : 64x64 tiles, collapse(2) over (ii,jj) tiles; each thread
 *                        owns whole C tiles, k-tiles accumulated inside
 * GFLOP/s = 2*N^3 / time / 1e9 (one multiply + one add per inner iteration).
 * Matrices are flattened row-major: A[i][j] = A[(long)i*n + j].
 * Usage: ./q13 [N]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define BS 64                                /* tile size for the blocked version */

static void mm_serial_ijk(const double *A, const double *B, double *C, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) {
            double s = 0.0;
            for (long k = 0; k < n; k++) s += A[i*n + k] * B[k*n + j];
            C[i*n + j] = s;
        }
}
/* each thread computes whole rows of C; s is private (declared inside) */
static void mm_par_ijk(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) {
            double s = 0.0;
            for (long k = 0; k < n; k++) s += A[i*n + k] * B[k*n + j];
            C[i*n + j] = s;
        }
}
/* i,k,j: A[i][k] is fixed in the inner loop, B and C rows are read/written with
 * stride 1 (cache friendly, vectorisable). Row i of C is owned by one thread. */
static void mm_par_ikj(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) C[i*n + j] = 0.0;
        for (long k = 0; k < n; k++) {
            double aik = A[i*n + k];
            for (long j = 0; j < n; j++) C[i*n + j] += aik * B[k*n + j];
        }
    }
}
/* One parallel region, two loops: the implicit barrier after the transpose loop
 * guarantees Bt is complete before any thread starts reading it. */
static void mm_par_transposed(const double *A, const double *B, double *C, long n) {
    double *Bt = malloc((size_t)(n * n) * sizeof(double));
    if (!Bt) { fprintf(stderr, "alloc failed (Bt)\n"); return; }
    #pragma omp parallel
    {
        #pragma omp for
        for (long i = 0; i < n; i++)
            for (long j = 0; j < n; j++) Bt[j*n + i] = B[i*n + j];
        /* implicit barrier */
        #pragma omp for
        for (long i = 0; i < n; i++)
            for (long j = 0; j < n; j++) {
                double s = 0.0;
                /* simd reduction lets the compiler split s into vector partial sums;
                 * without it the sum is one chain of dependent adds (no SIMD) */
                #pragma omp simd reduction(+:s)
                for (long k = 0; k < n; k++) s += A[i*n + k] * Bt[j*n + k];  /* both stride 1 */
                C[i*n + j] = s;
            }
    }
    free(Bt);
}
/* Tiled: a BSxBS tile of C stays in cache while we stream matching tiles of A
 * and B through it. Each (ii,jj) C tile belongs to exactly one iteration of the
 * collapsed loop -> no two threads write the same C element. */
static void mm_par_blocked(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for collapse(2) schedule(static)
    for (long ii = 0; ii < n; ii += BS)
        for (long jj = 0; jj < n; jj += BS) {
            long iend = ii + BS < n ? ii + BS : n;          /* last tile may be partial */
            long jend = jj + BS < n ? jj + BS : n;
            for (long i = ii; i < iend; i++)
                for (long j = jj; j < jend; j++) C[i*n + j] = 0.0;
            for (long kk = 0; kk < n; kk += BS) {
                long kend = kk + BS < n ? kk + BS : n;
                for (long i = ii; i < iend; i++)
                    for (long k = kk; k < kend; k++) {
                        double aik = A[i*n + k];
                        for (long j = jj; j < jend; j++) C[i*n + j] += aik * B[k*n + j];
                    }
            }
        }
}

/* relative tolerance: different loop orders add the products in different orders */
static int close_mat(const double *X, const double *Y, long n2) {
    for (long i = 0; i < n2; i++)
        if (fabs(X[i] - Y[i]) > 1e-9 * (1.0 + fabs(X[i]))) return 0;
    return 1;
}
static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }
static double gflops(double gflop, double t)  { return t > 0 ? gflop / t : 0.0; }

typedef void (*mm_fn)(const double *, const double *, double *, long);

static int run(long n) {
    long n2 = n * n;
    double *A  = malloc((size_t)n2 * sizeof(double));
    double *B  = malloc((size_t)n2 * sizeof(double));
    double *Cs = malloc((size_t)n2 * sizeof(double));
    double *Cp = malloc((size_t)n2 * sizeof(double));
    if (!A || !B || !Cs || !Cp) {
        fprintf(stderr, "alloc failed (N=%ld)\n", n);
        free(A); free(B); free(Cs); free(Cp); return 1;
    }
    for (long i = 0; i < n2; i++) {
        A[i]  = (double)((i % 13) - 6) * 0.25;
        B[i]  = (double)((i % 7) - 3) * 0.5;
        Cp[i] = 0.0;                         /* first touch, so page faults are not timed */
    }
    double gflop = 2.0 * (double)n * (double)n * (double)n / 1e9;

    double t0 = omp_get_wtime(); mm_serial_ijk(A, B, Cs, n); double ts = omp_get_wtime() - t0;
    printf("Q13: matrix multiply variants (N = %ld)\n", n);
    printf("serial ijk: %.4f s  (%.2f GFLOP/s)\n", ts, gflops(gflop, ts));
    printf("%-14s %8s %10s %8s %9s %6s\n", "method", "threads", "time(s)", "speedup", "GFLOP/s", "check");

    const char *names[] = {"par ijk", "par ikj", "par B^T", "par blocked64"};
    const mm_fn fns[]   = {mm_par_ijk, mm_par_ikj, mm_par_transposed, mm_par_blocked};
    const int threads[] = {1, 2, 4, 8};
    for (int m = 0; m < 4; m++) {
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            t0 = omp_get_wtime(); fns[m](A, B, Cp, n); double tp = omp_get_wtime() - t0;
            printf("%-14s %8d %10.4f %8.2f %9.2f %6s\n", names[m], threads[ti], tp,
                   speedup(ts, tp), gflops(gflop, tp), close_mat(Cs, Cp, n2) ? "OK" : "WRONG");
        }
    }
    printf("\n");
    free(A); free(B); free(Cs); free(Cp);
    return 0;
}

int main(int argc, char **argv) {
    long sizes[] = {512, 1024};              /* N=1024: serial ijk takes a few seconds */
    int nsizes = 2;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (sizes[0] < 1) { fprintf(stderr, "usage: %s [N>=1]\n", argv[0]); return 1; }
    for (int s = 0; s < nsizes; s++)
        if (run(sizes[s])) return 1;
    return 0;
}

/* Viva notes:
 * - Parallelise i (or (i,j)), never k: k is the reduction loop, so splitting it
 *   would make threads race on C[i][j] (or need a reduction per element). With i
 *   split, each thread owns whole rows of C -> no race, no synchronisation.
 * - Speedup is relative to serial ijk, so a cache-friendly order (ikj, B^T,
 *   blocked) shows speedup > 1 even on ONE thread: the gain is from memory
 *   access, not from threads. ijk reads B[k][j] with stride N -> a cache miss per
 *   multiply; ikj/B^T use stride-1 rows; blocking reuses each tile from cache.
 * - ikj's inner loop has independent j iterations, so GCC vectorises it. A dot
 *   product (ijk, B^T) is a chain s += ..., which C forbids reordering, so it runs
 *   at FP-add latency; `#pragma omp simd reduction(+:s)` permits the reordering
 *   (B^T is ~2x faster with it). That is SIMD inside one thread, not threads.
 * - Matmul is compute-bound (O(N^3) flops on O(N^2) data), so it scales much
 *   better with threads than the vector/matrix-add style programs.
 * - Blocked + collapse(2): (ii,jj) tiles are independent, collapsing them gives
 *   (N/64)^2 tasks to share, which balances better than N/64 tile-rows alone.
 *   Do not collapse kk: tiles for different kk add into the SAME C tile.
 * - Results differ from serial only in rounding (different summation order), so
 *   compare with a relative tolerance, never ==. schedule(dynamic) gains nothing
 *   here: every row (and every tile) has the same amount of work.
 */

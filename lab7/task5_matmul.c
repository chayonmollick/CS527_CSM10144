/* CS527 Computer Systems — OpenMP Lab
 * Task 5: Naive matrix multiply C = A x B (N x N, flattened row-major).
 *  - serial ijk (accumulate into a local sum, write C[i][j] once)
 *  - parallel on outer i loop
 *  - parallel collapse(2) on (i,j)  [NOT the reduction loop k]
 *  - bonus: i,k,j loop order (row-wise access to B and C)
 * Reports speedup and parallel efficiency (speedup / threads).
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void mm_serial(const double *A, const double *B, double *C, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0.0;
            for (int k = 0; k < n; k++) s += A[(long)i*n+k] * B[(long)k*n+j];
            C[(long)i*n+j] = s;
        }
}
static void mm_outer(const double *A, const double *B, double *C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0.0;
            for (int k = 0; k < n; k++) s += A[(long)i*n+k] * B[(long)k*n+j];
            C[(long)i*n+j] = s;
        }
}
static void mm_collapse(const double *A, const double *B, double *C, int n) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0.0;
            for (int k = 0; k < n; k++) s += A[(long)i*n+k] * B[(long)k*n+j];
            C[(long)i*n+j] = s;
        }
}
/* bonus: i,k,j order — C must be zeroed first; parallel over i is race-free. */
static void mm_ikj_parallel(const double *A, const double *B, double *C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) C[(long)i*n+j] = 0.0;
        for (int k = 0; k < n; k++) {
            double aik = A[(long)i*n+k];
            for (int j = 0; j < n; j++)
                C[(long)i*n+j] += aik * B[(long)k*n+j];
        }
    }
}
static int close_enough(const double *X, const double *Y, long n2) {
    for (long i = 0; i < n2; i++)
        if (fabs(X[i] - Y[i]) > 1e-6 * (1.0 + fabs(X[i]))) return 0;
    return 1;
}

int main(void) {
    const int sizes[]  = {256, 512, 1024};
    const int threads[] = {1, 2, 4, 8};

    printf("Task 5: Matrix-Matrix Multiplication (naive)\n");
    printf("%6s %8s %10s %10s %8s %7s %10s %8s %7s\n",
           "N", "threads", "serial(s)", "outer(s)", "sp", "eff",
           "collapse(s)", "sp", "eff");

    for (int si = 0; si < 3; si++) {
        int n = sizes[si];
        long n2 = (long)n * n;
        double *A  = malloc((size_t)n2 * sizeof(double));
        double *B  = malloc((size_t)n2 * sizeof(double));
        double *Cs = malloc((size_t)n2 * sizeof(double));
        double *Cp = malloc((size_t)n2 * sizeof(double));
        if (!A || !B || !Cs || !Cp) { printf("N=%d skipped (alloc)\n", n);
            free(A);free(B);free(Cs);free(Cp); continue; }
        for (long i = 0; i < n2; i++) { A[i] = (double)((i % 13) - 6); B[i] = (double)((i % 7) - 3); }

        double t0 = omp_get_wtime();
        mm_serial(A, B, Cs, n);
        double ts = omp_get_wtime() - t0;

        for (int ti = 0; ti < 4; ti++) {
            int nt = threads[ti];
            omp_set_num_threads(nt);

            double o0 = omp_get_wtime(); mm_outer(A, B, Cp, n);
            double to = omp_get_wtime() - o0;
            int ok1 = close_enough(Cs, Cp, n2);

            double k0 = omp_get_wtime(); mm_collapse(A, B, Cp, n);
            double tk = omp_get_wtime() - k0;
            int ok2 = close_enough(Cs, Cp, n2);

            double sp_o = to > 0 ? ts/to : 0.0, sp_k = tk > 0 ? ts/tk : 0.0;
            printf("%6d %8d %10.4f %10.4f %8.2f %7.2f %10.4f %8.2f %7.2f %s\n",
                   n, nt, ts, to, sp_o, sp_o/nt, tk, sp_k, sp_k/nt,
                   (ok1 && ok2) ? "" : "VERIFY-FAIL");
        }

        /* bonus: i,k,j loop order baseline + parallel at 8 threads */
        double b0 = omp_get_wtime(); mm_ikj_parallel(A, B, Cp, n);
        double tb = omp_get_wtime() - b0;
        printf("   bonus ikj (parallel, %d threads): %.4fs  (%s)\n",
               omp_get_max_threads(), tb,
               close_enough(Cs, Cp, n2) ? "verified" : "VERIFY-FAIL");

        free(A); free(B); free(Cs); free(Cp);
    }
    return 0;
}

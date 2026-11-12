/* CS527 Computer Systems — OpenMP Lab 8
 * Task 4: Matrix transpose  B[j][i] = A[i][j]  (out-of-place, N x N).
 * Out-of-place means each B element is written by exactly one iteration, so
 * there is no race. Compares outer-loop parallelism vs collapse(2).
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

static void transpose_serial(const double *A, double *B, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            B[(long)j*n+i] = A[(long)i*n+j];
}
static void transpose_outer(const double *A, double *B, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            B[(long)j*n+i] = A[(long)i*n+j];
}
static void transpose_collapse(const double *A, double *B, int n) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            B[(long)j*n+i] = A[(long)i*n+j];
}
static int correct(const double *A, const double *B, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (B[(long)j*n+i] != A[(long)i*n+j]) return 0;
    return 1;
}

int main(void) {
    const int sizes[]  = {1024, 4096};
    const int threads[] = {1, 2, 4, 8};
    printf("Task 4: Matrix Transpose\n");
    printf("%6s %8s %10s %10s %10s %9s %9s\n",
           "N", "threads", "serial(s)", "outer(s)", "collapse(s)", "sp_outer", "sp_coll");

    for (int si = 0; si < 2; si++) {
        int n = sizes[si]; long n2 = (long)n*n;
        double *A = malloc((size_t)n2*sizeof(double));
        double *B = malloc((size_t)n2*sizeof(double));
        if (!A || !B) { printf("N=%d skipped (alloc)\n", n); free(A); free(B); continue; }
        for (long i = 0; i < n2; i++) A[i] = (double)i;

        double t0 = omp_get_wtime(); transpose_serial(A, B, n); double ts = omp_get_wtime()-t0;
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double o0 = omp_get_wtime(); transpose_outer(A, B, n); double to = omp_get_wtime()-o0;
            int ok1 = correct(A, B, n);
            double k0 = omp_get_wtime(); transpose_collapse(A, B, n); double tk = omp_get_wtime()-k0;
            int ok2 = correct(A, B, n);
            printf("%6d %8d %10.6f %10.6f %10.6f %9.2f %9.2f %s\n",
                   n, threads[ti], ts, to, tk, to>0?ts/to:0.0, tk>0?ts/tk:0.0,
                   (ok1&&ok2)?"":"VERIFY-FAIL");
        }
        free(A); free(B);
    }
    return 0;
}

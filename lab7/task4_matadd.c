/* CS527 Computer Systems — OpenMP Lab
 * Task 4: Matrix addition  C = A + B  (N x N, flattened row-major 1D arrays).
 * Compares: serial, parallel outer loop only, and collapse(2).
 * Each timed op is repeated so small-N measurements are not just noise;
 * reps are scaled to keep total work roughly constant across sizes.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

static void madd_serial(const double *A, const double *B, double *C, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            C[(long)i*n+j] = A[(long)i*n+j] + B[(long)i*n+j];
}
static void madd_outer(const double *A, const double *B, double *C, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            C[(long)i*n+j] = A[(long)i*n+j] + B[(long)i*n+j];
}
static void madd_collapse(const double *A, const double *B, double *C, int n) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            C[(long)i*n+j] = A[(long)i*n+j] + B[(long)i*n+j];
}
static int identical(const double *X, const double *Y, long n2) {
    for (long i = 0; i < n2; i++) if (X[i] != Y[i]) return 0;
    return 1;
}

int main(void) {
    const int sizes[]  = {8, 4096};
    const int threads[] = {1, 2, 4, 8};

    printf("Task 4: Matrix Addition\n");
    printf("%6s %8s %10s %12s %12s %9s %9s\n",
           "N", "threads", "serial(s)", "outer(s)", "collapse(s)",
           "sp_outer", "sp_coll");

    for (int si = 0; si < 2; si++) {
        int n = sizes[si];
        long n2 = (long)n * n;
        /* Each rep opens a new parallel region. At N = 8 a call is only 64 adds,
         * so the time is fork/join overhead; cap reps so that case stays short. */
        long reps = 200000000L / n2; if (reps < 1) reps = 1;
        if (reps > 20000) reps = 20000;

        double *A  = malloc((size_t)n2 * sizeof(double));
        double *B  = malloc((size_t)n2 * sizeof(double));
        double *Cs = malloc((size_t)n2 * sizeof(double));
        double *Cp = malloc((size_t)n2 * sizeof(double));
        if (!A || !B || !Cs || !Cp) { printf("N=%d skipped (alloc)\n", n);
            free(A);free(B);free(Cs);free(Cp); continue; }
        for (long i = 0; i < n2; i++) { A[i] = (double)(i % 100); B[i] = (double)(i % 50); }

        double t0 = omp_get_wtime();
        for (long r = 0; r < reps; r++) madd_serial(A, B, Cs, n);
        double ts = omp_get_wtime() - t0;

        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double o0 = omp_get_wtime();
            for (long r = 0; r < reps; r++) madd_outer(A, B, Cp, n);
            double to = omp_get_wtime() - o0;
            int ok1 = identical(Cs, Cp, n2);

            double k0 = omp_get_wtime();
            for (long r = 0; r < reps; r++) madd_collapse(A, B, Cp, n);
            double tk = omp_get_wtime() - k0;
            int ok2 = identical(Cs, Cp, n2);

            printf("%6d %8d %10.6f %12.6f %12.6f %9.2f %9.2f %s\n",
                   n, threads[ti], ts, to, tk,
                   to > 0 ? ts/to : 0.0, tk > 0 ? ts/tk : 0.0,
                   (ok1 && ok2) ? "" : "VERIFY-FAIL");
        }
        printf("   (N=%d, reps=%ld)\n", n, reps);
        free(A); free(B); free(Cs); free(Cp);
    }
    return 0;
}

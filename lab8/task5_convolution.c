/* CS527 Computer Systems — OpenMP Lab 8
 * Task 5: 2D convolution with a 3x3 averaging filter, valid positions only
 * (no padding): an N x N image gives an (N-2) x (N-2) output.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const double K[3][3] = {{1,1,1},{1,1,1},{1,1,1}};   /* /9 averaging */

static void conv_serial(const double *in, double *out, int n) {
    int m = n - 2;
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++) {
            double s = 0.0;
            for (int di = 0; di < 3; di++)
                for (int dj = 0; dj < 3; dj++)
                    s += K[di][dj] * in[(long)(i+di)*n + (j+dj)];
            out[(long)i*m + j] = s / 9.0;
        }
}
static void conv_parallel(const double *in, double *out, int n) {
    int m = n - 2;
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++) {
            double s = 0.0;
            for (int di = 0; di < 3; di++)
                for (int dj = 0; dj < 3; dj++)
                    s += K[di][dj] * in[(long)(i+di)*n + (j+dj)];
            out[(long)i*m + j] = s / 9.0;
        }
}
static int close_enough(const double *x, const double *y, long m2) {
    for (long i = 0; i < m2; i++) if (fabs(x[i]-y[i]) > 1e-9) return 0;
    return 1;
}

int main(void) {
    /* sanity check on the 5x5 example from the lab sheet (center -> 70) */
    double img5[25]; for (int i = 0; i < 25; i++) img5[i] = (i+1)*10;
    double out5[9]; conv_serial(img5, out5, 5);
    printf("Task 5: 2D Convolution (3x3 averaging, valid)\n");
    printf("5x5 example, O[0][0] = %.1f (expected 70.0)\n\n", out5[0]);

    const int sizes[]  = {2048, 4096};
    const int threads[] = {1, 2, 4, 8};
    printf("%6s %8s %10s %12s %9s %7s\n",
           "N", "threads", "serial(s)", "parallel(s)", "speedup", "match");
    for (int si = 0; si < 2; si++) {
        int n = sizes[si]; long n2 = (long)n*n; long m2 = (long)(n-2)*(n-2);
        double *in = malloc((size_t)n2*sizeof(double));
        double *os = malloc((size_t)m2*sizeof(double));
        double *op = malloc((size_t)m2*sizeof(double));
        if (!in || !os || !op) { printf("N=%d skipped (alloc)\n", n); free(in);free(os);free(op); continue; }
        for (long i = 0; i < n2; i++) in[i] = (double)(i % 256);

        double t0 = omp_get_wtime(); conv_serial(in, os, n); double ts = omp_get_wtime()-t0;
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double p0 = omp_get_wtime(); conv_parallel(in, op, n); double tp = omp_get_wtime()-p0;
            printf("%6d %8d %10.6f %12.6f %9.2f %7s\n",
                   n, threads[ti], ts, tp, tp>0?ts/tp:0.0,
                   close_enough(os, op, m2) ? "yes" : "NO");
        }
        free(in); free(os); free(op);
    }
    return 0;
}

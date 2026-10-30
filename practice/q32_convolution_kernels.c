/* CS527 Computer Systems — OpenMP practice
 * Q32: Apply a 3x3 convolution filter to an N x N image in parallel with a
 *      selectable kernel (averaging, Gaussian blur, sharpen, Sobel-x, Sobel-y,
 *      gradient magnitude) and two border modes: "valid" (no padding, output
 *      (N-2) x (N-2)) and "same" (zero padding, output N x N). Reproduce the
 *      5x5 handout example, then time collapse(2) parallel vs serial.
 *  - one helper computes the 3x3 weighted sum at a centre pixel (zero outside the image)
 *  - serial: two nested loops over output pixels
 *  - parallel: the same loops under #pragma omp parallel for collapse(2)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum { AVERAGE, GAUSSIAN, SHARPEN, SOBEL_X, SOBEL_Y, GRADIENT, NKERNELS };
enum { VALID, SAME };

typedef struct { const char *name; double w[3][3]; double div; } kernel_t;
static const kernel_t KER[NKERNELS] = {
    {"average",  {{ 1, 1, 1}, { 1, 1, 1}, { 1, 1, 1}},  9},   /* handout filter */
    {"gaussian", {{ 1, 2, 1}, { 2, 4, 2}, { 1, 2, 1}}, 16},
    {"sharpen",  {{ 0,-1, 0}, {-1, 5,-1}, { 0,-1, 0}},  1},
    {"sobel-x",  {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}},  1},
    {"sobel-y",  {{-1,-2,-1}, { 0, 0, 0}, { 1, 2, 1}},  1},
    {"gradient", {{ 0, 0, 0}, { 0, 0, 0}, { 0, 0, 0}},  1},   /* sqrt(gx^2+gy^2) */
};

/* weighted sum of the 3x3 neighbourhood centred at input pixel (ci,cj);
 * neighbours outside the image count as 0 (zero padding). */
static double apply3x3(const double *in, long n, long ci, long cj, const double w[3][3]) {
    double s = 0.0;
    for (int di = -1; di <= 1; di++)
        for (int dj = -1; dj <= 1; dj++) {
            long r = ci + di, c = cj + dj;
            if (r < 0 || r >= n || c < 0 || c >= n) continue;
            s += w[di+1][dj+1] * in[r*n + c];
        }
    return s;
}
static double pixel(const double *in, long n, long ci, long cj, int k) {
    if (k == GRADIENT) {
        double gx = apply3x3(in, n, ci, cj, KER[SOBEL_X].w);
        double gy = apply3x3(in, n, ci, cj, KER[SOBEL_Y].w);
        return sqrt(gx*gx + gy*gy);
    }
    return apply3x3(in, n, ci, cj, KER[k].w) / KER[k].div;
}

/* valid: output m = n-2, out(i,j) centred at in(i+1,j+1); same: m = n, centred at in(i,j) */
static long out_size(long n, int mode) { return mode == VALID ? n - 2 : n; }

static void conv_serial(const double *in, double *out, long n, int k, int mode) {
    long m = out_size(n, mode), off = (mode == VALID) ? 1 : 0;
    for (long i = 0; i < m; i++)
        for (long j = 0; j < m; j++)
            out[i*m + j] = pixel(in, n, i + off, j + off, k);
}
static void conv_parallel(const double *in, double *out, long n, int k, int mode) {
    long m = out_size(n, mode), off = (mode == VALID) ? 1 : 0;
    #pragma omp parallel for collapse(2)       /* m*m independent output pixels */
    for (long i = 0; i < m; i++)
        for (long j = 0; j < m; j++)
            out[i*m + j] = pixel(in, n, i + off, j + off, k);
}

static int close_enough(const double *x, const double *y, long len) {
    for (long i = 0; i < len; i++) if (fabs(x[i] - y[i]) > 1e-9) return 0;
    return 1;
}
static void print_matrix(const double *a, long m) {
    for (long i = 0; i < m; i++) {
        printf("   ");
        for (long j = 0; j < m; j++) printf(" %8.2f", a[i*m + j]);
        printf("\n");
    }
}

int main(int argc, char **argv) {
    long n = (argc > 1) ? atol(argv[1]) : 4096;
    if (n < 3) { fprintf(stderr, "N must be >= 3\n"); return 1; }
    const int threads[] = {1, 2, 4, 8};
    printf("Q32: 3x3 convolution, selectable kernel, valid / same borders\n");

    /* ---- 1. handout example: 5x5 rows {10..50},{60..100},...,{210..250} ---- */
    double img5[25], out5[25];
    for (int i = 0; i < 25; i++) img5[i] = (i + 1) * 10;
    conv_parallel(img5, out5, 5, AVERAGE, VALID);
    int ok5 = 1;                                  /* expected O = 70 + 50*i + 10*j */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) if (fabs(out5[i*3 + j] - (70 + 50*i + 10*j)) > 1e-9) ok5 = 0;
    printf("\nHandout 5x5, averaging, valid -> 3x3 (O[1][1] in input coords = %.1f, expected 70)\n",
           out5[0]);
    print_matrix(out5, 3);
    printf("    check: %s\n", ok5 ? "OK" : "WRONG");

    printf("(the 5x5 image is a linear ramp, so the symmetric kernels whose weights\n"
           " sum to 1 -- average, gaussian, sharpen -- all return the same values)\n");
    for (int k = GAUSSIAN; k < NKERNELS; k++) {
        conv_parallel(img5, out5, 5, k, VALID);
        printf("%s, valid:\n", KER[k].name); print_matrix(out5, 3);
    }
    conv_parallel(img5, out5, 5, AVERAGE, SAME);
    printf("average, same (zero padding: borders get smaller values):\n"); print_matrix(out5, 5);

    /* ---- 2. every kernel x mode: parallel == serial (on a smaller image) ---- */
    long nc = n < 1024 ? n : 1024;
    double *in  = malloc((size_t)(n*n) * sizeof(double));
    double *os  = malloc((size_t)(n*n) * sizeof(double));
    double *op  = malloc((size_t)(n*n) * sizeof(double));
    if (!in || !os || !op) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < nc; i++)
        for (long j = 0; j < nc; j++) in[i*nc + j] = (double)((i*7 + j*13) % 256);
    printf("\nAll kernels on %ldx%ld, 8 threads vs serial:\n", nc, nc);
    omp_set_num_threads(8);
    for (int k = 0; k < NKERNELS; k++)
        for (int mode = VALID; mode <= SAME; mode++) {
            long m = out_size(nc, mode);
            conv_serial(in, os, nc, k, mode);
            conv_parallel(in, op, nc, k, mode);
            printf("  %-9s %-5s %s\n", KER[k].name, mode == VALID ? "valid" : "same",
                   close_enough(os, op, m*m) ? "OK" : "WRONG");
        }

    /* ---- 3. timing on the big image, averaging kernel, both modes ---- */
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) in[i*n + j] = (double)((i*7 + j*13) % 256);
    for (int mode = VALID; mode <= SAME; mode++) {
        long m = out_size(n, mode);
        memset(os, 0, (size_t)(m*m) * sizeof(double));     /* touch pages before timing */
        double t0 = omp_get_wtime(); conv_serial(in, os, n, AVERAGE, mode);
        double ts = omp_get_wtime() - t0;
        printf("\nTiming: %ldx%ld image, average kernel, %s mode\n", n, n,
               mode == VALID ? "valid" : "same");
        printf("%8s %12s %12s %9s %11s %7s\n",
               "threads", "serial(s)", "parallel(s)", "speedup", "efficiency", "check");
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            memset(op, 0, (size_t)(m*m) * sizeof(double));
            double p0 = omp_get_wtime(); conv_parallel(in, op, n, AVERAGE, mode);
            double tp = omp_get_wtime() - p0;
            double sp = tp > 0 ? ts / tp : 0.0;
            printf("%8d %12.6f %12.6f %9.2f %11.2f %7s\n", threads[ti], ts, tp, sp,
                   sp / threads[ti], close_enough(os, op, m*m) ? "OK" : "WRONG");
        }
    }
    free(in); free(os); free(op);
    return 0;
}

/* Viva notes:
 * - Every output pixel reads only the INPUT image and writes its own output
 *   cell, so all m*m pixels are independent: no race, no reduction. Writing
 *   back into the input (in place) would be a race -> always use 2 buffers.
 * - collapse(2) merges the i and j loops into one m*m iteration space, giving
 *   more, finer chunks for load balance; i and j must be perfectly nested and
 *   the inner bounds must not depend on i. Rows alone (m >= threads) also work.
 * - valid: output (n-2)x(n-2), only positions where the kernel fits fully, so
 *   out(i,j) is centred at in(i+1,j+1) -- the handout's O[1][1]=70 is out[0][0].
 *   same: zero padding, output n x n; border pixels see zeros (smaller values).
 * - Kernels: average and Gaussian (weights sum to 1 after /div) blur; sharpen
 *   (sum 1) boosts the centre; Sobel x/y (sum 0) estimate the derivative, so
 *   a flat area gives 0. Gradient magnitude = sqrt(gx^2 + gy^2) = edge strength.
 * - Memory-bound stencil (9 loads, ~9 FMAs per pixel), so speedup flattens once
 *   memory bandwidth is saturated.
 */

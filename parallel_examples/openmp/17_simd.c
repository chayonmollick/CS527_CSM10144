/* 17_simd.c - simd: vector (SIMD) instructions inside each thread
 *
 * What: "#pragma omp simd" asks the compiler to run several iterations at once with
 *       SIMD instructions (e.g. 4 doubles per AVX instruction) - inside ONE thread.
 *       "#pragma omp parallel for simd" splits the loop over the threads AND vectorises
 *       each thread's part. reduction works as it does for threads. (OpenMP 4.0)
 * When: simple loops over arrays with independent iterations: saxpy, dot product,
 *       scaling. Not when iteration i needs iteration i-1. gcc older than 12 does not
 *       vectorise at -O2 by itself (the lab PC), so there the pragma makes the difference.
 *       Same idea as the simulator's vector registers: 8 lanes per instruction.
 * Expected output (the same every run; only the times vary):
 *   y = 2x + y  with parallel for simd: 0.0030 s, y[0] = 1, y[3999999] = 31
 *   dot(x, y)   with simd reduction   : 0.0036 s, 650000000 (serial 650000000)
 *   both equal the serial loops: OK
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 4000000

int main(void) {
    double *x = malloc(N * sizeof *x), *y = malloc(N * sizeof *y);
    if (!x || !y) return 1;
    for (int i = 0; i < N; i++) { x[i] = i % 16; y[i] = 1.0; }

    double t = omp_get_wtime();
    #pragma omp parallel for simd           /* threads, and vector lanes inside each thread */
    for (int i = 0; i < N; i++)
        y[i] = 2.0 * x[i] + y[i];
    double t_saxpy = omp_get_wtime() - t;

    double dot = 0.0;
    t = omp_get_wtime();
    #pragma omp simd reduction(+:dot)       /* one thread; each lane sums, lanes added at the end */
    for (int i = 0; i < N; i++)
        dot += x[i] * y[i];
    double t_dot = omp_get_wtime() - t;

    long wrong = 0;                         /* serial check (exact: small whole numbers) */
    double s_dot = 0.0;
    for (int i = 0; i < N; i++) {
        double yi = 2.0 * (i % 16) + 1.0;
        if (y[i] != yi) wrong++;
        s_dot += (i % 16) * yi;
    }
    printf("y = 2x + y  with parallel for simd: %.4f s, y[0] = %.0f, y[%d] = %.0f\n",
           t_saxpy, y[0], N - 1, y[N - 1]);
    printf("dot(x, y)   with simd reduction   : %.4f s, %.0f (serial %.0f)\n", t_dot, dot, s_dot);
    int ok = wrong == 0 && dot == s_dot;
    printf("both equal the serial loops: %s\n", ok ? "OK" : "WRONG");
    free(x); free(y);
    return ok ? 0 : 1;
}

/* P079: 2D heat equation, explicit time stepping
 * Problem: Simulate heat diffusion on an N x N plate (flattened row-major,
 *   u[(long)i*N + j]) for T time steps with the explicit scheme
 *       u_new[i][j] = u[i][j] + r * (u[i-1][j] + u[i+1][j] + u[i][j-1] + u[i][j+1] - 4 u[i][j])
 *   for the interior points, with fixed (Dirichlet) boundary values: top edge
 *   100, other edges 0, interior initially 25. Use r = 0.2 (stable for
 *   r <= 0.25). Parallelise with OpenMP, verify against the serial code, and
 *   compare a "parallel for" per time step with one parallel region around the
 *   whole time loop.
 * Solution: double buffering: step t reads only u and writes only u_new, so
 *   all points of a step are independent ("for" over rows is race-free);
 *   then the two pointers are swapped. Both buffers hold the boundary values,
 *   which are never written. Steps are sequential: step t+1 needs all of step t.
 *   Version A: "#pragma omp parallel for" inside the time loop (one fork/join
 *   per step; the master swaps the pointers between regions).
 *   Version B: one "#pragma omp parallel" around the time loop; each step is an
 *   "omp for" whose implicit barrier ends the step, and every thread swaps its
 *   OWN private copies of the two pointers (firstprivate) in the same way, so
 *   no extra barrier or single is needed for the swap.
 *   Each point uses the same expression as the serial code: results are equal
 *   bit for bit.
 * Source: lab extension (stencil codes).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p079_heat_explicit.c -o p079 -lm
 * Run:   ./p079 [N] [T] [r]  (defaults N = 1000, T = 100, r = 0.2)
 */
#include "common.h"

static void init_plate(double *u, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            u[i * n + j] = (i == 0) ? 100.0 : (i == n - 1 || j == 0 || j == n - 1) ? 0.0 : 25.0;
}

/* All versions: copy the initial plate u0 into both buffers a and b, run T steps,
 * and return the buffer holding the final state. */
static double *heat_serial(const double *u0, double *a, double *b, long n, long T, double r) {
    memcpy(a, u0, (size_t)n * n * sizeof(double));
    memcpy(b, u0, (size_t)n * n * sizeof(double));       /* boundary in both buffers */
    for (long t = 0; t < T; t++) {
        for (long i = 1; i < n - 1; i++)
            for (long j = 1; j < n - 1; j++) {
                long k = i * n + j;
                b[k] = a[k] + r * (a[k - n] + a[k + n] + a[k - 1] + a[k + 1] - 4.0 * a[k]);
            }
        double *tmp = a; a = b; b = tmp;                  /* new becomes current */
    }
    return a;
}

/* Version A: one parallel for per time step. */
static double *heat_par_for(const double *u0, double *a, double *b, long n, long T, double r) {
    memcpy(a, u0, (size_t)n * n * sizeof(double));
    memcpy(b, u0, (size_t)n * n * sizeof(double));
    for (long t = 0; t < T; t++) {
        #pragma omp parallel for
        for (long i = 1; i < n - 1; i++)
            for (long j = 1; j < n - 1; j++) {
                long k = i * n + j;
                b[k] = a[k] + r * (a[k - n] + a[k + n] + a[k - 1] + a[k + 1] - 4.0 * a[k]);
            }
        double *tmp = a; a = b; b = tmp;                  /* after the join: safe */
    }
    return a;
}

/* Version B: one parallel region around the time loop. */
static double *heat_par_region(const double *u0, double *a, double *b, long n, long T, double r) {
    memcpy(a, u0, (size_t)n * n * sizeof(double));
    memcpy(b, u0, (size_t)n * n * sizeof(double));
    double *final_buf = (T % 2 == 0) ? a : b;             /* where the last step writes */
    #pragma omp parallel firstprivate(a, b)               /* private pointer copies */
    for (long t = 0; t < T; t++) {                        /* every thread runs the t loop */
        #pragma omp for
        for (long i = 1; i < n - 1; i++)
            for (long j = 1; j < n - 1; j++) {
                long k = i * n + j;
                b[k] = a[k] + r * (a[k - n] + a[k + n] + a[k - 1] + a[k + 1] - 4.0 * a[k]);
            }
        /* implicit barrier: the whole step is written before anyone reads it */
        double *tmp = a; a = b; b = tmp;                  /* each thread swaps its own copy */
    }
    return final_buf;
}

static void print_plate(const double *u, long n) {
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) printf(" %6.2f", u[i * n + j]);
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000);
    long T = arg_long(argc, argv, 2, 100);
    double r = arg_double(argc, argv, 3, 0.2);
    if (n < 3) n = 3;
    if (T < 0) T = 0;
    if (r > 0.25) printf("warning: r = %g > 0.25, the explicit scheme is unstable\n", r);

    /* Worked example: 7 x 7 plate after 10 steps (heat flows in from the top edge). */
    double s0[49], s1[49], s2[49], s3[49], s4[49];
    init_plate(s0, 7);
    double *ss = heat_serial(s0, s1, s2, 7, 10, 0.2);
    double *sp = heat_par_region(s0, s3, s4, 7, 10, 0.2);
    printf("P079: 2D heat equation, explicit steps, r = %.2f\n7 x 7 plate after 10 steps:\n", 0.2);
    print_plate(ss, 7);
    printf("7 x 7 example, parallel == serial: %s\n\n", ok_str(close_doubles(ss, sp, 49, 0.0)));

    size_t bytes = (size_t)n * n * sizeof(double);
    double *u0 = xmalloc(bytes), *a = xmalloc(bytes), *b = xmalloc(bytes);
    double *c = xmalloc(bytes), *d = xmalloc(bytes);
    init_plate(u0, n);
    omp_warmup();

    double ts, tp;
    double *us = NULL, *up = NULL;
    BEST_TIME(ts, us = heat_serial(u0, a, b, n, T, r));
    printf("N = %ld, T = %ld steps; centre value after T steps = %.6f\n", n, T, us[(n / 2) * n + n / 2]);
    for (int v = 0; v < 2; v++) {
        printf("\nVersion %s (check: equal to serial exactly)\n",
               v == 0 ? "A: parallel for per step" : "B: one parallel region, omp for per step");
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            if (v == 0) BEST_TIME(tp, up = heat_par_for(u0, c, d, n, T, r));
            else        BEST_TIME(tp, up = heat_par_region(u0, c, d, n, T, r));
            table_row(n, THREADS[k], ts, tp, close_doubles(us, up, n * n, 0.0));
        }
    }
    free(u0); free(a); free(b); free(c); free(d);
    return 0;
}

/* Notes:
 * - Updating in place (one array) would be a race AND a different algorithm:
 *   a point could read a neighbour that is already at time t+1.
 * - Double buffering: read u (time t), write u_new (time t+1), swap pointers
 *   (O(1), no copying). The swap must happen after ALL threads finished the
 *   step: after the join (A) or after the omp for barrier (B).
 * - In B the pointers are firstprivate: each thread swaps its own copy
 *   identically. Swapping SHARED pointers would need a single + barrier, or
 *   threads would swap them several times.
 * - B avoids T fork/joins (cheap with gcc's thread pool, so A and B are close
 *   for big N; B wins for small grids / many steps).
 * - Stability of the explicit scheme needs r = alpha*dt/dx^2 <= 0.25 in 2D.
 *   Memory-bound stencil (5 loads, 1 store per point): speedup flattens.
 */

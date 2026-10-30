/* P075: Dominant eigenvalue by power iteration
 * Problem: For a symmetric N x N matrix A (flattened row-major, A[(long)i*N + j])
 *   find the eigenvalue of largest magnitude by power iteration: start with a
 *   unit vector x and repeat y = A x, lambda = x . y (Rayleigh quotient, |x| = 1),
 *   x = y / |y|, until lambda changes by less than tol (relative). Parallelise
 *   with OpenMP and verify against the serial version and against a matrix
 *   whose eigenvalues are known.
 * Solution: each step has three parallel loops: (1) y = A x, parallel over rows
 *   (row i writes only y[i]); (2) x . y and y . y with reduction(+:xy, yy);
 *   (3) x = y / |y|, parallel over i. Between them there must be a barrier
 *   (all of y before the dot products, the full |y| before normalising), and
 *   the steps themselves are sequential.
 *   Version A: one "parallel for" per loop (3 fork/joins per step) - simplest.
 *   Version B: ONE parallel region around the whole iteration; the loops are
 *   "omp for" (implicit barriers) and a "single" does the scalar work (lambda,
 *   1/|y|, convergence flag, resetting the reduction variables). All threads
 *   read the shared flag "done" after a barrier, so they all leave the while
 *   loop in the same step. B saves the fork/join per loop, which only matters
 *   when N is small; for large N the O(N^2) matvec dominates and A = B.
 *   Test matrix: A = Q D Q with Q = I - 2uu^T (|u| = 1, a Householder
 *   reflection), so A is symmetric with eigenvalues exactly D = diag(d):
 *   d[0] = 8 (dominant), the others in [1, 5]; the dominant eigenvector is Q e0.
 * Source: lab extension (numerical linear algebra).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p075_power_iteration.c -o p075 -lm
 * Run:   ./p075 [N] [tol]    (defaults N = 1500, tol = 1e-12)
 */
#include "common.h"

#define MAX_ITERS 1000
#define LAMBDA1 8.0                  /* dominant eigenvalue of the test matrix */

static double row_dot(const double *a, const double *x, long n) {
    double s = 0.0;
    for (long j = 0; j < n; j++) s += a[j] * x[j];
    return s;
}

static void unit_start(double *x, long n) {
    for (long i = 0; i < n; i++) x[i] = 1.0 / sqrt((double)n);
}

static double power_serial(const double *A, double *x, double *y, long n, double tol, long *iters) {
    double lam = 0.0, lam_old;
    long it = 0;
    unit_start(x, n);
    do {
        lam_old = lam;
        for (long i = 0; i < n; i++) y[i] = row_dot(A + i * n, x, n);      /* y = A x */
        double xy = 0.0, yy = 0.0;
        for (long i = 0; i < n; i++) { xy += x[i] * y[i]; yy += y[i] * y[i]; }
        lam = xy;                                                           /* x.Ax, |x| = 1 */
        double inv = 1.0 / sqrt(yy);
        for (long i = 0; i < n; i++) x[i] = y[i] * inv;                     /* x = y / |y| */
        it++;
    } while (fabs(lam - lam_old) > tol * fabs(lam) && it < MAX_ITERS);
    *iters = it;
    return lam;
}

/* Version A: a parallel region for each of the three loops. */
static double power_par_loops(const double *A, double *x, double *y, long n, double tol, long *iters) {
    double lam = 0.0, lam_old;
    long it = 0;
    unit_start(x, n);
    do {
        lam_old = lam;
        #pragma omp parallel for
        for (long i = 0; i < n; i++) y[i] = row_dot(A + i * n, x, n);
        double xy = 0.0, yy = 0.0;
        #pragma omp parallel for reduction(+:xy, yy)
        for (long i = 0; i < n; i++) { xy += x[i] * y[i]; yy += y[i] * y[i]; }
        lam = xy;
        double inv = 1.0 / sqrt(yy);
        #pragma omp parallel for
        for (long i = 0; i < n; i++) x[i] = y[i] * inv;
        it++;
    } while (fabs(lam - lam_old) > tol * fabs(lam) && it < MAX_ITERS);
    *iters = it;
    return lam;
}

/* Version B: one parallel region; worksharing loops + single for the scalars. */
static double power_par_region(const double *A, double *x, double *y, long n, double tol, long *iters) {
    double lam = 0.0, xy = 0.0, yy = 0.0, inv = 0.0;     /* shared */
    long it = 0;
    int done = 0;
    unit_start(x, n);
    #pragma omp parallel
    {
        while (!done) {                       /* everyone reads done after a barrier */
            #pragma omp for
            for (long i = 0; i < n; i++) y[i] = row_dot(A + i * n, x, n);
            /* implicit barrier: y complete */
            #pragma omp for reduction(+:xy, yy)
            for (long i = 0; i < n; i++) { xy += x[i] * y[i]; yy += y[i] * y[i]; }
            /* implicit barrier: xy, yy complete */
            #pragma omp single
            {
                double lam_old = lam;
                lam = xy;
                inv = 1.0 / sqrt(yy);
                xy = yy = 0.0;                /* reduction targets must start at 0 next step */
                it++;
                done = !(fabs(lam - lam_old) > tol * fabs(lam)) || it >= MAX_ITERS;
            }   /* implicit barrier: everyone sees lam, inv, done */
            #pragma omp for
            for (long i = 0; i < n; i++) x[i] = y[i] * inv;
            /* implicit barrier: x complete before the next matvec reads it */
        }
    }
    *iters = it;
    return lam;
}

/* A = (I - 2uu^T) D (I - 2uu^T) with |u| = 1:
 *   A[i][j] = d[i]*(i==j) + u[i]*u[j]*(4s - 2d[i] - 2d[j]),  s = sum_k d[k]*u[k]^2.
 * Eigenvalues are exactly d[k]; v1 = e0 - 2u*u[0] is the dominant eigenvector. */
static void make_matrix(double *A, double *v1, long n) {
    double *d = xmalloc((size_t)n * sizeof(double));
    double *u = xmalloc((size_t)n * sizeof(double));
    fill_double(u, n, -1.0, 1.0, 75);
    double uu = 0.0, s = 0.0;
    for (long i = 0; i < n; i++) uu += u[i] * u[i];
    for (long i = 0; i < n; i++) u[i] /= sqrt(uu);
    d[0] = LAMBDA1;
    for (long k = 1; k < n; k++) d[k] = 1.0 + 4.0 * (double)(k - 1) / (double)(n > 2 ? n - 2 : 1);
    for (long k = 0; k < n; k++) s += d[k] * u[k] * u[k];
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            A[i * n + j] = (i == j ? d[i] : 0.0) + u[i] * u[j] * (4.0 * s - 2.0 * d[i] - 2.0 * d[j]);
    for (long i = 0; i < n; i++) v1[i] = (i == 0 ? 1.0 : 0.0) - 2.0 * u[i] * u[0];
    free(d); free(u);
}

/* sin of the angle between unit vectors x and v (0 = same direction up to sign). */
static double sin_angle(const double *x, const double *v, long n) {
    double c = 0.0;
    for (long i = 0; i < n; i++) c += x[i] * v[i];
    double s2 = 1.0 - c * c;
    return s2 > 0.0 ? sqrt(s2) : 0.0;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1500);
    double tol = arg_double(argc, argv, 2, 1e-12);
    if (n < 2) n = 2;

    /* Worked example: [2 1; 1 2] has eigenvalues 3 and 1, eigenvector (1,1)/sqrt(2). */
    double A2[4] = {2, 1, 1, 2}, x2[2], y2[2];
    long it2;
    double l2 = power_par_region(A2, x2, y2, 2, 1e-14, &it2);
    printf("P075: power iteration\n2x2 example [2 1; 1 2]: lambda = %.12f (exact 3), "
           "x = (%.6f, %.6f), %ld iterations  %s\n\n", l2, x2[0], x2[1], it2, ok_str(close1(l2, 3.0, 1e-12)));

    double *A  = xmalloc((size_t)n * n * sizeof(double));
    double *v1 = xmalloc((size_t)n * sizeof(double));
    double *xs = xmalloc((size_t)n * sizeof(double)), *ys = xmalloc((size_t)n * sizeof(double));
    double *xp = xmalloc((size_t)n * sizeof(double)), *yp = xmalloc((size_t)n * sizeof(double));
    make_matrix(A, v1, n);
    omp_warmup();

    double ts, tp, ls = 0, lp = 0;
    long its = 0, itp = 0;
    BEST_TIME(ts, ls = power_serial(A, xs, ys, n, tol, &its));
    double sin_s = sin_angle(xs, v1, n);
    printf("N = %ld, exact dominant eigenvalue %.1f (next largest 5.0)\n", n, LAMBDA1);
    printf("serial: lambda = %.12f after %ld iterations, |lambda - 8| = %.1e, "
           "sin(angle to exact eigenvector) = %.1e  %s\n",
           ls, its, fabs(ls - LAMBDA1), sin_s, ok_str(close1(ls, LAMBDA1, 1e-10) && sin_s < 1e-4));

    const char *name[2] = {"A: a parallel for per loop (3 fork/joins per step)",
                           "B: one parallel region, omp for + single"};
    for (int v = 0; v < 2; v++) {
        printf("\nVersion %s   (check: lambda vs serial and vs 8, eigenvector)\n", name[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            if (v == 0) BEST_TIME(tp, lp = power_par_loops(A, xp, yp, n, tol, &itp));
            else        BEST_TIME(tp, lp = power_par_region(A, xp, yp, n, tol, &itp));
            int ok = close1(ls, lp, 1e-10) && close1(lp, LAMBDA1, 1e-10) && sin_angle(xp, v1, n) < 1e-4;
            table_row(n, THREADS[k], ts, tp, ok);
        }
        printf("lambda = %.12f after %ld iterations\n", lp, itp);
    }
    free(A); free(v1); free(xs); free(ys); free(xp); free(yp);
    return 0;
}

/* Notes:
 * - Steps are sequential (x_{k+1} needs all of x_k); the parallelism is inside
 *   a step: the matvec (N^2, rows independent) and two O(N) loops.
 * - The dot products use reduction(+:...); a different addition order changes
 *   the last bits, so compare lambda with a tolerance (iteration counts can
 *   differ by one in a borderline case).
 * - In version B the reduction variables are shared and the reduction ADDS
 *   to their current value, so a single resets them to 0 for the next step.
 *   The exit flag is written in a single (barrier after it), so every thread
 *   sees the same value and leaves the loop together (no deadlock at a barrier).
 * - Convergence rate = |lambda2 / lambda1| per step for x (5/8 here); the
 *   Rayleigh quotient converges twice as fast for symmetric A.
 * - Normalising every step prevents overflow/underflow of x.
 */

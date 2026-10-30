/* P074: Jacobi iteration for a linear system Ax = b
 * Problem: Solve Ax = b, A a dense N x N strictly diagonally dominant matrix
 *   (flattened row-major, A[(long)i*N + j]), with the Jacobi method: start from
 *   x = 0 and repeat x_new[i] = (b[i] - sum_{j != i} a_ij * x[j]) / a_ii until
 *   max_i |x_new[i] - x[i]| < tol (or a maximum number of iterations).
 *   Parallelise with OpenMP, verify against the serial solver (same iteration
 *   count, same x) and check the residual b - Ax.
 * Solution: in one iteration every x_new[i] reads only the OLD vector x, so the
 *   N rows are independent and "parallel for" over i is race-free, provided x
 *   and x_new are different arrays (double buffering: swap the two pointers
 *   after each iteration). The convergence test needs the largest change over
 *   all i: reduction(max:diff). Max is exact in any order and every x_new[i] is
 *   computed by the same code as in the serial solver, so the parallel run takes
 *   exactly the same number of iterations and gives the same x.
 *   (Strict diagonal dominance, |a_ii| > sum_{j != i} |a_ij|, guarantees that
 *   Jacobi converges.)
 * Source: lab extension (iterative solvers).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p074_jacobi_linear_solver.c -o p074 -lm
 * Run:   ./p074 [N] [tol]    (defaults N = 2000, tol = 1e-10)
 */
#include "common.h"

#define MAX_ITERS 1000

/* One Jacobi row: (b[i] - sum_{j != i} a_ij x[j]) / a_ii, using the OLD x. */
static double jacobi_row(const double *A, const double *b, const double *x, long n, long i) {
    const double *a = A + i * n;
    double s = b[i];
    for (long j = 0; j < i; j++)     s -= a[j] * x[j];
    for (long j = i + 1; j < n; j++) s -= a[j] * x[j];
    return s / a[i];
}

/* Returns the number of iterations; the solution is left in x. */
static long jacobi_serial(const double *A, const double *b, double *x, long n, double tol) {
    double *xo = xmalloc((size_t)n * sizeof(double));     /* old x */
    double *xn = xmalloc((size_t)n * sizeof(double));     /* new x */
    for (long i = 0; i < n; i++) xo[i] = 0.0;
    long it = 0;
    double diff;
    do {
        diff = 0.0;
        for (long i = 0; i < n; i++) {
            xn[i] = jacobi_row(A, b, xo, n, i);
            double d = fabs(xn[i] - xo[i]);
            if (d > diff) diff = d;
        }
        double *t = xo; xo = xn; xn = t;                  /* new becomes old */
        it++;
    } while (diff >= tol && it < MAX_ITERS);
    memcpy(x, xo, (size_t)n * sizeof(double));
    free(xo); free(xn);
    return it;
}

static long jacobi_parallel(const double *A, const double *b, double *x, long n, double tol) {
    double *xo = xmalloc((size_t)n * sizeof(double));
    double *xn = xmalloc((size_t)n * sizeof(double));
    #pragma omp parallel for
    for (long i = 0; i < n; i++) xo[i] = 0.0;
    long it = 0;
    double diff;
    do {
        diff = 0.0;
        #pragma omp parallel for reduction(max:diff)
        for (long i = 0; i < n; i++) {
            xn[i] = jacobi_row(A, b, xo, n, i);           /* reads xo only, writes xn[i] only */
            double d = fabs(xn[i] - xo[i]);
            if (d > diff) diff = d;
        }                                                 /* implicit barrier: xn complete */
        double *t = xo; xo = xn; xn = t;                  /* swap by the master thread */
        it++;
    } while (diff >= tol && it < MAX_ITERS);
    memcpy(x, xo, (size_t)n * sizeof(double));
    free(xo); free(xn);
    return it;
}

/* max_i |b_i - (Ax)_i| / |a_ii|  (residual of the diagonally scaled system). */
static double scaled_residual(const double *A, const double *b, const double *x, long n) {
    double r = 0.0;
    #pragma omp parallel for reduction(max:r)
    for (long i = 0; i < n; i++) {
        double s = b[i];
        for (long j = 0; j < n; j++) s -= A[i * n + j] * x[j];
        double e = fabs(s) / fabs(A[i * n + i]);
        if (e > r) r = e;
    }
    return r;
}

/* Off-diagonal entries uniform in [0, 1), a_ii = 1 + 2 * sum_{j != i} |a_ij|:
 * strictly dominant, and each iteration shrinks the error by about 1/2. */
static void make_system(double *A, double *b, long n) {
    fill_double(A, n * n, 0.0, 1.0, 74);
    fill_double(b, n, -1.0, 1.0, 75);
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) if (j != i) s += fabs(A[i * n + j]);
        A[i * n + i] = 1.0 + 2.0 * s;
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 2000);
    double tol = arg_double(argc, argv, 2, 1e-10);
    if (n < 1) n = 1;

    /* Worked example: [4 -1 0; -1 4 -1; 0 -1 4] x = [2 4 10] has solution x = [1 2 3]. */
    double A3[9] = {4, -1, 0,   -1, 4, -1,   0, -1, 4}, b3[3] = {2, 4, 10}, x3[3];
    const double want3[3] = {1, 2, 3};
    long it3 = jacobi_parallel(A3, b3, x3, 3, 1e-12);
    printf("P074: Jacobi iteration for Ax = b\n");
    printf("3x3 example: x = [%.6f %.6f %.6f] after %ld iterations (exact [1 2 3])  %s\n\n",
           x3[0], x3[1], x3[2], it3, ok_str(close_doubles(x3, want3, 3, 1e-10)));

    double *A  = xmalloc((size_t)n * n * sizeof(double));
    double *b  = xmalloc((size_t)n * sizeof(double));
    double *xs = xmalloc((size_t)n * sizeof(double));
    double *xp = xmalloc((size_t)n * sizeof(double));
    make_system(A, b, n);
    omp_warmup();

    double ts, tp;
    long its = 0, itp = 0;
    BEST_TIME(ts, its = jacobi_serial(A, b, xs, n, tol));
    double res = scaled_residual(A, b, xs, n);
    printf("N = %ld, tol = %.0e: serial converged in %ld iterations, "
           "max |b - Ax| / |a_ii| = %.2e  %s\n",
           n, tol, its, res, ok_str(its < MAX_ITERS && res < 100 * tol));
    printf("check = same iteration count and the same x as the serial solver\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, itp = jacobi_parallel(A, b, xp, n, tol));
        table_row(n, THREADS[k], ts, tp, itp == its && close_doubles(xs, xp, n, 1e-12));
    }
    printf("parallel: %ld iterations, max |b - Ax| / |a_ii| = %.2e\n", itp, scaled_residual(A, b, xp, n));

    free(A); free(b); free(xs); free(xp);
    return 0;
}

/* Notes:
 * - Jacobi vs Gauss-Seidel: Jacobi uses only old values, so all rows are
 *   independent (easy to parallelise) but needs two arrays. Gauss-Seidel uses
 *   new values as soon as they exist: fewer iterations but a sequential
 *   dependence (needs red-black or wavefront ordering, see P077/P078).
 * - Writing into x in place in the parallel loop would be a race: another
 *   thread could read x[j] before or after it was updated (results would
 *   depend on timing).
 * - reduction(max:diff) (OpenMP 3.1) gives each thread a private max that is
 *   combined at the end; max is order-independent, so iteration counts match.
 * - The iteration loop is sequential (iteration k+1 needs all of iteration k).
 *   Each iteration costs N^2 multiply-adds; the dense row reads make it
 *   memory-bound, so speedup flattens when bandwidth is saturated.
 * - Convergence is guaranteed for strictly diagonally dominant A; cap the
 *   iterations anyway (MAX_ITERS) in case it is not.
 */

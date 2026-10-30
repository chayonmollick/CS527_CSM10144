/* CS527 Computer Systems — OpenMP practice
 * Q33: Simulate 2D steady-state heat diffusion on an N x N plate with fixed
 *      boundary temperatures using Jacobi iteration: every interior point
 *      becomes the average of its 4 neighbours; repeat until the largest
 *      change max|new - old| < tol (or max_iters). Parallelise with OpenMP.
 *  - serial: two buffers, swap pointers after each sweep
 *  - parallel A: "parallel for reduction(max:diff)" for every sweep (fork/join per iteration)
 *  - parallel B: ONE parallel region around the whole iteration loop; "omp for" per sweep,
 *    a "single" swaps the buffers and decides whether to stop
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define TOL       1e-2    /* stop when max |new - old| < TOL ...        */
#define MAX_ITERS 5000    /* ... or after this many sweeps              */
#define HOT       100.0   /* top edge temperature, other edges are 0    */
#define INIT      25.0    /* interior start = mean of the 4 edges (see notes) */

/* top row = HOT, other edges 0, interior INIT; both buffers get the boundary
 * because after a swap the other buffer becomes "current". */
static void init_grid(double *g, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) {
            int edge = (i == 0 || i == n - 1 || j == 0 || j == n - 1);
            g[i*n + j] = (i == 0) ? HOT : (edge ? 0.0 : INIT);
        }
}

/* Every function returns the iteration count; on return *cur = newest grid. */
static long jacobi_serial(double **cur, double **nxt, long n, double *diff_out) {
    double *u = *cur, *v = *nxt, diff;
    long it = 0;
    do {
        diff = 0.0;
        for (long i = 1; i < n - 1; i++)
            for (long j = 1; j < n - 1; j++) {
                v[i*n + j] = 0.25 * (u[(i-1)*n + j] + u[(i+1)*n + j] + u[i*n + j-1] + u[i*n + j+1]);
                double d = fabs(v[i*n + j] - u[i*n + j]);
                if (d > diff) diff = d;
            }
        double *t = u; u = v; v = t;          /* new grid becomes current */
        it++;
    } while (diff >= TOL && it < MAX_ITERS);
    *cur = u; *nxt = v; *diff_out = diff;
    return it;
}

/* A: a parallel for (with its fork/join + barrier) for every sweep. */
static long jacobi_parfor(double **cur, double **nxt, long n, double *diff_out) {
    double *u = *cur, *v = *nxt, diff;
    long it = 0;
    do {
        diff = 0.0;
        #pragma omp parallel for reduction(max:diff)
        for (long i = 1; i < n - 1; i++)
            for (long j = 1; j < n - 1; j++) {
                v[i*n + j] = 0.25 * (u[(i-1)*n + j] + u[(i+1)*n + j] + u[i*n + j-1] + u[i*n + j+1]);
                double d = fabs(v[i*n + j] - u[i*n + j]);
                if (d > diff) diff = d;
            }
        double *t = u; u = v; v = t;          /* serial part: done by master */
        it++;
    } while (diff >= TOL && it < MAX_ITERS);
    *cur = u; *nxt = v; *diff_out = diff;
    return it;
}

/* B: threads are created once; every thread runs the while loop. u, v, diff,
 * it, done are shared (declared outside the region). */
static long jacobi_region(double **cur, double **nxt, long n, double *diff_out) {
    double *u = *cur, *v = *nxt, diff = 0.0, last = 0.0;
    long it = 0;
    int done = 0;
    #pragma omp parallel
    {
        while (!done) {
            #pragma omp for reduction(max:diff)
            for (long i = 1; i < n - 1; i++)
                for (long j = 1; j < n - 1; j++) {
                    v[i*n + j] = 0.25 * (u[(i-1)*n + j] + u[(i+1)*n + j] + u[i*n + j-1] + u[i*n + j+1]);
                    double d = fabs(v[i*n + j] - u[i*n + j]);
                    if (d > diff) diff = d;
                }
            /* implicit barrier: sweep complete, diff is the global max */
            #pragma omp single
            {
                double *t = u; u = v; v = t;
                it++;
                last = diff;
                done = (diff < TOL || it >= MAX_ITERS);
                diff = 0.0;                    /* reset for the next sweep */
            }
            /* implicit barrier: all threads see the new u, v, done */
        }
    }
    *cur = u; *nxt = v; *diff_out = last;
    return it;
}

static double max_abs_diff(const double *a, const double *b, long len) {
    double m = 0.0;
    for (long i = 0; i < len; i++) { double d = fabs(a[i] - b[i]); if (d > m) m = d; }
    return m;
}

typedef long (*solver_t)(double **, double **, long, double *);

int main(int argc, char **argv) {
    long sizes[2] = {128, 600}; int nsizes = 2;     /* avoid power-of-2 N, see notes */
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    const int threads[] = {1, 2, 4, 8};
    const solver_t solver[2] = {jacobi_parfor, jacobi_region};
    const char *name[2] = {"A: parallel for per sweep", "B: one parallel region"};
    printf("Q33: Jacobi heat diffusion (top edge %.0f, others 0, interior starts at %.0f),\n"
           "     tol %g, max %d iters\n", HOT, INIT, TOL, MAX_ITERS);

    for (int si = 0; si < nsizes; si++) {
        long n = sizes[si], n2 = n * n;
        if (n < 3) { fprintf(stderr, "N must be >= 3\n"); return 1; }
        double *s1 = malloc((size_t)n2 * sizeof(double)), *s2 = malloc((size_t)n2 * sizeof(double));
        double *p1 = malloc((size_t)n2 * sizeof(double)), *p2 = malloc((size_t)n2 * sizeof(double));
        if (!s1 || !s2 || !p1 || !p2) { fprintf(stderr, "alloc failed\n"); return 1; }

        init_grid(s1, n); init_grid(s2, n);
        double *su = s1, *sv = s2, sdiff;
        double t0 = omp_get_wtime();
        long sit = jacobi_serial(&su, &sv, n, &sdiff);
        double ts = omp_get_wtime() - t0;
        printf("\nN = %ld: serial %ld iterations (%s), last diff %.3e, centre T = %.4f, %.4f s\n",
               n, sit, sit < MAX_ITERS ? "converged" : "hit max_iters", sdiff,
               su[(n/2)*n + n/2], ts);

        for (int m = 0; m < 2; m++) {
            printf("[%s]\n%8s %12s %12s %9s %11s %7s %7s\n", name[m], "threads",
                   "serial(s)", "parallel(s)", "speedup", "efficiency", "iters", "check");
            for (int ti = 0; ti < 4; ti++) {
                omp_set_num_threads(threads[ti]);
                init_grid(p1, n); init_grid(p2, n);
                double *pu = p1, *pv = p2, pdiff;
                double p0 = omp_get_wtime();
                long pit = solver[m](&pu, &pv, n, &pdiff);
                double tp = omp_get_wtime() - p0;
                int ok = (pit == sit) && max_abs_diff(pu, su, n2) < 1e-12;
                double sp = tp > 0 ? ts / tp : 0.0;
                printf("%8d %12.6f %12.6f %9.2f %11.2f %7ld %7s\n", threads[ti], ts, tp,
                       sp, sp / threads[ti], pit, ok ? "OK" : "WRONG");
            }
        }
        free(s1); free(s2); free(p1); free(p2);
    }
    return 0;
}

/* Viva notes:
 * - Two buffers are essential: Jacobi reads only the OLD grid, so all points
 *   of a sweep are independent. Updating in place would read a mix of old and
 *   new values (race + a different method, Gauss-Seidel).
 * - Convergence test: diff = max over points, a max-reduction
 *   (reduction(max:diff), OpenMP 3.1). The iteration loop itself is
 *   sequential: sweep k+1 needs all of sweep k (barrier between sweeps).
 * - A vs B: A forks/joins the team every sweep (libgomp keeps a thread pool,
 *   so that is a wake-up + a join barrier); B creates the team once but still
 *   needs 2 barriers per sweep (end of "omp for", end of "single"), so with
 *   gcc they perform about the same. Either way, small grids (N=128) get
 *   SLOWER with more threads: ~10 us of work per sweep vs barrier cost.
 * - In B the stop decision must be the SAME for every thread: it is computed
 *   once inside single into a shared flag, and diff is reset there too. If
 *   every thread tested diff itself while another thread reset it, some
 *   threads could leave the loop and the rest would deadlock at a barrier.
 * - Max is exact (no rounding), so parallel gives bit-identical grids and the
 *   same iteration count as serial; a SUM-based residual could differ slightly.
 * - Gotchas seen while testing: N=512 (rows of exactly 4096 bytes) ran ~5x
 *   slower than N=500/600 -- power-of-2 strides cause cache-set conflicts.
 *   Starting the interior at 0 instead of 25 makes far cells hold tiny
 *   (subnormal) values on big grids, which are very slow on x86 CPUs.
 */

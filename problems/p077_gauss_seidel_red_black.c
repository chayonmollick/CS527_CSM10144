/* P077: Equation solver - red-black Gauss-Seidel vs Jacobi
 * Problem: (Lecture slides, equation solver kernel.) An (N+2) x (N+2) grid A
 *   (flattened row-major, A[(long)i*(N+2) + j]) has fixed boundary values; the
 *   N x N interior points are updated in place,
 *       A[i][j] = 0.2 * (A[i][j] + A[i-1][j] + A[i+1][j] + A[i][j-1] + A[i][j+1]),
 *   sweep after sweep, until the average change per point,
 *   diff / (N*N) with diff = sum |A_new - A_old|, is below tol.
 *   The in-place sequential sweep uses the NEW values of the up and left
 *   neighbours, so its iterations are not independent. Parallelise it with
 *   red-black ordering, compare with Jacobi (double buffer), verify that the
 *   parallel red-black run equals the serial red-black run exactly, and show
 *   that all methods converge.
 * Solution: colour point (i,j) red if (i+j) is even and black if odd. The four
 *   neighbours of a red point are black and vice versa. A sweep updates ALL red
 *   points (in parallel: they only read black points, which nobody writes in
 *   this half-sweep), then, after a barrier, all black points (they read the
 *   new red values). Each colour half-sweep is "omp for reduction(+:diff)" over
 *   rows inside one parallel region; the implicit barrier at the end of the red
 *   loop is exactly the barrier we need. Every point gets the same value in any
 *   execution order, so parallel red-black == serial red-black bit for bit.
 *   Red-black is a different ORDER from the row-by-row sweep, so its values
 *   differ from the sequential Gauss-Seidel values, but it solves the same
 *   equations at the same rate (equally valid within tol).
 *   Jacobi reads only the old grid A and writes a second grid B (then swaps):
 *   trivially parallel, but it needs more sweeps (about 1.5-2x here).
 *   Part 1 runs every method to convergence on a small grid (Nc = 100,
 *   tol = 1e-5). Part 2 times a big grid (N = 1000) for a fixed number of
 *   sweeps (tol = 0, so the sweep cap stops it): converging a 1000 x 1000 grid
 *   takes about 100x more sweeps (sweeps grow like N^2).
 *   Boundary: top row 100, other edges 0; interior starts at 25 (not 0, to
 *   avoid slow subnormal numbers).
 * Source: lecture slides (equation solver); lab extension.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p077_gauss_seidel_red_black.c -o p077 -lm
 * Run:   ./p077 [N] [sweeps] [Nc] [tol]   (defaults 1000, 100, 100, 1e-5)
 */
#include "common.h"

#define MAX_SWEEPS 50000

static void init_grid(double *A, long n) {
    long w = n + 2;
    for (long i = 0; i < w; i++)
        for (long j = 0; j < w; j++)
            A[i * w + j] = (i == 0) ? 100.0 : (i == w - 1 || j == 0 || j == w - 1) ? 0.0 : 25.0;
}

/* All solvers: sweep until diff/(n*n) < tol or max_sweeps; return the sweeps done. */

/* Sequential Gauss-Seidel, the order in the slides (row by row, in place). */
static long gs_serial(double *A, long n, double tol, long max_sweeps) {
    long w = n + 2, it = 0;
    double diff;
    do {
        diff = 0.0;
        for (long i = 1; i <= n; i++)
            for (long j = 1; j <= n; j++) {
                double old = A[i * w + j];
                A[i * w + j] = 0.2 * (old + A[(i - 1) * w + j] + A[(i + 1) * w + j]
                                          + A[i * w + j - 1] + A[i * w + j + 1]);
                diff += fabs(A[i * w + j] - old);
            }
        it++;
    } while (diff / ((double)n * n) >= tol && it < max_sweeps);
    return it;
}

/* Update the points of one colour in row i (colour 0 = red: (i+j) even,
 * colour 1 = black: (i+j) odd). Returns the sum of |change|. */
static double sweep_row_colour(double *A, long n, long i, int colour) {
    long w = n + 2;
    double diff = 0.0;
    for (long j = 1 + ((i + 1 + colour) & 1); j <= n; j += 2) {   /* first j of that colour */
        double old = A[i * w + j];
        A[i * w + j] = 0.2 * (old + A[(i - 1) * w + j] + A[(i + 1) * w + j]
                                  + A[i * w + j - 1] + A[i * w + j + 1]);
        diff += fabs(A[i * w + j] - old);
    }
    return diff;
}

static long rb_serial(double *A, long n, double tol, long max_sweeps) {
    long it = 0;
    double diff;
    do {
        diff = 0.0;
        for (long i = 1; i <= n; i++) diff += sweep_row_colour(A, n, i, 0);   /* red */
        for (long i = 1; i <= n; i++) diff += sweep_row_colour(A, n, i, 1);   /* black */
        it++;
    } while (diff / ((double)n * n) >= tol && it < max_sweeps);
    return it;
}

static long rb_parallel(double *A, long n, double tol, long max_sweeps) {
    long it = 0;
    double diff;
    do {
        diff = 0.0;
        #pragma omp parallel
        {
            #pragma omp for reduction(+:diff)
            for (long i = 1; i <= n; i++) diff += sweep_row_colour(A, n, i, 0);   /* red */
            /* implicit barrier: every red point is new before any black point starts */
            #pragma omp for reduction(+:diff)
            for (long i = 1; i <= n; i++) diff += sweep_row_colour(A, n, i, 1);   /* black */
        }
        it++;
    } while (diff / ((double)n * n) >= tol && it < max_sweeps);
    return it;
}

/* Jacobi: B = update(A) from OLD values only, then swap. *res = final grid. */
static long jacobi_serial(double *A, double *B, long n, double tol, long max_sweeps, double **res) {
    long w = n + 2, it = 0;
    double diff;
    do {
        diff = 0.0;
        for (long i = 1; i <= n; i++)
            for (long j = 1; j <= n; j++) {
                B[i * w + j] = 0.2 * (A[i * w + j] + A[(i - 1) * w + j] + A[(i + 1) * w + j]
                                      + A[i * w + j - 1] + A[i * w + j + 1]);
                diff += fabs(B[i * w + j] - A[i * w + j]);
            }
        double *t = A; A = B; B = t;
        it++;
    } while (diff / ((double)n * n) >= tol && it < max_sweeps);
    *res = A;
    return it;
}

static long jacobi_parallel(double *A, double *B, long n, double tol, long max_sweeps, double **res) {
    long w = n + 2, it = 0;
    double diff;
    do {
        diff = 0.0;
        #pragma omp parallel for reduction(+:diff)
        for (long i = 1; i <= n; i++)
            for (long j = 1; j <= n; j++) {
                B[i * w + j] = 0.2 * (A[i * w + j] + A[(i - 1) * w + j] + A[(i + 1) * w + j]
                                      + A[i * w + j - 1] + A[i * w + j + 1]);
                diff += fabs(B[i * w + j] - A[i * w + j]);
            }
        double *t = A; A = B; B = t;          /* after the implicit barrier */
        it++;
    } while (diff / ((double)n * n) >= tol && it < max_sweeps);
    *res = A;
    return it;
}

static double max_abs_diff(const double *x, const double *y, long m) {
    double d = 0.0;
    for (long k = 0; k < m; k++) if (fabs(x[k] - y[k]) > d) d = fabs(x[k] - y[k]);
    return d;
}

/* Grids for one problem size: G = sequential GS, Rs/Rp = red-black serial/parallel,
 * J1,J2 / K1,K2 = Jacobi serial/parallel buffers. */
typedef struct { double *G, *Rs, *Rp, *J1, *J2, *K1, *K2; } grids_t;

static grids_t alloc_grids(long n) {
    size_t bytes = (size_t)(n + 2) * (size_t)(n + 2) * sizeof(double);
    grids_t g = {xmalloc(bytes), xmalloc(bytes), xmalloc(bytes), xmalloc(bytes),
                 xmalloc(bytes), xmalloc(bytes), xmalloc(bytes)};
    return g;
}
static void free_grids(grids_t *g) {
    free(g->G); free(g->Rs); free(g->Rp); free(g->J1); free(g->J2); free(g->K1); free(g->K2);
}

int main(int argc, char *argv[]) {
    long n      = arg_long(argc, argv, 1, 1000);     /* timing grid */
    long sweeps = arg_long(argc, argv, 2, 100);      /* sweeps timed */
    long nc     = arg_long(argc, argv, 3, 100);      /* convergence grid */
    double tol  = arg_double(argc, argv, 4, 1e-5);
    if (n < 1) n = 1;
    if (nc < 1) nc = 1;
    if (sweeps < 1) sweeps = 1;

    printf("P077: equation solver, red-black Gauss-Seidel vs Jacobi\n");
    printf("colouring of a 6 x 6 interior (R = (i+j) even, updated first; B = odd):\n");
    for (long i = 1; i <= 6; i++) {
        printf("   ");
        for (long j = 1; j <= 6; j++) printf(" %c", (i + j) % 2 == 0 ? 'R' : 'B');
        printf("\n");
    }
    omp_warmup();

    /* ---- Part 1: run to convergence on a small grid ---- */
    long cc = (nc + 2) * (nc + 2);
    grids_t c = alloc_grids(nc);
    double *Js = NULL, *Jp = NULL;
    init_grid(c.G, nc);  init_grid(c.Rs, nc);  init_grid(c.Rp, nc);
    init_grid(c.J1, nc); init_grid(c.J2, nc);  init_grid(c.K1, nc); init_grid(c.K2, nc);
    long it_gs = gs_serial(c.G, nc, tol, MAX_SWEEPS);
    long it_rb = rb_serial(c.Rs, nc, tol, MAX_SWEEPS);
    long it_rp = rb_parallel(c.Rp, nc, tol, MAX_SWEEPS);
    long it_js = jacobi_serial(c.J1, c.J2, nc, tol, MAX_SWEEPS, &Js);
    long it_jp = jacobi_parallel(c.K1, c.K2, nc, tol, MAX_SWEEPS, &Jp);
    printf("\nPart 1: run to convergence, Nc = %ld, tol = %.0e (average change per point), "
           "%d threads\n", nc, tol, omp_get_max_threads());
    printf("  sequential Gauss-Seidel : %6ld sweeps\n", it_gs);
    printf("  red-black serial        : %6ld sweeps, max |RB - GS|     = %.2e\n",
           it_rb, max_abs_diff(c.Rs, c.G, cc));
    printf("  red-black parallel      : %6ld sweeps, equals RB serial exactly: %s\n",
           it_rp, ok_str(it_rp == it_rb && close_doubles(c.Rs, c.Rp, cc, 0.0)));
    printf("  Jacobi serial           : %6ld sweeps, max |Jacobi - GS| = %.2e\n",
           it_js, max_abs_diff(Js, c.G, cc));
    printf("  Jacobi parallel         : %6ld sweeps, equals Jacobi serial exactly: %s\n",
           it_jp, ok_str(it_jp == it_js && close_doubles(Js, Jp, cc, 0.0)));
    printf("  all converged (fewer than %d sweeps): %s\n", MAX_SWEEPS,
           ok_str(it_gs < MAX_SWEEPS && it_rb < MAX_SWEEPS && it_js < MAX_SWEEPS));
    free_grids(&c);

    /* ---- Part 2: timing, fixed number of sweeps on a big grid ---- */
    long cells = (n + 2) * (n + 2), it_s = 0, it_p = 0;
    grids_t g = alloc_grids(n);
    double ts, tp;
    printf("\nPart 2: timing, N = %ld, %ld sweeps each (tol = 0, so the sweep cap stops it)\n", n, sweeps);
    BEST_TIME(ts, (init_grid(g.Rs, n), it_s = rb_serial(g.Rs, n, 0.0, sweeps)));
    printf("Red-black Gauss-Seidel (check: parallel grid == serial red-black grid exactly)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, (init_grid(g.Rp, n), it_p = rb_parallel(g.Rp, n, 0.0, sweeps)));
        table_row(n, THREADS[k], ts, tp, it_p == it_s && close_doubles(g.Rs, g.Rp, cells, 0.0));
    }
    BEST_TIME(ts, (init_grid(g.J1, n), init_grid(g.J2, n),
                   it_s = jacobi_serial(g.J1, g.J2, n, 0.0, sweeps, &Js)));
    printf("\nJacobi, double buffer (check: parallel grid == serial Jacobi grid exactly)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, (init_grid(g.K1, n), init_grid(g.K2, n),
                       it_p = jacobi_parallel(g.K1, g.K2, n, 0.0, sweeps, &Jp)));
        table_row(n, THREADS[k], ts, tp, it_p == it_s && close_doubles(Js, Jp, cells, 0.0));
    }
    free_grids(&g);
    return 0;
}

/* Notes:
 * - Why plain "parallel for" on the in-place sweep is wrong: A[i][j] needs the
 *   NEW A[i-1][j] and A[i][j-1]; another thread may not have computed them
 *   yet (or may overwrite a value we still need) -> a data race, and the
 *   result depends on timing.
 * - Red-black: red points depend only on black ones and vice versa, so each
 *   colour is fully parallel; 2 barriers per sweep. Same answer in any order
 *   -> deterministic, and parallel == serial red-black exactly.
 * - diff is a sum -> reduction(+:diff); its rounding depends on the order, so
 *   in a borderline case the stopping sweep could differ (practically never).
 * - Jacobi: no dependence within a sweep (double buffer), but more sweeps than
 *   Gauss-Seidel. Wavefront (P078) keeps the exact sequential GS order.
 * - The slides' test "average change < tol" measures the change per sweep,
 *   not the error: on a big grid with a loose tol it stops far from the
 *   solution (and a slow method like Jacobi stops even earlier).
 */

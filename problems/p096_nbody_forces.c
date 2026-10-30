/* P096: N-body forces (full loop vs Newton's third law)
 * Problem: N particles in 2D have positions (x[i], y[i]) and masses m[i]. The
 *   gravitational force on particle i (with softening eps, G = 1) is
 *       F_i = sum over j != i of m_i m_j (r_j - r_i) / (|r_j - r_i|^2 + eps^2)^(3/2).
 *   Compute all forces with OpenMP in two ways: (a) the full O(N^2) double loop
 *   without Newton's third law; (b) using symmetry F_ij = -F_ji: loop over j > i
 *   only, add the pair force to particle i and subtract it from particle j.
 *   Make (b) race-free in two ways: (b1) atomics, (b2) per-thread force arrays
 *   reduced at the end. Verify all versions against the serial full loop, and
 *   compare the times. Why does (b) do half the work but need synchronisation?
 * Solution: (a) parallel for over i: iteration i writes only fx[i], fy[i], so
 *   there is no race (each thread owns its rows); schedule(static) since every
 *   row costs N-1 pair forces. (b) iteration i also writes fx[j], fy[j] for every
 *   j > i, and another thread (another i) may be updating the same j: a race.
 *   (b1) each fx[j] -= ... is #pragma omp atomic, and the row total for i is added
 *   with one atomic at the end (row i may also be another thread's j).
 *   (b2) thread t accumulates into its own arrays fxt[t][.], fyt[t][.] (no
 *   sharing), then a second parallel for adds the T arrays for each particle.
 *   The triangular loop (row i has N-1-i pairs) is unbalanced: rows get
 *   cheaper as i grows, so (b) uses schedule(dynamic, 16) (see Notes on guided).
 * Source: textbook ch. 30 (n-body force computation).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p096_nbody_forces.c -o p096 -lm
 * Run:   ./p096 [N]          (default N = 4000)
 */
#include "common.h"

#define EPS2 1e-4       /* softening eps^2 (eps = 0.01): no blow-up when particles are close */

/* Force of particle j on particle i, multiplied out: fx += s*dx, fy += s*dy. */
static inline double pair_scale(double dx, double dy, double mi, double mj) {
    double r2 = dx * dx + dy * dy + EPS2;
    return mi * mj / (r2 * sqrt(r2));
}

static void forces_full_serial(long n, const double *x, const double *y, const double *m,
                               double *fx, double *fy) {
    for (long i = 0; i < n; i++) {
        double sx = 0.0, sy = 0.0;
        for (long j = 0; j < n; j++) {
            if (j == i) continue;
            double dx = x[j] - x[i], dy = y[j] - y[i], s = pair_scale(dx, dy, m[i], m[j]);
            sx += s * dx; sy += s * dy;
        }
        fx[i] = sx; fy[i] = sy;
    }
}

static void forces_sym_serial(long n, const double *x, const double *y, const double *m,
                              double *fx, double *fy) {
    for (long i = 0; i < n; i++) { fx[i] = 0.0; fy[i] = 0.0; }
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++) {
            double dx = x[j] - x[i], dy = y[j] - y[i], s = pair_scale(dx, dy, m[i], m[j]);
            fx[i] += s * dx; fy[i] += s * dy;      /* force of j on i */
            fx[j] -= s * dx; fy[j] -= s * dy;      /* equal and opposite on j */
        }
}

/* (a) full loop: row i is computed and written by one thread only. */
static void forces_full_parallel(long n, const double *x, const double *y, const double *m,
                                 double *fx, double *fy) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) {
        double sx = 0.0, sy = 0.0;
        for (long j = 0; j < n; j++) {
            if (j == i) continue;
            double dx = x[j] - x[i], dy = y[j] - y[i], s = pair_scale(dx, dy, m[i], m[j]);
            sx += s * dx; sy += s * dy;
        }
        fx[i] = sx; fy[i] = sy;
    }
}

/* (b1) symmetric loop, atomic updates of the shared force arrays. */
static void forces_sym_atomic(long n, const double *x, const double *y, const double *m,
                              double *fx, double *fy) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) { fx[i] = 0.0; fy[i] = 0.0; }
    #pragma omp parallel for schedule(dynamic, 16)
    for (long i = 0; i < n; i++) {
        double sx = 0.0, sy = 0.0;                 /* row i in registers */
        for (long j = i + 1; j < n; j++) {
            double dx = x[j] - x[i], dy = y[j] - y[i], s = pair_scale(dx, dy, m[i], m[j]);
            sx += s * dx; sy += s * dy;
            #pragma omp atomic
            fx[j] -= s * dx;
            #pragma omp atomic
            fy[j] -= s * dy;
        }
        #pragma omp atomic
        fx[i] += sx;                               /* another thread may be adding to fx[i] */
        #pragma omp atomic
        fy[i] += sy;
    }
}

/* (b2) symmetric loop, every thread has private force arrays; sum them after. */
static void forces_sym_private(long n, const double *x, const double *y, const double *m,
                               double *fx, double *fy) {
    int T = omp_get_max_threads();
    double *fxt = xmalloc((size_t)T * (size_t)n * sizeof(double));   /* T arrays of n */
    double *fyt = xmalloc((size_t)T * (size_t)n * sizeof(double));
    #pragma omp parallel
    {
        int t = omp_get_thread_num(), nt = omp_get_num_threads();
        double *myx = fxt + (size_t)t * n, *myy = fyt + (size_t)t * n;
        for (long i = 0; i < n; i++) { myx[i] = 0.0; myy[i] = 0.0; }
        #pragma omp for schedule(dynamic, 16)
        for (long i = 0; i < n; i++)
            for (long j = i + 1; j < n; j++) {
                double dx = x[j] - x[i], dy = y[j] - y[i], s = pair_scale(dx, dy, m[i], m[j]);
                myx[i] += s * dx; myy[i] += s * dy;
                myx[j] -= s * dx; myy[j] -= s * dy;
            }
        /* implicit barrier: every thread's arrays are complete */
        #pragma omp for
        for (long i = 0; i < n; i++) {
            double sx = 0.0, sy = 0.0;
            for (int u = 0; u < nt; u++) {             /* add the nt private arrays */
                sx += fxt[(size_t)u * n + i];
                sy += fyt[(size_t)u * n + i];
            }
            fx[i] = sx; fy[i] = sy;
        }
    }
    free(fxt); free(fyt);
}

/* Forces summed in a different order differ in the last bits, and single forces
 * can nearly cancel, so compare the largest difference with the largest force. */
static int forces_close(long n, const double *ax, const double *ay,
                        const double *bx, const double *by) {
    double maxf = 0.0, maxd = 0.0;
    for (long i = 0; i < n; i++) {
        maxf = fmax(maxf, fmax(fabs(ax[i]), fabs(ay[i])));
        maxd = fmax(maxd, fmax(fabs(ax[i] - bx[i]), fabs(ay[i] - by[i])));
    }
    return maxd <= 1e-9 * maxf;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 4000);
    double *x = xmalloc((size_t)n * sizeof(double)), *y = xmalloc((size_t)n * sizeof(double));
    double *m = xmalloc((size_t)n * sizeof(double));
    double *fxs = xmalloc((size_t)n * sizeof(double)), *fys = xmalloc((size_t)n * sizeof(double));
    double *fx2 = xmalloc((size_t)n * sizeof(double)), *fy2 = xmalloc((size_t)n * sizeof(double));
    double *fxp = xmalloc((size_t)n * sizeof(double)), *fyp = xmalloc((size_t)n * sizeof(double));
    fill_double(x, n, 0.0, 1.0, 1);
    fill_double(y, n, 0.0, 1.0, 2);
    fill_double(m, n, 0.5, 1.5, 3);
    omp_warmup();

    double tfull, tsym, tp;
    BEST_TIME(tfull, forces_full_serial(n, x, y, m, fxs, fys));   /* the reference */
    BEST_TIME(tsym, forces_sym_serial(n, x, y, m, fx2, fy2));
    printf("P096: 2D gravity forces, N = %ld particles, eps^2 = %g\n", n, EPS2);
    printf("serial full %.6f s, serial symmetric %.6f s (%.2fx less time), symmetric %s\n",
           tfull, tsym, tfull / tsym, ok_str(forces_close(n, fxs, fys, fx2, fy2)));

    printf("(a) full N^2 loop, parallel for over i (no race)   [serial = full]\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, forces_full_parallel(n, x, y, m, fxp, fyp));
        table_row(n, THREADS[k], tfull, tp, forces_close(n, fxs, fys, fxp, fyp));
    }
    printf("(b1) symmetric j > i, atomic updates, dynamic   [serial = symmetric]\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, forces_sym_atomic(n, x, y, m, fxp, fyp));
        table_row(n, THREADS[k], tsym, tp, forces_close(n, fxs, fys, fxp, fyp));
    }
    printf("(b2) symmetric j > i, per-thread force arrays + sum, dynamic   [serial = symmetric]\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, forces_sym_private(n, x, y, m, fxp, fyp));
        table_row(n, THREADS[k], tsym, tp, forces_close(n, fxs, fys, fxp, fyp));
    }
    free(x); free(y); free(m); free(fxs); free(fys); free(fx2); free(fy2); free(fxp); free(fyp);
    return 0;
}

/* Notes:
 * - Symmetry halves the pair computations (N(N-1)/2 instead of N(N-1)), but each
 *   pair now writes TWO particles, one of them (j) owned by no particular
 *   thread: that write is the race, so it needs atomics or private copies.
 *   The serial gain is often well below 2x: the j loop now also writes fx[j]
 *   (more memory traffic, harder to vectorise than the read-only full loop).
 * - (b1) does an atomic per pair: correct but slow (and contended when threads
 *   hit the same j); (b2) has no synchronisation in the loop but needs T*N extra
 *   memory and an O(T*N) reduction at the end. OpenMP 4.5 one-liner for (b2):
 *   reduction(+:fx[:n], fy[:n]) on the parallel for.
 * - Triangular loop: row i has N-1-i pairs, so schedule(static) gives the first
 *   thread most of the work. guided hands out the BIGGEST chunks first, so it
 *   balances only when later rows are the expensive ones (loop j < i); with
 *   j > i the first guided chunk (N/T rows) holds the heaviest rows - use dynamic.
 * - (a) does twice the arithmetic but scales perfectly (no sharing); with many
 *   threads it can beat (b1).
 * - Results differ from the serial sum in the last bits (different addition
 *   order), so compare against the largest force with a tolerance.
 */

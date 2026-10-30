/* P066: When is collapse(2) legal, and when is it correct?
 * Problem: For each loop nest decide whether collapse can be used, demonstrate
 *   what goes wrong otherwise, and give a correct parallel version:
 *   (a) matvec with y[i] = 0 inside the i loop, before the j loop:
 *       for i { y[i] = 0; for j y[i] += A[i][j]*x[j]; }   with collapse(2);
 *   (b) the same with the initialisation moved to a separate loop, so the nest
 *       for i for j y[i] += A[i][j]*x[j] is perfectly nested: collapse(2)
 *       compiles, but is the result right?
 *   (c) a triple loop for i for j for k X[i][j] += f(i, j, k): may we use
 *       collapse(2) on (i, j)? collapse(3)?
 *   Print the explanations, show the wrong results, and verify and time the
 *   correct versions.
 * Solution: collapse(n) needs (legality) perfectly nested, rectangular loops
 *   (OpenMP 4.5) and (correctness) no two iterations of the collapsed space
 *   writing the same location. (a) is not perfectly nested: gcc 9 and 12 reject it
 *   ("not enough perfectly nested loops", checked); gcc 14+ accept it as OpenMP 5.1
 *   "intervening code" but may run y[i] = 0 on every collapsed iteration, so the
 *   result is wrong anyway (compile with -DDEMO_IMPERFECT to try it). (b) is
 *   legal but RACES: iterations (i, j1) and (i, j2) may run on different threads
 *   and both update y[i]. Fixes: parallelise only i (private sum per row), or
 *   keep collapse(2) and add reduction(+:y[:n]) (or an atomic, slow). (c)
 *   collapse(2) is correct: each (i, j) owns X[i][j] and runs its k loop
 *   serially; collapse(3) races because different k for the same (i, j) go to
 *   different threads. The race demos use schedule(static, 1) so that neighbouring
 *   iterations really run at the same time; integer-valued data makes every
 *   correct sum exact, so lost updates are counted exactly. Matrices are flattened
 *   row-major: A[i][j] = A[i*N + j], X[i][j] = X[i*NJ + j].
 * Source: textbook (Eijkhout, Parallel Programming) Ex 19.11, 19.14, 19.15, 19.16.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p066_collapse_correctness.c -o p066 -lm
 * Run:   ./p066 [N] [NK]   (defaults N = 1500 for (b); NI = NJ = NK = 100 for (c))
 */
#include "common.h"

/* ---------------- (b) y = A x ---------------- */
static void matvec_serial(const double *A, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j] * x[j];
        y[i] = s;
    }
}

/* WRONG: legal (perfectly nested), but iterations of one row share y[i]. */
static void matvec_collapse_race(const double *A, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = 0.0;             /* separate init loop */
    #pragma omp parallel for collapse(2) schedule(static, 1)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            y[i] += A[i*n + j] * x[j];                   /* RACE on y[i] */
}

/* Fix 1: parallel over i only; row i (and y[i]) belongs to one thread. */
static void matvec_rows(const double *A, const double *x, double *y, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        double s = 0.0;
        for (long j = 0; j < n; j++) s += A[i*n + j] * x[j];
        y[i] = s;
    }
}

/* Fix 2: keep collapse(2) but give every thread a private y (array reduction). */
static void matvec_collapse_reduction(const double *A, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = 0.0;
    #pragma omp parallel for collapse(2) reduction(+:y[:n])
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            y[i] += A[i*n + j] * x[j];
}

/* Fix 3: collapse(2) + atomic update: correct, but n*n contended atomics. */
static void matvec_collapse_atomic(const double *A, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = 0.0;
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) {
            #pragma omp atomic
            y[i] += A[i*n + j] * x[j];
        }
}

#ifdef DEMO_IMPERFECT
/* (a) NOT perfectly nested. gcc <= 13: compile error. gcc >= 14: compiles, but
   y[i] = 0 may be executed for every (i, j) iteration -> wrong result. */
static void matvec_imperfect(const double *A, const double *x, double *y, long n) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < n; i++) {
        y[i] = 0.0;                                      /* code between the loops */
        for (long j = 0; j < n; j++)
            y[i] += A[i*n + j] * x[j];
    }
}
#endif

/* ---------------- (c) X[i][j] += f(i, j, k) ---------------- */
static double f(long i, long j, long k) { return (double)((i * 7 + j * 3 + k * 5) % 11); }

static void triple_serial(double *X, long ni, long nj, long nk) {
    for (long i = 0; i < ni; i++)
        for (long j = 0; j < nj; j++) {
            X[i*nj + j] = 0.0;
            for (long k = 0; k < nk; k++) X[i*nj + j] += f(i, j, k);
        }
}

/* Correct: collapse(2) on (i, j); the k loop runs inside one iteration. */
static void triple_collapse2(double *X, long ni, long nj, long nk) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < ni; i++)
        for (long j = 0; j < nj; j++) {
            double s = 0.0;                              /* private accumulator */
            for (long k = 0; k < nk; k++) s += f(i, j, k);
            X[i*nj + j] = s;
        }
}

/* WRONG: collapse(3) gives different k of the same (i, j) to different threads. */
static void triple_collapse3_race(double *X, long ni, long nj, long nk) {
    for (long t = 0; t < ni * nj; t++) X[t] = 0.0;
    #pragma omp parallel for collapse(3) schedule(static, 1)
    for (long i = 0; i < ni; i++)
        for (long j = 0; j < nj; j++)
            for (long k = 0; k < nk; k++)
                X[i*nj + j] += f(i, j, k);               /* RACE on X[i][j] */
}

static long count_diff(const double *a, const double *b, long n) {
    long bad = 0;
    for (long i = 0; i < n; i++) bad += (a[i] != b[i]);
    return bad;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1500);
    long nk = arg_long(argc, argv, 2, 100);
    long ni = nk, nj = nk;
    double *A = xmalloc((size_t)n * n * sizeof(double));
    double *x = xmalloc((size_t)n * sizeof(double));
    double *ys = xmalloc((size_t)n * sizeof(double)), *yp = xmalloc((size_t)n * sizeof(double));
    fill_double(A, n * n, 0.0, 1.0, 66);
    fill_double(x, n, 0.0, 1.0, 67);
    /* small integers: every partial sum is exact, so ANY difference is a lost update */
    for (long t = 0; t < n * n; t++) A[t] = floor(A[t] * 10.0);
    for (long t = 0; t < n; t++) x[t] = floor(x[t] * 10.0);
    omp_warmup();

    printf("P066: collapse legality and correctness, N = %ld (part b), NI = NJ = NK = %ld (part c)\n",
           n, nk);

    /* ---------- (a) ---------- */
    printf("\n(a) for i { y[i] = 0; for j y[i] += A[i][j]*x[j]; }  with collapse(2)\n"
           "    NOT perfectly nested: \"y[i] = 0\" sits between the two loops.\n"
           "    OpenMP 4.5 / gcc 9 and 12 (checked): compile error \"not enough perfectly nested loops\".\n"
           "    gcc 14+ (OpenMP 5.1 'intervening code') compiles it, but the code between the\n"
           "    loops may run for EVERY (i, j) iteration: with gcc 15, y[i] ends up as just the\n"
           "    last product. Either way: do not collapse; move y[i] = 0 into its own loop or\n"
           "    use a private sum per row (fix 1 below).\n");
#ifdef DEMO_IMPERFECT
    omp_set_num_threads(4);
    matvec_serial(A, x, ys, n);
    matvec_imperfect(A, x, yp, n);
    printf("    DEMO_IMPERFECT build, 4 threads: %ld of %ld entries wrong -> %s\n",
           count_diff(ys, yp, n), n, ok_str(count_diff(ys, yp, n) == 0));
#else
    printf("    (compile with -DDEMO_IMPERFECT to see it: an error on gcc 9/12, wrong y on gcc 15)\n");
#endif

    /* ---------- (b) ---------- */
    double ts, tp;
    BEST_TIME(ts, matvec_serial(A, x, ys, n));
    printf("\n(b) init loop first, then collapse(2) on: for i for j y[i] += A[i][j]*x[j]\n"
           "    Legal (perfectly nested, rectangular) but WRONG: the j iterations of one row\n"
           "    are spread over threads that all read-modify-write the same y[i].\n");
    printf("    race demo, schedule(static, 1):\n");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        matvec_collapse_race(A, x, yp, n);
        long bad = count_diff(ys, yp, n);
        printf("      %d thread(s): %6ld of %ld entries of y wrong -> %s\n", THREADS[k], bad, n,
               bad ? "WRONG (lost updates)" : (THREADS[k] == 1 ? "OK (one thread cannot race)"
                                                             : "correct by luck this run"));
    }
    printf("\n    Fix 1: parallel for over i only (private sum, one write of y[i] per row)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(yp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, matvec_rows(A, x, yp, n));
        table_row(n, THREADS[k], ts, tp, count_diff(ys, yp, n) == 0);
    }
    printf("    Fix 2: collapse(2) + reduction(+:y[:n]) (private y per thread)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(yp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, matvec_collapse_reduction(A, x, yp, n));
        table_row(n, THREADS[k], ts, tp, count_diff(ys, yp, n) == 0);
    }
    omp_set_num_threads(4);
    BEST_TIME(tp, matvec_collapse_atomic(A, x, yp, n));
    printf("    Fix 3: collapse(2) + atomic, 4 threads: %.6f s (serial %.6f s) -> %s, but slow\n",
           tp, ts, ok_str(count_diff(ys, yp, n) == 0));

    /* ---------- (c) ---------- */
    long nij = ni * nj;
    double *Xs = xmalloc((size_t)nij * sizeof(double)), *Xp = xmalloc((size_t)nij * sizeof(double));
    BEST_TIME(ts, triple_serial(Xs, ni, nj, nk));
    printf("\n(c) for i for j for k X[i][j] += f(i,j,k)\n"
           "    collapse(3): legal but WRONG, the k iterations of one (i, j) share X[i][j].\n");
    printf("    race demo, collapse(3) schedule(static, 1):\n");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        triple_collapse3_race(Xp, ni, nj, nk);
        long bad = count_diff(Xs, Xp, nij);
        printf("      %d thread(s): %6ld of %ld entries of X wrong -> %s\n", THREADS[k], bad, nij,
               bad ? "WRONG (lost updates)" : (THREADS[k] == 1 ? "OK (one thread cannot race)"
                                                             : "correct by luck this run"));
    }
    printf("    collapse(2) on (i, j): correct, each (i, j) runs its whole k loop\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Xp, 0, (size_t)nij * sizeof(double));
        BEST_TIME(tp, triple_collapse2(Xp, ni, nj, nk));
        table_row(nij, THREADS[k], ts, tp, count_diff(Xs, Xp, nij) == 0);
    }

    free(A); free(x); free(ys); free(yp); free(Xs); free(Xp);
    return 0;
}

/* Notes:
 * - Two separate questions: LEGAL (perfectly nested, rectangular bounds, no code
 *   between the loops in OpenMP 4.5) and CORRECT (no two collapsed iterations write
 *   the same location). collapse(k) is safe when the written element is indexed
 *   by ALL k collapsed loop variables (C[i][j] = ..., X[i][j] over (i, j)).
 * - y[i] += ... over (i, j): the write depends on i only -> do not collapse j;
 *   parallelise i, or collapse with reduction(+:y[:n]) / atomic.
 * - A race does not always show: with the default static schedule, a row is split
 *   between two threads only at chunk boundaries, so the bug can hide in testing.
 *   schedule(static, 1) is used here only to make it visible.
 * - Triangular loops (j from i) cannot be collapsed in OpenMP 4.5 (non-rectangular);
 *   OpenMP 5.0 allows some non-rectangular forms, but not the lab's gcc 9.
 * - Atomic is correct but every update serialises on one cache line: much slower
 *   than a private sum per row.
 */

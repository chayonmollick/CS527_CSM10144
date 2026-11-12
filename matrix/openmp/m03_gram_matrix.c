/* M03: Gram matrix G = A^T A
 * Problem: A is an m x n matrix of doubles (m rows, n columns). Compute the n x n
 *   Gram matrix G = A^T A, G[i][j] = sum_k A[k][i] * A[k][j] (the dot product of
 *   columns i and j), serially and with OpenMP in three ways: (a) column dot
 *   products, (b) transpose A first, then dot products of contiguous rows,
 *   (c) row-oriented accumulation G += (row k)^T (row k) with a private G per
 *   thread. G is symmetric, so compute only j >= i and mirror. Verify against
 *   serial and a 3 x 2 example, and report the timings for 1, 2, 4, 8 threads.
 * Solution: (a) parallel for over i, inner j = i..n-1: iteration i writes only
 *   G[i][j] (j >= i), so there is no race. Row i has n - i entries (a triangular
 *   loop), so schedule(dynamic, 4). The k loop reads A down two columns (stride n).
 *   (b) the same loops on At = A^T (n x m, built by a parallel transpose): both
 *   columns are now contiguous rows of At (cache-friendly, vectorisable).
 *   (c) a parallel for over the rows k of A with "G[i][j] += A[k][i]*A[k][j]"
 *   RACES: every k updates every G[i][j]. Each thread therefore accumulates its
 *   rows into its own zeroed copy Gl and adds Gl into G once, inside a critical
 *   section (n^2/2 adds per thread instead of one critical per update).
 *   The mirror step G[j][i] = G[i][j] is a separate parallel loop: the implicit
 *   barrier before it guarantees the upper triangle is complete.
 * Source: lab extension (normal equations A^T A x = A^T b; MATRIX_RULES.md section 3).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m03_gram_matrix.c -o m03 -lm
 * Run:   ./m03 [m] [n]   (defaults m = 1500 rows, n = 300 columns)
 */
#include "../../problems/common.h"

/* Copy the upper triangle into the lower one: G[i][j] = G[j][i] for j < i.
 * Iteration i writes only row i (left of the diagonal): no race. */
static void mirror_parallel(double *G, long n) {
    #pragma omp parallel for schedule(dynamic, 8)
    for (long i = 1; i < n; i++)
        for (long j = 0; j < i; j++) G[i*n + j] = G[j*n + i];
}

/* Serial reference: row-oriented accumulation (contiguous), upper triangle. */
static void gram_serial(const double *A, long m, long n, double *G) {
    for (long t = 0; t < n * n; t++) G[t] = 0.0;
    for (long k = 0; k < m; k++) {
        const double *a = A + k*n;                       /* row k of A */
        for (long i = 0; i < n; i++) {
            double ai = a[i];
            for (long j = i; j < n; j++) G[i*n + j] += ai * a[j];
        }
    }
    for (long i = 1; i < n; i++)
        for (long j = 0; j < i; j++) G[i*n + j] = G[j*n + i];
}

/* (a) column dot products, stride-n reads. */
static void gram_columns(const double *A, long m, long n, double *G) {
    #pragma omp parallel for schedule(dynamic, 4)
    for (long i = 0; i < n; i++)
        for (long j = i; j < n; j++) {
            double s = 0.0;
            for (long k = 0; k < m; k++) s += A[k*n + i] * A[k*n + j];
            G[i*n + j] = s;
        }
    mirror_parallel(G, n);
}

/* (b) transpose first (At is n x m), then dot products of contiguous rows. */
static void gram_transposed(const double *A, long m, long n, double *G, double *At) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        for (long k = 0; k < m; k++) At[i*m + k] = A[k*n + i];
    #pragma omp parallel for schedule(dynamic, 4)
    for (long i = 0; i < n; i++)
        for (long j = i; j < n; j++) {
            const double *x = At + i*m, *y = At + j*m;
            double s = 0.0;
            #pragma omp simd reduction(+:s)              /* let the compiler vectorise the sum */
            for (long k = 0; k < m; k++) s += x[k] * y[k];
            G[i*n + j] = s;
        }
    mirror_parallel(G, n);
}

/* (c) rows of A in parallel, private G per thread, merged once in a critical section. */
static void gram_private(const double *A, long m, long n, double *G) {
    for (long t = 0; t < n * n; t++) G[t] = 0.0;
    #pragma omp parallel
    {
        double *Gl = xmalloc((size_t)n * n * sizeof(double));   /* this thread's G */
        memset(Gl, 0, (size_t)n * n * sizeof(double));
        #pragma omp for schedule(static) nowait
        for (long k = 0; k < m; k++) {
            const double *a = A + k*n;
            for (long i = 0; i < n; i++) {
                double ai = a[i];
                for (long j = i; j < n; j++) Gl[i*n + j] += ai * a[j];
            }
        }
        #pragma omp critical
        for (long i = 0; i < n; i++)
            for (long j = i; j < n; j++) G[i*n + j] += Gl[i*n + j];
        free(Gl);
    }
    mirror_parallel(G, n);
}

static int is_symmetric(const double *G, long n) {
    int ok = 1;
    #pragma omp parallel for reduction(&&:ok)
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++) ok = ok && (G[i*n + j] == G[j*n + i]);
    return ok;
}

static void hand_example(void) {
    const double A[6] = {1, 2,
                         3, 4,
                         5, 6};
    const double expect[4] = {35, 44,
                              44, 56};
    double Ga[4], Gb[4], Gc[4], At[6];
    gram_columns(A, 3, 2, Ga);
    gram_transposed(A, 3, 2, Gb, At);
    gram_private(A, 3, 2, Gc);
    int ok = close_doubles(Ga, expect, 4, 0) && close_doubles(Gb, expect, 4, 0)
          && close_doubles(Gc, expect, 4, 0);
    printf("Hand example: A = [1 2; 3 4; 5 6] -> G = A^T A = [%g %g; %g %g] "
           "(expected [35 44; 44 56]): %s\n\n", Ga[0], Ga[1], Ga[2], Ga[3], ok_str(ok));
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 1500), n = arg_long(argc, argv, 2, 300);
    if (m < 1 || n < 1) { fprintf(stderr, "m and n must be >= 1\n"); return 1; }
    double *A = xmalloc((size_t)m * n * sizeof(double));
    double *At = xmalloc((size_t)m * n * sizeof(double));
    double *Gs = xmalloc((size_t)n * n * sizeof(double));
    double *Gp = xmalloc((size_t)n * n * sizeof(double));
    fill_double(A, m * n, -1.0, 1.0, 3);
    omp_warmup();

    hand_example();
    double ts, tp;
    BEST_TIME(ts, gram_serial(A, m, n, Gs));
    printf("M03: A is %ld x %ld, G = A^T A is %ld x %ld (m*n*(n+1)/2 = %.0f multiply-adds)\n",
           m, n, n, n, (double)m * n * (n + 1) / 2);
    printf("serial (row-oriented, upper triangle + mirror): G symmetric: %s, G[0][0] = %.6f\n",
           ok_str(is_symmetric(Gs, n)), Gs[0]);

    printf("(a) column dot products, parallel over i, schedule(dynamic, 4)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, gram_columns(A, m, n, Gp));
        table_row(n, THREADS[k], ts, tp, close_doubles(Gs, Gp, n * n, 1e-9) && is_symmetric(Gp, n));
    }
    printf("(b) parallel transpose, then dot products of contiguous rows of A^T\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, gram_transposed(A, m, n, Gp, At));
        table_row(n, THREADS[k], ts, tp, close_doubles(Gs, Gp, n * n, 1e-9) && is_symmetric(Gp, n));
    }
    printf("(c) rows of A in parallel, private G per thread + critical merge\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, gram_private(A, m, n, Gp));
        table_row(n, THREADS[k], ts, tp, close_doubles(Gs, Gp, n * n, 1e-9) && is_symmetric(Gp, n));
    }
    free(A); free(At); free(Gs); free(Gp);
    return 0;
}

/* Notes:
 * - Sizes: A is m x n, A^T is n x m, so A^T A is n x n (columns . columns) and
 *   A A^T is m x m (rows . rows: contiguous in row-major storage, no transpose needed).
 * - G is symmetric ((A^T A)^T = A^T A) and positive semidefinite: G[i][i] = |column i|^2
 *   >= 0. Least squares min |Ax - b| solves the normal equations A^T A x = A^T b
 *   (e.g. by Cholesky, see m04).
 * - Computing only j >= i halves the work (m n (n+1)/2 multiply-adds instead of
 *   m n^2) but makes the rows unequal: row i has n - i entries, so a plain static
 *   schedule gives the first thread most of the work -> dynamic (or static, 1).
 *   collapse(2) is not allowed on (i, j): the j bound depends on i (OpenMP 4.5).
 * - (a) vs (b): the same arithmetic; (a) reads two columns with stride n, (b) pays
 *   an O(mn) transpose once and then streams contiguous rows (much faster for big m).
 *   A dot product is a reduction, which gcc does not vectorise at -O2 unless it may
 *   reorder the additions: "#pragma omp simd reduction(+:s)" (OpenMP 4.0) allows it.
 *   The serial reference (row-oriented axpy) vectorises without help, which is why
 *   (a) on one thread is slower than serial.
 * - (c) without the private copies would be a race on every G[i][j]
 *   (load-add-store from several threads). reduction(+:G[:n*n]) also works, but gcc
 *   puts each private copy on the thread's stack: n*n doubles is too big for big n.
 *   "nowait" lets a thread start merging as soon as its own rows are done; the
 *   critical section still adds the copies one thread at a time.
 * - Sums run in a different order in (b) (simd lanes) and (c) (thread blocks), so
 *   compare with a relative tolerance.
 */

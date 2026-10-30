/* P078: Wavefront (anti-diagonal) parallelisation
 * Problem: (Lecture slides; textbook Ex 24.2.) On an N x N grid (flattened
 *   row-major, X[(long)i*N + j]) with row 0 and column 0 given, compute
 *       x[i][j] = x[i-1][j] + x[i][j-1]     for i, j = 1..N-1
 *   (here mod 1000000007 so the numbers stay bounded; with all 1s on the
 *   border x[i][j] = C(i+j, i), Pascal's triangle). Also do K in-place
 *   Gauss-Seidel sweeps A[i][j] = 0.2*(A[i][j] + up + down + left + right).
 *   Neither loop nest can be parallelised over rows or over columns. Find the
 *   parallelism, implement it with OpenMP and verify EXACT equality with the
 *   serial row-by-row loops.
 * Solution: (i,j) needs (i-1,j) and (i,j-1), which lie on the previous
 *   anti-diagonal d-1 (d = i+j). All points on one anti-diagonal are
 *   independent. So loop over d sequentially and run the points of diagonal d
 *   in parallel ("omp for" over i, with j = d - i), inside one parallel region;
 *   the implicit barrier after each diagonal makes diagonal d complete before
 *   d+1 starts. Every point is computed from exactly the same inputs as in the
 *   serial loop, so the result is identical. For the in-place Gauss-Seidel
 *   sweep the same works: up/left lie on diagonal d-1 (already new this sweep)
 *   and down/right on d+1 (still old), exactly as in the row-by-row order.
 *   Load imbalance: diagonal lengths grow 1, 2, ..., N-1 and shrink again, so
 *   near the corners there are fewer points than threads, and each diagonal
 *   costs a barrier (2N barriers). Consecutive points on a diagonal are N-1
 *   elements apart in memory (poor cache use). Blocked wavefront: cut the grid
 *   into B x B tiles and run the tiles on each tile anti-diagonal in parallel;
 *   inside a tile go row by row (cache-friendly, only 2N/B barriers).
 * Source: lecture slides (wavefront); textbook Ex 24.2.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p078_wavefront_antidiagonal.c -o p078 -lm
 * Run:   ./p078 [N] [B] [Ngs] [K]   (defaults N = 3000, tile B = 100, Ngs = 1000, K = 3)
 */
#include "common.h"

#define MOD 1000000007L

static long lmin(long a, long b) { return a < b ? a : b; }
static long lmax(long a, long b) { return a > b ? a : b; }

/* Row 0 and column 0: all 1 (Pascal) or pseudo-random values in [0, MOD). */
static void init_border(long *X, long n, int pascal) {
    unsigned long long s = 78;
    for (long k = 0; k < n; k++) {
        X[k]     = pascal ? 1 : (long)(lcg_next(&s) % MOD);      /* row 0 */
        X[k * n] = pascal ? 1 : (long)(lcg_next(&s) % MOD);      /* column 0 */
    }
}

/* ---------- recurrence x[i][j] = x[i-1][j] + x[i][j-1] (mod MOD) ---------- */

static void rec_serial(long *X, long n) {
    for (long i = 1; i < n; i++)
        for (long j = 1; j < n; j++)
            X[i * n + j] = (X[(i - 1) * n + j] + X[i * n + j - 1]) % MOD;
}

static void rec_wavefront(long *X, long n) {
    #pragma omp parallel
    for (long d = 2; d <= 2 * (n - 1); d++) {         /* every thread walks the diagonals */
        long ilo = lmax(1, d - (n - 1)), ihi = lmin(n - 1, d - 1);
        #pragma omp for
        for (long i = ilo; i <= ihi; i++) {
            long j = d - i;
            X[i * n + j] = (X[(i - 1) * n + j] + X[i * n + j - 1]) % MOD;
        }   /* implicit barrier: diagonal d done before d+1 */
    }
}

/* Tiles of B x B interior points; tile (bi,bj) covers rows 1+bi*B .. and
 * columns 1+bj*B .. . Tile (bi,bj) needs tiles (bi-1,bj) and (bi,bj-1): all
 * tiles with the same bi+bj = t are independent. */
static void rec_blocked(long *X, long n, long B) {
    long T = (n - 1 + B - 1) / B;                     /* tiles per side */
    #pragma omp parallel
    for (long t = 0; t <= 2 * (T - 1); t++) {
        #pragma omp for schedule(dynamic, 1)
        for (long bi = lmax(0, t - (T - 1)); bi <= lmin(T - 1, t); bi++) {
            long bj = t - bi;
            long i0 = 1 + bi * B, i1 = lmin(n - 1, i0 + B - 1);
            long j0 = 1 + bj * B, j1 = lmin(n - 1, j0 + B - 1);
            for (long i = i0; i <= i1; i++)           /* row by row inside the tile */
                for (long j = j0; j <= j1; j++)
                    X[i * n + j] = (X[(i - 1) * n + j] + X[i * n + j - 1]) % MOD;
        }
    }
}

/* ---------- in-place Gauss-Seidel sweeps on an n x n grid (border fixed) ---------- */

static void gs_init(double *A, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            A[i * n + j] = (i == 0) ? 100.0 : (i == n - 1 || j == 0 || j == n - 1) ? 0.0 : 25.0;
}

static inline void gs_point(double *A, long n, long i, long j) {
    long k = i * n + j;
    A[k] = 0.2 * (A[k] + A[k - n] + A[k + n] + A[k - 1] + A[k + 1]);
}

static void gs_serial(double *A, long n, long sweeps) {
    for (long s = 0; s < sweeps; s++)
        for (long i = 1; i < n - 1; i++)
            for (long j = 1; j < n - 1; j++) gs_point(A, n, i, j);
}

static void gs_wavefront(double *A, long n, long sweeps) {
    long m = n - 2;                                   /* interior points per side: 1..m */
    #pragma omp parallel
    for (long s = 0; s < sweeps; s++)
        for (long d = 2; d <= 2 * m; d++) {
            #pragma omp for
            for (long i = lmax(1, d - m); i <= lmin(m, d - 1); i++) gs_point(A, n, i, d - i);
        }
}

int main(int argc, char *argv[]) {
    long n   = arg_long(argc, argv, 1, 3000);
    long B   = arg_long(argc, argv, 2, 100);
    long ngs = arg_long(argc, argv, 3, 1000);
    long K   = arg_long(argc, argv, 4, 3);
    if (n < 2) n = 2;
    if (B < 1) B = 1;
    if (ngs < 3) ngs = 3;
    if (K < 1) K = 1;

    /* Worked example: 6 x 6 with a border of 1s gives Pascal's triangle along the diagonals. */
    long P[36];
    init_border(P, 6, 1);
    rec_wavefront(P, 6);
    printf("P078: wavefront along anti-diagonals\n"
           "6 x 6 example, x[i][j] = x[i-1][j] + x[i][j-1], border 1s:\n");
    for (long i = 0; i < 6; i++) {
        for (long j = 0; j < 6; j++) printf(" %4ld", P[i * 6 + j]);
        printf("\n");
    }
    printf("anti-diagonal d = i+j = 4 holds (1,3) (2,2) (3,1): 4 6 4, computed together "
           "from d = 3: 3 3. Example C(10,5) = x[5][5] = %ld  %s\n", P[35], ok_str(P[35] == 252));
    printf("diagonal lengths for N = %ld: 1, 2, ..., %ld, ..., 2, 1 (%ld diagonals, %ld barriers)\n\n",
           n, n - 1, 2 * n - 3, 2 * n - 3);

    size_t cells = (size_t)n * (size_t)n;
    long *Xs = xmalloc(cells * sizeof(long)), *Xp = xmalloc(cells * sizeof(long));
    omp_warmup();
    double ts, tp;
    init_border(Xs, n, 0);
    init_border(Xp, n, 0);
    BEST_TIME(ts, rec_serial(Xs, n));
    printf("Recurrence mod 1e9+7, N = %ld (check: equal to the serial loop exactly)\n", n);
    printf("(a) point-wise wavefront, omp for over each anti-diagonal\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        for (size_t c = 0; c < cells; c++) Xp[c] = -1;
        init_border(Xp, n, 0);
        BEST_TIME(tp, rec_wavefront(Xp, n));
        table_row(n, THREADS[k], ts, tp, same_longs(Xs, Xp, (long)cells));
    }
    printf("(b) blocked wavefront, %ld x %ld tiles, omp for over each tile anti-diagonal\n", B, B);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        for (size_t c = 0; c < cells; c++) Xp[c] = -1;
        init_border(Xp, n, 0);
        BEST_TIME(tp, rec_blocked(Xp, n, B));
        table_row(n, THREADS[k], ts, tp, same_longs(Xs, Xp, (long)cells));
    }
    free(Xs); free(Xp);

    size_t gcells = (size_t)ngs * (size_t)ngs;
    double *As = xmalloc(gcells * sizeof(double)), *Ap = xmalloc(gcells * sizeof(double));
    BEST_TIME(ts, (gs_init(As, ngs), gs_serial(As, ngs, K)));
    printf("\nIn-place Gauss-Seidel, %ld x %ld grid, %ld sweeps (check: equal to the "
           "row-by-row sweep exactly)\n", ngs, ngs, K);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, (gs_init(Ap, ngs), gs_wavefront(Ap, ngs, K)));
        table_row(ngs, THREADS[k], ts, tp, close_doubles(As, Ap, (long)gcells, 0.0));
    }
    free(As); free(Ap);
    return 0;
}

/* Notes:
 * - Row-parallel is wrong: row i needs the finished row i-1 (and x[i][j-1]
 *   from the same row): loop-carried dependences in BOTH directions.
 * - Anti-diagonal d = i+j: inputs are on d-1 only, so a diagonal is a set of
 *   independent tasks; the d loop stays sequential (barrier per diagonal).
 * - Parallelism is limited: average diagonal length is about N/2, and the
 *   first/last diagonals have fewer points than threads (ramp up / ramp down).
 * - Point-wise timings: with 1 thread the diagonal order can even beat the row
 *   loop (the row loop is one long dependence chain, x[i][j] waits for
 *   x[i][j-1]; points on a diagonal are independent, so the CPU overlaps
 *   them). With more threads it gets SLOWER: 2N barriers, each guarding only
 *   about N/2 cheap points, and points N-1 elements apart (poor cache use).
 *   Tiles fix both: rows inside a tile are contiguous, only 2N/B barriers.
 * - Same idea with tasks: one task per tile with depend(in: left, up tile)
 *   depend(out: this tile) - see P089.
 */

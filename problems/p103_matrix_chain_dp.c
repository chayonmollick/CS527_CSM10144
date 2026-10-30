/* P103: Wavefront dynamic programming (matrix chain, LCS)
 * Problem: Parallelise two dynamic programs with OpenMP.
 *   (1) Matrix-chain order: matrices A1..An, Ai is p[i-1] x p[i]. The fewest
 *       scalar multiplications for Ai..Aj is m[i][i] = 0,
 *       m[i][j] = min over i <= k < j of m[i][k] + m[k+1][j] + p[i-1] p[k] p[j].
 *       Also record the best split s[i][j] and print the optimal parenthesisation
 *       for p = {30, 35, 15, 5, 10, 20, 25} (answer 15125).
 *   (2) Longest common subsequence of strings a (length n) and b (length m):
 *       c[i][j] = c[i-1][j-1] + 1 if a[i-1] = b[j-1], else max(c[i-1][j], c[i][j-1]).
 *   Verify the parallel tables against the serial ones and report time and
 *   speedup for 1, 2, 4 and 8 threads.
 * Solution: find cells that do not depend on each other (a "wavefront").
 *   (1) m[i][j] needs only shorter chains (m[i][k], m[k+1][j] with length < L =
 *       j-i+1), so all cells with the same chain length L are independent: loop L
 *       = 2..n sequentially, parallel for over i (j = i+L-1). Each cell is written
 *       by one thread; the implicit barrier finishes length L before L+1.
 *   (2) c[i][j] needs cells on the two previous anti-diagonals (i+j = d-1, d-2),
 *       so cells with the same d = i+j are independent:
 *       (2a) loop d sequentially, omp for over i on each anti-diagonal (one
 *            parallel region, barrier per diagonal);
 *       (2b) tiled wavefront: the same idea on B x B tiles - tile (I,J) needs
 *            tiles (I-1,J), (I,J-1), (I-1,J-1), so tiles with the same I+J run in
 *            parallel and each tile is filled row by row (cache-friendly, n/B
 *            times fewer barriers).
 * Source: textbook dynamic programming (CLRS 15.2 and 15.4), wavefront pattern.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p103_matrix_chain_dp.c -o p103 -lm
 * Run:   ./p103 [n_chain] [n_lcs]   (defaults 700 matrices, LCS of two 3000-char strings)
 */
#include "common.h"

#define TILE 256                /* LCS tile size for (2b) */

/* ================= (1) matrix-chain order ================= */
/* m and s are (n+1) x (n+1), row-major, indices 1..n used. */
static void mcm_serial(long n, const long *p, long *m, int *s) {
    long w = n + 1;
    for (long i = 1; i <= n; i++) m[i * w + i] = 0;
    for (long L = 2; L <= n; L++)                      /* chain length */
        for (long i = 1; i <= n - L + 1; i++) {
            long j = i + L - 1, best = LONG_MAX;
            int bk = (int)i;
            for (long k = i; k < j; k++) {
                long cost = m[i * w + k] + m[(k + 1) * w + j] + p[i - 1] * p[k] * p[j];
                if (cost < best) { best = cost; bk = (int)k; }
            }
            m[i * w + j] = best; s[i * w + j] = bk;
        }
}

static void mcm_parallel(long n, const long *p, long *m, int *s) {
    long w = n + 1;
    for (long i = 1; i <= n; i++) m[i * w + i] = 0;
    for (long L = 2; L <= n; L++) {                    /* sequential: needs shorter chains */
        #pragma omp parallel for schedule(static)
        for (long i = 1; i <= n - L + 1; i++) {        /* cells of length L: independent */
            long j = i + L - 1, best = LONG_MAX;
            int bk = (int)i;
            for (long k = i; k < j; k++) {
                long cost = m[i * w + k] + m[(k + 1) * w + j] + p[i - 1] * p[k] * p[j];
                if (cost < best) { best = cost; bk = (int)k; }
            }
            m[i * w + j] = best; s[i * w + j] = bk;
        }                                              /* barrier: length L done */
    }
}

static void print_parens(const int *s, long w, long i, long j) {
    if (i == j) { printf("A%ld", i); return; }
    printf("(");
    print_parens(s, w, i, s[i * w + j]);
    print_parens(s, w, s[i * w + j] + 1, j);
    printf(")");
}

static void mcm_example(void) {
    const long p[7] = {30, 35, 15, 5, 10, 20, 25};
    long m[49] = {0};
    int s[49] = {0};
    mcm_parallel(6, p, m, s);
    printf("CLRS example p = {30,35,15,5,10,20,25}: cost %ld (expect 15125, %s), order ",
           m[1 * 7 + 6], ok_str(m[1 * 7 + 6] == 15125));
    print_parens(s, 7, 1, 6);
    printf("\n");
}

/* ================= (2) longest common subsequence ================= */
/* c is (n+1) x (m+1); row 0 and column 0 are 0. */
static inline void lcs_cell(const char *a, const char *b, int *c, long w, long i, long j) {
    if (a[i - 1] == b[j - 1]) c[i * w + j] = c[(i - 1) * w + j - 1] + 1;
    else {
        int up = c[(i - 1) * w + j], left = c[i * w + j - 1];
        c[i * w + j] = up > left ? up : left;
    }
}

static void lcs_zero_border(int *c, long n, long m) {
    long w = m + 1;
    for (long j = 0; j <= m; j++) c[j] = 0;
    for (long i = 0; i <= n; i++) c[i * w] = 0;
}

static int lcs_serial(const char *a, long n, const char *b, long m, int *c) {
    long w = m + 1;
    lcs_zero_border(c, n, m);
    for (long i = 1; i <= n; i++)
        for (long j = 1; j <= m; j++) lcs_cell(a, b, c, w, i, j);
    return c[n * w + m];
}

/* (2a) anti-diagonal wavefront over single cells. */
static int lcs_diagonal(const char *a, long n, const char *b, long m, int *c) {
    long w = m + 1;
    lcs_zero_border(c, n, m);
    #pragma omp parallel
    for (long d = 2; d <= n + m; d++) {                /* every thread runs all d */
        long ilo = d - m > 1 ? d - m : 1, ihi = d - 1 < n ? d - 1 : n;
        #pragma omp for schedule(static)
        for (long i = ilo; i <= ihi; i++) lcs_cell(a, b, c, w, i, d - i);
    }                                                  /* barrier of omp for: d done */
    return c[n * w + m];
}

/* (2b) wavefront over TILE x TILE blocks; each block row by row. */
static int lcs_tiled(const char *a, long n, const char *b, long m, int *c) {
    long w = m + 1;
    long NB = (n + TILE - 1) / TILE, MB = (m + TILE - 1) / TILE;   /* tiles per side */
    lcs_zero_border(c, n, m);
    #pragma omp parallel
    for (long D = 0; D <= NB + MB - 2; D++) {          /* tile anti-diagonal I + J = D */
        long Ilo = D - (MB - 1) > 0 ? D - (MB - 1) : 0, Ihi = D < NB - 1 ? D : NB - 1;
        #pragma omp for schedule(dynamic, 1)
        for (long I = Ilo; I <= Ihi; I++) {
            long J = D - I;
            long i0 = 1 + I * TILE, i1 = i0 + TILE - 1 < n ? i0 + TILE - 1 : n;
            long j0 = 1 + J * TILE, j1 = j0 + TILE - 1 < m ? j0 + TILE - 1 : m;
            for (long i = i0; i <= i1; i++)
                for (long j = j0; j <= j1; j++) lcs_cell(a, b, c, w, i, j);
        }
    }
    return c[n * w + m];
}

int main(int argc, char *argv[]) {
    long nc = arg_long(argc, argv, 1, 700);
    long nl = arg_long(argc, argv, 2, 3000);
    if (nc < 1 || nl < 1) { fprintf(stderr, "sizes must be >= 1\n"); return 1; }
    omp_warmup();
    double ts, tp;

    /* ---- (1) matrix chain ---- */
    printf("P103: wavefront dynamic programming\n");
    mcm_example();
    long w = nc + 1;
    long *p = xmalloc((size_t)w * sizeof(long));
    for (long i = 0; i <= nc; i++) p[i] = 5 + (long)((i * 7919 + 13) % 96);   /* dims 5..100 */
    long *ms = calloc((size_t)(w * w), sizeof(long)), *mp = calloc((size_t)(w * w), sizeof(long));
    int *ss = calloc((size_t)(w * w), sizeof(int)), *sp = calloc((size_t)(w * w), sizeof(int));
    if (!ms || !mp || !ss || !sp) { fprintf(stderr, "out of memory\n"); return 1; }
    BEST_TIME(ts, mcm_serial(nc, p, ms, ss));
    printf("(1) matrix chain, n = %ld matrices: minimum cost %ld\n", nc, ms[1 * w + nc]);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, mcm_parallel(nc, p, mp, sp));
        table_row(nc, THREADS[k], ts, tp, same_longs(ms, mp, w * w) && same_ints(ss, sp, w * w));
    }
    free(p); free(ms); free(mp); free(ss); free(sp);

    /* ---- (2) LCS ---- */
    char *a = xmalloc((size_t)nl), *b = xmalloc((size_t)nl);
    unsigned long long seed = 777;
    for (long i = 0; i < nl; i++) a[i] = "ACGT"[lcg_next(&seed) % 4];
    for (long i = 0; i < nl; i++) b[i] = "ACGT"[lcg_next(&seed) % 4];
    size_t cells = (size_t)(nl + 1) * (size_t)(nl + 1);
    int *cs = xmalloc(cells * sizeof(int)), *cp = xmalloc(cells * sizeof(int));
    int rs = 0, rp = 0;
    BEST_TIME(ts, rs = lcs_serial(a, nl, b, nl, cs));
    printf("(2) LCS of two random ACGT strings of length %ld: LCS length %d\n", nl, rs);
    printf("(2a) anti-diagonal wavefront, one cell at a time\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = lcs_diagonal(a, nl, b, nl, cp));
        table_row(nl, THREADS[k], ts, tp, rp == rs && same_ints(cs, cp, (long)cells));
    }
    printf("(2b) tiled wavefront, %d x %d tiles\n", TILE, TILE);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = lcs_tiled(a, nl, b, nl, cp));
        table_row(nl, THREADS[k], ts, tp, rp == rs && same_ints(cs, cp, (long)cells));
    }
    free(a); free(b); free(cs); free(cp);
    return 0;
}

/* Notes:
 * - Wavefront = cells whose inputs are all on EARLIER fronts; one front is a
 *   parallel for, the fronts themselves are sequential (barrier between them).
 * - Matrix chain: each cell of length L costs L-1 steps, so a front is evenly
 *   balanced (static is fine); use schedule(dynamic) when cells on a front cost
 *   different amounts. Long chains have few cells (n-L+1), so late fronts use
 *   few threads - and the earliest fronts are too cheap to be worth splitting.
 *   m[k+1][j] walks DOWN a column (one cache line per k), so the loop is
 *   memory-bound and stops scaling early; fix: also store m[j][i] = m[i][j] and
 *   read m[j][k+1] instead (a row) - same values, contiguous access.
 * - LCS cell by cell (2a): a front has at most n cells, each only a few
 *   instructions, consecutive cells are m ints apart in memory, and there are
 *   2n barriers - so it can be slower than serial. Tiling (2b) fixes all three.
 * - No race: in every front each cell is written by exactly one thread, and it
 *   only reads cells from earlier fronts, which no one writes any more.
 * - Ties in the matrix chain (strict <, smallest k) are broken the same way in
 *   both versions, so even the split table s[][] is identical.
 */

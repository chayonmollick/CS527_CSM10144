/* P085: Cellular automata (1D elementary rules, 2D majority vote)
 * Problem: (a) 1D elementary cellular automaton: N cells of 0/1, rule number
 *   R (0..255). In each generation every cell looks at (left, self, right) =
 *   a 3-bit number k = 4*left + 2*self + right and becomes bit k of R. Cells
 *   outside the array are always 0 (fixed boundary). Run G generations (e.g.
 *   rule 30, rule 90). (b) 2D majority vote on an H x W grid of 0/1 (row-major
 *   c[(long)i*W + j], outside = 0): a cell becomes 1 if at least 5 of the 9
 *   cells of its 3x3 neighbourhood (itself included) are 1, else 0; run S steps.
 *   Parallelise both with OpenMP, verify the final state against the serial
 *   code, and print the first generations of a small rule-90 run.
 * Solution: generation g+1 is computed only from generation g, so with two
 *   arrays (double buffering: read cur, write next, then swap pointers) all
 *   cells of a generation are independent: "parallel for" over cells (1D) or
 *   "parallel for collapse(2)" over (i, j) (2D) is race-free. Generations are
 *   sequential (implicit barrier at the end of each parallel for; the swap is
 *   done by the master between generations). Integer states -> results must
 *   be identical to serial.
 * Source: lab extension (stencils / simulations).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p085_cellular_automaton.c -o p085 -lm
 * Run:   ./p085 [N] [G] [H] [S]   (defaults N = 1e6, G = 100, H = W = 1500, S = 6)
 */
#include "common.h"

typedef unsigned char u8;

/* New state of a cell from (left, self, right): bit 4l+2c+r of the rule. */
static inline u8 apply_rule(int rule, u8 l, u8 c, u8 r) {
    return (u8)((rule >> (4 * l + 2 * c + r)) & 1);
}

/* ---------- (a) 1D elementary automaton ---------- */

static void ca1d_step(const u8 *cur, u8 *next, long n, int rule, long i) {
    u8 l = i > 0 ? cur[i - 1] : 0, r = i < n - 1 ? cur[i + 1] : 0;   /* 0 outside */
    next[i] = apply_rule(rule, l, cur[i], r);
}

/* Run G generations from init; returns the buffer (a or b) with the final state. */
static u8 *ca1d_serial(const u8 *init, u8 *a, u8 *b, long n, int rule, long G) {
    memcpy(a, init, (size_t)n);
    for (long g = 0; g < G; g++) {
        for (long i = 0; i < n; i++) ca1d_step(a, b, n, rule, i);
        u8 *t = a; a = b; b = t;
    }
    return a;
}

static u8 *ca1d_parallel(const u8 *init, u8 *a, u8 *b, long n, int rule, long G) {
    memcpy(a, init, (size_t)n);
    for (long g = 0; g < G; g++) {                      /* generations: sequential */
        #pragma omp parallel for
        for (long i = 0; i < n; i++) ca1d_step(a, b, n, rule, i);   /* reads a, writes b[i] */
        u8 *t = a; a = b; b = t;                         /* after the implicit barrier */
    }
    return a;
}

/* ---------- (b) 2D majority vote ---------- */

static inline u8 majority_at(const u8 *c, long H, long W, long i, long j) {
    int ones = 0;
    for (long di = -1; di <= 1; di++)
        for (long dj = -1; dj <= 1; dj++) {
            long y = i + di, x = j + dj;
            if (y >= 0 && y < H && x >= 0 && x < W) ones += c[y * W + x];   /* outside = 0 */
        }
    return ones >= 5;
}

static u8 *maj_serial(const u8 *init, u8 *a, u8 *b, long H, long W, long S) {
    memcpy(a, init, (size_t)(H * W));
    for (long s = 0; s < S; s++) {
        for (long i = 0; i < H; i++)
            for (long j = 0; j < W; j++) b[i * W + j] = majority_at(a, H, W, i, j);
        u8 *t = a; a = b; b = t;
    }
    return a;
}

static u8 *maj_parallel(const u8 *init, u8 *a, u8 *b, long H, long W, long S) {
    memcpy(a, init, (size_t)(H * W));
    for (long s = 0; s < S; s++) {
        #pragma omp parallel for collapse(2)
        for (long i = 0; i < H; i++)
            for (long j = 0; j < W; j++) b[i * W + j] = majority_at(a, H, W, i, j);
        u8 *t = a; a = b; b = t;
    }
    return a;
}

static void random_bits(u8 *c, long n, unsigned long long seed, int percent_ones) {
    unsigned long long s = seed;
    for (long i = 0; i < n; i++) c[i] = (u8)(lcg_next(&s) % 100 < (unsigned long)percent_ones);
}

static long count_ones(const u8 *c, long n) {
    long k = 0;
    for (long i = 0; i < n; i++) k += c[i];
    return k;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000000);
    long G = arg_long(argc, argv, 2, 100);
    long H = arg_long(argc, argv, 3, 1500), W = H;
    long S = arg_long(argc, argv, 4, 6);
    if (n < 1) n = 1;
    if (G < 0) G = 0;
    if (H < 1) H = W = 1;
    if (S < 0) S = 0;

    /* Worked example: rule 90 from a single 1 draws the Sierpinski triangle. */
    enum { TW = 63, TG = 32 };
    u8 x[TW], y[TW], *cur = x, *nxt = y;
    memset(x, 0, TW);
    x[TW / 2] = 1;
    printf("P085: cellular automata\n"
           "Rule 90, %d cells, one 1 in the middle, generations 0..%d:\n", TW, TG - 1);
    for (int g = 0; g < TG; g++) {
        for (int i = 0; i < TW; i++) putchar(cur[i] ? '#' : '.');
        putchar('\n');
        #pragma omp parallel for
        for (long i = 0; i < TW; i++) ca1d_step(cur, nxt, TW, 90, i);
        u8 *t = cur; cur = nxt; nxt = t;
    }

    u8 *init = xmalloc((size_t)n), *a = xmalloc((size_t)n), *b = xmalloc((size_t)n);
    u8 *c = xmalloc((size_t)n), *d = xmalloc((size_t)n);
    random_bits(init, n, 85, 50);
    omp_warmup();
    double ts, tp;
    int rules[2] = {30, 90};
    for (int r = 0; r < 2; r++) {
        u8 *fs = NULL, *fp = NULL;
        BEST_TIME(ts, fs = ca1d_serial(init, a, b, n, rules[r], G));
        printf("\n1D rule %d, N = %ld cells, G = %ld generations, random start: %ld ones at the end"
               " (check: final state equal to serial)\n", rules[r], n, G, count_ones(fs, n));
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            BEST_TIME(tp, fp = ca1d_parallel(init, c, d, n, rules[r], G));
            table_row(n, THREADS[k], ts, tp, memcmp(fs, fp, (size_t)n) == 0);
        }
    }
    free(init); free(a); free(b); free(c); free(d);

    long cells = H * W;
    u8 *g0 = xmalloc((size_t)cells), *ga = xmalloc((size_t)cells), *gb = xmalloc((size_t)cells);
    u8 *gc = xmalloc((size_t)cells), *gd = xmalloc((size_t)cells), *ms = NULL, *mp = NULL;
    random_bits(g0, cells, 86, 50);
    BEST_TIME(ts, ms = maj_serial(g0, ga, gb, H, W, S));
    printf("\n2D majority vote, %ld x %ld grid, %ld steps: ones %ld -> %ld"
           " (check: final grid equal to serial)\n", H, W, S, count_ones(g0, cells), count_ones(ms, cells));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, mp = maj_parallel(g0, gc, gd, H, W, S));
        table_row(cells, THREADS[k], ts, tp, memcmp(ms, mp, (size_t)cells) == 0);
    }
    free(g0); free(ga); free(gb); free(gc); free(gd);
    return 0;
}

/* Notes:
 * - In-place update (one array) is wrong even serially: cell i would see the
 *   NEW value of cell i-1. Double buffering + pointer swap fixes it and makes
 *   all cells of a generation independent.
 * - The swap happens between parallel regions (or, inside one region, after
 *   the barrier: by a single thread or with private pointer copies, see P079).
 * - Rule number: bit k of R is the new state for neighbourhood k = 4l+2c+r;
 *   rule 90 = new state is l XOR r (Sierpinski), rule 30 = l XOR (c OR r).
 * - Boundary: fixed 0 outside; a periodic (wrap-around) boundary would use
 *   cur[(i-1+n)%n] and cur[(i+1)%n].
 * - Per generation the work is tiny (one byte per cell): for small N the
 *   fork/join per generation dominates; use one region with omp for inside.
 */

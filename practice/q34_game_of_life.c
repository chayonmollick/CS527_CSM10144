/* CS527 Computer Systems — OpenMP practice
 * Q34: Simulate Conway's Game of Life for G generations on an N x N grid
 *      (cells outside the grid are dead) in parallel with OpenMP, counting
 *      the live population. Verify the final grid exactly against the serial
 *      run, and show a glider moving for a few generations.
 *  - rules: live cell survives with 2 or 3 live neighbours, dead cell with exactly 3 becomes live
 *  - grid stored with a 1-cell dead border ((N+2) x (N+2)) so no bounds checks are needed
 *  - serial: double buffering (read cur, write nxt, swap pointers)
 *  - parallel: parallel for collapse(2) over all cells + reduction(+:pop) for the population
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char cell_t;                  /* 1 = alive, 0 = dead */

static unsigned long long rng = 42;            /* deterministic, serial only */
static unsigned next_rand(void) {
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng >> 33);
}

/* w = n + 2 = row length including the border; cell (i,j) for 1 <= i,j <= n */
static int neighbours(const cell_t *g, long w, long i, long j) {
    return g[(i-1)*w + j-1] + g[(i-1)*w + j] + g[(i-1)*w + j+1]
         + g[ i   *w + j-1]                  + g[ i   *w + j+1]
         + g[(i+1)*w + j-1] + g[(i+1)*w + j] + g[(i+1)*w + j+1];
}
static cell_t rule(cell_t alive, int nb) { return (cell_t)(nb == 3 || (alive && nb == 2)); }

/* one generation; returns the new population */
static long step_serial(const cell_t *cur, cell_t *nxt, long n) {
    long w = n + 2, pop = 0;
    for (long i = 1; i <= n; i++)
        for (long j = 1; j <= n; j++) {
            cell_t c = rule(cur[i*w + j], neighbours(cur, w, i, j));
            nxt[i*w + j] = c;
            pop += c;
        }
    return pop;
}
static long step_parallel(const cell_t *cur, cell_t *nxt, long n) {
    long w = n + 2, pop = 0;
    #pragma omp parallel for collapse(2) reduction(+:pop)
    for (long i = 1; i <= n; i++)
        for (long j = 1; j <= n; j++) {        /* reads cur only, writes own cell */
            cell_t c = rule(cur[i*w + j], neighbours(cur, w, i, j));
            nxt[i*w + j] = c;
            pop += c;
        }
    return pop;
}

/* run G generations from grid a (b = scratch, both with dead borders);
 * returns the final population, *final points at the buffer holding it */
static long run(cell_t *a, cell_t *b, long n, long gens, int parallel, cell_t **final) {
    cell_t *cur = a, *nxt = b;
    long pop = 0;
    for (long g = 0; g < gens; g++) {          /* generations are sequential */
        pop = parallel ? step_parallel(cur, nxt, n) : step_serial(cur, nxt, n);
        cell_t *t = cur; cur = nxt; nxt = t;
    }
    *final = cur;
    return pop;
}

static void print_grid(const cell_t *g, long n) {
    long w = n + 2;
    for (long i = 1; i <= n; i++) {
        printf("    ");
        for (long j = 1; j <= n; j++) putchar(g[i*w + j] ? '#' : '.');
        putchar('\n');
    }
}

int main(int argc, char **argv) {
    long n = (argc > 1) ? atol(argv[1]) : 2000;
    long gens = (argc > 2) ? atol(argv[2]) : 100;
    if (n < 1 || gens < 1) { fprintf(stderr, "usage: ./q34 [N >= 1] [G >= 1]\n"); return 1; }
    const int threads[] = {1, 2, 4, 8};
    printf("Q34: Game of Life with OpenMP\n");

    /* ---- glider demo on an 8x8 grid ---- */
    const long gn = 8, gw = gn + 2;
    cell_t ga[100] = {0}, gb[100] = {0}, start[100] = {0};   /* (8+2)^2 cells */
    start[1*gw + 2] = 1;                       /* .#.            */
    start[2*gw + 3] = 1;                       /* ..#            */
    start[3*gw + 1] = start[3*gw + 2] = start[3*gw + 3] = 1;   /* ### */
    memcpy(ga, start, sizeof ga);
    omp_set_num_threads(4);
    printf("\nGlider (parallel step), generations 0..4:\n");
    for (int g = 0; g <= 4; g++) {
        printf("  gen %d:\n", g); print_grid(ga, gn);
        if (g < 4) { step_parallel(ga, gb, gn); memcpy(ga, gb, sizeof ga); }
    }
    int glider_ok = 1, alive = 0;              /* after 4 gens: same shape, moved (+1,+1) */
    for (long i = 1; i < gn; i++)
        for (long j = 1; j < gn; j++)
            if (ga[(i+1)*gw + j+1] != start[i*gw + j]) glider_ok = 0;
    for (int k = 0; k < 100; k++) alive += ga[k];
    if (alive != 5) glider_ok = 0;             /* and nothing else is alive */
    printf("  glider moved one cell down-right after 4 generations: %s\n",
           glider_ok ? "OK" : "WRONG");

    /* ---- big random grid: serial vs parallel ---- */
    long w = n + 2, cells = w * w;
    cell_t *init = calloc((size_t)cells, 1);
    cell_t *a = malloc((size_t)cells), *b = malloc((size_t)cells);
    if (!init || !a || !b) { fprintf(stderr, "alloc failed\n"); return 1; }
    long pop0 = 0;
    for (long i = 1; i <= n; i++)
        for (long j = 1; j <= n; j++) { init[i*w + j] = (next_rand() % 100) < 30; pop0 += init[i*w + j]; }

    memcpy(a, init, (size_t)cells); memcpy(b, init, (size_t)cells);   /* borders 0 in both */
    cell_t *sfin;
    double t0 = omp_get_wtime();
    long spop = run(a, b, n, gens, 0, &sfin);
    double ts = omp_get_wtime() - t0;
    cell_t *ref = malloc((size_t)cells);
    if (!ref) { fprintf(stderr, "alloc failed\n"); return 1; }
    memcpy(ref, sfin, (size_t)cells);
    printf("\n%ldx%ld grid, 30%% alive (%ld cells), %ld generations: final population %ld\n",
           n, n, pop0, gens, spop);
    printf("%8s %12s %12s %9s %11s %7s\n",
           "threads", "serial(s)", "parallel(s)", "speedup", "efficiency", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        memcpy(a, init, (size_t)cells); memcpy(b, init, (size_t)cells);
        cell_t *pfin;
        double p0 = omp_get_wtime();
        long ppop = run(a, b, n, gens, 1, &pfin);
        double tp = omp_get_wtime() - p0;
        int ok = (ppop == spop) && memcmp(pfin, ref, (size_t)cells) == 0;
        double sp = tp > 0 ? ts / tp : 0.0;
        printf("%8d %12.6f %12.6f %9.2f %11.2f %7s\n",
               threads[ti], ts, tp, sp, sp / threads[ti], ok ? "OK" : "WRONG");
    }
    free(init); free(a); free(b); free(ref);
    return 0;
}

/* Viva notes:
 * - Double buffering: every cell's next state depends on the CURRENT states
 *   of its neighbours. Updating in place would let a cell see neighbours that
 *   are already in the next generation (wrong answer + data race). With cur
 *   read-only and each thread writing distinct nxt cells there is no race.
 * - Generations are inherently sequential (gen g+1 needs all of gen g); the
 *   implicit barrier at the end of each parallel for is that synchronisation.
 * - collapse(2) turns the N x N cell loops into one N*N iteration space;
 *   pop += c from all threads would race, so reduction(+:pop) gives each
 *   thread a private counter summed at the end. Integer sums are exact, so
 *   the population and grid match the serial run exactly.
 * - Dead border: storing (N+2) x (N+2) with a ring of 0s that is never
 *   written means the 8-neighbour sum needs no if-checks at the edges.
 *   (A torus would wrap indices with % n instead.)
 * - 1 byte per cell keeps the grid small and cache friendly; a glider
 *   returns to its shape every 4 generations, shifted by one cell diagonally.
 */

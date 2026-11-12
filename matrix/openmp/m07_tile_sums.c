/* M07: Tile (block) sums of a matrix
 * Problem: Split an N x N integer matrix A into b x b tiles; when N is not a
 *   multiple of b the last tile row and tile column are smaller. There are
 *   nt = ceil(N / b) tiles per side, and tile (bi, bj) covers rows
 *   bi*b .. min(bi*b + b, N) - 1 and the same range of columns. Compute the
 *   nt x nt matrix S of tile sums, S[bi][bj] = sum of the elements of tile
 *   (bi, bj), serially and with OpenMP in three ways; verify against serial, check
 *   that the sum of S equals the sum of A, check a 5 x 5 example, report timings.
 * Solution: (a) collapse(2) over the tiles (bi, bj): iteration (bi, bj) sums its
 *   own tile and writes S[bi][bj] once, so there is no race and no reduction.
 *   (b) parallel for over the ROWS of A: row i adds into the whole tile row
 *   S[i/b][*], and the b rows of one tile row may belong to different threads,
 *   so S needs the array reduction reduction(+:S[:nt*nt]) (OpenMP 4.5).
 *   (c) the same row loop with a private, zeroed copy of S per thread (heap),
 *   added into S once per thread inside a critical section: what the array
 *   reduction does, by hand, and safe for any nt.
 *   Integer sums (long) are exact in any order, so all results must be equal.
 * Source: lab extension (MATRIX_RULES.md section 5: tiles; related: problems/p083
 *   pooling, the same windows with stride = window size).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m07_tile_sums.c -o m07 -lm
 * Run:   ./m07 [N] [b]   (defaults N = 3000, b = 64: 47 x 47 tiles, the last ones 56 wide)
 */
#include "../../problems/common.h"

static long imin(long a, long b) { return a < b ? a : b; }

/* Row i, tile column bj: sum of A[i][bj*b .. end). */
static long row_piece(const int *A, long n, long b, long i, long bj) {
    long s = 0, j1 = imin(bj*b + b, n);
    for (long j = bj*b; j < j1; j++) s += A[i*n + j];
    return s;
}

static void tiles_serial(const int *A, long n, long b, long *S) {
    long nt = (n + b - 1) / b;
    for (long t = 0; t < nt * nt; t++) S[t] = 0;
    for (long i = 0; i < n; i++)
        for (long bj = 0; bj < nt; bj++) S[(i / b)*nt + bj] += row_piece(A, n, b, i, bj);
}

/* (a) one tile per iteration. */
static void tiles_by_tile(const int *A, long n, long b, long *S) {
    long nt = (n + b - 1) / b;
    #pragma omp parallel for collapse(2) schedule(static)
    for (long bi = 0; bi < nt; bi++)
        for (long bj = 0; bj < nt; bj++) {
            long s = 0, i1 = imin(bi*b + b, n);
            for (long i = bi*b; i < i1; i++) s += row_piece(A, n, b, i, bj);
            S[bi*nt + bj] = s;                         /* one writer per tile */
        }
}

/* (b) rows in parallel, array reduction on S (private copies on the thread stacks). */
static void tiles_by_rows(const int *A, long n, long b, long *S) {
    long nt = (n + b - 1) / b, ns = nt * nt;
    for (long t = 0; t < ns; t++) S[t] = 0;
    #pragma omp parallel for reduction(+:S[:ns])
    for (long i = 0; i < n; i++)
        for (long bj = 0; bj < nt; bj++) S[(i / b)*nt + bj] += row_piece(A, n, b, i, bj);
}

/* (c) rows in parallel, private S per thread on the heap, merged in a critical section. */
static void tiles_private(const int *A, long n, long b, long *S) {
    long nt = (n + b - 1) / b, ns = nt * nt;
    for (long t = 0; t < ns; t++) S[t] = 0;
    #pragma omp parallel
    {
        long *Sl = xmalloc((size_t)ns * sizeof(long));
        memset(Sl, 0, (size_t)ns * sizeof(long));
        #pragma omp for schedule(static) nowait
        for (long i = 0; i < n; i++)
            for (long bj = 0; bj < nt; bj++) Sl[(i / b)*nt + bj] += row_piece(A, n, b, i, bj);
        #pragma omp critical
        for (long t = 0; t < ns; t++) S[t] += Sl[t];
        free(Sl);
    }
}

static void hand_example(void) {
    int A[25];
    long Sa[9], Sb[9], Sc[9];
    const long expect[9] = {16, 24, 15,
                            56, 64, 35,
                            43, 47, 25};
    for (int k = 0; k < 25; k++) A[k] = k + 1;          /* 1..25 row by row */
    tiles_by_tile(A, 5, 2, Sa);
    tiles_by_rows(A, 5, 2, Sb);
    tiles_private(A, 5, 2, Sc);
    printf("Hand example: A = 5 x 5 with 1..25 row by row, b = 2 -> 3 x 3 tiles\n");
    printf("  S = [%ld %ld %ld; %ld %ld %ld; %ld %ld %ld]\n",
           Sa[0], Sa[1], Sa[2], Sa[3], Sa[4], Sa[5], Sa[6], Sa[7], Sa[8]);
    printf("  expected [16 24 15; 56 64 35; 43 47 25] (1+2+6+7 = 16, ..., 25 alone): %s\n\n",
           ok_str(same_longs(Sa, expect, 9) && same_longs(Sb, expect, 9)
                  && same_longs(Sc, expect, 9)));
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 3000), b = arg_long(argc, argv, 2, 64);
    if (n < 1 || b < 1) { fprintf(stderr, "N and b must be >= 1\n"); return 1; }
    long nt = (n + b - 1) / b, ns = nt * nt;
    int *A = xmalloc((size_t)n * n * sizeof(int));
    long *Ss = xmalloc((size_t)ns * sizeof(long)), *Sp = xmalloc((size_t)ns * sizeof(long));
    fill_int(A, n * n, -100, 100, 41);
    omp_warmup();

    hand_example();
    double ts, tp;
    BEST_TIME(ts, tiles_serial(A, n, b, Ss));
    long sumA = 0, sumS = 0;
    for (long k = 0; k < n * n; k++) sumA += A[k];
    for (long t = 0; t < ns; t++) sumS += Ss[t];
    printf("M07: N = %ld, b = %ld: %ld x %ld tiles, the last tile row/column is %ld wide\n",
           n, b, nt, nt, n - (nt - 1) * b);
    printf("serial: sum of S = %ld, sum of A = %ld: %s\n", sumS, sumA, ok_str(sumS == sumA));

    printf("(a) collapse(2) over the tiles, one write per tile\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, tiles_by_tile(A, n, b, Sp));
        table_row(n, THREADS[k], ts, tp, same_longs(Ss, Sp, ns));
    }
    if (ns * (long)sizeof(long) <= 128 * 1024) {
        printf("(b) parallel for over rows, reduction(+:S[:nt*nt])\n");
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            BEST_TIME(tp, tiles_by_rows(A, n, b, Sp));
            table_row(n, THREADS[k], ts, tp, same_longs(Ss, Sp, ns));
        }
    } else {
        printf("(b) skipped: S has %ld entries, too many for private copies on the "
               "thread stacks\n", ns);
    }
    printf("(c) parallel for over rows, private S per thread + critical merge\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, tiles_private(A, n, b, Sp));
        table_row(n, THREADS[k], ts, tp, same_longs(Ss, Sp, ns));
    }
    free(A); free(Ss); free(Sp);
    return 0;
}

/* Notes:
 * - Tile index of element (i, j): (i / b, j / b); tile (bi, bj) starts at
 *   (bi*b, bj*b) and ends at min(start + b, N) (exclusive). nt = (N + b - 1) / b.
 *   Forgetting the min() reads past the end of a row (or of A) on the last tiles.
 * - (a) has nt^2 independent iterations: collapse(2) gives the threads enough of
 *   them even when nt is small. The edge tiles are smaller, so a little imbalance.
 *   Each tile reads b short row pieces of b ints (stride N between them).
 * - (b) and (c) read A row by row (contiguous); the price is that rows of the
 *   same tile row, on different threads, update the same S entries: a race
 *   without the reduction / private copies.
 * - reduction(+:S[:ns]): gcc puts each thread's private copy on its stack, so a
 *   huge S (small b) can overflow the stack of the worker threads; (c) keeps the
 *   copies on the heap. The merge in (c) costs nt^2 adds per thread.
 * - Blocking/tiling is the same index pattern used by blocked matrix multiply and
 *   blocked transpose (practice/q13, q14): work on one cache-sized tile at a time.
 * - Pooling (problems/p083) is this computation with "mean" or "max" instead of
 *   "sum" and with a stride that may differ from the window size.
 * - O(N^2) integer adds: memory-bound, the speedup flattens with more threads.
 */

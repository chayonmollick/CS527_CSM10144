/* P084: Summed-area table (2D prefix sum, integral image) and rectangle queries
 * Problem: Given an H x W integer matrix A (row-major A[(long)i*W + j]), compute
 *   S[i][j] = sum of A[0..i][0..j]. Then answer Q queries "sum of A over the
 *   rectangle rows r1..r2, columns c1..c2" in O(1) each:
 *       sum = S[r2][c2] - S[r1-1][c2] - S[r2][c1-1] + S[r1-1][c1-1]
 *   (terms with index -1 are 0). Parallelise building S and answering the
 *   queries with OpenMP; verify S against the serial code and some query
 *   answers against brute-force sums.
 * Solution: build S in two passes. Pass 1: prefix sum along each row; rows are
 *   independent, so "parallel for" over rows (each thread runs whole rows,
 *   sequentially along the row). Pass 2: prefix sum down each column of the
 *   pass-1 result; columns are independent, so parallel over columns.
 *   Version C: "parallel for" over columns j, each thread walks down its
 *   column (simple, but each step jumps W elements: poor cache use).
 *   Version B: each thread takes one contiguous block of columns
 *   (W*t/p .. W*(t+1)/p - 1, from its thread number t of p) and goes row by
 *   row inside it (i outer, j inner), so memory is read contiguously - the
 *   same order as the serial code. A thread only reads values of its own
 *   columns, which it wrote itself, so no barrier is needed between rows.
 *   Queries only read S and write their own answer: "parallel for" over
 *   queries. Integers -> results must match exactly.
 * Source: lab extension (prefix sums / image processing).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p084_summed_area_table.c -o p084 -lm
 * Run:   ./p084 [H] [W] [Q]  (defaults H = 3000, W = 2500, Q = 2e6)
 */
#include "common.h"

static void sat_serial(const int *A, long *S, long H, long W) {
    for (long i = 0; i < H; i++) {                 /* pass 1: rows */
        long run = 0;
        for (long j = 0; j < W; j++) { run += A[i * W + j]; S[i * W + j] = run; }
    }
    for (long i = 1; i < H; i++)                   /* pass 2: columns, row by row */
        for (long j = 0; j < W; j++) S[i * W + j] += S[(i - 1) * W + j];
}

/* Pass 1 in parallel: one row per iteration. */
static void rows_parallel(const int *A, long *S, long H, long W) {
    #pragma omp parallel for
    for (long i = 0; i < H; i++) {
        long run = 0;
        for (long j = 0; j < W; j++) { run += A[i * W + j]; S[i * W + j] = run; }
    }
}

/* Version C: pass 2 parallel over single columns (strided walk down a column). */
static void sat_par_columns(const int *A, long *S, long H, long W) {
    rows_parallel(A, S, H, W);
    #pragma omp parallel for
    for (long j = 0; j < W; j++)
        for (long i = 1; i < H; i++) S[i * W + j] += S[(i - 1) * W + j];
}

/* Version B: pass 2 with one contiguous block of columns per thread; the thread
 * walks down the rows of its block (i outer, j inner: contiguous access). */
static void sat_par_blocks(const int *A, long *S, long H, long W) {
    rows_parallel(A, S, H, W);
    #pragma omp parallel
    {
        int t = omp_get_thread_num(), p = omp_get_num_threads();
        long j0 = W * t / p, j1 = W * (t + 1) / p;     /* my columns: j0 .. j1-1 */
        for (long i = 1; i < H; i++)
            for (long j = j0; j < j1; j++) S[i * W + j] += S[(i - 1) * W + j];
    }
}

/* S[i][j], or 0 if i or j is -1 (above / left of the matrix). */
static inline long S_at(const long *S, long W, long i, long j) {
    return (i < 0 || j < 0) ? 0 : S[i * W + j];
}
static inline long rect_sum(const long *S, long W, const long *q) {   /* q = {r1, c1, r2, c2} */
    return S_at(S, W, q[2], q[3]) - S_at(S, W, q[0] - 1, q[3])
         - S_at(S, W, q[2], q[1] - 1) + S_at(S, W, q[0] - 1, q[1] - 1);
}
static void queries_serial(const long *S, long W, const long *q, long Q, long *ans) {
    for (long t = 0; t < Q; t++) ans[t] = rect_sum(S, W, q + 4 * t);
}
static void queries_parallel(const long *S, long W, const long *q, long Q, long *ans) {
    #pragma omp parallel for
    for (long t = 0; t < Q; t++) ans[t] = rect_sum(S, W, q + 4 * t);
}

static long brute_sum(const int *A, long W, const long *q) {
    long s = 0;
    for (long i = q[0]; i <= q[2]; i++)
        for (long j = q[1]; j <= q[3]; j++) s += A[i * W + j];
    return s;
}

/* Q random rectangles with 0 <= r1 <= r2 < H and 0 <= c1 <= c2 < W. */
static void make_queries(long *q, long Q, long H, long W) {
    unsigned long long s = 84;
    for (long t = 0; t < Q; t++) {
        long a = (long)(lcg_next(&s) % (unsigned long)H), b = (long)(lcg_next(&s) % (unsigned long)H);
        long c = (long)(lcg_next(&s) % (unsigned long)W), d = (long)(lcg_next(&s) % (unsigned long)W);
        q[4 * t]     = a < b ? a : b;   q[4 * t + 2] = a < b ? b : a;
        q[4 * t + 1] = c < d ? c : d;   q[4 * t + 3] = c < d ? d : c;
    }
}

int main(int argc, char *argv[]) {
    long H = arg_long(argc, argv, 1, 3000);
    long W = arg_long(argc, argv, 2, 2500);
    long Q = arg_long(argc, argv, 3, 2000000);
    if (H < 1) H = 1;
    if (W < 1) W = 1;
    if (Q < 1) Q = 1;

    /* Worked example: 4 x 4. */
    int a4[16] = {1, 2, 3, 4,   5, 6, 7, 8,   9, 10, 11, 12,   13, 14, 15, 16};
    long s4[16], q4[4] = {1, 1, 2, 2};
    sat_par_blocks(a4, s4, 4, 4);
    printf("P084: summed-area table\n4 x 4 example:      A               S\n");
    for (int i = 0; i < 4; i++)
        printf("   %3d %3d %3d %3d    %4ld %4ld %4ld %4ld\n", a4[4 * i], a4[4 * i + 1], a4[4 * i + 2],
               a4[4 * i + 3], s4[4 * i], s4[4 * i + 1], s4[4 * i + 2], s4[4 * i + 3]);
    long r4 = rect_sum(s4, 4, q4);
    printf("sum of rows 1..2, cols 1..2 = S[2][2] - S[0][2] - S[2][0] + S[0][0] = %ld - %ld - %ld + %ld"
           " = %ld (6+7+10+11 = 34)  %s\n\n", s4[10], s4[2], s4[8], s4[0], r4, ok_str(r4 == 34));

    int  *A  = xmalloc((size_t)H * W * sizeof(int));
    long *Ss = xmalloc((size_t)H * W * sizeof(long)), *Sp = xmalloc((size_t)H * W * sizeof(long));
    long *q  = xmalloc((size_t)Q * 4 * sizeof(long));
    long *as = xmalloc((size_t)Q * sizeof(long)), *ap = xmalloc((size_t)Q * sizeof(long));
    fill_int(A, H * W, 0, 9, 84);
    make_queries(q, Q, H, W);
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, sat_serial(A, Ss, H, W));
    printf("H = %ld, W = %ld, total = S[H-1][W-1] = %ld\n", H, W, Ss[H * W - 1]);
    for (int v = 0; v < 2; v++) {
        printf("\nBuild S, version %s (check: S equal to serial)\n",
               v == 0 ? "C: rows, then parallel for over single columns"
                      : "B: rows, then one block of columns per thread");
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            if (v == 0) BEST_TIME(tp, sat_par_columns(A, Sp, H, W));
            else        BEST_TIME(tp, sat_par_blocks(A, Sp, H, W));
            table_row(H * W, THREADS[k], ts, tp, same_longs(Ss, Sp, H * W));
        }
    }

    int brute_ok = 1;
    for (long t = 0; t < 20 && t < Q; t++) {
        queries_serial(Ss, W, q + 4 * t, 1, as);
        if (as[0] != brute_sum(A, W, q + 4 * t)) brute_ok = 0;
    }
    printf("\nfirst 20 queries vs brute-force sums: %s\n", ok_str(brute_ok));
    BEST_TIME(ts, queries_serial(Ss, W, q, Q, as));
    printf("%ld rectangle queries (check: answers equal to serial)\n", Q);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, queries_parallel(Ss, W, q, Q, ap));
        table_row(Q, THREADS[k], ts, tp, same_longs(as, ap, Q));
    }
    free(A); free(Ss); free(Sp); free(q); free(as); free(ap);
    return 0;
}

/* Notes:
 * - A prefix sum along a row is sequential (S[j] needs S[j-1]), but different
 *   rows are independent - that is the parallelism (same for columns).
 * - Pass 2 over single columns reads S with stride W: one cache line per
 *   element (8 useful bytes of 64) and false sharing where neighbouring
 *   columns belong to different threads. Blocks of columns walked row by row
 *   read whole cache lines, like the serial loop.
 * - Both passes move all of S through memory twice: memory-bound, so the
 *   speedup is limited by bandwidth, not by the number of cores.
 * - Inclusion-exclusion: S(r2,c2) - S(r1-1,c2) - S(r2,c1-1) + S(r1-1,c1-1);
 *   the top-left block is subtracted twice, so add it back once.
 * - Use long for S: sums grow like H*W*max(A) and overflow int quickly.
 * - Queries are independent and read-only on S: perfectly parallel, but
 *   random access into S (cache misses) dominates.
 */

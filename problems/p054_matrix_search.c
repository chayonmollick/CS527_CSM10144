/* P054: Search a value in a matrix: count, first occurrence, list of positions
 * Problem: Given an R x C int matrix (row-major) and a value v, find in
 *   parallel with OpenMP: (a) how many times v occurs; (b) the first
 *   occurrence in row-major order (or "none"); (c) the list of all (row, col)
 *   positions of v in row-major order. Verify against serial code for a value
 *   that occurs many times, exactly once and not at all, and report time and
 *   speedup for 1, 2, 4 and 8 threads.
 * Solution: (a) parallel for over rows with reduction(+:count). (b) linear
 *   index r*C + c is the row-major order, so the first occurrence is the
 *   MINIMUM matching index: reduction(min:first) with first = R*C meaning
 *   "none"; inside a row the scan breaks at the first match. (c) ordered
 *   output = blocked count + scan: each thread takes a contiguous block of
 *   rows, pass 1 counts its matches, barrier, single turns the counts into
 *   offsets, implicit barrier, pass 2 writes its matches from its offset.
 *   Blocks are in row order, so the list is in row-major order; each thread
 *   writes its own slice -> race-free.
 * Source: lab extension (Lab 8 search task; reductions min/+, ordered output).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p054_matrix_search.c -o p054 -lm
 * Run:   ./p054 [R] [C]      (defaults R = C = 4000)
 */
#include "common.h"

/* ---------- (a) count ---------- */
static long count_serial(const int *m, long R, long C, int v) {
    long cnt = 0;
    for (long i = 0; i < R * C; i++) cnt += (m[i] == v);
    return cnt;
}
static long count_parallel(const int *m, long R, long C, int v) {
    long cnt = 0;
    #pragma omp parallel for reduction(+:cnt)
    for (long r = 0; r < R; r++)
        for (long c = 0; c < C; c++) cnt += (m[r * C + c] == v);
    return cnt;
}

/* ---------- (b) first occurrence: linear index, R*C = not found ---------- */
static long first_serial(const int *m, long R, long C, int v) {
    for (long i = 0; i < R * C; i++) if (m[i] == v) return i;   /* serial can stop early */
    return R * C;
}
static long first_parallel(const int *m, long R, long C, int v) {
    long first = R * C;
    #pragma omp parallel for reduction(min:first)
    for (long r = 0; r < R; r++)
        for (long c = 0; c < C; c++)
            if (m[r * C + c] == v) {
                if (r * C + c < first) first = r * C + c;
                break;                          /* rest of this row comes later */
            }
    return first;
}

/* ---------- (c) all positions in row-major order ---------- */
static long list_serial(const int *m, long R, long C, int v, long *pr, long *pc) {
    long k = 0;
    for (long r = 0; r < R; r++)
        for (long c = 0; c < C; c++)
            if (m[r * C + c] == v) { pr[k] = r; pc[k] = c; k++; }
    return k;
}
static long list_parallel(const int *m, long R, long C, int v, long *pr, long *pc) {
    long *off = xmalloc((size_t)omp_get_max_threads() * sizeof(long));
    long total = 0;
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long r0 = R * tid / nt, r1 = R * (tid + 1) / nt;          /* my rows [r0, r1) */

        long cnt = 0;                                             /* pass 1: count */
        for (long r = r0; r < r1; r++)
            for (long c = 0; c < C; c++) cnt += (m[r * C + c] == v);
        off[tid] = cnt;

        #pragma omp barrier
        #pragma omp single
        {
            long run = 0;
            for (int t = 0; t < nt; t++) { long ct = off[t]; off[t] = run; run += ct; }
            total = run;
        }                                                         /* implicit barrier */

        long k = off[tid];                                        /* pass 2: write my slice */
        for (long r = r0; r < r1; r++)
            for (long c = 0; c < C; c++)
                if (m[r * C + c] == v) { pr[k] = r; pc[k] = c; k++; }
    }
    free(off);
    return total;
}

int main(int argc, char *argv[]) {
    long R = arg_long(argc, argv, 1, 4000L);
    long C = arg_long(argc, argv, 2, 4000L);
    if (R < 1) R = 1;
    if (C < 1) C = 1;
    long N = R * C;
    int *m = xmalloc((size_t)N * sizeof(int));
    fill_int(m, N, 0, 99, 54);                       /* each value 0..99 occurs ~N/100 times */
    long plant = (2 * R / 3) * C + C / 3;             /* the only 1000 in the matrix */
    m[plant] = 1000;
    omp_warmup();

    const char *cname[3] = {"many", "one", "none"};
    const int vals[3] = {42, 1000, -5};
    long cnt_s[3], first_s[3], *pr_s[3], *pc_s[3], *pr_p[3], *pc_p[3];
    printf("P054: search in a %ld x %ld matrix (values 0..99, one planted 1000)\n", R, C);
    for (int q = 0; q < 3; q++) {
        cnt_s[q] = count_serial(m, R, C, vals[q]);
        first_s[q] = first_serial(m, R, C, vals[q]);
        size_t b = (size_t)cnt_s[q] * sizeof(long);   /* output arrays sized by the count */
        pr_s[q] = xmalloc(b); pc_s[q] = xmalloc(b); pr_p[q] = xmalloc(b); pc_p[q] = xmalloc(b);
        list_serial(m, R, C, vals[q], pr_s[q], pc_s[q]);
        if (first_s[q] < N)
            printf("  case %-4s: v = %4d, count = %8ld, first at (%ld, %ld)\n", cname[q], vals[q],
                   cnt_s[q], first_s[q] / C, first_s[q] % C);
        else
            printf("  case %-4s: v = %4d, count = %8ld, first: none\n", cname[q], vals[q], cnt_s[q]);
    }

    double ts, tp;
    long rs = 0, rp = 0;                              /* serial / parallel result of the timed case */
    printf("\n(a) count, reduction(+)   (timed: case many; check: all three cases)\n");
    BEST_TIME(ts, rs = count_serial(m, R, C, vals[0]));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int ok = 1;
        for (int q = 0; q < 3; q++) ok = ok && count_parallel(m, R, C, vals[q]) == cnt_s[q];
        BEST_TIME(tp, rp = count_parallel(m, R, C, vals[0]));
        table_row(N, THREADS[k], ts, tp, ok && rp == rs);
    }

    printf("\n(b) first occurrence, reduction(min) of r*C+c   (timed: case one; serial stops early)\n");
    BEST_TIME(ts, rs = first_serial(m, R, C, vals[1]));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int ok = 1;
        for (int q = 0; q < 3; q++) ok = ok && first_parallel(m, R, C, vals[q]) == first_s[q];
        BEST_TIME(tp, rp = first_parallel(m, R, C, vals[1]));
        table_row(N, THREADS[k], ts, tp, ok && rp == rs);
    }

    printf("\n(c) list of (row, col) in row-major order, count + scan + write   (timed: case many)\n");
    BEST_TIME(ts, rs = list_serial(m, R, C, vals[0], pr_s[0], pc_s[0]));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int ok = 1;
        for (int q = 0; q < 3; q++) {
            long got = list_parallel(m, R, C, vals[q], pr_p[q], pc_p[q]);
            ok = ok && got == cnt_s[q] && same_longs(pr_p[q], pr_s[q], got)
                    && same_longs(pc_p[q], pc_s[q], got);
        }
        BEST_TIME(tp, rp = list_parallel(m, R, C, vals[0], pr_p[0], pc_p[0]));
        table_row(N, THREADS[k], ts, tp, ok && rp == rs);
    }

    for (int q = 0; q < 3; q++) { free(pr_s[q]); free(pc_s[q]); free(pr_p[q]); free(pc_p[q]); }
    free(m);
    return 0;
}

/* Notes:
 * - `break` is allowed in the INNER loop (not the parallel one): it ends this
 *   row's scan at its first match. You cannot break out of an omp for loop.
 * - First occurrence: the serial loop stops at the first hit; the parallel one
 *   scans every row (no early exit), so if v is near the start, serial wins.
 *   Early-exit options: a shared "found" flag checked per row, or
 *   #pragma omp cancel for (needs OMP_CANCELLATION=true).
 * - reduction(min:first) starts every private copy at the largest long and
 *   combines with min; the initial R*C makes "not found" fall out naturally.
 * - (c) with a shared counter k (atomic capture) would be race-free but the
 *   positions would come out in a random order; count + scan keeps row-major
 *   order without any atomics.
 * - Few rows and many columns: use collapse(2) or parallelise the column loop
 *   (the linear index i = r*C + c works for all three parts).
 */

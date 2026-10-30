/* P062: Row with the largest sum, column with the smallest sum, row maxima
 * Problem: For an N x N integer matrix A find, serially and with OpenMP,
 *   (1) the row with the maximum row sum, (2) the column with the minimum column
 *   sum (ties broken in favour of the smaller index in both cases) and (3) the
 *   maximum element of every row, rmax[i] = max_j A[i][j]. Verify against serial
 *   (including deliberately tied rows/columns) and report the timings.
 * Solution: (1a) per-thread best + critical: each thread scans its rows with a
 *   private (sum, index) best, then merges it into the shared best once inside
 *   "#pragma omp critical". (1b) two phases: all row sums R[i] in parallel (each
 *   thread writes its own R[i]), then an argmax over R with a user-defined
 *   reduction (#pragma omp declare reduction, OpenMP 4.0). (2) column sums with
 *   reduction(+:C[:n]) over rows (contiguous reads; rows share C), then an argmin
 *   with per-thread best + critical. Every merge uses "better value, or equal
 *   value and smaller index", so the answer does not depend on the schedule.
 *   (3) parallel for over rows; each thread writes its own rmax[i].
 *   Matrices are flattened row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (Lab 7 Task 3 max/min with index, applied to a matrix).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p062_row_col_extremes.c -o p062 -lm
 * Run:   ./p062 [N]   (default N = 4000)
 */
#include "common.h"

typedef struct { long val; long idx; } ValIdx;

/* Larger value wins; on a tie the smaller index wins. */
static ValIdx better_max(ValIdx a, ValIdx b) {
    return (a.val > b.val || (a.val == b.val && a.idx < b.idx)) ? a : b;
}
/* Smaller value wins; on a tie the smaller index wins. */
static ValIdx better_min(ValIdx a, ValIdx b) {
    return (a.val < b.val || (a.val == b.val && a.idx < b.idx)) ? a : b;
}

/* User-defined "maxloc" reduction: private copies start at the worst value. */
#pragma omp declare reduction(maxloc : ValIdx : omp_out = better_max(omp_out, omp_in)) \
        initializer(omp_priv = (ValIdx){LONG_MIN, LONG_MAX})

static long row_sum(const int *A, long n, long i) {
    long s = 0;
    for (long j = 0; j < n; j++) s += A[i*n + j];
    return s;
}

/* ---------- (1) row with the maximum sum ---------- */
static ValIdx max_row_serial(const int *A, long n) {
    ValIdx best = {LONG_MIN, LONG_MAX};
    for (long i = 0; i < n; i++) {
        long s = row_sum(A, n, i);
        if (s > best.val) { best.val = s; best.idx = i; }   /* '>' keeps the first */
    }
    return best;
}

/* (1a) per-thread best + one critical merge per thread */
static ValIdx max_row_critical(const int *A, long n) {
    ValIdx best = {LONG_MIN, LONG_MAX};                  /* shared */
    #pragma omp parallel
    {
        ValIdx mine = {LONG_MIN, LONG_MAX};              /* private */
        #pragma omp for nowait
        for (long i = 0; i < n; i++) {
            ValIdx cand = {row_sum(A, n, i), i};
            mine = better_max(mine, cand);
        }
        #pragma omp critical
        best = better_max(best, mine);                   /* T merges, not N */
    }
    return best;
}

/* (1b) all row sums in parallel, then a parallel argmax (declare reduction) */
static ValIdx max_row_two_phase(const int *A, long n, long *R) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) R[i] = row_sum(A, n, i);   /* own R[i]: no race */

    ValIdx best = {LONG_MIN, LONG_MAX};
    #pragma omp parallel for reduction(maxloc:best)
    for (long i = 0; i < n; i++) {
        ValIdx cand = {R[i], i};
        best = better_max(best, cand);
    }
    return best;
}

/* ---------- (2) column with the minimum sum ---------- */
static ValIdx min_col_serial(const int *A, long n, long *C) {
    for (long j = 0; j < n; j++) C[j] = 0;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) C[j] += A[i*n + j];   /* row order: contiguous */
    ValIdx best = {LONG_MAX, LONG_MAX};
    for (long j = 0; j < n; j++)
        if (C[j] < best.val) { best.val = C[j]; best.idx = j; }
    return best;
}

static ValIdx min_col_par(const int *A, long n, long *C) {
    for (long j = 0; j < n; j++) C[j] = 0;
    #pragma omp parallel for reduction(+:C[:n])
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) C[j] += A[i*n + j];   /* private C per thread */

    ValIdx best = {LONG_MAX, LONG_MAX};
    #pragma omp parallel
    {
        ValIdx mine = {LONG_MAX, LONG_MAX};
        #pragma omp for nowait
        for (long j = 0; j < n; j++) {
            ValIdx cand = {C[j], j};
            mine = better_min(mine, cand);
        }
        #pragma omp critical
        best = better_min(best, mine);
    }
    return best;
}

/* ---------- (3) maximum element of every row ---------- */
static void row_max_serial(const int *A, long n, int *rmax) {
    for (long i = 0; i < n; i++) {
        int m = A[i*n];
        for (long j = 1; j < n; j++) if (A[i*n + j] > m) m = A[i*n + j];
        rmax[i] = m;
    }
}
static void row_max_par(const int *A, long n, int *rmax) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        int m = A[i*n];
        for (long j = 1; j < n; j++) if (A[i*n + j] > m) m = A[i*n + j];
        rmax[i] = m;                                     /* own element */
    }
}

static void tiny_example(void) {
    int A[16] = {3, 1, 4, 1,
                 5, 2, 6, 2,
                 2, 1, 3, 1,
                 6, 1, 7, 1};
    long R[4], C[4];
    int rmax[4];
    omp_set_num_threads(4);
    ValIdx r1 = max_row_critical(A, 4), r2 = max_row_two_phase(A, 4, R);
    ValIdx c = min_col_par(A, 4, C);
    row_max_par(A, 4, rmax);
    printf("Worked example: A = [3 1 4 1; 5 2 6 2; 2 1 3 1; 6 1 7 1]\n");
    printf("  row sums 9 15 7 15 -> max row = %ld (sum %ld) / %ld (sum %ld)  (expected 1: tie with row 3)\n",
           r1.idx, r1.val, r2.idx, r2.val);
    printf("  col sums 16 5 20 5 -> min col = %ld (sum %ld)  (expected 1: tie with col 3)\n",
           c.idx, c.val);
    printf("  row maxima = %d %d %d %d  (expected 4 6 3 7)\n", rmax[0], rmax[1], rmax[2], rmax[3]);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 4000);
    int *A = xmalloc((size_t)n * n * sizeof(int));
    long *Rbuf = xmalloc((size_t)n * sizeof(long)), *Cs = xmalloc((size_t)n * sizeof(long));
    long *Cp = xmalloc((size_t)n * sizeof(long));
    int *rms = xmalloc((size_t)n * sizeof(int)), *rmp = xmalloc((size_t)n * sizeof(int));
    fill_int(A, n * n, 0, 99, 2024);
    omp_warmup();

    tiny_example();
    printf("\nP062: N = %ld, entries in [0, 99]\n", n);
    double ts, tp;

    /* Part 1. Force a tie: copy the best row into the LAST row (which belongs to
       the last thread); the answer must still be the original, smaller index. */
    ValIdx rs = max_row_serial(A, n), rp;
    int tie = rs.idx != n - 1;
    if (tie) memcpy(&A[(n - 1)*n], &A[rs.idx*n], (size_t)n * sizeof(int));
    BEST_TIME(ts, rs = max_row_serial(A, n));
    printf("\nPart 1: row with max sum = %ld (sum %ld); %s\n", rs.idx, rs.val,
           tie ? "the last row is a copy of it (tie)" : "no tie forced (it is the last row)");
    printf("(a) per-thread best + critical\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = max_row_critical(A, n));
        table_row(n, THREADS[k], ts, tp, rp.idx == rs.idx && rp.val == rs.val);
    }
    printf("(b) row sums in parallel, then argmax with declare reduction(maxloc)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = max_row_two_phase(A, n, Rbuf));
        table_row(n, THREADS[k], ts, tp, rp.idx == rs.idx && rp.val == rs.val);
    }

    /* Part 2. Force a column tie the same way (copy the min column into column n-1). */
    ValIdx cs = min_col_serial(A, n, Cs), cp;
    tie = cs.idx != n - 1;
    if (tie)
        for (long i = 0; i < n; i++) A[i*n + n - 1] = A[i*n + cs.idx];
    BEST_TIME(ts, cs = min_col_serial(A, n, Cs));
    printf("\nPart 2: column with min sum = %ld (sum %ld); %s\n", cs.idx, cs.val,
           tie ? "the last column is a copy of it (tie)" : "no tie forced (it is the last column)");
    printf("column sums with reduction(+:C[:n]), argmin with per-thread best + critical\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, cp = min_col_par(A, n, Cp));
        table_row(n, THREADS[k], ts, tp,
                  cp.idx == cs.idx && cp.val == cs.val && same_longs(Cs, Cp, n));
    }

    /* Part 3 */
    BEST_TIME(ts, row_max_serial(A, n, rms));
    printf("\nPart 3: maximum element of every row, parallel over rows\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(rmp, 0, (size_t)n * sizeof(int));
        BEST_TIME(tp, row_max_par(A, n, rmp));
        table_row(n, THREADS[k], ts, tp, same_ints(rms, rmp, n));
    }

    free(A); free(Rbuf); free(Cs); free(Cp); free(rms); free(rmp);
    return 0;
}

/* Notes:
 * - reduction(max:v) (OpenMP 3.1) gives the maximum VALUE only; for its index you
 *   need per-thread (value, index) + critical, or a declare reduction on a struct.
 * - The critical section runs once per thread (T times), not once per row; a
 *   critical inside the loop would serialise the whole scan.
 * - Ties: merge with "greater, or equal and smaller index". A plain "if (s > best)"
 *   in each thread plus an unordered merge could return the larger tied index,
 *   depending on which thread enters the critical section first.
 * - Column sums: parallel over rows needs reduction(+:C[:n]) (rows share C);
 *   parallel over columns needs none but reads with stride N.
 * - Sums of ints are kept in long to avoid overflow; integer results match exactly.
 */

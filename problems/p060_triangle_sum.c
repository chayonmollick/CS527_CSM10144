/* P060: Sum of the strictly upper triangle: load balance and schedules
 * Problem: Compute S = sum of A[i][j] over the strictly upper triangle (j > i) of
 *   an N x N integer matrix, serially and with OpenMP parallel over the rows.
 *   Row i has only N-1-i elements, so the work per iteration shrinks with i.
 *   Compare schedule(static), schedule(static, 1), schedule(dynamic, 16) and
 *   schedule(guided), and show the "row pairing" trick (pair row i with row
 *   N-1-i) that makes a plain static schedule balanced. Verify every version
 *   and report the timings and the work done by each thread.
 * Solution: all versions use reduction(+:s) on a long sum (integer data: exact).
 *   static gives thread 0 the first N/T rows, which hold the longest rows (with
 *   T = 4 it gets 7/16 of the work instead of 1/4); (static, 1) deals rows round
 *   robin; dynamic hands out 16 rows at a time to whichever thread is free;
 *   guided starts with big chunks that shrink. Row pairing: iteration p does rows
 *   p and N-1-p, which together hold (N-1-p) + p = N-1 elements, so every
 *   iteration has the same work and static is balanced (for odd N the middle row
 *   is paired with itself and counted once). Matrices are flattened row-major:
 *   A[i][j] = A[i*N + j].
 * Source: lab extension (Eijkhout Ch. 19 loop schedules, triangular loops).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p060_triangle_sum.c -o p060 -lm
 * Run:   ./p060 [N]   (default N = 5000)
 */
#include "common.h"

/* Sum of row i to the right of the diagonal: j = i+1 .. n-1 (n-1-i elements). */
static long upper_row(const int *A, long n, long i) {
    long s = 0;
    for (long j = i + 1; j < n; j++) s += A[i*n + j];
    return s;
}

static long tri_serial(const int *A, long n) {
    long s = 0;
    for (long i = 0; i < n; i++) s += upper_row(A, n, i);
    return s;
}

static long tri_static(const int *A, long n) {
    long s = 0;
    #pragma omp parallel for reduction(+:s) schedule(static)
    for (long i = 0; i < n; i++) s += upper_row(A, n, i);
    return s;
}

static long tri_static1(const int *A, long n) {
    long s = 0;
    #pragma omp parallel for reduction(+:s) schedule(static, 1)
    for (long i = 0; i < n; i++) s += upper_row(A, n, i);
    return s;
}

static long tri_dynamic(const int *A, long n) {
    long s = 0;
    #pragma omp parallel for reduction(+:s) schedule(dynamic, 16)
    for (long i = 0; i < n; i++) s += upper_row(A, n, i);
    return s;
}

static long tri_guided(const int *A, long n) {
    long s = 0;
    #pragma omp parallel for reduction(+:s) schedule(guided)
    for (long i = 0; i < n; i++) s += upper_row(A, n, i);
    return s;
}

/* Row pairing: iteration p handles rows p and n-1-p (equal total work per p). */
static long tri_pairing(const int *A, long n) {
    long s = 0;
    #pragma omp parallel for reduction(+:s) schedule(static)
    for (long p = 0; p < (n + 1) / 2; p++) {
        long q = n - 1 - p;
        s += upper_row(A, n, p);
        if (q != p) s += upper_row(A, n, q);   /* odd n: middle row only once */
    }
    return s;
}

/* Elements summed by each thread (work balance), for static vs pairing. */
static void work_per_thread(long n, int pairing, int nt, long *work) {
    for (int t = 0; t < nt; t++) work[t] = 0;
    omp_set_num_threads(nt);
    if (!pairing) {
        #pragma omp parallel for schedule(static)
        for (long i = 0; i < n; i++) work[omp_get_thread_num()] += n - 1 - i;
    } else {
        #pragma omp parallel for schedule(static)
        for (long p = 0; p < (n + 1) / 2; p++) {
            long q = n - 1 - p;
            work[omp_get_thread_num()] += (n - 1 - p) + (q != p ? n - 1 - q : 0);
        }
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 5000);
    int *A = xmalloc((size_t)n * n * sizeof(int));
    fill_int(A, n * n, -100, 100, 9);
    omp_warmup();

    int B[16];
    for (int k = 0; k < 16; k++) B[k] = k + 1;
    printf("Worked example: A = [1 2 3 4; 5 6 7 8; 9 10 11 12; 13 14 15 16]\n");
    printf("  strictly upper sum = %ld (2+3+4 + 7+8 + 12 = 36), pairing gives %ld\n",
           tri_static(B, 4), tri_pairing(B, 4));

    long rs = 0, rp = 0;
    double ts, tp;
    BEST_TIME(ts, rs = tri_serial(A, n));
    printf("\nP060: N = %ld, strictly upper triangle has %ld elements, serial sum = %ld\n",
           n, n * (n - 1) / 2, rs);

    const char *title[5] = {
        "(1) schedule(static): contiguous blocks of rows, thread 0 gets the long rows",
        "(2) schedule(static, 1): rows dealt round robin",
        "(3) schedule(dynamic, 16): 16 rows at a time to any free thread",
        "(4) schedule(guided): shrinking chunks",
        "(5) row pairing (p with N-1-p), schedule(static): equal work per iteration"};
    long (*fn[5])(const int *, long) = {tri_static, tri_static1, tri_dynamic, tri_guided,
                                        tri_pairing};
    for (int v = 0; v < 5; v++) {
        printf("\n%s\n", title[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            rp = -1;
            BEST_TIME(tp, rp = fn[v](A, n));
            table_row(n, THREADS[k], ts, tp, rs == rp);
        }
    }

    long work[4];
    printf("\nElements summed by each of 4 threads (ideal: %ld each):\n", n * (n - 1) / 2 / 4);
    work_per_thread(n, 0, 4, work);
    printf("  static     : %ld %ld %ld %ld\n", work[0], work[1], work[2], work[3]);
    work_per_thread(n, 1, 4, work);
    printf("  row pairing: %ld %ld %ld %ld\n", work[0], work[1], work[2], work[3]);

    free(A);
    return 0;
}

/* Notes:
 * - Row i has N-1-i elements: the work is triangular. static splits the ROWS
 *   evenly, not the WORK: with 4 threads, thread 0 gets 1 - (3/4)^2 = 7/16 of the
 *   elements, so the speedup is capped near 16/7 = 2.3.
 * - collapse(2) cannot fix it in OpenMP 4.5: j starts at i+1, so the nest is not
 *   rectangular. Fixes: (static,1), dynamic, guided, or reshape the loop (pairing).
 * - Pairing rows p and N-1-p gives every iteration exactly N-1 elements, so the
 *   cheapest schedule (static, no runtime bookkeeping) is balanced.
 * - dynamic has a small cost per chunk (shared counter); chunk 16 keeps it low.
 *   guided starts with N/T-sized chunks, which shrink as the loop proceeds; here
 *   its first, biggest chunks hold the LONGEST rows, so it balances worse than
 *   dynamic. guided suits loops whose cost GROWS with i (e.g. this loop reversed).
 * - reduction(+:s) is still needed: every row adds into the same s.
 */

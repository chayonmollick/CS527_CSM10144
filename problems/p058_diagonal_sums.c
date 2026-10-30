/* P058: Trace, anti-diagonal, border and all diagonal sums
 * Problem: For an N x N integer matrix A compute, serially and with OpenMP:
 *   (1) the trace (sum of the main diagonal A[i][i]), the anti-diagonal sum
 *   (elements with i + j = N-1) and the sum of the border elements (first and
 *   last row and column, each corner counted once); (2) the sum of every one of
 *   the 2N-1 diagonals j - i = d, d = -(N-1)..N-1, stored in D[d + N-1], parallel
 *   over the diagonals. Verify against serial and report the timings.
 * Solution: (1) one parallel for over the rows with reduction(+:tr, an, bo): row i
 *   contributes A[i][i], A[i][N-1-i] and its border elements (the whole row if it
 *   is the first or last row, else its two end elements), so no element is
 *   counted twice. (2a) parallel for over the diagonal index k: each thread sums
 *   whole diagonals into a private s and writes D[k] once (race-free); diagonal
 *   lengths run 1, 2, .., N, .., 2, 1, so schedule(dynamic, 16) balances them.
 *   (2b) cache-friendly alternative: walk the matrix row by row and add A[i][j]
 *   into D[j - i + N-1]; rows share D, so use reduction(+:D[:2N-1]).
 *   Integer data with long sums, so results must match exactly. Matrices are
 *   flattened row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (matrix index regions).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p058_diagonal_sums.c -o p058 -lm
 * Run:   ./p058 [N]   (default N = 3000)
 */
#include "common.h"

/* ---------- (1) trace, anti-diagonal, border: O(N) work ---------- */
static void sums3_serial(const int *A, long n, long *tr, long *an, long *bo) {
    long t = 0, a = 0, b = 0;
    for (long i = 0; i < n; i++) {
        t += A[i*n + i];
        a += A[i*n + (n - 1 - i)];
        if (i == 0 || i == n - 1)
            for (long j = 0; j < n; j++) b += A[i*n + j];   /* top / bottom row */
        else
            b += A[i*n] + A[i*n + n - 1];                     /* left + right end */
    }
    *tr = t; *an = a; *bo = b;
}

static void sums3_parallel(const int *A, long n, long *tr, long *an, long *bo) {
    long t = 0, a = 0, b = 0;                 /* reduction needs plain variables */
    #pragma omp parallel for reduction(+:t, a, b)
    for (long i = 0; i < n; i++) {
        t += A[i*n + i];
        a += A[i*n + (n - 1 - i)];
        if (i == 0 || i == n - 1)
            for (long j = 0; j < n; j++) b += A[i*n + j];
        else
            b += A[i*n] + A[i*n + n - 1];
    }
    *tr = t; *an = a; *bo = b;
}

/* ---------- (2) all 2N-1 diagonal sums, D[k], k = d + N-1, d = j - i ---------- */
/* Serial reference: row-by-row (cache-friendly). */
static void diag_sums_serial(const int *A, long n, long *D) {
    for (long k = 0; k < 2*n - 1; k++) D[k] = 0;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            D[j - i + n - 1] += A[i*n + j];
}

/* (2a) parallel over diagonals: thread owns D[k]; walks A with stride N+1. */
static void diag_sums_by_diagonal(const int *A, long n, long *D) {
    #pragma omp parallel for schedule(dynamic, 16)
    for (long k = 0; k < 2*n - 1; k++) {
        long d = k - (n - 1);                 /* d = j - i */
        long i0 = d < 0 ? -d : 0;             /* first element is (i0, i0 + d) */
        long len = n - (d < 0 ? -d : d);      /* diagonal length N - |d| */
        long s = 0;
        for (long t = 0; t < len; t++) s += A[(i0 + t)*n + (i0 + t + d)];
        D[k] = s;                             /* one write per k: no race */
    }
}

/* (2b) parallel over rows: rows share D -> array reduction. */
static void diag_sums_by_row(const int *A, long n, long *D) {
    long nd = 2*n - 1;
    for (long k = 0; k < nd; k++) D[k] = 0;
    #pragma omp parallel for reduction(+:D[:nd])
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            D[j - i + n - 1] += A[i*n + j];   /* private copy of D per thread */
}

static void tiny_example(void) {
    int A[16];
    long tr, an, bo, D[7];
    for (int k = 0; k < 16; k++) A[k] = k + 1;          /* 1..16 row by row */
    sums3_parallel(A, 4, &tr, &an, &bo);
    diag_sums_by_diagonal(A, 4, D);
    printf("Worked example: A = [ 1 2 3 4; 5 6 7 8; 9 10 11 12; 13 14 15 16 ]\n");
    printf("  trace = %ld (1+6+11+16 = 34), anti-diagonal = %ld (4+7+10+13 = 34), "
           "border = %ld (102)\n", tr, an, bo);
    printf("  diagonal sums for d = j-i = -3..3: ");
    for (int k = 0; k < 7; k++) printf("%ld ", D[k]);
    printf("  (expected 13 23 30 34 21 11 4)\n");
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 3000);
    long nd = 2*n - 1;
    int *A = xmalloc((size_t)n * n * sizeof(int));
    long *Ds = xmalloc((size_t)nd * sizeof(long)), *Dp = xmalloc((size_t)nd * sizeof(long));
    fill_int(A, n * n, -50, 50, 7);
    omp_warmup();

    tiny_example();
    printf("\nP058: N = %ld, integer entries in [-50, 50]\n", n);

    /* Part 1: trace, anti-diagonal, border (all three checked together) */
    long ts3[3], tp3[3];
    double ts, tp;
    BEST_TIME(ts, sums3_serial(A, n, &ts3[0], &ts3[1], &ts3[2]));
    printf("serial: trace = %ld, anti-diagonal = %ld, border = %ld\n", ts3[0], ts3[1], ts3[2]);
    printf("\nPart 1: trace + anti-diagonal + border in one loop, reduction(+:t, a, b)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        tp3[0] = tp3[1] = tp3[2] = -1;
        BEST_TIME(tp, sums3_parallel(A, n, &tp3[0], &tp3[1], &tp3[2]));
        table_row(n, THREADS[k], ts, tp, same_longs(ts3, tp3, 3));
    }

    /* Part 2: all diagonal sums */
    BEST_TIME(ts, diag_sums_serial(A, n, Ds));
    long total = 0, dtotal = 0;
    for (long k = 0; k < n * n; k++) total += A[k];
    for (long k = 0; k < nd; k++) dtotal += Ds[k];
    printf("\nPart 2: the %ld diagonal sums (serial = row-by-row loop); "
           "sum of all D = %ld, sum of A = %ld: %s\n", nd, dtotal, total, ok_str(dtotal == total));
    printf("(a) parallel over diagonals k, schedule(dynamic, 16), stride N+1 access\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Dp, 0, (size_t)nd * sizeof(long));
        BEST_TIME(tp, diag_sums_by_diagonal(A, n, Dp));
        table_row(n, THREADS[k], ts, tp, same_longs(Ds, Dp, nd));
    }
    printf("(b) parallel over rows i, reduction(+:D[:2N-1]), contiguous access\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(Dp, 0, (size_t)nd * sizeof(long));
        BEST_TIME(tp, diag_sums_by_row(A, n, Dp));
        table_row(n, THREADS[k], ts, tp, same_longs(Ds, Dp, nd));
    }

    free(A); free(Ds); free(Dp);
    return 0;
}

/* Notes:
 * - Index regions: main diagonal i == j, anti-diagonal i + j == N-1, diagonal d is
 *   j - i == d (d > 0 above the main diagonal), border i == 0 || i == N-1 ||
 *   j == 0 || j == N-1. For odd N the centre element is on both diagonals.
 * - Part 1 is only O(N) work: the parallel version is no faster (fork/join costs
 *   microseconds); parallelism pays only for the O(N^2) diagonal sums.
 * - (2a) is race-free because each k writes only D[k]; the per-diagonal sum s is
 *   private (declared in the loop). Diagonal lengths differ (1..N), so a plain
 *   static schedule gives the threads with the middle diagonals most of the work.
 * - (2a) reads A with stride N+1 (a new cache line per element); (2b) reads rows
 *   contiguously but every row touches 2N-1 different D entries, so it needs
 *   reduction(+:D[:nd]) (private D per thread, combined at the end). Without the
 *   reduction, two rows adding into the same D[k] race and lose updates.
 * - Integer sums are exact and order-independent, so same_longs must match exactly.
 */

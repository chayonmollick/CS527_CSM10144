/* P063: Rotate a square matrix by 90 degrees (out of place and in place)
 * Problem: Rotate an N x N integer matrix A by 90 degrees clockwise and
 *   anticlockwise, serially and with OpenMP: (1) out of place into a new matrix B
 *   (clockwise: B[j][N-1-i] = A[i][j]; anticlockwise: B[N-1-j][i] = A[i][j]) and
 *   (2) in place, without a second matrix (clockwise = transpose in place, then
 *   reverse every row; anticlockwise = transpose in place, then reverse the order
 *   of the rows). Verify the versions against each other, show 3x3 and 4x4
 *   examples and report the timings.
 * Solution: (1) a rotation maps every (i, j) to a different (row, col) of B, so a
 *   parallel for over i writes distinct elements: no race. (2) one parallel
 *   region, two omp for loops. Phase 1, transpose: iteration i swaps A[i][j] with
 *   A[j][i] for j > i only, so each pair is swapped exactly once and by one thread
 *   (rows have unequal work -> schedule(dynamic, 16)). Phase 2 (reverse each row,
 *   or swap row i with row N-1-i for i < N/2) touches disjoint pairs. The implicit
 *   barrier at the end of the first omp for guarantees phase 2 starts only after
 *   the whole transpose is finished. Matrices are flattened row-major:
 *   A[i][j] = A[i*N + j].
 * Source: lab extension (classic interview problem "rotate image").
 * Build: gcc -O2 -Wall -Wextra -fopenmp p063_rotate_matrix.c -o p063 -lm
 * Run:   ./p063 [N]   (default N = 3000)
 */
#include "common.h"

static void swap_int(int *a, int *b) { int t = *a; *a = *b; *b = t; }

/* ---------- (1) out of place ---------- */
static void rot_cw_out_serial(const int *A, int *B, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            B[j*n + (n - 1 - i)] = A[i*n + j];
}
static void rot_cw_out_par(const int *A, int *B, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            B[j*n + (n - 1 - i)] = A[i*n + j];      /* distinct target per (i, j) */
}
static void rot_acw_out_serial(const int *A, int *B, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            B[(n - 1 - j)*n + i] = A[i*n + j];
}
static void rot_acw_out_par(const int *A, int *B, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            B[(n - 1 - j)*n + i] = A[i*n + j];
}

/* ---------- (2) in place ---------- */
static void rot_cw_in_serial(int *A, long n) {
    for (long i = 0; i < n; i++)                        /* transpose */
        for (long j = i + 1; j < n; j++) swap_int(&A[i*n + j], &A[j*n + i]);
    for (long i = 0; i < n; i++)                        /* reverse each row */
        for (long j = 0; j < n / 2; j++) swap_int(&A[i*n + j], &A[i*n + (n - 1 - j)]);
}
static void rot_cw_in_par(int *A, long n) {
    #pragma omp parallel
    {
        #pragma omp for schedule(dynamic, 16)
        for (long i = 0; i < n; i++)                    /* phase 1: transpose */
            for (long j = i + 1; j < n; j++) swap_int(&A[i*n + j], &A[j*n + i]);
        /* implicit barrier: no thread starts phase 2 before phase 1 is complete */
        #pragma omp for
        for (long i = 0; i < n; i++)                    /* phase 2: reverse each row */
            for (long j = 0; j < n / 2; j++) swap_int(&A[i*n + j], &A[i*n + (n - 1 - j)]);
    }
}
static void rot_acw_in_serial(int *A, long n) {
    for (long i = 0; i < n; i++)                        /* transpose */
        for (long j = i + 1; j < n; j++) swap_int(&A[i*n + j], &A[j*n + i]);
    for (long i = 0; i < n / 2; i++)                    /* swap row i, row n-1-i */
        for (long j = 0; j < n; j++) swap_int(&A[i*n + j], &A[(n - 1 - i)*n + j]);
}
static void rot_acw_in_par(int *A, long n) {
    #pragma omp parallel
    {
        #pragma omp for schedule(dynamic, 16)
        for (long i = 0; i < n; i++)
            for (long j = i + 1; j < n; j++) swap_int(&A[i*n + j], &A[j*n + i]);
        /* implicit barrier */
        #pragma omp for
        for (long i = 0; i < n / 2; i++)                /* only the first half */
            for (long j = 0; j < n; j++) swap_int(&A[i*n + j], &A[(n - 1 - i)*n + j]);
    }
}

static void print_mat(const char *name, const int *A, long n) {
    printf("  %s:\n", name);
    for (long i = 0; i < n; i++) {
        printf("   ");
        for (long j = 0; j < n; j++) printf(" %3d", A[i*n + j]);
        printf("\n");
    }
}

static void tiny_examples(void) {
    int A[9], B[9], C[16];
    for (int k = 0; k < 9; k++) A[k] = k + 1;
    for (int k = 0; k < 16; k++) C[k] = k + 1;
    printf("Worked examples:\n");
    print_mat("A (3x3)", A, 3);
    rot_cw_out_par(A, B, 3);
    print_mat("clockwise, out of place      (expected 7 4 1 / 8 5 2 / 9 6 3)", B, 3);
    rot_acw_in_par(A, 3);
    print_mat("anticlockwise, in place      (expected 3 6 9 / 2 5 8 / 1 4 7)", A, 3);
    print_mat("A (4x4)", C, 4);
    rot_cw_in_par(C, 4);
    print_mat("clockwise, in place (expected 13 9 5 1 / 14 10 6 2 / 15 11 7 3 / 16 12 8 4)", C, 4);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 3000);
    long nn = n * n;
    size_t bytes = (size_t)nn * sizeof(int);
    int *A0 = xmalloc(bytes), *Rcw = xmalloc(bytes), *Racw = xmalloc(bytes), *W = xmalloc(bytes);
    fill_int(A0, nn, 0, 1000000, 63);
    omp_warmup();

    tiny_examples();
    printf("\nP063: rotate an N x N matrix, N = %ld\n", n);

    double ts, tp;
    BEST_TIME(ts, rot_cw_out_serial(A0, Rcw, n));          /* references */
    rot_acw_out_serial(A0, Racw, n);

    printf("\n(1a) clockwise, out of place: B[j][N-1-i] = A[i][j], parallel over i\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(W, 0, bytes);
        BEST_TIME(tp, rot_cw_out_par(A0, W, n));
        table_row(n, THREADS[k], ts, tp, same_ints(Rcw, W, nn));
    }

    BEST_TIME(ts, rot_acw_out_serial(A0, W, n));
    printf("(1b) anticlockwise, out of place: B[N-1-j][i] = A[i][j], parallel over i\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(W, 0, bytes);
        BEST_TIME(tp, rot_acw_out_par(A0, W, n));
        table_row(n, THREADS[k], ts, tp, same_ints(Racw, W, nn));
    }

    /* In place: time repeated rotations of W (same cost each time), then verify
       ONE rotation of a fresh copy against the out-of-place result. */
    memcpy(W, A0, bytes);
    BEST_TIME(ts, rot_cw_in_serial(W, n));
    printf("(2a) clockwise, in place: transpose, barrier, reverse each row\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rot_cw_in_par(W, n));
        memcpy(W, A0, bytes);
        rot_cw_in_par(W, n);
        table_row(n, THREADS[k], ts, tp, same_ints(Rcw, W, nn));
    }

    memcpy(W, A0, bytes);
    BEST_TIME(ts, rot_acw_in_serial(W, n));
    printf("(2b) anticlockwise, in place: transpose, barrier, swap row i with row N-1-i\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rot_acw_in_par(W, n));
        memcpy(W, A0, bytes);
        rot_acw_in_par(W, n);
        table_row(n, THREADS[k], ts, tp, same_ints(Racw, W, nn));
    }

    /* Algebraic checks: four clockwise turns = identity; cw then acw = identity. */
    omp_set_num_threads(4);
    memcpy(W, A0, bytes);
    for (int t = 0; t < 4; t++) rot_cw_in_par(W, n);
    int four = same_ints(A0, W, nn);
    memcpy(W, A0, bytes);
    rot_cw_in_par(W, n);
    rot_acw_in_par(W, n);
    printf("\nChecks: 4 clockwise turns = A: %s, clockwise then anticlockwise = A: %s\n",
           ok_str(four), ok_str(same_ints(A0, W, nn)));

    free(A0); free(Rcw); free(Racw); free(W);
    return 0;
}

/* Notes:
 * - Out of place is the simple, race-free choice: each A[i][j] goes to its own
 *   B element. Either the reads or the writes are strided (a rotation turns rows
 *   into columns), so it is memory-bound.
 * - In-place transpose must loop over j > i only: looping over all j would swap
 *   every pair twice (no change) and two threads could swap the same pair at once.
 * - The two phases must not overlap: the implicit barrier at the end of the first
 *   "omp for" (do NOT add nowait there) makes every thread wait until the
 *   transpose is complete before any row is reversed.
 * - Triangular transpose: row i has N-1-i swaps, so use dynamic/static,1; the
 *   row reversal has equal work per row, so the default static is fine.
 * - Rotations: cw = transpose + reverse rows; acw = transpose + reverse the row
 *   order (or reverse rows first, then transpose); 180 = reverse the whole array.
 */

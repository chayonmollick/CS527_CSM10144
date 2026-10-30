/* P064: Flip a matrix horizontally, vertically and rotate it by 180 degrees in place
 * Problem: For an M x N integer matrix A, write serial and OpenMP functions that,
 *   in place, (1) flip it horizontally (reverse every row), (2) flip it
 *   vertically (swap row i with row M-1-i) and (3) rotate it by 180 degrees
 *   (A[i][j] <-> A[M-1-i][N-1-j]). Parallelise only over the first half where
 *   each swap pairs two elements, and explain why that is race-free. Verify the
 *   results and report the timings.
 * Solution: an in-place flip is a set of disjoint swaps. Looping over the first
 *   half only makes each pair belong to exactly one iteration, so no two threads
 *   ever touch the same element. (1) parallel for over rows i, inner j < N/2 swaps
 *   A[i][j] with A[i][N-1-j] (each row belongs to one thread). (2) parallel for
 *   over i < M/2, swapping whole rows i and M-1-i. (3) the 180-degree rotation
 *   reverses the flattened array: parallel for over k < M*N/2 swapping A[k] with
 *   A[M*N-1-k]. For odd sizes the middle row/column/element stays in place.
 *   Matrices are flattened row-major: A[i][j] = A[i*N + j].
 * Source: lab extension (image flips: mirror / upside down).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p064_flip_matrix.c -o p064 -lm
 * Run:   ./p064 [M] [N]   (defaults M = N = 3000)
 */
#include "common.h"

static void swap_int(int *a, int *b) { int t = *a; *a = *b; *b = t; }

/* ---------- (1) horizontal flip: reverse every row ---------- */
static void flip_h_serial(int *A, long m, long n) {
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n / 2; j++) swap_int(&A[i*n + j], &A[i*n + (n - 1 - j)]);
}
static void flip_h_par(int *A, long m, long n) {
    #pragma omp parallel for
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n / 2; j++)                 /* first half of the row */
            swap_int(&A[i*n + j], &A[i*n + (n - 1 - j)]);
}

/* ---------- (2) vertical flip: swap row i with row m-1-i ---------- */
static void flip_v_serial(int *A, long m, long n) {
    for (long i = 0; i < m / 2; i++)
        for (long j = 0; j < n; j++) swap_int(&A[i*n + j], &A[(m - 1 - i)*n + j]);
}
static void flip_v_par(int *A, long m, long n) {
    #pragma omp parallel for
    for (long i = 0; i < m / 2; i++)                     /* first half of the rows */
        for (long j = 0; j < n; j++) swap_int(&A[i*n + j], &A[(m - 1 - i)*n + j]);
}

/* ---------- (3) 180-degree rotation: reverse the flattened array ---------- */
static void rot180_serial(int *A, long m, long n) {
    long total = m * n;
    for (long k = 0; k < total / 2; k++) swap_int(&A[k], &A[total - 1 - k]);
}
static void rot180_par(int *A, long m, long n) {
    long total = m * n;
    #pragma omp parallel for
    for (long k = 0; k < total / 2; k++)                 /* first half of the elements */
        swap_int(&A[k], &A[total - 1 - k]);
}

static void print_mat(const char *name, const int *A, long m, long n) {
    printf("  %s:\n", name);
    for (long i = 0; i < m; i++) {
        printf("   ");
        for (long j = 0; j < n; j++) printf(" %3d", A[i*n + j]);
        printf("\n");
    }
}

static void tiny_example(void) {
    int A[12], H[12], V[12], R[12];
    for (int k = 0; k < 12; k++) A[k] = k + 1;
    memcpy(H, A, sizeof A); memcpy(V, A, sizeof A); memcpy(R, A, sizeof A);
    flip_h_par(H, 3, 4);
    flip_v_par(V, 3, 4);
    rot180_par(R, 3, 4);
    printf("Worked example (3x4):\n");
    print_mat("A", A, 3, 4);
    print_mat("horizontal flip (rows reversed)", H, 3, 4);
    print_mat("vertical flip (row order reversed; middle row stays)", V, 3, 4);
    print_mat("rotate 180", R, 3, 4);
}

int main(int argc, char *argv[]) {
    long m = arg_long(argc, argv, 1, 3000);
    long n = arg_long(argc, argv, 2, m);
    long mn = m * n;
    size_t bytes = (size_t)mn * sizeof(int);
    int *A0 = xmalloc(bytes), *Ref = xmalloc(bytes), *W = xmalloc(bytes);
    fill_int(A0, mn, 0, 1000000, 64);
    omp_warmup();

    tiny_example();
    printf("\nP064: in-place flips of a %ld x %ld matrix (the tables' N column shows M*N)\n",
           m, n);

    const char *title[3] = {
        "(1) horizontal flip: parallel over rows i, swap A[i][j] <-> A[i][N-1-j] for j < N/2",
        "(2) vertical flip: parallel over i < M/2, swap row i <-> row M-1-i",
        "(3) rotate 180: parallel over k < M*N/2, swap A[k] <-> A[M*N-1-k]"};
    void (*serial_fn[3])(int *, long, long) = {flip_h_serial, flip_v_serial, rot180_serial};
    void (*par_fn[3])(int *, long, long) = {flip_h_par, flip_v_par, rot180_par};

    double ts, tp;
    for (int v = 0; v < 3; v++) {
        memcpy(Ref, A0, bytes);
        serial_fn[v](Ref, m, n);                         /* reference result */
        memcpy(W, A0, bytes);
        BEST_TIME(ts, serial_fn[v](W, m, n));            /* repeated flips: same cost */
        printf("\n%s\n", title[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            BEST_TIME(tp, par_fn[v](W, m, n));
            memcpy(W, A0, bytes);                        /* verify ONE flip of A0 */
            par_fn[v](W, m, n);
            table_row(mn, THREADS[k], ts, tp, same_ints(Ref, W, mn));
        }
    }

    /* Algebraic checks (4 threads): a flip is its own inverse; H then V = 180. */
    omp_set_num_threads(4);
    memcpy(W, A0, bytes);
    flip_h_par(W, m, n); flip_h_par(W, m, n);
    int hh = same_ints(A0, W, mn);
    memcpy(W, A0, bytes);
    flip_h_par(W, m, n); flip_v_par(W, m, n);
    memcpy(Ref, A0, bytes);
    rot180_par(Ref, m, n);
    printf("\nChecks: flip_h twice = A: %s, flip_h then flip_v = rotate 180: %s\n",
           ok_str(hh), ok_str(same_ints(Ref, W, mn)));

    /* Pitfall (serial, so no race): swapping over ALL rows undoes every swap. */
    memcpy(W, A0, bytes);
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) swap_int(&W[i*n + j], &W[(m - 1 - i)*n + j]);
    printf("Pitfall: vertical flip looping over all M rows leaves A unchanged: %s\n",
           same_ints(A0, W, mn) ? "yes (each pair swapped twice)" : "no");

    free(A0); free(Ref); free(W);
    return 0;
}

/* Notes:
 * - Race-freedom: the swaps are disjoint pairs and the loop visits each pair once
 *   (first half only), so every element is read and written by exactly one thread.
 * - If the loop ran over ALL rows, iteration i and iteration M-1-i would swap the
 *   same pair: serially the flip is undone (shown above); in parallel two threads
 *   may swap the same pair at the same time -> data race, unpredictable result.
 * - Odd sizes: the middle row (vertical) / middle column (horizontal) / middle
 *   element (180) pairs with itself and is simply never visited (M/2 rounds down).
 * - (1) has M iterations of N/2 swaps; (2) has only M/2 iterations of N swaps
 *   (whole rows, contiguous); (3) is one flat loop of M*N/2 swaps. All are
 *   memory-bound: speedup stops at memory bandwidth.
 * - collapse(2) is also legal for (1): for (i < M) for (j < N/2) is rectangular.
 */

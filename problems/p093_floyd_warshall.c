/* P093: All-pairs shortest paths (Floyd-Warshall)
 * Problem: Given a dense weighted directed graph on n vertices as an n x n
 *   matrix d (d[i][j] = weight of edge i->j, INF if there is no edge, 0 on the
 *   diagonal), compute the length of the shortest path between every pair of
 *   vertices with the Floyd-Warshall algorithm:
 *       for k: for i: for j: d[i][j] = min(d[i][j], d[i][k] + d[k][j])
 *   Parallelise it with OpenMP, verify against the serial version on a random
 *   graph, print the result for a small 4-vertex example, and report time and
 *   speedup for 1, 2, 4 and 8 threads.
 * Solution: the k loop must stay sequential: step k uses the matrix produced by
 *   step k-1. For a fixed k the (i, j) updates are independent: they read row k
 *   and column k, which do not change during step k (d[i][k] + d[k][k] = d[i][k]
 *   because d[k][k] = 0, so no update of row/column k is ever an improvement),
 *   and each d[i][j] is written by the one thread that owns row i. So the i loop
 *   is a parallel for, with a barrier between steps.
 *   (a) #pragma omp parallel for on the i loop inside the k loop (n fork/joins).
 *   (b) one parallel region around the k loop; every thread runs the k loop and
 *       #pragma omp for splits the i loop; its implicit barrier ends step k.
 * Source: classic dynamic-programming / graph exercise (lab extension).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p093_floyd_warshall.c -o p093 -lm
 * Run:   ./p093 [n]          (default n = 500)
 */
#include "common.h"

#define INF (INT_MAX / 2)       /* "no edge"; INF + INF still fits in an int */

/* Random digraph: edge i->j with probability ~1/4, weight 1..100. */
static void make_graph(int *d, long n, unsigned long long seed) {
    unsigned long long s = seed;
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) {
            unsigned long r = lcg_next(&s);
            if (i == j)          d[i * n + j] = 0;
            else if (r % 4 == 0) d[i * n + j] = 1 + (int)((r >> 2) % 100);
            else                 d[i * n + j] = INF;
        }
}

static void fw_serial(int *d, long n) {
    for (long k = 0; k < n; k++)
        for (long i = 0; i < n; i++) {
            int dik = d[i * n + k];
            if (dik == INF) continue;             /* no path i->k: nothing improves */
            for (long j = 0; j < n; j++) {
                int t = dik + d[k * n + j];
                if (t < d[i * n + j]) d[i * n + j] = t;
            }
        }
}

/* (a) parallel for on i inside the sequential k loop. */
static void fw_parallel_for(int *d, long n) {
    for (long k = 0; k < n; k++) {
        #pragma omp parallel for schedule(static)
        for (long i = 0; i < n; i++) {
            int dik = d[i * n + k];
            if (dik == INF) continue;
            for (long j = 0; j < n; j++) {
                int t = dik + d[k * n + j];
                if (t < d[i * n + j]) d[i * n + j] = t;
            }
        }
    }
}

/* (b) one parallel region; the implicit barrier of "omp for" separates steps. */
static void fw_one_region(int *d, long n) {
    #pragma omp parallel
    for (long k = 0; k < n; k++) {               /* every thread runs all k (k is private) */
        #pragma omp for schedule(static)
        for (long i = 0; i < n; i++) {
            int dik = d[i * n + k];
            if (dik == INF) continue;
            for (long j = 0; j < n; j++) {
                int t = dik + d[k * n + j];
                if (t < d[i * n + j]) d[i * n + j] = t;
            }
        }                                          /* implicit barrier: step k done */
    }
}

static void print_matrix(const int *d, long n) {
    for (long i = 0; i < n; i++) {
        printf("   ");
        for (long j = 0; j < n; j++) {
            if (d[i * n + j] == INF) printf(" %4s", "INF");
            else                     printf(" %4d", d[i * n + j]);
        }
        printf("\n");
    }
}

/* Small example: 4 vertices, edges 0->1 (3), 0->3 (7), 1->0 (8), 1->2 (2),
 * 2->0 (5), 2->3 (1), 3->0 (2). */
static void small_example(void) {
    const int I = INF;
    int d[16] = {0, 3, I, 7,
                 8, 0, 2, I,
                 5, I, 0, 1,
                 2, I, I, 0};
    const int expect[16] = {0, 3, 5, 6,
                            5, 0, 2, 3,
                            3, 6, 0, 1,
                            2, 5, 7, 0};
    printf("4-vertex example, input (INF = no edge):\n");
    print_matrix(d, 4);
    fw_one_region(d, 4);
    printf("shortest distances (parallel):  %s\n", ok_str(same_ints(d, expect, 16)));
    print_matrix(d, 4);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 500);
    int *g  = xmalloc((size_t)(n * n) * sizeof(int));   /* the input graph */
    int *ds = xmalloc((size_t)(n * n) * sizeof(int));   /* serial result */
    int *dp = xmalloc((size_t)(n * n) * sizeof(int));   /* parallel result */
    make_graph(g, n, 7);
    omp_warmup();
    printf("P093: Floyd-Warshall, n = %ld vertices\n", n);
    small_example();

    /* The algorithm works in place, so every timed run starts from a fresh copy
     * of g (the memcpy is O(n^2), negligible next to the O(n^3) work). */
    size_t bytes = (size_t)(n * n) * sizeof(int);
    double ts, tp;
    BEST_TIME(ts, (memcpy(ds, g, bytes), fw_serial(ds, n)));
    long reach = 0;
    for (long x = 0; x < n * n; x++) reach += ds[x] < INF;
    printf("serial: %ld of %ld ordered pairs reachable, d[0][n-1] = %d\n",
           reach, n * n, ds[n - 1]);

    printf("(a) parallel for on i inside the k loop\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, (memcpy(dp, g, bytes), fw_parallel_for(dp, n)));
        table_row(n, THREADS[t], ts, tp, same_ints(ds, dp, n * n));
    }
    printf("(b) one parallel region, omp for on i (barrier per k)\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, (memcpy(dp, g, bytes), fw_one_region(dp, n)));
        table_row(n, THREADS[t], ts, tp, same_ints(ds, dp, n * n));
    }
    free(g); free(ds); free(dp);
    return 0;
}

/* Notes:
 * - The k loop cannot be parallel: step k needs the paths through 0..k-1 found
 *   by the earlier steps (a loop-carried dependence).
 * - No race inside step k: d[i][j] is written only by the thread that owns row
 *   i; row k and column k are read by everyone but never change in step k
 *   (d[k][k] = 0, so min(d[k][j], d[k][k] + d[k][j]) = d[k][j]).
 * - (b) avoids n fork/joins: the threads stay alive and meet at the implicit
 *   barrier of each omp for. Never put nowait there: step k+1 would read rows
 *   that step k is still updating.
 * - The j loop is innermost so rows are read contiguously (cache friendly, and
 *   the compiler can vectorise it); the work is O(n^3), the memory O(n^2).
 * - INF = INT_MAX/2 so INF + INF does not overflow; rows with d[i][k] = INF
 *   are skipped (no path through k can help them).
 */

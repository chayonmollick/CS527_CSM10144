/* P097: K-means clustering
 * Problem: Cluster N points in 2D into K clusters with K-means for a fixed
 *   number of iterations. Start from K deterministic centroids (points
 *   0, N/K, 2N/K, ...). Each iteration: (1) assignment: give every point the
 *   label of its nearest centroid (ties -> smaller cluster number); (2) update:
 *   move each centroid to the mean of its points (a cluster with no points
 *   keeps its centroid). Parallelise with OpenMP, verify labels and centroids
 *   against the serial code, and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: the iterations are sequential (each needs the previous centroids).
 *   Assignment: parallel for over points; point i writes only label[i], the
 *   centroids are only read - no race. Update: many points add into the same
 *   sx[c], sy[c], cnt[c], so the sums need a reduction:
 *   (a) array-section reduction: reduction(+:sx[:K], sy[:K], cnt[:K]) gives every
 *       thread private zeroed arrays and adds them at the end (OpenMP 4.5).
 *   (b) by hand: one parallel region per iteration; each thread assigns its
 *       points and sums into local arrays, then adds them into the shared
 *       arrays inside a critical section (K adds per thread, not per point).
 *   The K divisions are done serially. Point coordinates are whole numbers, so
 *   all sums are exact in any order and the parallel centroids match the serial
 *   ones exactly (with real-valued data compare centroids with a tolerance).
 * Source: data-parallel clustering lab extension (array reductions).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p097_kmeans.c -o p097 -lm
 * Run:   ./p097 [N] [K] [iters]   (defaults N = 5e5, K = 8, iters = 10; K <= 64)
 */
#include "common.h"

#define KMAX 64

/* Points around K random centres; coordinates are integers stored as doubles. */
static void make_points(long n, int k, double *px, double *py) {
    unsigned long long s = 99;
    double cxs[KMAX], cys[KMAX];
    for (int c = 0; c < k; c++) {
        cxs[c] = (double)(lcg_next(&s) % 1001);
        cys[c] = (double)(lcg_next(&s) % 1001);
    }
    for (long i = 0; i < n; i++) {
        int c = (int)(lcg_next(&s) % (unsigned long)k);
        px[i] = cxs[c] + (double)((long)(lcg_next(&s) % 201) - 100);
        py[i] = cys[c] + (double)((long)(lcg_next(&s) % 201) - 100);
    }
}

static void init_centroids(long n, int k, const double *px, const double *py,
                           double *cx, double *cy) {
    for (int c = 0; c < k; c++) { cx[c] = px[c * (n / k)]; cy[c] = py[c * (n / k)]; }
}

/* Nearest centroid; strict < keeps the smaller c on ties. */
static inline int nearest(double x, double y, const double *cx, const double *cy, int k) {
    int best = 0;
    double bd = (x - cx[0]) * (x - cx[0]) + (y - cy[0]) * (y - cy[0]);
    for (int c = 1; c < k; c++) {
        double d = (x - cx[c]) * (x - cx[c]) + (y - cy[c]) * (y - cy[c]);
        if (d < bd) { bd = d; best = c; }
    }
    return best;
}

/* New centroid = mean of its points; an empty cluster keeps its old centroid. */
static void move_centroids(int k, const double *sx, const double *sy, const long *cnt,
                           double *cx, double *cy) {
    for (int c = 0; c < k; c++)
        if (cnt[c] > 0) { cx[c] = sx[c] / (double)cnt[c]; cy[c] = sy[c] / (double)cnt[c]; }
}

static void kmeans_serial(long n, int k, int iters, const double *px, const double *py,
                          int *label, double *cx, double *cy) {
    double sx[KMAX], sy[KMAX]; long cnt[KMAX];
    init_centroids(n, k, px, py, cx, cy);
    for (int it = 0; it < iters; it++) {
        for (long i = 0; i < n; i++) label[i] = nearest(px[i], py[i], cx, cy, k);
        for (int c = 0; c < k; c++) { sx[c] = 0.0; sy[c] = 0.0; cnt[c] = 0; }
        for (long i = 0; i < n; i++) {
            int c = label[i];
            sx[c] += px[i]; sy[c] += py[i]; cnt[c]++;
        }
        move_centroids(k, sx, sy, cnt, cx, cy);
    }
}

/* (a) parallel for for the assignment, array-section reduction for the sums. */
static void kmeans_reduction(long n, int k, int iters, const double *px, const double *py,
                             int *label, double *cx, double *cy) {
    double *sx = xmalloc(KMAX * sizeof(double)), *sy = xmalloc(KMAX * sizeof(double));
    long *cnt = xmalloc(KMAX * sizeof(long));
    init_centroids(n, k, px, py, cx, cy);
    for (int it = 0; it < iters; it++) {
        #pragma omp parallel for schedule(static)
        for (long i = 0; i < n; i++) label[i] = nearest(px[i], py[i], cx, cy, k);

        for (int c = 0; c < k; c++) { sx[c] = 0.0; sy[c] = 0.0; cnt[c] = 0; }
        #pragma omp parallel for schedule(static) reduction(+:sx[:k], sy[:k], cnt[:k])
        for (long i = 0; i < n; i++) {
            int c = label[i];
            sx[c] += px[i]; sy[c] += py[i]; cnt[c]++;
        }
        move_centroids(k, sx, sy, cnt, cx, cy);
    }
    free(sx); free(sy); free(cnt);
}

/* (b) one region per iteration: local sums per thread, merged in a critical. */
static void kmeans_local(long n, int k, int iters, const double *px, const double *py,
                         int *label, double *cx, double *cy) {
    double sx[KMAX], sy[KMAX]; long cnt[KMAX];
    init_centroids(n, k, px, py, cx, cy);
    for (int it = 0; it < iters; it++) {
        for (int c = 0; c < k; c++) { sx[c] = 0.0; sy[c] = 0.0; cnt[c] = 0; }
        #pragma omp parallel
        {
            double lsx[KMAX] = {0}, lsy[KMAX] = {0};    /* this thread's sums */
            long lcnt[KMAX] = {0};
            #pragma omp for schedule(static)
            for (long i = 0; i < n; i++) {
                int c = nearest(px[i], py[i], cx, cy, k);
                label[i] = c;
                lsx[c] += px[i]; lsy[c] += py[i]; lcnt[c]++;
            }
            #pragma omp critical
            for (int c = 0; c < k; c++) { sx[c] += lsx[c]; sy[c] += lsy[c]; cnt[c] += lcnt[c]; }
        }                                        /* barrier: all sums merged */
        move_centroids(k, sx, sy, cnt, cx, cy);
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 500000);
    int k = (int)arg_long(argc, argv, 2, 8);
    int iters = (int)arg_long(argc, argv, 3, 10);
    if (k < 1 || k > KMAX || n < k) {
        fprintf(stderr, "need 1 <= K <= %d and N >= K\n", KMAX);
        return 1;
    }
    double *px = xmalloc((size_t)n * sizeof(double)), *py = xmalloc((size_t)n * sizeof(double));
    int *ls = xmalloc((size_t)n * sizeof(int)), *lp = xmalloc((size_t)n * sizeof(int));
    double cxs[KMAX], cys[KMAX], cxp[KMAX], cyp[KMAX];
    make_points(n, k, px, py);
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, kmeans_serial(n, k, iters, px, py, ls, cxs, cys));
    printf("P097: K-means, N = %ld points, K = %d, %d iterations; serial centroids:\n",
           n, k, iters);
    for (int c = 0; c < k; c++) printf("  c%-2d (%9.3f, %9.3f)\n", c, cxs[c], cys[c]);

    printf("(a) parallel for + reduction(+:sx[:K], sy[:K], cnt[:K])\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, kmeans_reduction(n, k, iters, px, py, lp, cxp, cyp));
        table_row(n, THREADS[t], ts, tp, same_ints(ls, lp, n) &&
                  close_doubles(cxs, cxp, k, 1e-12) && close_doubles(cys, cyp, k, 1e-12));
    }
    printf("(b) per-thread sums merged in a critical section\n");
    table_header();
    for (int t = 0; t < NTHREADS; t++) {
        omp_set_num_threads(THREADS[t]);
        BEST_TIME(tp, kmeans_local(n, k, iters, px, py, lp, cxp, cyp));
        table_row(n, THREADS[t], ts, tp, same_ints(ls, lp, n) &&
                  close_doubles(cxs, cxp, k, 1e-12) && close_doubles(cys, cyp, k, 1e-12));
    }
    free(px); free(py); free(ls); free(lp);
    return 0;
}

/* Notes:
 * - Assignment is embarrassingly parallel (own label[i], centroids read-only);
 *   the update is a histogram-like sum: sx[label[i]] += ... from many threads at
 *   once is a race, hence the array reduction or local arrays.
 * - Array-section reduction on a pointer needs the length: sx[:k]. Each thread
 *   gets k private zeroed elements, combined at the end of the loop.
 * - (b) merges only K values per thread in the critical section, so it is cheap;
 *   a critical or atomic per POINT would serialise the whole loop.
 * - Iterations cannot run in parallel: iteration t+1 needs the centroids of t.
 *   The serial centroid division (K values) is negligible.
 * - Floating-point sums in a different order can move a centroid in the last
 *   bit and flip a point that is exactly halfway; integer coordinates avoid it.
 */

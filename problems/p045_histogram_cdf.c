/* P045: Histogram of doubles into B bins, then the cumulative histogram (CDF)
 * Problem: Given N doubles in [lo, hi), count how many fall into each of B
 *   equal-width bins (bin = (int)((x - lo) / width), width = (hi - lo) / B; clamp
 *   so that rounding never gives bin B). Then compute the cumulative histogram
 *   cum[b] = h[0] + ... + h[b] and the CDF cum[b] / N. Parallelise with OpenMP
 *   in two ways, verify against the serial histogram and report time and
 *   speedup for 1, 2, 4 and 8 threads.
 * Solution: h[bin]++ from several threads is a race (two threads can hit the
 *   same bin). (1) reduction(+:h[:B]) (OpenMP 4.5 array section): every thread
 *   gets a private zeroed copy of h, the copies are added at the end.
 *   (2) the same done by hand: each thread fills its own local histogram
 *   (omp for nowait), then adds it into the shared h with one atomic per bin
 *   (B atomics per thread instead of N). The CDF is a prefix sum over only B
 *   numbers, so it is done serially (for huge B use the blocked scan of P037).
 * Source: lab extension (Lab 8 histogram task; reduction on arrays).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p045_histogram_cdf.c -o p045 -lm
 * Run:   ./p045 [N] [B]      (defaults N = 1e7, B = 100; data in [0, 10))
 */
#include "common.h"

#define LO 0.0
#define HI 10.0

static inline int bin_of(double x, double lo, double width, int nb) {
    int b = (int)((x - lo) / width);
    if (b >= nb) b = nb - 1;                     /* rounding can give nb for x just below hi */
    if (b < 0) b = 0;
    return b;
}

static void hist_serial(const double *x, long n, long *h, int nb, double lo, double width) {
    for (int b = 0; b < nb; b++) h[b] = 0;
    for (long i = 0; i < n; i++) h[bin_of(x[i], lo, width, nb)]++;
}

/* (1) array-section reduction: private copy of h[0..nb) per thread. */
static void hist_reduction(const double *x, long n, long *h, int nb, double lo, double width) {
    for (int b = 0; b < nb; b++) h[b] = 0;
    #pragma omp parallel for reduction(+:h[:nb])
    for (long i = 0; i < n; i++) h[bin_of(x[i], lo, width, nb)]++;
}

/* (2) explicit private histograms merged with one atomic per bin. */
static void hist_private(const double *x, long n, long *h, int nb, double lo, double width) {
    for (int b = 0; b < nb; b++) h[b] = 0;
    #pragma omp parallel
    {
        long *loc = xmalloc((size_t)nb * sizeof(long));   /* my own histogram */
        for (int b = 0; b < nb; b++) loc[b] = 0;
        #pragma omp for nowait
        for (long i = 0; i < n; i++) loc[bin_of(x[i], lo, width, nb)]++;
        for (int b = 0; b < nb; b++) {
            #pragma omp atomic
            h[b] += loc[b];
        }
        free(loc);
    }                                                     /* barrier: h complete */
}

static void cumulative(const long *h, long *cum, int nb) {
    long s = 0;
    for (int b = 0; b < nb; b++) { s += h[b]; cum[b] = s; }
}

int main(int argc, char *argv[]) {
    long n  = arg_long(argc, argv, 1, 10000000L);
    int  nb = (int)arg_long(argc, argv, 2, 100L);
    if (n < 1) n = 1;
    if (nb < 1) nb = 1;
    double width = (HI - LO) / nb;
    double *x = xmalloc((size_t)n * sizeof(double));
    long *hs = xmalloc((size_t)nb * sizeof(long)), *hp = xmalloc((size_t)nb * sizeof(long));
    long *cs = xmalloc((size_t)nb * sizeof(long)), *cp = xmalloc((size_t)nb * sizeof(long));
    unsigned long long seed = 45;                 /* triangular data: mean of two uniforms */
    for (long i = 0; i < n; i++) {
        double u1 = lcg_next(&seed) / 2147483648.0, u2 = lcg_next(&seed) / 2147483648.0;
        x[i] = LO + (HI - LO) * 0.5 * (u1 + u2);
    }
    omp_warmup();
    double ts, tp;

    BEST_TIME(ts, hist_serial(x, n, hs, nb, LO, width));
    cumulative(hs, cs, nb);
    printf("P045: histogram + CDF, N = %ld, B = %d bins on [%.1f, %.1f), total count %s\n",
           n, nb, LO, HI, ok_str(cs[nb - 1] == n));

    printf("\n(1) reduction(+:h[:B])\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, hist_reduction(x, n, hp, nb, LO, width));
        cumulative(hp, cp, nb);
        table_row(n, THREADS[k], ts, tp, same_longs(hp, hs, nb) && same_longs(cp, cs, nb));
    }

    printf("\n(2) private histogram per thread + one atomic per bin\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, hist_private(x, n, hp, nb, LO, width));
        cumulative(hp, cp, nb);
        table_row(n, THREADS[k], ts, tp, same_longs(hp, hs, nb) && same_longs(cp, cs, nb));
    }

    printf("\nCDF (triangular data: expect 0.125, 0.5, 0.875 at x = 2.5, 5, 7.5):\n");
    const double qx[] = {2.5, 5.0, 7.5, 10.0};
    for (int q = 0; q < 4; q++) {
        int b = (int)((qx[q] - LO) / width + 0.5) - 1;   /* last bin ending at or before qx */
        if (b < 0) b = 0;
        if (b > nb - 1) b = nb - 1;
        printf("  P(x < %6.3f) = %.4f\n", LO + (b + 1) * width, (double)cs[b] / (double)n);
    }
    free(x); free(hs); free(hp); free(cs); free(cp);
    return 0;
}

/* Notes:
 * - Fixes for the h[bin]++ race, slowest to fastest: critical (one lock for all
 *   bins), atomic per element (N atomics, slow when bins are hot), private
 *   histograms + merge (reduction does this for you).
 * - Array-section reduction (h[:B]) is OpenMP 4.5. Each private copy may live
 *   on the thread's stack, so for very large B use heap histograms as in (2).
 * - Clamp the bin: (x - lo) / width can round up to B for x just below hi,
 *   which would write past the end of h.
 * - CDF = prefix sum of the histogram; cum[B-1] must equal N (check). With
 *   only B values the serial scan is cheap; parallel scan only for huge B.
 * - Merge in (2) needs atomic (or critical) because all threads add into the
 *   same h[b]; it is only B updates per thread, so the cost is negligible.
 */

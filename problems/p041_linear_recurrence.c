/* P041: First-order linear recurrence x[i] = c[i]*x[i-1] + d[i] (affine scan)
 * Problem: Given c[N] and d[N], compute x[0] = d[0] and
 *   x[i] = c[i]*x[i-1] + d[i] for i = 1..N-1, serially and with OpenMP.
 *   Verify within a tolerance and report the time, speedup and efficiency for
 *   1, 2, 4 and 8 threads. (Use |c[i]| <= 1 so x stays bounded.)
 * Solution: step i is the affine map f_i(x) = c[i]*x + d[i]; x[i] = f_i(x[i-1])
 *   with x[-1] = 0. Composing two affine maps gives an affine map:
 *   apply (A,B) then (c,d):  x -> c*(A*x + B) + d = (c*A)*x + (c*B + d).
 *   Composition is associative, so the recurrence is a SCAN over maps.
 *   Blocked two-pass scan in one parallel region: pass 1, each thread composes
 *   the maps of its block [lo, hi) into one map (A_t, B_t) (block in -> block
 *   out); barrier; single runs the p block maps in order to get the value
 *   entering each block, xin[t] = x[lo_t - 1]; implicit barrier; pass 2, each
 *   thread replays the recurrence over its block starting from xin[t]. Each
 *   thread writes only x[lo..hi) and the shared xin[] is read after the
 *   barriers -> race-free.
 * Source: lab extension (generalised prefix sum; prefix sum is c[i] = 1).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p041_linear_recurrence.c -o p041 -lm
 * Run:   ./p041 [N]          (default N = 1e7)
 */
#include "common.h"

static void recur_serial(const double *c, const double *d, double *x, long n) {
    double v = 0.0;                                   /* x[-1] = 0, so x[0] = d[0] */
    for (long i = 0; i < n; i++) { v = c[i] * v + d[i]; x[i] = v; }
}

static void recur_parallel(const double *c, const double *d, double *x, long n) {
    int maxt = omp_get_max_threads();
    double *A = xmalloc((size_t)maxt * sizeof(double));   /* block map: out = A*in + B */
    double *B = xmalloc((size_t)maxt * sizeof(double));
    double *xin = xmalloc((size_t)maxt * sizeof(double)); /* value entering each block */
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;

        double a = 1.0, b = 0.0;                      /* pass 1: identity map, then */
        for (long i = lo; i < hi; i++) {              /* compose with f_i on the right */
            a = c[i] * a;
            b = c[i] * b + d[i];
        }
        A[tid] = a; B[tid] = b;

        #pragma omp barrier
        #pragma omp single
        {                                             /* run the block maps in order */
            double v = 0.0;                           /* x[-1] */
            for (int t = 0; t < nt; t++) { xin[t] = v; v = A[t] * v + B[t]; }
        }                                             /* implicit barrier */

        double v = xin[tid];                          /* pass 2: replay my block */
        for (long i = lo; i < hi; i++) { v = c[i] * v + d[i]; x[i] = v; }
    }
    free(A); free(B); free(xin);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    if (n < 1) n = 1;
    double *c  = xmalloc((size_t)n * sizeof(double)), *d  = xmalloc((size_t)n * sizeof(double));
    double *xs = xmalloc((size_t)n * sizeof(double)), *xp = xmalloc((size_t)n * sizeof(double));
    fill_double(c, n, -1.0, 1.0, 41);                 /* |c| <= 1 keeps x bounded */
    fill_double(d, n, -1.0, 1.0, 4141);
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, recur_serial(c, d, xs, n));
    printf("P041: x[i] = c[i]*x[i-1] + d[i], N = %ld, x[N-1] = %.10f\n", n, xs[n - 1]);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(xp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, recur_parallel(c, d, xp, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(xp, xs, n, 1e-9));
    }
    free(c); free(d); free(xs); free(xp);
    return 0;
}

/* Notes:
 * - Why a plain parallel for fails: x[i] needs x[i-1] (flow dependence,
 *   distance 1). The fix is to scan with an associative operator; here the
 *   "values" being scanned are maps (A, B), not numbers.
 * - Order matters: composition is associative but NOT commutative, so the
 *   single must combine blocks in thread order 0, 1, ..., p-1.
 * - Special cases: c[i] = 1 -> prefix sum; d[i] = 0 -> prefix product;
 *   constant c -> A_t = c^(block length).
 * - Work ~3N multiply-adds vs N serially (pass 1 does 2 per element), so the
 *   ideal speedup is about p/2 - p/3; also memory-bound.
 * - Results differ from serial only by rounding (different evaluation order),
 *   hence the tolerance check. |c| <= 1 keeps errors from growing; with |c| > 1
 *   the recurrence itself is unstable (serial too).
 */

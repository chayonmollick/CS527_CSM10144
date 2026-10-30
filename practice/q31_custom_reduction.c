/* CS527 Computer Systems — OpenMP practice
 * Q31: Find the maximum and minimum of an array together with their indices
 *      using a user-defined reduction (#pragma omp declare reduction), with
 *      ties resolved to the smallest index so the answer equals the serial
 *      one. Compare with per-thread locals + critical. Also find the second
 *      largest distinct value in parallel.
 *  - serial single pass (strict > / < keeps the first occurrence)
 *  - declare reduction maxloc / minloc on struct {val, idx}: one pass, reduction(maxloc:mx) reduction(minloc:mn)
 *  - per-thread local best + critical merge (the manual equivalent)
 *  - second largest distinct value: per-thread top-2, merged in a critical section
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>

typedef struct { double val; long idx; } loc_t;
typedef struct { double first, second; } top2_t;   /* first > second, distinct */

static unsigned long long rng = 42;            /* deterministic, serial only */
static unsigned next_rand(void) {
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng >> 33);
}

/* larger value wins; equal values -> smaller index (= first occurrence) */
static loc_t loc_max(loc_t a, loc_t b) {
    if (b.val > a.val || (b.val == a.val && b.idx < a.idx)) return b;
    return a;
}
static loc_t loc_min(loc_t a, loc_t b) {
    if (b.val < a.val || (b.val == a.val && b.idx < a.idx)) return b;
    return a;
}

/* omp_out/omp_in = two partial results to combine; omp_priv = each thread's
 * private starting value (must be the identity of the operation). */
#pragma omp declare reduction(maxloc : loc_t : omp_out = loc_max(omp_out, omp_in)) \
        initializer(omp_priv = (loc_t){-DBL_MAX, LONG_MAX})
#pragma omp declare reduction(minloc : loc_t : omp_out = loc_min(omp_out, omp_in)) \
        initializer(omp_priv = (loc_t){DBL_MAX, LONG_MAX})

static void maxmin_serial(const double *a, long n, loc_t *mx, loc_t *mn) {
    loc_t bx = {a[0], 0}, bn = {a[0], 0};
    for (long i = 1; i < n; i++) {
        if (a[i] > bx.val) { bx.val = a[i]; bx.idx = i; }
        if (a[i] < bn.val) { bn.val = a[i]; bn.idx = i; }
    }
    *mx = bx; *mn = bn;
}

static void maxmin_declare(const double *a, long n, loc_t *mx, loc_t *mn) {
    loc_t bx = {-DBL_MAX, LONG_MAX}, bn = {DBL_MAX, LONG_MAX};
    /* body = the serial loop: a thread visits its own i in increasing order,
     * so strict > keeps its first occurrence; loc_max() breaks ties between
     * threads when the runtime combines the private copies. */
    #pragma omp parallel for reduction(maxloc:bx) reduction(minloc:bn)
    for (long i = 0; i < n; i++) {
        if (a[i] > bx.val) { bx.val = a[i]; bx.idx = i; }
        if (a[i] < bn.val) { bn.val = a[i]; bn.idx = i; }
    }
    *mx = bx; *mn = bn;
}

static void maxmin_critical(const double *a, long n, loc_t *mx, loc_t *mn) {
    loc_t gx = {-DBL_MAX, LONG_MAX}, gn = {DBL_MAX, LONG_MAX};
    #pragma omp parallel
    {
        loc_t bx = {-DBL_MAX, LONG_MAX}, bn = {DBL_MAX, LONG_MAX};   /* thread-local */
        #pragma omp for nowait
        for (long i = 0; i < n; i++) {
            if (a[i] > bx.val) { bx.val = a[i]; bx.idx = i; }
            if (a[i] < bn.val) { bn.val = a[i]; bn.idx = i; }
        }
        #pragma omp critical          /* once per thread, not per element */
        { gx = loc_max(gx, bx); gn = loc_min(gn, bn); }
    }
    *mx = gx; *mn = gn;
}

/* ---- second largest distinct value ---- */
static void top2_add(top2_t *t, double x) {
    if (x > t->first)                         { t->second = t->first; t->first = x; }
    else if (x < t->first && x > t->second)   t->second = x;   /* skip duplicates of first */
}
static top2_t top2_serial(const double *a, long n) {
    top2_t t = {-DBL_MAX, -DBL_MAX};
    for (long i = 0; i < n; i++) top2_add(&t, a[i]);
    return t;
}
static top2_t top2_parallel(const double *a, long n) {
    top2_t g = {-DBL_MAX, -DBL_MAX};
    #pragma omp parallel
    {
        top2_t t = {-DBL_MAX, -DBL_MAX};      /* per-thread top-2 */
        #pragma omp for nowait
        for (long i = 0; i < n; i++) top2_add(&t, a[i]);
        #pragma omp critical                  /* merge: insert both of them */
        { top2_add(&g, t.first); top2_add(&g, t.second); }
    }
    return g;
}

static int same_loc(loc_t a, loc_t b) { return a.val == b.val && a.idx == b.idx; }

static void header(const char *title) {
    printf("\n%s\n%8s %12s %12s %9s %11s %7s\n", title,
           "threads", "serial(s)", "parallel(s)", "speedup", "efficiency", "check");
}
static void row(int t, double ts, double tp, int ok) {
    double sp = tp > 0 ? ts / tp : 0.0;
    printf("%8d %12.6f %12.6f %9.2f %11.2f %7s\n",
           t, ts, tp, sp, sp / t, ok ? "OK" : "WRONG");
}

int main(int argc, char **argv) {
    long n = (argc > 1) ? atol(argv[1]) : 30000000L;
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    const int threads[] = {1, 2, 4, 8};
    double *a = malloc((size_t)n * sizeof(double));
    if (!a) { fprintf(stderr, "alloc failed\n"); return 1; }
    /* values 0.0 .. 99999.9 in steps of 0.1: max and min repeat many times */
    for (long i = 0; i < n; i++) a[i] = (double)(next_rand() % 1000000) / 10.0;

    printf("Q31: Custom reduction (maxloc/minloc) and second largest, N = %ld\n", n);
    top2_t st = top2_serial(a, n);            /* also a warm-up: 1st pass is slower */
    loc_t sx, sn;
    double t0 = omp_get_wtime(); maxmin_serial(a, n, &sx, &sn); double ts = omp_get_wtime() - t0;
    long nmax = 0; for (long i = 0; i < n; i++) nmax += (a[i] == sx.val);
    printf("serial: max = %.1f @ %ld (occurs %ld times), min = %.1f @ %ld\n",
           sx.val, sx.idx, nmax, sn.val, sn.idx);
    printf("        second largest distinct = %.1f\n", st.second);

    loc_t px, pn;
    header("[declare reduction maxloc + minloc, one pass]");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime(); maxmin_declare(a, n, &px, &pn); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, same_loc(px, sx) && same_loc(pn, sn));
    }
    printf("parallel: max = %.1f @ %ld, min = %.1f @ %ld\n", px.val, px.idx, pn.val, pn.idx);

    header("[per-thread local + critical merge]");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime(); maxmin_critical(a, n, &px, &pn); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, same_loc(px, sx) && same_loc(pn, sn));
    }

    t0 = omp_get_wtime(); st = top2_serial(a, n); double ts2 = omp_get_wtime() - t0;
    header("[second largest distinct: per-thread top-2 + critical merge]");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime(); top2_t pt = top2_parallel(a, n); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts2, tp, pt.first == st.first && pt.second == st.second);
    }

    free(a);
    return 0;
}

/* Viva notes:
 * - Built-in reduction(max:) gives only the value; the index needs a struct.
 *   declare reduction(name : type : combiner) initializer(omp_priv = identity)
 *   lets reduction() work on that struct (OpenMP 4.0, gcc >= 4.9).
 * - Identity: {-DBL_MAX, LONG_MAX} for maxloc -- loses to every real element.
 *   Without an initializer the private copy is uninitialised for a struct.
 * - Tie-break on the smaller index makes the combiner associative AND
 *   commutative, so the result is the serial first occurrence regardless of
 *   how threads split the array or the order partial results are combined.
 *   Without it the reported index could change from run to run.
 * - Local + critical is the manual version of the same thing: a critical per
 *   ELEMENT would serialise the loop; one per THREAD costs nothing.
 * - Second largest: each thread keeps its top-2 distinct values; the global
 *   top-2 is among the union of those, so inserting both of every thread's
 *   values into a global top-2 is enough.
 */

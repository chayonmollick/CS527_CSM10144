/* P101: User-defined reductions on structs
 * Problem: Use OpenMP user-defined reductions (declare reduction) to compute,
 *   each in ONE parallel loop with ONE reduction variable of struct type:
 *   (a) the complex sum S = sum over k = 1..N of e^(i k t) / k for t = 1, with a
 *       struct {double re, im;} (the series tends to -ln(2 sin(t/2)) + i(pi - t)/2);
 *   (b) the centroid of the N random 3D points that lie inside the unit sphere
 *       (sum of x, y, z and the count in one struct);
 *   (c) the bounding box of all N points (min and max of x, y and z in one struct).
 *   Verify against serial loops and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: reduction(+:) only works on arithmetic types. For a struct we declare
 *   #pragma omp declare reduction(name : Type : combiner) initializer(...):
 *   every thread gets a private copy omp_priv set by the initializer (the
 *   identity: zeros for sums, +inf/-inf for min/max), updates it with no
 *   sharing, and at the end the copies are merged pairwise with the combiner
 *   (omp_out = merge(omp_out, omp_in)). Then reduction(name:var) on the parallel
 *   for is race-free exactly like reduction(+:s).
 * Source: lecture slides on reductions (user-defined reductions, OpenMP 4.0+).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p101_struct_reduction.c -o p101 -lm
 * Run:   ./p101 [N]          (default N = 4e6 terms and points)
 */
#include "common.h"

/* ---- (a) complex numbers ---- */
typedef struct { double re, im; } Complex;
static Complex cadd(Complex a, Complex b) { Complex c = {a.re + b.re, a.im + b.im}; return c; }
static Complex czero(void) { Complex c = {0.0, 0.0}; return c; }
#pragma omp declare reduction(csum : Complex : omp_out = cadd(omp_out, omp_in)) \
    initializer(omp_priv = czero())

/* ---- (b) sum of coordinates + count, for a centroid ---- */
typedef struct { double x, y, z; long count; } Sum3;
static Sum3 sum3_add(Sum3 a, Sum3 b) {
    Sum3 c = {a.x + b.x, a.y + b.y, a.z + b.z, a.count + b.count};
    return c;
}
static Sum3 sum3_zero(void) { Sum3 c = {0.0, 0.0, 0.0, 0}; return c; }
#pragma omp declare reduction(sum3 : Sum3 : omp_out = sum3_add(omp_out, omp_in)) \
    initializer(omp_priv = sum3_zero())

/* ---- (c) bounding box ---- */
typedef struct { double xmin, xmax, ymin, ymax, zmin, zmax; } Box;
static Box box_merge(Box a, Box b) {
    Box c = {fmin(a.xmin, b.xmin), fmax(a.xmax, b.xmax), fmin(a.ymin, b.ymin),
             fmax(a.ymax, b.ymax), fmin(a.zmin, b.zmin), fmax(a.zmax, b.zmax)};
    return c;
}
static Box box_empty(void) {                    /* identity: any point beats it */
    Box c = {HUGE_VAL, -HUGE_VAL, HUGE_VAL, -HUGE_VAL, HUGE_VAL, -HUGE_VAL};
    return c;
}
static Box box_point(Box b, double x, double y, double z) {
    Box p = {x, x, y, y, z, z};
    return box_merge(b, p);
}
#pragma omp declare reduction(bbox : Box : omp_out = box_merge(omp_out, omp_in)) \
    initializer(omp_priv = box_empty())

/* Points in [-1,1) x [-1,1) x [0,1), all coordinates from ONE generator stream.
 * (fill_double with seeds 11, 12, 13 would give correlated x, y, z: LCG streams
 * started at equally spaced seeds satisfy z = 2y - x (mod 1), i.e. planes.) */
static void make_points(long n, double *x, double *y, double *z) {
    unsigned long long s = 2026;
    for (long i = 0; i < n; i++) {
        x[i] = -1.0 + 2.0 * (double)lcg_next(&s) / 2147483648.0;
        y[i] = -1.0 + 2.0 * (double)lcg_next(&s) / 2147483648.0;
        z[i] = (double)lcg_next(&s) / 2147483648.0;
    }
}

/* ---- serial versions ---- */
static Complex series_serial(long n, double t) {
    Complex s = czero();
    for (long k = 1; k <= n; k++) { s.re += cos(k * t) / k; s.im += sin(k * t) / k; }
    return s;
}
static Sum3 sphere_serial(long n, const double *x, const double *y, const double *z) {
    Sum3 s = sum3_zero();
    for (long i = 0; i < n; i++)
        if (x[i] * x[i] + y[i] * y[i] + z[i] * z[i] <= 1.0) {
            s.x += x[i]; s.y += y[i]; s.z += z[i]; s.count++;
        }
    return s;
}
static Box box_serial(long n, const double *x, const double *y, const double *z) {
    Box b = box_empty();
    for (long i = 0; i < n; i++) b = box_point(b, x[i], y[i], z[i]);
    return b;
}

/* ---- parallel versions: the same loops plus reduction(name:var) ---- */
static Complex series_parallel(long n, double t) {
    Complex s = czero();
    #pragma omp parallel for reduction(csum:s)
    for (long k = 1; k <= n; k++) { s.re += cos(k * t) / k; s.im += sin(k * t) / k; }
    return s;
}
static Sum3 sphere_parallel(long n, const double *x, const double *y, const double *z) {
    Sum3 s = sum3_zero();
    #pragma omp parallel for reduction(sum3:s)
    for (long i = 0; i < n; i++)
        if (x[i] * x[i] + y[i] * y[i] + z[i] * z[i] <= 1.0) {
            s.x += x[i]; s.y += y[i]; s.z += z[i]; s.count++;
        }
    return s;
}
static Box box_parallel(long n, const double *x, const double *y, const double *z) {
    Box b = box_empty();
    #pragma omp parallel for reduction(bbox:b)
    for (long i = 0; i < n; i++) b = box_point(b, x[i], y[i], z[i]);
    return b;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 4000000);
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    double *x = xmalloc((size_t)n * sizeof(double)), *y = xmalloc((size_t)n * sizeof(double));
    double *z = xmalloc((size_t)n * sizeof(double));
    make_points(n, x, y, z);
    omp_warmup();
    double ts, tp, t = 1.0;
    printf("P101: user-defined reductions on structs, N = %ld\n", n);

    /* (a) complex series */
    Complex cs = czero(), cp = czero();
    BEST_TIME(ts, cs = series_serial(n, t));
    printf("(a) complex sum: serial (%.10f, %.10f), limit (%.10f, %.10f)\n",
           cs.re, cs.im, -log(2.0 * sin(t / 2.0)), (PI - t) / 2.0);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, cp = series_parallel(n, t));
        table_row(n, THREADS[k], ts, tp, close1(cs.re, cp.re, 1e-9) && close1(cs.im, cp.im, 1e-9));
    }

    /* (b) centroid of the points inside the unit sphere (a half ball here) */
    Sum3 ss = sum3_zero(), sp = sum3_zero();
    BEST_TIME(ts, ss = sphere_serial(n, x, y, z));
    double c = ss.count > 0 ? 1.0 / (double)ss.count : 0.0;
    printf("(b) centroid: %ld points inside, centroid (%.5f, %.5f, %.5f) "
           "(half ball: expect about (0, 0, 0.375))\n", ss.count, ss.x * c, ss.y * c, ss.z * c);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, sp = sphere_parallel(n, x, y, z));
        table_row(n, THREADS[k], ts, tp, sp.count == ss.count && close1(ss.x, sp.x, 1e-9) &&
                  close1(ss.y, sp.y, 1e-9) && close1(ss.z, sp.z, 1e-9));
    }

    /* (c) bounding box (min/max do not depend on order: results are exact) */
    Box bs = box_empty(), bp = box_empty();
    BEST_TIME(ts, bs = box_serial(n, x, y, z));
    printf("(c) bounding box: x [%.6f, %.6f]  y [%.6f, %.6f]  z [%.6f, %.6f]\n",
           bs.xmin, bs.xmax, bs.ymin, bs.ymax, bs.zmin, bs.zmax);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, bp = box_parallel(n, x, y, z));
        table_row(n, THREADS[k], ts, tp, memcmp(&bs, &bp, sizeof(Box)) == 0);
    }
    free(x); free(y); free(z);
    return 0;
}

/* Notes:
 * - declare reduction parts: name, type, combiner (omp_out = f(omp_out, omp_in)),
 *   initializer (omp_priv = identity). The identity must not change the result
 *   when merged: 0 for sums, +inf for a min, -inf for a max.
 * - Without the reduction, s.re += ... from many threads is a race on the shared
 *   struct; a critical per iteration would work but serialise the loop.
 * - Before OpenMP 4.0 (or without declare reduction): give each thread a local
 *   struct, update it in the loop, merge once per thread in a critical section.
 * - (a)/(b) are floating-point sums in a different order: compare with a
 *   tolerance. (c) min/max are exact in any order, so the boxes match bit for bit.
 * - The series converges slowly (error ~ 1/N), so the serial value differs from
 *   the limit in the 7th digit for N = 4e6 - that is the maths, not a bug.
 */

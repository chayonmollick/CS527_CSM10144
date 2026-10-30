/* CS527 Computer Systems — OpenMP problem bank: shared helpers.
 * Every pNNN_*.c includes this file. If you copy a problem elsewhere, copy
 * common.h next to it (or paste the parts you use into your file).
 *   BEST_TIME(t, stmt)        fastest of REPS runs of stmt, in seconds
 *   table_header / table_row  the standard threads | serial | parallel | speedup | efficiency table
 *   fill_int / fill_double    deterministic data (never call rand() in parallel code)
 *   same_* / close_*          correctness checks against the serial result
 */
#ifndef CS527_COMMON_H
#define CS527_COMMON_H

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define REPS 3                                  /* timings are the best of REPS runs */
#define NTHREADS 4
static const int THREADS[NTHREADS] = {1, 2, 4, 8};   /* thread counts in every table */
#define PI 3.14159265358979323846               /* M_PI is not guaranteed by C99 */

/* Run stmt REPS times and store the fastest wall-clock time in t. */
#define BEST_TIME(t, stmt) do {                                         \
        double best_ = 1e30;                                            \
        for (int r_ = 0; r_ < REPS; r_++) {                             \
            double t0_ = omp_get_wtime();                               \
            stmt;                                                       \
            double dt_ = omp_get_wtime() - t0_;                         \
            if (dt_ < best_) best_ = dt_;                               \
        }                                                               \
        (t) = best_;                                                    \
    } while (0)

/* argv[i] as a number (accepts 1e7 style), or def when it is absent. */
static inline long arg_long(int argc, char **argv, int i, long def) {
    return argc > i ? (long)strtod(argv[i], NULL) : def;
}
static inline double arg_double(int argc, char **argv, int i, double def) {
    return argc > i ? strtod(argv[i], NULL) : def;
}

/* malloc that stops the program with a clear message instead of returning NULL. */
static inline void *xmalloc(size_t bytes) {
    void *p = malloc(bytes ? bytes : 1);
    if (!p) { fprintf(stderr, "out of memory (%zu bytes): try a smaller N\n", bytes); exit(1); }
    return p;
}

/* Deterministic pseudo-random generator (64-bit LCG, returns 31 random bits). */
static inline unsigned long lcg_next(unsigned long long *state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned long)(*state >> 33);
}
/* Scramble a seed (splitmix64) so that nearby seeds such as 11, 12, 13 start
 * independent-looking LCG streams instead of correlated ones. */
static inline unsigned long long seed_mix(unsigned long long z) {
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
/* a[i] uniform in [lo, hi] (inclusive). */
static inline void fill_int(int *a, long n, int lo, int hi, unsigned long long seed) {
    unsigned long long s = seed_mix(seed);
    unsigned long range = (unsigned long)((long)hi - (long)lo + 1);
    for (long i = 0; i < n; i++) a[i] = (int)((long)lo + (long)(lcg_next(&s) % range));
}
/* a[i] uniform in [lo, hi). */
static inline void fill_double(double *a, long n, double lo, double hi, unsigned long long seed) {
    unsigned long long s = seed_mix(seed);
    for (long i = 0; i < n; i++) a[i] = lo + (hi - lo) * (double)lcg_next(&s) / 2147483648.0;
}

/* Correctness checks. Integer results must match exactly; floating-point results
 * computed in a different order differ in the last bits, so use a relative tolerance. */
static inline int same_ints(const int *x, const int *y, long n) {
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}
static inline int same_longs(const long *x, const long *y, long n) {
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}
static inline int close1(double x, double y, double rel_tol) {
    return fabs(x - y) <= rel_tol * (1.0 + fabs(x));
}
static inline int close_doubles(const double *x, const double *y, long n, double rel_tol) {
    for (long i = 0; i < n; i++) if (!close1(x[i], y[i], rel_tol)) return 0;
    return 1;
}
static inline const char *ok_str(int ok) { return ok ? "OK" : "WRONG"; }

/* Start the thread pool before timing. The body must not be empty: gcc deletes
 * an empty parallel region. */
static inline void omp_warmup(void) {
    #pragma omp parallel
    { volatile int id_ = omp_get_thread_num(); (void)id_; }
}

/* Standard timing table: speedup = serial / parallel, efficiency = speedup / threads. */
static inline void table_header(void) {
    printf("%12s %8s %12s %12s %8s %8s %6s\n",
           "N", "threads", "serial(s)", "parallel(s)", "speedup", "effic.", "check");
}
static inline void table_row(long n, int threads, double ts, double tp, int ok) {
    double s = tp > 0 ? ts / tp : 0.0;
    printf("%12ld %8d %12.6f %12.6f %8.2f %8.2f %6s\n", n, threads, ts, tp, s, s / threads, ok_str(ok));
}

#endif

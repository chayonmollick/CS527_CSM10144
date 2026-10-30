/* CS527 Computer Systems — OpenMP practice
 * Q00: TEMPLATE for a new question. Copy it, then edit only the three
 * marked parts: kernel_serial, kernel_parallel and init_data.
 *   cp q00_template.c myq.c && gcc -O2 -Wall -Wextra -fopenmp myq.c -o myq -lm && ./myq 10000000
 * Prints: correctness check, then threads | serial | parallel | speedup | efficiency.
 * Thread count is set in code with omp_set_num_threads() (say so in the report).
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define REPS 3                      /* each timing is the best of REPS runs */

/* ---- EDIT 1: the serial version (example: b[i] = a[i]^2, returns sum of b) ---- */
static double kernel_serial(const double *a, double *b, long n) {
    double sum = 0.0;
    for (long i = 0; i < n; i++) { b[i] = a[i] * a[i]; sum += b[i]; }
    return sum;
}

/* ---- EDIT 2: the parallel version (same result) ---- */
static double kernel_parallel(const double *a, double *b, long n) {
    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < n; i++) { b[i] = a[i] * a[i]; sum += b[i]; }
    return sum;
}

/* ---- EDIT 3: input data (deterministic; no rand() inside parallel code) ---- */
static void init_data(double *a, long n) {
    for (long i = 0; i < n; i++) a[i] = (double)(i % 1000) * 0.5;
}

/* Element-wise check with a relative tolerance (use == for integer results). */
static int close_enough(const double *x, const double *y, long n) {
    for (long i = 0; i < n; i++)
        if (fabs(x[i] - y[i]) > 1e-9 * (1.0 + fabs(x[i]))) return 0;
    return 1;
}

int main(int argc, char *argv[]) {
    long sizes[] = {100000L, 1000000L, 10000000L};      /* 1e8 doubles x3 = 2.4 GB: pass it as N */
    int nsizes = 3;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }   /* ./myq N runs just N */
    const int threads[] = {1, 2, 4, 8};
    long nmax = sizes[nsizes - 1];

    double *a  = malloc((size_t)nmax * sizeof(double));
    double *bs = malloc((size_t)nmax * sizeof(double));
    double *bp = malloc((size_t)nmax * sizeof(double));
    if (!a || !bs || !bp) { fprintf(stderr, "alloc failed (try a smaller N)\n"); return 1; }
    init_data(a, nmax);

    #pragma omp parallel            /* warm up the thread pool so it isn't timed; the body */
    { volatile int id = omp_get_thread_num(); (void)id; }   /* can't be empty: gcc drops empty regions */

    printf("Q00 template (best of %d runs, %d logical cores)\n", REPS, omp_get_num_procs());
    printf("%11s %8s %12s %12s %8s %8s %8s\n",
           "N", "threads", "serial(s)", "parallel(s)", "speedup", "effic.", "correct");
    for (int si = 0; si < nsizes; si++) {
        long n = sizes[si];
        double rs = 0, rp = 0, ts = 1e30;
        for (int r = 0; r < REPS; r++) {
            double t0 = omp_get_wtime();
            rs = kernel_serial(a, bs, n);
            double t = omp_get_wtime() - t0;
            if (t < ts) ts = t;
        }
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double tp = 1e30;
            for (int r = 0; r < REPS; r++) {
                double t0 = omp_get_wtime();
                rp = kernel_parallel(a, bp, n);
                double t = omp_get_wtime() - t0;
                if (t < tp) tp = t;
            }
            int ok = close_enough(bs, bp, n) && fabs(rs - rp) <= 1e-9 * (1.0 + fabs(rs));
            printf("%11ld %8d %12.6f %12.6f %8.2f %8.2f %8s\n",
                   n, threads[ti], ts, tp, ts / tp, ts / tp / threads[ti], ok ? "yes" : "NO");
        }
    }
    free(a); free(bs); free(bp);
    return 0;
}

/* Viva notes:
 * - speedup S = T_serial / T_parallel; efficiency E = S / threads.
 * - Floating-point sums in a different order differ in the last bits, so compare
 *   with a relative tolerance; integer results must match exactly.
 * - Best-of-REPS timing removes noise from page faults and the first-touch cost.
 */

/* CS527 Computer Systems — OpenMP Lab
 * Task 1: Vector Addition  c[i] = a[i] + b[i]
 *
 * Builds a serial and an OpenMP version, checks they agree element-wise,
 * and reports timing for several N and thread counts.
 * Threads are set in-code with omp_set_num_threads() so one run produces
 * the whole table.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define REPS 3                      /* each timing is the best of REPS runs */

static void vec_add_serial(const double *a, const double *b, double *c, long n) {
    for (long i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

static void vec_add_parallel(const double *a, const double *b, double *c, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

static int identical(const double *x, const double *y, long n) {
    for (long i = 0; i < n; i++)
        if (x[i] != y[i]) return 0;
    return 1;
}

int main(void) {
    const long sizes[]  = {100000L, 1000000L, 10000000L, 100000000L};
    const int  threads[] = {1, 2, 4, 8};

    printf("Task 1: Vector Addition\n");
    printf("%12s %8s %12s %12s %9s %7s\n",
           "N", "threads", "serial(s)", "parallel(s)", "speedup", "match");

    for (int si = 0; si < 4; si++) {
        long n = sizes[si];
        double *a  = malloc((size_t)n * sizeof(double));
        double *b  = malloc((size_t)n * sizeof(double));
        double *cs = malloc((size_t)n * sizeof(double));
        double *cp = malloc((size_t)n * sizeof(double));
        if (!a || !b || !cs || !cp) {
            printf("%12ld  (skipped: could not allocate %.1f GB)\n",
                   n, 4.0 * (double)n * sizeof(double) / 1e9);
            free(a); free(b); free(cs); free(cp);
            continue;
        }
        /* integer-valued doubles -> addition is exact, so == is a fair check.
         * cs/cp are written here too: the first write to a page costs a page
         * fault, which would otherwise be charged to whichever run comes first
         * (-1, not 0: gcc turns a zero fill after malloc into calloc, which
         * leaves the pages untouched). */
        for (long i = 0; i < n; i++) {
            a[i] = (double)(i % 1000);
            b[i] = (double)((i % 7) - 3);
            cs[i] = cp[i] = -1.0;
        }

        double t_serial = 1e30;                 /* best of REPS runs */
        for (int r = 0; r < REPS; r++) {
            double t0 = omp_get_wtime();
            vec_add_serial(a, b, cs, n);
            double t = omp_get_wtime() - t0;
            if (t < t_serial) t_serial = t;
        }

        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double t_par = 1e30;
            for (int r = 0; r < REPS; r++) {
                double p0 = omp_get_wtime();
                vec_add_parallel(a, b, cp, n);
                double t = omp_get_wtime() - p0;
                if (t < t_par) t_par = t;
            }
            printf("%12ld %8d %12.6f %12.6f %9.2f %7s\n",
                   n, threads[ti], t_serial, t_par,
                   t_par > 0 ? t_serial / t_par : 0.0,
                   identical(cs, cp, n) ? "yes" : "NO");
        }
        free(a); free(b); free(cs); free(cp);
    }
    return 0;
}

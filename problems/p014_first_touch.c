/* P014: First touch: who initialises an array decides where its memory lives
 * Problem: Arrays a, b, c (N doubles) are used in the parallel loop c[i] = a[i] + 3*b[i].
 *   Compare (a) initialising the arrays serially, then processing in parallel, with
 *   (b) initialising them with the same "parallel for schedule(static)" that later
 *   processes them. Also time the very first initialisation of fresh memory (page faults)
 *   against a second initialisation of the same memory. Verify the results are equal.
 * Solution: malloc only reserves address space; a physical page is allocated (and on a
 *   NUMA machine placed on the memory of the socket of the touching thread) when it is
 *   first WRITTEN. With serial init every page sits next to thread 0's socket and all
 *   other threads read remote memory; with parallel static init thread t first-touches
 *   exactly the block it will later process, so its pages are local. Both versions are
 *   race-free (each i is written by one thread) and give identical c = 2.5*i.
 * Source: textbook ch. 25 (affinity / first touch).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p014_first_touch.c -o p014 -lm
 * Run:   ./p014 [N]          (default N = 1e7: 3 arrays = 240 MB)
 */
#include "common.h"

static void init_serial(double *a, double *b, double *c, long n) {
    for (long i = 0; i < n; i++) { a[i] = (double)i; b[i] = 0.5 * (double)i; c[i] = 0.0; }
}

/* Same static schedule as the triad: thread t touches the block it will process. */
static void init_parallel(double *a, double *b, double *c, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) { a[i] = (double)i; b[i] = 0.5 * (double)i; c[i] = 0.0; }
}

static void triad_serial(double *c, const double *a, const double *b, long n) {
    for (long i = 0; i < n; i++) c[i] = a[i] + 3.0 * b[i];
}

static void triad(double *c, const double *a, const double *b, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) c[i] = a[i] + 3.0 * b[i];
}

/* c[i] = i + 3*(0.5 i) = 2.5 i exactly in double arithmetic (i < 2^50). */
static int check_c(const double *c, long n) {
    for (long i = 0; i < n; i++) if (c[i] != 2.5 * (double)i) return 0;
    return 1;
}

static double seconds_of(void (*init)(double *, double *, double *, long),
                         double *a, double *b, double *c, long n) {
    double t0 = omp_get_wtime();
    init(a, b, c, n);
    return omp_get_wtime() - t0;
}

/* One variant: fresh arrays, init twice (first touch, then again), timed triad, check. */
static int run_variant(void (*init)(double *, double *, double *, long), long n,
                       double *t_first, double *t_again, double *t_triad) {
    size_t bytes = (size_t)n * sizeof(double);
    double *a = xmalloc(bytes), *b = xmalloc(bytes), *c = xmalloc(bytes);
    *t_first = seconds_of(init, a, b, c, n);       /* page faults happen here */
    *t_again = seconds_of(init, a, b, c, n);       /* same work, pages already mapped */
    BEST_TIME(*t_triad, triad(c, a, b, n));
    int ok = check_c(c, n);
    free(a); free(b); free(c);                     /* next variant gets fresh memory */
    return ok;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    if (n < 1) n = 1;
    omp_set_dynamic(0);
    omp_warmup();

    /* serial baseline for the triad speedup */
    size_t bytes = (size_t)n * sizeof(double);
    double *a = xmalloc(bytes), *b = xmalloc(bytes), *c = xmalloc(bytes);
    init_serial(a, b, c, n);
    double ts;
    BEST_TIME(ts, triad_serial(c, a, b, n));
    int all_ok = check_c(c, n);
    free(a); free(b); free(c);

    printf("P014: first touch, c[i] = a[i] + 3*b[i], N = %ld (%.0f MB per run), serial triad %.4f s\n",
           n, 3.0 * (double)bytes / 1e6, ts);
    printf("init columns: one timing of the FIRST init of fresh memory and of a SECOND init;\n"
           "triad columns: best of %d; speedup = serial triad / triad\n", REPS);
    printf("%8s %11s %11s %11s %11s | %12s %12s %8s %8s %6s\n", "threads",
           "ser.init#1", "ser.init#2", "par.init#1", "par.init#2",
           "triad(ser.i)", "triad(par.i)", "sp(ser)", "sp(par)", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        double f1, a1, ta, f2, a2, tb;
        int ok = run_variant(init_serial, n, &f1, &a1, &ta);
        ok &= run_variant(init_parallel, n, &f2, &a2, &tb);
        all_ok &= ok;
        printf("%8d %11.4f %11.4f %11.4f %11.4f | %12.5f %12.5f %8.2f %8.2f %6s\n",
               THREADS[k], f1, a1, f2, a2, ta, tb, ts / ta, ts / tb, ok_str(ok));
    }
    printf("\ninit #1 > init #2: the difference is the page-fault cost of first touch (the OS\n"
           "zero-fills and maps every page). Parallel first touch spreads those faults over\n"
           "the threads. triad(ser.i) vs triad(par.i) differs only on a multi-socket NUMA\n"
           "machine; on a laptop/single-socket PC both are the same.\n");
    printf("All results c[i] = 2.5*i: %s\n", ok_str(all_ok));
    return 0;
}

/* Notes:
 * - First-touch policy (Linux default): a page is placed on the NUMA node of the core
 *   that first writes it; malloc itself places nothing.
 * - So initialise data with the SAME parallel loop/schedule that will use it; with
 *   serial init all threads fetch from one socket's memory (remote, bandwidth of one
 *   memory controller).
 * - On a single-socket machine all memory is "local": only the page-fault cost of the
 *   first touch is visible. Large pages (THP) cut the number of faults.
 * - Thread placement must also stay fixed: OMP_PROC_BIND=true / OMP_PLACES=cores keep
 *   thread t on the same core, otherwise "local" data can become remote.
 * - Time steady state (after first touch), or report the first run separately: the
 *   first run of any benchmark pays the page faults.
 */

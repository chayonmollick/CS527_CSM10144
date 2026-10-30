/* CS527 Computer Systems — OpenMP Lab 9
 * Task 2: Loop scheduling — schedule(static) vs schedule(dynamic, chunk) vs
 * schedule(guided) on an expensive per-element update repeated work[i] times.
 * Three workloads:
 *   uniform  : every element costs the same
 *   sawtooth : the handout's pattern, cost proportional to (i % 1000)
 *   ramp     : cost grows linearly with i (late iterations are much heavier)
 * The work per element is scaled so every N does the same total work (the
 * handout's literal (i % 1000) * 100 would run for minutes at N = 1e7).
 * Usage: ./task2_schedule [N] [avg_work_per_element] [dynamic_chunk]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define REPS 3                      /* every timing is the best of REPS runs */
#define TOTAL_WORK 2e8              /* inner-loop steps per run, for every N */

enum { UNIFORM, SAWTOOTH, RAMP, NWORKLOADS };
static const char *wl_name[] = {"uniform", "sawtooth (handout: i % 1000)", "ramp (cost grows with i)"};

static int chunk = 100;             /* chunk size for schedule(dynamic, chunk) */

static void make_work(int *w, long n, int wl, double avg) {
    for (long i = 0; i < n; i++) {
        if (wl == UNIFORM)       w[i] = (int)avg;
        else if (wl == SAWTOOTH) w[i] = (int)((i % 1000) * (2.0 * avg / 999.0));
        else                     w[i] = (int)(2.0 * avg * (double)i / (double)n);
    }
}

/* The simulated expensive operation (same as the handout). */
static inline double heavy(double x, int work) {
    for (int j = 0; j < work; j++) x = x * 1.0000001 + 0.0000001;
    return x;
}

static void run_serial(const double *a, double *b, const int *w, long n) {
    for (long i = 0; i < n; i++) b[i] = heavy(a[i], w[i]);
}
static void run_static(const double *a, double *b, const int *w, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) b[i] = heavy(a[i], w[i]);
}
static void run_dynamic(const double *a, double *b, const int *w, long n) {
    #pragma omp parallel for schedule(dynamic, chunk)
    for (long i = 0; i < n; i++) b[i] = heavy(a[i], w[i]);
}
static void run_guided(const double *a, double *b, const int *w, long n) {
    #pragma omp parallel for schedule(guided)
    for (long i = 0; i < n; i++) b[i] = heavy(a[i], w[i]);
}
/* Kind and chunk are picked at run time with omp_set_schedule() (chunk study). */
static void run_runtime(const double *a, double *b, const int *w, long n) {
    #pragma omp parallel for schedule(runtime)
    for (long i = 0; i < n; i++) b[i] = heavy(a[i], w[i]);
}

typedef void (*kernel_fn)(const double *, double *, const int *, long);

static double best_time(kernel_fn f, const double *a, double *b, const int *w, long n) {
    double best = 1e30;
    for (int r = 0; r < REPS; r++) {
        double t0 = omp_get_wtime();
        f(a, b, w, n);
        double t = omp_get_wtime() - t0;
        if (t < best) best = t;
    }
    return best;
}

/* Every element is computed independently by the same code, so results are bit-identical. */
static int same(const double *x, const double *y, long n) {
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}

int main(int argc, char *argv[]) {
    long sizes[] = {100000L, 1000000L, 10000000L};
    int nsizes = 3;
    double avg_arg = 0;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (argc > 2) avg_arg = atof(argv[2]);
    if (argc > 3) chunk = atoi(argv[3]);
    const int threads[] = {1, 2, 4, 8};
    long nmax = sizes[nsizes - 1];

    double *a   = malloc((size_t)nmax * sizeof(double));
    double *b   = malloc((size_t)nmax * sizeof(double));
    double *ref = malloc((size_t)nmax * sizeof(double));
    int    *w   = malloc((size_t)nmax * sizeof(int));
    if (!a || !b || !ref || !w) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < nmax; i++) a[i] = i * 0.001;

    #pragma omp parallel
    { }                             /* warm up the thread pool */

    printf("Task 2: static vs dynamic,%d vs guided (best of %d runs, %d logical cores)\n",
           chunk, REPS, omp_get_num_procs());
    printf("Each cell: time(s) / speedup / efficiency. ok = all three match serial.\n");
    for (int wl = 0; wl < NWORKLOADS; wl++) {
        printf("\nWorkload: %s\n", wl_name[wl]);
        printf("%9s %4s %6s %9s | %9s %5s %5s | %9s %5s %5s | %9s %5s %5s | %3s\n",
               "N", "thr", "avg", "serial", "static", "S", "E", "dynamic", "S", "E",
               "guided", "S", "E", "ok");
        for (int si = 0; si < nsizes; si++) {
            long n = sizes[si];
            double avg = avg_arg > 0 ? avg_arg : TOTAL_WORK / (double)n;
            make_work(w, n, wl, avg);
            double ts = best_time(run_serial, a, ref, w, n);
            for (int ti = 0; ti < 4; ti++) {
                int p = threads[ti];
                omp_set_num_threads(p);
                double t1 = best_time(run_static,  a, b, w, n); int ok = same(b, ref, n);
                double t2 = best_time(run_dynamic, a, b, w, n); ok &= same(b, ref, n);
                double t3 = best_time(run_guided,  a, b, w, n); ok &= same(b, ref, n);
                printf("%9ld %4d %6.0f %9.5f | %9.5f %5.2f %5.2f | %9.5f %5.2f %5.2f | %9.5f %5.2f %5.2f | %3s\n",
                       n, p, avg, ts, t1, ts/t1, ts/t1/p, t2, ts/t2, ts/t2/p, t3, ts/t3, ts/t3/p,
                       ok ? "yes" : "NO");
            }
        }
    }

    /* Chunk-size study with schedule(runtime): small chunks pay scheduling overhead
     * on every grab; big chunks bring back load imbalance on the ramp workload. */
    if (argc <= 1) {
        long n = 1000000L;
        double avg = TOTAL_WORK / (double)n;
        const int chunks[] = {1, 10, 100, 1000, 10000, 125000};
        omp_set_num_threads(8);
        make_work(w, n, UNIFORM, avg); double tsu = best_time(run_serial, a, ref, w, n);
        make_work(w, n, RAMP, avg);    double tsr = best_time(run_serial, a, ref, w, n);
        printf("\nChunk-size study: N = %ld, 8 threads, schedule(runtime) set via omp_set_schedule()\n", n);
        printf("serial: uniform %.5f s, ramp %.5f s\n", tsu, tsr);
        printf("%-8s %7s | %12s %8s | %12s %8s\n", "kind", "chunk",
               "uniform(s)", "speedup", "ramp(s)", "speedup");
        for (int k = 0; k < 2; k++) {
            omp_sched_t kind = k == 0 ? omp_sched_dynamic : omp_sched_static;
            for (int c = 0; c < 6; c++) {
                omp_set_schedule(kind, chunks[c]);
                make_work(w, n, UNIFORM, avg); double tu = best_time(run_runtime, a, b, w, n);
                make_work(w, n, RAMP, avg);    double tr = best_time(run_runtime, a, b, w, n);
                int ok = same(b, ref, n);
                printf("%-8s %7d | %12.5f %8.2f | %12.5f %8.2f %s\n", k == 0 ? "dynamic" : "static",
                       chunks[c], tu, tsu / tu, tr, tsr / tr, ok ? "" : "WRONG");
            }
        }
    }
    free(a); free(b); free(ref); free(w);
    return 0;
}

/* CS527 Computer Systems — OpenMP practice
 * Q24: Using `parallel sections`, compute the sum, maximum and minimum of an array
 *      and sort a copy of it, with the four jobs running concurrently. Print which
 *      thread ran which section and compare with running the four jobs serially.
 *  - #pragma omp parallel sections + #pragma omp section: each section runs exactly
 *    once, on some thread of the team (task parallelism: different jobs at once)
 *  - total time ~ the slowest section (here the sort) -> load imbalance
 *  - team of 2 / 8 threads: fewer threads than sections -> a thread runs several;
 *    more threads than sections -> the extra threads just wait at the implicit barrier
 * Usage: ./q24_sections [N]     (default N = 4e6)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    long sum;
    int max, min;
    int who[4];          /* thread that ran each section */
    double secs[4];      /* time taken by each section */
} Result;

static const char *JOB[4] = {"sum", "max", "min", "sort a copy"};

static long arr_sum(const int *a, long n) {
    long s = 0;
    for (long i = 0; i < n; i++) s += a[i];
    return s;
}
static int arr_max(const int *a, long n) {
    int m = a[0];
    for (long i = 1; i < n; i++) if (a[i] > m) m = a[i];
    return m;
}
static int arr_min(const int *a, long n) {
    int m = a[0];
    for (long i = 1; i < n; i++) if (a[i] < m) m = a[i];
    return m;
}
static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}
static void sort_copy(const int *a, int *b, long n) {
    memcpy(b, a, (size_t)n * sizeof(int));
    qsort(b, (size_t)n, sizeof(int), cmp_int);
}

static void run_serial(const int *a, int *b, long n, Result *r) {
    double t;
    t = omp_get_wtime(); r->sum = arr_sum(a, n); r->secs[0] = omp_get_wtime() - t;
    t = omp_get_wtime(); r->max = arr_max(a, n); r->secs[1] = omp_get_wtime() - t;
    t = omp_get_wtime(); r->min = arr_min(a, n); r->secs[2] = omp_get_wtime() - t;
    t = omp_get_wtime(); sort_copy(a, b, n);     r->secs[3] = omp_get_wtime() - t;
    for (int k = 0; k < 4; k++) r->who[k] = 0;
}

static void run_sections(const int *a, int *b, long n, Result *r) {
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            double t = omp_get_wtime();
            r->sum = arr_sum(a, n);
            r->secs[0] = omp_get_wtime() - t;
            r->who[0] = omp_get_thread_num();
        }
        #pragma omp section
        {
            double t = omp_get_wtime();
            r->max = arr_max(a, n);
            r->secs[1] = omp_get_wtime() - t;
            r->who[1] = omp_get_thread_num();
        }
        #pragma omp section
        {
            double t = omp_get_wtime();
            r->min = arr_min(a, n);
            r->secs[2] = omp_get_wtime() - t;
            r->who[2] = omp_get_thread_num();
        }
        #pragma omp section
        {
            double t = omp_get_wtime();
            sort_copy(a, b, n);
            r->secs[3] = omp_get_wtime() - t;
            r->who[3] = omp_get_thread_num();
        }
    }   /* implicit barrier: all four sections have finished here */
}

int main(int argc, char **argv) {
    long n = (argc > 1) ? atol(argv[1]) : 4000000L;   /* 4e6 */
    if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
    int *a   = malloc((size_t)n * sizeof(int));
    int *ref = malloc((size_t)n * sizeof(int));        /* serially sorted copy */
    int *b   = malloc((size_t)n * sizeof(int));        /* sorted by a section */
    if (!a || !ref || !b) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < n; i++) a[i] = (int)((i * 1103515245L + 12345L) % 1000003L) - 500000;

    printf("Q24: sum / max / min / sort of N = %ld ints as 4 parallel sections\n", n);
    Result s;
    double t0 = omp_get_wtime();
    run_serial(a, ref, n, &s);
    double ts = omp_get_wtime() - t0;
    printf("\n[serial] the four jobs one after another:\n");
    for (int k = 0; k < 4; k++) printf("    %-12s %.4f s\n", JOB[k], s.secs[k]);
    printf("    total        %.4f s   (sum = %ld, max = %d, min = %d)\n", ts, s.sum, s.max, s.min);

    const int teams[3] = {4, 2, 8};
    for (int ti = 0; ti < 3; ti++) {
        int nt = teams[ti];
        omp_set_num_threads(nt);
        Result r;
        memset(b, 0, (size_t)n * sizeof(int));
        double p0 = omp_get_wtime();
        run_sections(a, b, n, &r);
        double tp = omp_get_wtime() - p0;

        int ok = r.sum == s.sum && r.max == s.max && r.min == s.min &&
                 memcmp(b, ref, (size_t)n * sizeof(int)) == 0;
        printf("\n[parallel sections, %d threads]%s\n", nt,
               ti == 0 ? "  (which thread gets which section varies run to run)" : "");
        for (int k = 0; k < 4; k++)
            printf("    section %d %-12s ran on thread %d, %.4f s\n", k, JOB[k], r.who[k], r.secs[k]);
        int used[64] = {0}, idle = 0;
        for (int k = 0; k < 4; k++) used[r.who[k]] = 1;
        for (int t = 0; t < nt; t++) idle += !used[t];
        printf("    total %.4f s vs serial %.4f s -> speedup %.2fx; threads that ran no section: %d\n",
               tp, ts, tp > 0 ? ts / tp : 0.0, idle);
        printf("    results equal serial (sum, max, min, sorted copy): %s\n", ok ? "OK" : "WRONG");
    }
    printf("\nThe sort is far slower than the other three jobs, so parallel time ~ sort time:\n"
           "best possible speedup = serial total / slowest section = %.2fx, whatever the team size.\n"
           "To do better, split the big job: e.g. sort the two halves in two sections and merge,\n"
           "or parallelise the sort itself with tasks.\n",
           ts / s.secs[3]);

    free(a); free(ref); free(b);
    return 0;
}

/* Viva notes:
 * - sections = a FIXED number of different code blocks, each run once by some thread;
 *   parallelism is at most the number of sections, and the time is set by the slowest
 *   one (load imbalance). There is an implicit barrier at the end (unless nowait).
 * - More threads than sections: extra threads get nothing and wait at the barrier.
 *   Fewer threads: a thread runs several sections one after another (assignment is
 *   up to the runtime and changes between runs).
 * - parallel for = data parallelism: the SAME code on different iterations; scales with
 *   the data size. sections = task parallelism with a static, known-in-advance set of jobs.
 * - tasks (#pragma omp task) = dynamic task parallelism: tasks can be created at run
 *   time, in loops or recursively (e.g. quicksort, tree/linked-list traversal), and are
 *   queued and picked up by idle threads; sections cannot do that.
 * - Each section here reads the shared array a and writes its own output, so no
 *   synchronisation is needed inside the sections.
 */

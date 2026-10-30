/* CS527 Computer Systems — OpenMP Lab 9
 * Task 1: Merge sort with OpenMP tasks.
 *  - serial top-down merge sort (baseline)
 *  - parallel merge sort: the two halves are sorted as independent tasks,
 *    taskwait, then merged; subarrays below a cutoff are sorted serially
 * Verifies the parallel output is sorted and identical to the serial output,
 * then sweeps the cutoff to show its effect on performance.
 * Usage: ./task1_mergesort [N] [cutoff]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPS 3                      /* every timing is the best of REPS runs */

static long cutoff = 2048;          /* subarrays shorter than this are sorted serially */

/* Merge sorted a[left..mid] and a[mid+1..right] through tmp[left..right]. */
static void merge(int *a, int *tmp, long left, long mid, long right) {
    long i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right) tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while (i <= mid)   tmp[k++] = a[i++];
    while (j <= right) tmp[k++] = a[j++];
    memcpy(a + left, tmp + left, (size_t)(right - left + 1) * sizeof(int));
}

static void merge_sort_serial(int *a, int *tmp, long left, long right) {
    if (left >= right) return;
    long mid = left + (right - left) / 2;
    merge_sort_serial(a, tmp, left, mid);
    merge_sort_serial(a, tmp, mid + 1, right);
    merge(a, tmp, left, mid, right);
}

/* Each call owns the disjoint range [left,right] of a and tmp, so the two child
 * tasks never touch the same elements and need no locking. */
static void merge_sort_parallel(int *a, int *tmp, long left, long right) {
    if (left >= right) return;
    if (right - left < cutoff) { merge_sort_serial(a, tmp, left, right); return; }
    long mid = left + (right - left) / 2;
    #pragma omp task shared(a, tmp) firstprivate(left, mid)
    merge_sort_parallel(a, tmp, left, mid);
    #pragma omp task shared(a, tmp) firstprivate(mid, right)
    merge_sort_parallel(a, tmp, mid + 1, right);
    #pragma omp taskwait            /* both halves must be sorted before merging */
    merge(a, tmp, left, mid, right);
}

static void sort_parallel(int *a, int *tmp, long n) {
    #pragma omp parallel
    {
        #pragma omp single          /* one thread starts the recursion, the team runs the tasks */
        merge_sort_parallel(a, tmp, 0, n - 1);
    }
}

/* Number of tasks merge_sort_parallel creates for n elements (same recursion, no sorting). */
static long count_tasks(long left, long right) {
    if (left >= right || right - left < cutoff) return 0;
    long mid = left + (right - left) / 2;
    return 2 + count_tasks(left, mid) + count_tasks(mid + 1, right);
}

static int is_sorted(const int *a, long n) {
    for (long i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}

/* Best-of-REPS time; every run starts from the same unsorted input. */
static double time_sort(int parallel, const int *in, int *a, int *tmp, long n) {
    double best = 1e30;
    for (int r = 0; r < REPS; r++) {
        memcpy(a, in, (size_t)n * sizeof(int));
        double t0 = omp_get_wtime();
        if (parallel) sort_parallel(a, tmp, n);
        else          merge_sort_serial(a, tmp, 0, n - 1);
        double t = omp_get_wtime() - t0;
        if (t < best) best = t;
    }
    return best;
}

static void fill_random(int *a, long n) {
    unsigned long long x = 88172645463325252ULL;
    for (long i = 0; i < n; i++) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        a[i] = (int)(x >> 33);
    }
}

int main(int argc, char *argv[]) {
    long sizes[] = {100000L, 1000000L, 10000000L};
    int nsizes = 3;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (argc > 2) cutoff = atol(argv[2]);
    const int threads[] = {1, 2, 4, 8};
    long nmax = sizes[nsizes - 1];

    int *in  = malloc((size_t)nmax * sizeof(int));
    int *ref = malloc((size_t)nmax * sizeof(int));
    int *a   = malloc((size_t)nmax * sizeof(int));
    int *tmp = malloc((size_t)nmax * sizeof(int));   /* allocated once, shared by all tasks */
    if (!in || !ref || !a || !tmp) { fprintf(stderr, "alloc failed\n"); return 1; }

    #pragma omp parallel            /* warm up the thread pool so it isn't timed; the body */
    { volatile int id = omp_get_thread_num(); (void)id; }   /* can't be empty: gcc drops empty regions */

    printf("Task 1: Merge sort with OpenMP tasks (cutoff = %ld, best of %d runs)\n", cutoff, REPS);
    printf("Threads set with omp_set_num_threads(); this machine has %d logical cores.\n\n",
           omp_get_num_procs());
    printf("%10s %8s %8s %12s %12s %8s %8s %8s\n",
           "N", "tasks", "threads", "serial(s)", "parallel(s)", "speedup", "effic.", "correct");
    for (int si = 0; si < nsizes; si++) {
        long n = sizes[si];
        fill_random(in, n);
        double ts = time_sort(0, in, ref, tmp, n);      /* ref = serially sorted copy */
        if (!is_sorted(ref, n)) { printf("serial sort is WRONG\n"); return 1; }
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double tp = time_sort(1, in, a, tmp, n);
            int ok = is_sorted(a, n) && memcmp(a, ref, (size_t)n * sizeof(int)) == 0;
            double sp = ts / tp;
            printf("%10ld %8ld %8d %12.6f %12.6f %8.2f %8.2f %8s\n",
                   n, count_tasks(0, n - 1), threads[ti], ts, tp, sp, sp / threads[ti],
                   ok ? "yes" : "NO");
        }
        printf("\n");
    }

    /* Cutoff study: fixed N and 8 threads, vary the serial threshold. */
    if (argc <= 2) {
        const long cutoffs[] = {16, 256, 2048, 16384, 131072, 1048576};
        long n = nmax;
        fill_random(in, n);
        double ts = time_sort(0, in, ref, tmp, n);
        omp_set_num_threads(8);
        printf("Cutoff study: N = %ld, 8 threads, serial = %.6f s\n", n, ts);
        printf("%10s %10s %12s %8s %8s\n", "cutoff", "tasks", "parallel(s)", "speedup", "correct");
        for (int c = 0; c < 6; c++) {
            cutoff = cutoffs[c];
            double tp = time_sort(1, in, a, tmp, n);
            int ok = is_sorted(a, n) && memcmp(a, ref, (size_t)n * sizeof(int)) == 0;
            printf("%10ld %10ld %12.6f %8.2f %8s\n", cutoff, count_tasks(0, n - 1), tp, ts / tp,
                   ok ? "yes" : "NO");
        }
    }
    free(in); free(ref); free(a); free(tmp);
    return 0;
}

/* P048: Shell sort with the gap subsequences sorted in parallel
 * Problem: Implement Shell sort and parallelise it with OpenMP: for each gap,
 *   the gap independent subsequences (s, s+gap, s+2*gap, ...), s = 0..gap-1,
 *   are insertion-sorted in parallel. Verify that the result is sorted and
 *   identical to qsort's output, report time and speedup for 1, 2, 4, 8 threads,
 *   and explain why the last pass (gap = 1) is serial.
 * Solution: for a fixed gap the subsequences partition the indices (index i
 *   belongs to subsequence i % gap), so insertion-sorting them touches disjoint
 *   elements -> `parallel for` over the start s is race-free. The implicit
 *   barrier after each gap's loop is needed: the next gap mixes elements of all
 *   subsequences. Two schedules are compared: schedule(dynamic) balances
 *   data-dependent insertion costs; schedule(static) gives each thread a range
 *   of NEIGHBOURING starts, so it owns contiguous ints in every row (no false
 *   sharing) - the subsequences have equal length, so static is also balanced.
 *   Gaps: Knuth's 1, 4, 13, 40, ... (h = 3h + 1).
 * Source: lab extension (Lab 9 sorting; insertion sort per independent subsequence).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p048_shell_sort.c -o p048 -lm
 * Run:   ./p048 [N]          (default N = 1e6)
 */
#include "common.h"

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}
static int is_sorted_int(const int *a, long n) {
    for (long i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}

/* Largest Knuth gap below n/3 (at least 1); the next smaller gap is h / 3. */
static long first_gap(long n) {
    long h = 1;
    while (3 * h + 1 < n / 3) h = 3 * h + 1;
    return h;
}

/* Insertion sort of the subsequence a[s], a[s+gap], a[s+2*gap], ... */
static void gap_insertion_sort(int *a, long n, long s, long gap) {
    for (long i = s + gap; i < n; i += gap) {
        int v = a[i];
        long j = i;
        while (j >= s + gap && a[j - gap] > v) { a[j] = a[j - gap]; j -= gap; }
        a[j] = v;
    }
}

/* Classic serial Shell sort: one sweep over i per gap (interleaves the
 * subsequences, which is cache-friendly). This is the baseline. */
static void shell_serial(int *a, long n) {
    for (long gap = first_gap(n); gap >= 1; gap /= 3)
        for (long i = gap; i < n; i++) {
            int v = a[i];
            long j = i;
            while (j >= gap && a[j - gap] > v) { a[j] = a[j - gap]; j -= gap; }
            a[j] = v;
        }
}

static void shell_parallel_dynamic(int *a, long n) {
    for (long gap = first_gap(n); gap >= 1; gap /= 3) {
        #pragma omp parallel for schedule(dynamic)
        for (long s = 0; s < gap; s++)                  /* gap independent subsequences */
            gap_insertion_sort(a, n, s, gap);
        /* implicit barrier: this gap is finished before the next one starts */
    }
}

static void shell_parallel_static(int *a, long n) {
    for (long gap = first_gap(n); gap >= 1; gap /= 3) {
        #pragma omp parallel for schedule(static)       /* thread t: starts [t*gap/p, (t+1)*gap/p) */
        for (long s = 0; s < gap; s++)
            gap_insertion_sort(a, n, s, gap);
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000000L);
    if (n < 1) n = 1;
    int *in  = xmalloc((size_t)n * sizeof(int)), *ref = xmalloc((size_t)n * sizeof(int));
    int *w   = xmalloc((size_t)n * sizeof(int));
    fill_int(in, n, -1000000000, 1000000000, 48);
    omp_warmup();

    memcpy(ref, in, (size_t)n * sizeof(int));
    double t0 = omp_get_wtime();
    qsort(ref, (size_t)n, sizeof(int), cmp_int);
    double tq = omp_get_wtime() - t0;

    /* Every timed run starts from the unsorted input (the memcpy is inside the timing). */
    double ts, tp;
    BEST_TIME(ts, { memcpy(w, in, (size_t)n * sizeof(int)); shell_serial(w, n); });
    int ok_serial = is_sorted_int(w, n) && memcmp(w, ref, (size_t)n * sizeof(int)) == 0;
    printf("P048: Shell sort (Knuth gaps, first gap %ld), N = %ld, qsort %.6f s, serial %s\n",
           first_gap(n), n, tq, ok_str(ok_serial));
    printf("\n(a) parallel for over subsequences, schedule(dynamic)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(w, in, (size_t)n * sizeof(int)); shell_parallel_dynamic(w, n); });
        table_row(n, THREADS[k], ts, tp,
                  is_sorted_int(w, n) && memcmp(w, ref, (size_t)n * sizeof(int)) == 0);
    }
    printf("\n(b) parallel for over subsequences, schedule(static)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(w, in, (size_t)n * sizeof(int)); shell_parallel_static(w, n); });
        table_row(n, THREADS[k], ts, tp,
                  is_sorted_int(w, n) && memcmp(w, ref, (size_t)n * sizeof(int)) == 0);
    }

    /* How much time the serial gap = 1 pass takes on the 4-sorted array. */
    memcpy(w, in, (size_t)n * sizeof(int));
    omp_set_num_threads(8);
    for (long gap = first_gap(n); gap > 1; gap /= 3) {      /* all gaps except the last */
        #pragma omp parallel for schedule(static)
        for (long s = 0; s < gap; s++) gap_insertion_sort(w, n, s, gap);
    }
    double tg = omp_get_wtime();
    gap_insertion_sort(w, n, 0, 1);
    tg = omp_get_wtime() - tg;
    printf("\nlast pass (gap = 1, one subsequence, serial): %.6f s on the nearly sorted array\n", tg);

    free(in); free(ref); free(w);
    return 0;
}

/* Notes:
 * - Gap = 1 is a single subsequence (the whole array), so there is only one
 *   iteration of the parallel for: one thread works, the rest wait. Likewise a
 *   gap g < threads keeps only g threads busy. It stays cheap because after the
 *   larger gaps the array is nearly sorted (insertion sort is ~O(N) then).
 * - Different gaps cannot overlap: gap h/3 mixes elements of all h-subsequences,
 *   hence the barrier (end of parallel for) between gaps.
 * - The parallel version walks each subsequence with stride gap (one cache line
 *   per element for large gaps), while the serial baseline sweeps i
 *   contiguously; that locality loss is why 1 thread is slower than serial.
 * - schedule(dynamic) (chunk 1) hands neighbouring starts s, s+1 to different
 *   threads, which then write neighbouring ints of the same cache line: false
 *   sharing, so it scales worse than static here. Use dynamic with a chunk of
 *   >= 16 ints, or static, when the pieces are equally long.
 * - Shell sort is O(N^1.5) with Knuth gaps: fine for 1e6, but qsort or a
 *   parallel merge sort (P052) is the better choice for large N.
 */

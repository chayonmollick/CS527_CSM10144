/* CS527 Computer Systems — OpenMP Lab
 * Task 3: Maximum/minimum value AND the index where it occurs.
 *  - value only: reduction(max:) / reduction(min:)
 *  - (value,index): critical-section vs manual per-thread reduction
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

static void serial_max(const int *a, long n, int *val, long *idx) {
    int bv = a[0]; long bi = 0;
    for (long i = 1; i < n; i++) if (a[i] > bv) { bv = a[i]; bi = i; }
    *val = bv; *idx = bi;
}
static void serial_min(const int *a, long n, int *val, long *idx) {
    int bv = a[0]; long bi = 0;
    for (long i = 1; i < n; i++) if (a[i] < bv) { bv = a[i]; bi = i; }
    *val = bv; *idx = bi;
}

static int max_val_reduction(const int *a, long n) {
    int mx = INT_MIN;
    #pragma omp parallel for reduction(max:mx)
    for (long i = 0; i < n; i++) if (a[i] > mx) mx = a[i];
    return mx;
}

/* (value,index) via a critical section (matches the lab's sample). */
static void max_idx_critical(const int *a, long n, int *val, long *idx) {
    int bv = a[0]; long bi = 0;
    #pragma omp parallel for
    for (long i = 1; i < n; i++) {
        #pragma omp critical
        {
            if (a[i] > bv) { bv = a[i]; bi = i; }
        }
    }
    *val = bv; *idx = bi;
}

/* (value,index) via manual per-thread reduction: each thread keeps its own
 * best over its chunk, then a short serial pass combines the per-thread bests. */
static void max_idx_manual(const int *a, long n, int *val, long *idx) {
    int nt = omp_get_max_threads();
    int  *lv = malloc((size_t)nt * sizeof(int));
    long *li = malloc((size_t)nt * sizeof(long));
    for (int t = 0; t < nt; t++) { lv[t] = INT_MIN; li[t] = -1; }

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int bv = INT_MIN; long bi = -1;
        #pragma omp for
        for (long i = 0; i < n; i++)
            if (a[i] > bv) { bv = a[i]; bi = i; }
        lv[tid] = bv; li[tid] = bi;
    }
    int bv = INT_MIN; long bi = 0;
    for (int t = 0; t < nt; t++)
        if (li[t] >= 0 && lv[t] > bv) { bv = lv[t]; bi = li[t]; }
    *val = bv; *idx = bi;
    free(lv); free(li);
}

int main(void) {
    const long n = 100000000L;                  /* 1e8 */
    int *a = malloc((size_t)n * sizeof(int));
    if (!a) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < n; i++) a[i] = (int)((i * 1103515245L + 12345L) % 1000000);
    a[n/3]     = 2000000;   /* a known, unique maximum */
    a[2*n/3]   = -2000000;  /* a known, unique minimum */

    int sv, mv; long si, mi;
    serial_max(a, n, &sv, &si);
    serial_min(a, n, &mv, &mi);
    printf("Task 3: Max/Min with index  (N = %ld)\n", n);
    printf("serial : max=%d @ %ld, min=%d @ %ld\n", sv, si, mv, mi);

    printf("reduction(max:) value = %d  (%s)\n",
           max_val_reduction(a, n), max_val_reduction(a, n) == sv ? "OK" : "WRONG");

    int cv; long ci;
    max_idx_manual(a, n, &cv, &ci);
    printf("manual (value,index)  = (%d,%ld)  (%s)\n",
           cv, ci, (cv == sv && ci == si) ? "OK" : "WRONG");

    /* timing: manual at full sizes; critical only at a small N (it serialises
     * every iteration and does not scale to 1e8). */
    const long sizes[]  = {10000000L, 100000000L};
    const int  threads[] = {1, 2, 4, 8};
    printf("\n[manual per-thread reduction] timing for (max,index):\n");
    printf("%12s %8s %12s\n", "N", "threads", "time(s)");
    for (int si2 = 0; si2 < 2; si2++) {
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            int v; long ix;
            double p0 = omp_get_wtime();
            max_idx_manual(a, sizes[si2], &v, &ix);
            double tp = omp_get_wtime() - p0;
            printf("%12ld %8d %12.6f\n", sizes[si2], threads[ti], tp);
        }
    }

    const long smallN = 2000000L;
    printf("\n[critical vs manual] N=%ld:\n", smallN);
    printf("%8s %12s %12s\n", "threads", "critical(s)", "manual(s)");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        int v1; long i1; double c0 = omp_get_wtime();
        max_idx_critical(a, smallN, &v1, &i1);
        double tc = omp_get_wtime() - c0;
        int v2; long i2; double m0 = omp_get_wtime();
        max_idx_manual(a, smallN, &v2, &i2);
        double tm = omp_get_wtime() - m0;
        printf("%8d %12.6f %12.6f  %s\n", threads[ti], tc, tm,
               (v1 == v2 && i1 == i2) ? "" : "MISMATCH");
    }

    free(a);
    return 0;
}

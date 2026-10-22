/* CS527 Computer Systems — OpenMP Lab 8
 * Task 2: Parallel sorting via odd-even transposition sort (the
 * parallel-friendly form of bubble sort).
 *
 * Synchronisation: within one phase the compare-exchange pairs are disjoint
 * (even phase touches (0,1),(2,3),... ; odd phase (1,2),(3,4),...), so they
 * run in parallel with no race. Between phases a barrier is required so that
 * every swap of the current phase is finished before the next phase reads
 * those elements -- the implicit barrier at the end of `parallel for` provides
 * exactly that.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

static void bubble_serial(int *a, long n) {
    for (long k = 0; k < n; k++)
        for (long i = 0; i + 1 < n; i++)
            if (a[i] > a[i+1]) { int t = a[i]; a[i] = a[i+1]; a[i+1] = t; }
}

static void oddeven_parallel(int *a, long n) {
    for (long phase = 0; phase < n; phase++) {
        long start = (phase % 2 == 0) ? 0 : 1;
        #pragma omp parallel for
        for (long i = start; i < n - 1; i += 2)
            if (a[i] > a[i+1]) { int t = a[i]; a[i] = a[i+1]; a[i+1] = t; }
        /* implicit barrier here: phases are serialised, pairs within are not */
    }
}

static int is_sorted(const int *a, long n) {
    for (long i = 0; i + 1 < n; i++) if (a[i] > a[i+1]) return 0;
    return 1;
}
static int same_multiset(const int *a, const int *b, long n) {
    int *x = malloc((size_t)n*sizeof(int)), *y = malloc((size_t)n*sizeof(int));
    for (long i=0;i<n;i++){x[i]=a[i];y[i]=b[i];}
    qsort(x,n,sizeof(int),cmp_int); qsort(y,n,sizeof(int),cmp_int);
    int ok=1; for(long i=0;i<n;i++) if(x[i]!=y[i]){ok=0;break;}
    free(x); free(y); return ok;
}

int main(void) {
    const long sizes[] = {2000, 8000, 20000};   /* O(N^2): keep modest */
    const int  threads[] = {1, 2, 4, 8};
    printf("Task 2: Parallel Sort (odd-even transposition)\n");
    printf("%8s %8s %12s %12s %9s %8s\n",
           "N", "threads", "serial(s)", "parallel(s)", "speedup", "sorted");

    for (int si = 0; si < 3; si++) {
        long n = sizes[si];
        int *orig = malloc((size_t)n*sizeof(int));
        int *ws   = malloc((size_t)n*sizeof(int));
        int *wp   = malloc((size_t)n*sizeof(int));
        srand(42 + si);
        for (long i=0;i<n;i++) orig[i] = rand() % 100000;

        for (long i=0;i<n;i++) ws[i]=orig[i];
        double t0 = omp_get_wtime(); bubble_serial(ws, n); double tsr = omp_get_wtime()-t0;

        for (int ti=0; ti<4; ti++) {
            omp_set_num_threads(threads[ti]);
            for (long i=0;i<n;i++) wp[i]=orig[i];
            double p0 = omp_get_wtime(); oddeven_parallel(wp, n); double tp = omp_get_wtime()-p0;
            int ok = is_sorted(wp, n) && same_multiset(orig, wp, n);
            printf("%8ld %8d %12.6f %12.6f %9.2f %8s\n",
                   n, threads[ti], tsr, tp, tp>0?tsr/tp:0.0, ok?"yes":"NO");
        }
        free(orig); free(ws); free(wp);
    }
    return 0;
}

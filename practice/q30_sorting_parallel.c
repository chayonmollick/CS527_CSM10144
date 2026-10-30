/* CS527 Computer Systems — OpenMP practice
 * Q30: Implement practical parallel sorting algorithms with OpenMP and verify
 *      each against qsort(): (a) chunk sort + parallel pairwise merging,
 *      (b) rank (enumeration) sort, (c) counting sort for a small key range.
 *  - (a) p threads qsort p chunks, then log2(p) rounds of independent pairwise merges
 *  - (b) rank of a[i] = #smaller elements (ties by index), out[rank] = a[i]; O(n^2), parallel over i
 *  - (c) counting sort: parallel histogram with reduction(+:cnt[:R]), serial prefix, parallel fill
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R        1000     /* key range for (b) and (c): values 0..R-1   */
#define RANK_MAX 50000    /* (b) is O(n^2): never run it above this n   */

static unsigned long long rng = 42;            /* deterministic, serial only */
static unsigned next_rand(void) {
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng >> 33);
}
static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}
static int is_sorted(const int *a, long n) {
    for (long i = 0; i + 1 < n; i++) if (a[i] > a[i+1]) return 0;
    return 1;
}
static void header(void) {
    printf("%8s %12s %12s %9s %11s %7s\n",
           "threads", "serial(s)", "parallel(s)", "speedup", "efficiency", "check");
}
static void row(int t, double ts, double tp, int ok) {
    double sp = tp > 0 ? ts / tp : 0.0;
    printf("%8d %12.6f %12.6f %9.2f %11.2f %7s\n",
           t, ts, tp, sp, sp / t, ok ? "OK" : "WRONG");
}

/* ---------------- (a) chunk sort + pairwise merge rounds ---------------- */
static void merge(const int *x, long nx, const int *y, long ny, int *out) {
    long i = 0, j = 0, k = 0;
    while (i < nx && j < ny) out[k++] = (x[i] <= y[j]) ? x[i++] : y[j++];
    while (i < nx) out[k++] = x[i++];
    while (j < ny) out[k++] = y[j++];
}
static long min_l(long a, long b) { return a < b ? a : b; }

static void chunk_merge_sort(int *a, long n) {
    int p = omp_get_max_threads();             /* one chunk per thread */
    int  *tmp = malloc((size_t)n * sizeof(int));
    long *bnd = malloc((size_t)(p + 1) * sizeof(long));
    if (!tmp || !bnd) { fprintf(stderr, "alloc failed\n"); exit(1); }
    for (int k = 0; k <= p; k++) bnd[k] = n * k / p;   /* chunk k = [bnd[k], bnd[k+1]) */

    #pragma omp parallel for schedule(static, 1)
    for (int k = 0; k < p; k++)
        qsort(a + bnd[k], (size_t)(bnd[k+1] - bnd[k]), sizeof(int), cmp_int);

    /* round w: merge sorted run [k, k+w) with run [k+w, k+2w) (in chunks).
     * Pairs in a round touch disjoint ranges -> independent; the implicit
     * barrier of each parallel for separates the rounds. */
    int *src = a, *dst = tmp;
    for (int w = 1; w < p; w *= 2) {
        #pragma omp parallel for schedule(static, 1)
        for (int k = 0; k < p; k += 2 * w) {
            long lo  = bnd[k];
            long mid = bnd[min_l(k + w, p)];
            long hi  = bnd[min_l(k + 2 * w, p)];
            merge(src + lo, mid - lo, src + mid, hi - mid, dst + lo);  /* odd one out: copy */
        }
        int *t = src; src = dst; dst = t;
    }
    if (src != a) memcpy(a, src, (size_t)n * sizeof(int));
    free(tmp); free(bnd);
}

/* ---------------- (b) rank / enumeration sort ---------------- */
/* rank(i) = #{j : a[j] < a[i]  or  (a[j] == a[i] and j < i)}  -> all ranks distinct */
static void rank_sort_serial(const int *a, int *out, long n) {
    for (long i = 0; i < n; i++) {
        long r = 0;
        for (long j = 0; j < i; j++)     r += (a[j] <= a[i]);   /* equal & before i */
        for (long j = i + 1; j < n; j++) r += (a[j] <  a[i]);
        out[r] = a[i];
    }
}
static void rank_sort_parallel(const int *a, int *out, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) {               /* each i writes a DIFFERENT out[r] */
        long r = 0;
        for (long j = 0; j < i; j++)     r += (a[j] <= a[i]);
        for (long j = i + 1; j < n; j++) r += (a[j] <  a[i]);
        out[r] = a[i];
    }
}

/* ---------------- (c) counting sort, keys in 0..R-1 ---------------- */
static void counting_sort_serial(const int *a, int *out, long n) {
    long cnt[R] = {0};
    for (long i = 0; i < n; i++) cnt[a[i]]++;
    long k = 0;
    for (int v = 0; v < R; v++)
        for (long c = 0; c < cnt[v]; c++) out[k++] = v;
}
static void counting_sort_parallel(const int *a, int *out, long n) {
    long cnt[R] = {0}, start[R];
    #pragma omp parallel for reduction(+:cnt[:R])   /* private histogram per thread */
    for (long i = 0; i < n; i++) cnt[a[i]]++;
    long s = 0;                                      /* serial exclusive prefix sum */
    for (int v = 0; v < R; v++) { start[v] = s; s += cnt[v]; }
    #pragma omp parallel for schedule(static)       /* value v fills its own range */
    for (int v = 0; v < R; v++)
        for (long c = 0; c < cnt[v]; c++) out[start[v] + c] = v;
}

int main(int argc, char **argv) {
    long n = 5000000L, nr = 20000L;
    if (argc > 1) { n = atol(argv[1]); nr = n <= RANK_MAX ? n : RANK_MAX; }
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    const int threads[] = {1, 2, 4, 8};
    printf("Q30: Parallel sorting (chunk+merge, rank sort, counting sort)\n");

    int *orig = malloc((size_t)n * sizeof(int));     /* full-range keys */
    int *keys = malloc((size_t)n * sizeof(int));     /* keys in 0..R-1  */
    int *ref  = malloc((size_t)n * sizeof(int));
    int *w    = malloc((size_t)n * sizeof(int));
    if (!orig || !keys || !ref || !w) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < n; i++) orig[i] = (int)(next_rand() % 1000000000u);
    for (long i = 0; i < n; i++) keys[i] = (int)(next_rand() % R);

    /* (a) -------------------------------------------------------------- */
    memcpy(ref, orig, (size_t)n * sizeof(int));
    double t0 = omp_get_wtime(); qsort(ref, (size_t)n, sizeof(int), cmp_int);
    double ts = omp_get_wtime() - t0;
    printf("\n(a) chunk qsort + pairwise merge rounds, N = %ld (serial = qsort)\n", n);
    header();
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        memcpy(w, orig, (size_t)n * sizeof(int));
        double p0 = omp_get_wtime(); chunk_merge_sort(w, n); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, is_sorted(w, n) && memcmp(w, ref, (size_t)n * sizeof(int)) == 0);
    }

    /* (b) -------------------------------------------------------------- */
    memcpy(ref, keys, (size_t)nr * sizeof(int));          /* first nr keys */
    qsort(ref, (size_t)nr, sizeof(int), cmp_int);
    t0 = omp_get_wtime(); rank_sort_serial(keys, w, nr); ts = omp_get_wtime() - t0;
    printf("\n(b) rank sort, N = %ld (keys 0..%d, many ties), serial %s\n", nr, R - 1,
           memcmp(w, ref, (size_t)nr * sizeof(int)) == 0 ? "OK" : "WRONG");
    header();
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        memset(w, 0, (size_t)nr * sizeof(int));
        double p0 = omp_get_wtime(); rank_sort_parallel(keys, w, nr); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, memcmp(w, ref, (size_t)nr * sizeof(int)) == 0);
    }

    /* (c) -------------------------------------------------------------- */
    memcpy(ref, keys, (size_t)n * sizeof(int));
    t0 = omp_get_wtime(); qsort(ref, (size_t)n, sizeof(int), cmp_int);
    double tq = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); counting_sort_serial(keys, w, n); ts = omp_get_wtime() - t0;
    printf("\n(c) counting sort, N = %ld, keys 0..%d (qsort %.6f s), serial %s\n", n, R - 1, tq,
           memcmp(w, ref, (size_t)n * sizeof(int)) == 0 ? "OK" : "WRONG");
    header();
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        memset(w, 0, (size_t)n * sizeof(int));
        double p0 = omp_get_wtime(); counting_sort_parallel(keys, w, n); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, memcmp(w, ref, (size_t)n * sizeof(int)) == 0);
    }

    free(orig); free(keys); free(ref); free(w);
    return 0;
}

/* Viva notes:
 * - (a) Work: p chunk sorts of n/p in parallel, then ceil(log2 p) merge rounds.
 *   Round r has p/2^r independent merges, so the LAST merge (n elements) is
 *   done by one thread: O(n) serial tail limits speedup. Fix: split the last
 *   merges with binary search ("merge path") so several threads share them.
 * - (b) Rank sort: rank(i) depends only on the input, never on out[], and the
 *   index tie-break makes every rank unique, so out[rank] = a[i] needs no lock
 *   and is stable. O(n^2) work -> only for small n, but embarrassingly parallel.
 * - (c) Histogram: cnt[a[i]]++ from many threads is a race; reduction(+:cnt[:R])
 *   (OpenMP 4.5 array section) gives each thread a private copy summed at the
 *   end. The prefix over R values is cheap, so it stays serial; each value then
 *   fills a disjoint output range. O(n + R), beats qsort for small R.
 * - For records with payload, counting sort needs stable placement: per-thread
 *   histograms, prefix over (value, thread) to get each thread's write offsets.
 * - Odd-even transposition (lab) is O(n^2) with n barriers; these are the
 *   practical alternatives.
 */

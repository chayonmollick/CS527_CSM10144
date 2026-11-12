/* M08: Sparse matrix: COO to CSR conversion
 * Problem: An n x n sparse matrix is given in coordinate (COO) format: nnz triplets
 *   (row[k], col[k], val[k]), k = 0..nnz-1, in no particular order. Convert it to
 *   compressed sparse row (CSR) format: row_ptr[n+1], col_idx[nnz], val[nnz], where
 *   the nonzeros of row r sit at positions row_ptr[r] .. row_ptr[r+1]-1 in the order
 *   in which they appear in the COO list. Do it serially and with OpenMP, verify
 *   the arrays against serial, check y = A x from CSR against y from COO, check a
 *   4 x 4 example by hand, and report the timings.
 * Solution: (a) a counting sort by row in three steps, all in one parallel region
 *   with barriers between them:
 *   (1) row counts = a histogram of row[k]. Many k hit the same row, so a plain
 *       cnt[row[k]]++ in a parallel for races. Thread t counts its own contiguous
 *       block of COO entries into its own array cnt[t][*].
 *   (2) row_ptr = exclusive prefix sum of the row counts, as a blocked two-pass
 *       scan (block sums per thread, a serial scan of the T block sums in a single,
 *       then every thread finishes its block). At the same time each row's
 *       per-thread counts become offsets: thread t's entries of row r go after
 *       those of threads 0..t-1.
 *   (3) scatter: thread t walks its block in COO order and puts entry k at
 *       row_ptr[r] + offset[t][r]++. Every position is written once, and inside
 *       each row the entries keep their COO order, so the result is IDENTICAL to
 *       the serial (stable) conversion, for any number of threads.
 *   (b) with atomics instead: "omp atomic" counts, a serial scan, "omp atomic
 *       capture" pos = next[r]++ to claim positions. Simpler, but the order inside
 *       a row depends on timing, so each row is then sorted by COO index.
 * Source: lab extension (sparse formats; problems/p076 multiplies a CSR matrix).
 * Build: gcc -O2 -Wall -Wextra -fopenmp -I../../problems m08_coo_to_csr.c -o m08 -lm
 * Run:   ./m08 [n] [nnz]   (defaults n = 200000 rows, nnz = 2000000)
 */
#include "../../problems/common.h"

typedef struct { long *row_ptr; int *col_idx; double *val; } csr_t;

static csr_t csr_alloc(long n, long nnz) {
    csr_t c;
    c.row_ptr = xmalloc((size_t)(n + 1) * sizeof(long));
    c.col_idx = xmalloc((size_t)nnz * sizeof(int));
    c.val = xmalloc((size_t)nnz * sizeof(double));
    return c;
}
static void csr_free(csr_t c) { free(c.row_ptr); free(c.col_idx); free(c.val); }

static int same_csr(csr_t a, csr_t b, long n, long nnz) {
    return same_longs(a.row_ptr, b.row_ptr, n + 1) && same_ints(a.col_idx, b.col_idx, nnz)
        && memcmp(a.val, b.val, (size_t)nnz * sizeof(double)) == 0;
}

/* Serial reference: stable counting sort by row. next[] is n longs of scratch. */
static void coo_to_csr_serial(long n, long nnz, const int *row, const int *col,
                              const double *val, csr_t c, long *next) {
    for (long r = 0; r <= n; r++) c.row_ptr[r] = 0;
    for (long k = 0; k < nnz; k++) c.row_ptr[row[k] + 1]++;          /* counts */
    for (long r = 0; r < n; r++) c.row_ptr[r + 1] += c.row_ptr[r];   /* prefix sum */
    for (long r = 0; r < n; r++) next[r] = c.row_ptr[r];
    for (long k = 0; k < nnz; k++) {                                  /* scatter */
        long pos = next[row[k]]++;
        c.col_idx[pos] = col[k];
        c.val[pos] = val[k];
    }
}

/* (a) deterministic: per-thread counts and offsets, one parallel region.
 * Scratch (allocated once by the caller, T >= number of threads):
 *   cnt  T*n longs, cnt[t*n + r] = thread t's count (later: offset) for row r;
 *   part T longs, the block sums of the scan. */
static void coo_to_csr_parallel(long n, long nnz, const int *row, const int *col,
                                const double *val, csr_t c, long *cnt, long *part) {
    #pragma omp parallel
    {
        int t = omp_get_thread_num(), nth = omp_get_num_threads();
        long lo = nnz * t / nth, hi = nnz * (t + 1) / nth;   /* my COO entries */
        long rlo = n * t / nth, rhi = n * (t + 1) / nth;     /* my rows */
        long *my = cnt + (long)t * n;

        /* (1) histogram of my entries, in my own array */
        for (long r = 0; r < n; r++) my[r] = 0;
        for (long k = lo; k < hi; k++) my[row[k]]++;
        #pragma omp barrier

        /* (2a) for my rows: counts of threads 0..nth-1 -> offsets; row length */
        for (long r = rlo; r < rhi; r++) {
            long run = 0;
            for (int u = 0; u < nth; u++) {
                long cu = cnt[(long)u*n + r];
                cnt[(long)u*n + r] = run;
                run += cu;
            }
            c.row_ptr[r + 1] = run;                           /* length of row r for now */
        }
        /* (2b) exclusive scan of the row lengths: my block sum ... */
        long s = 0;
        for (long r = rlo; r < rhi; r++) s += c.row_ptr[r + 1];
        part[t] = s;
        #pragma omp barrier
        #pragma omp single
        {                                                     /* ... scan of T block sums ... */
            long run = 0;
            for (int u = 0; u < nth; u++) { long pu = part[u]; part[u] = run; run += pu; }
            c.row_ptr[0] = 0;
        }                                                     /* (barrier) */
        long run = part[t];                                   /* ... and finish my block */
        for (long r = rlo; r < rhi; r++) { run += c.row_ptr[r + 1]; c.row_ptr[r + 1] = run; }
        #pragma omp barrier

        /* (3) scatter my entries in COO order to my reserved positions */
        for (long k = lo; k < hi; k++) {
            long r = row[k], pos = c.row_ptr[r] + my[r]++;
            c.col_idx[pos] = col[k];
            c.val[pos] = val[k];
        }
    }
}

/* (b) atomics; src[] (nnz longs) records which COO entry went to each position. */
static void coo_to_csr_atomic(long n, long nnz, const int *row, const int *col, const double *val,
                              csr_t c, long *next, long *src) {
    #pragma omp parallel for
    for (long r = 0; r <= n; r++) c.row_ptr[r] = 0;
    #pragma omp parallel for
    for (long k = 0; k < nnz; k++) {
        #pragma omp atomic
        c.row_ptr[row[k] + 1]++;
    }
    for (long r = 0; r < n; r++) c.row_ptr[r + 1] += c.row_ptr[r];   /* O(n): serial */
    #pragma omp parallel for
    for (long r = 0; r < n; r++) next[r] = c.row_ptr[r];
    #pragma omp parallel for
    for (long k = 0; k < nnz; k++) {
        long pos;
        #pragma omp atomic capture
        pos = next[row[k]]++;
        src[pos] = k;
    }
    #pragma omp parallel for schedule(dynamic, 256)
    for (long r = 0; r < n; r++) {
        long a = c.row_ptr[r], e = c.row_ptr[r + 1];
        for (long p = a + 1; p < e; p++) {                   /* insertion sort by COO index */
            long v = src[p], q = p - 1;
            while (q >= a && src[q] > v) { src[q + 1] = src[q]; q--; }
            src[q + 1] = v;
        }
        for (long p = a; p < e; p++) { c.col_idx[p] = col[src[p]]; c.val[p] = val[src[p]]; }
    }
}

/* y = A x from CSR (parallel over rows) and from COO (serial scatter-add). */
static void spmv_csr(long n, csr_t c, const double *x, double *y) {
    #pragma omp parallel for schedule(dynamic, 256)
    for (long r = 0; r < n; r++) {
        double s = 0.0;
        for (long p = c.row_ptr[r]; p < c.row_ptr[r + 1]; p++) s += c.val[p] * x[c.col_idx[p]];
        y[r] = s;
    }
}
static void spmv_coo(long n, long nnz, const int *row, const int *col, const double *val,
                     const double *x, double *y) {
    for (long r = 0; r < n; r++) y[r] = 0.0;
    for (long k = 0; k < nnz; k++) y[row[k]] += val[k] * x[col[k]];
}

static void hand_example(void) {
    /* A = [4 0 0 1; 0 0 3 0; 0 5 0 7; 2 0 0 0], entries listed in a scrambled order */
    const int row[6] = {2, 0, 3, 0, 2, 1}, col[6] = {1, 3, 0, 0, 3, 2};
    const double val[6] = {5, 1, 2, 4, 7, 3};
    const long rp[5] = {0, 2, 3, 5, 6};
    const int ci[6] = {3, 0, 2, 1, 3, 0};
    const double cv[6] = {1, 4, 3, 5, 7, 2};
    csr_t a = csr_alloc(4, 6), b = csr_alloc(4, 6);
    long next[4], src[6];
    int T = omp_get_max_threads();
    long *cnt = xmalloc((size_t)T * 4 * sizeof(long)), *part = xmalloc((size_t)T * sizeof(long));
    coo_to_csr_parallel(4, 6, row, col, val, a, cnt, part);
    coo_to_csr_atomic(4, 6, row, col, val, b, next, src);
    int ok = same_longs(a.row_ptr, rp, 5) && same_ints(a.col_idx, ci, 6)
          && close_doubles(a.val, cv, 6, 0) && same_csr(a, b, 4, 6);
    printf("Hand example: A = [4 0 0 1; 0 0 3 0; 0 5 0 7; 2 0 0 0],\n"
           "  COO (row,col,val) = (2,1,5) (0,3,1) (3,0,2) (0,0,4) (2,3,7) (1,2,3)\n");
    printf("  row_ptr = %ld %ld %ld %ld %ld, col_idx = %d %d %d %d %d %d\n",
           a.row_ptr[0], a.row_ptr[1], a.row_ptr[2], a.row_ptr[3], a.row_ptr[4],
           a.col_idx[0], a.col_idx[1], a.col_idx[2], a.col_idx[3], a.col_idx[4], a.col_idx[5]);
    printf("  expected row_ptr = 0 2 3 5 6, col_idx = 3 0 2 1 3 0, val = 1 4 3 5 7 2: %s\n\n",
           ok_str(ok));
    csr_free(a); csr_free(b); free(cnt); free(part);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 200000), nnz = arg_long(argc, argv, 2, 2000000);
    if (n < 1 || nnz < 1 || n > INT_MAX) {
        fprintf(stderr, "need 1 <= n <= INT_MAX and nnz >= 1\n");
        return 1;
    }
    int *row = xmalloc((size_t)nnz * sizeof(int)), *col = xmalloc((size_t)nnz * sizeof(int));
    double *val = xmalloc((size_t)nnz * sizeof(double));
    double *x = xmalloc((size_t)n * sizeof(double));
    double *y1 = xmalloc((size_t)n * sizeof(double)), *y2 = xmalloc((size_t)n * sizeof(double));
    long *next = xmalloc((size_t)n * sizeof(long)), *src = xmalloc((size_t)nnz * sizeof(long));
    fill_int(row, nnz, 0, (int)(n - 1), 51);
    fill_int(col, nnz, 0, (int)(n - 1), 52);
    fill_double(val, nnz, -1.0, 1.0, 53);
    fill_double(x, n, -1.0, 1.0, 54);
    csr_t cs = csr_alloc(n, nnz), cp = csr_alloc(n, nnz);
    int maxT = omp_get_max_threads();        /* scratch for (a), big enough for every */
    for (int k = 0; k < NTHREADS; k++)       /* thread count used below */
        if (THREADS[k] > maxT) maxT = THREADS[k];
    long *cnt = xmalloc((size_t)maxT * n * sizeof(long));
    long *part = xmalloc((size_t)maxT * sizeof(long));
    omp_warmup();

    hand_example();
    double ts, tp;
    BEST_TIME(ts, coo_to_csr_serial(n, nnz, row, col, val, cs, next));
    spmv_coo(n, nnz, row, col, val, x, y1);
    spmv_csr(n, cs, x, y2);
    printf("M08: n = %ld rows, nnz = %ld random entries (%.1f per row on average)\n",
           n, nnz, (double)nnz / n);
    printf("serial CSR: row_ptr[n] = %ld, y = A x from CSR equals y from COO: %s\n",
           cs.row_ptr[n], ok_str(cs.row_ptr[n] == nnz && close_doubles(y1, y2, n, 1e-12)));

    printf("(a) per-thread counts + blocked scan + scatter to reserved slots (deterministic)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(cp.col_idx, 0, (size_t)nnz * sizeof(int));
        BEST_TIME(tp, coo_to_csr_parallel(n, nnz, row, col, val, cp, cnt, part));
        table_row(nnz, THREADS[k], ts, tp, same_csr(cs, cp, n, nnz));
    }
    printf("(b) atomic counts + atomic capture slots + per-row sort by COO index\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(cp.col_idx, 0, (size_t)nnz * sizeof(int));
        BEST_TIME(tp, coo_to_csr_atomic(n, nnz, row, col, val, cp, next, src));
        table_row(nnz, THREADS[k], ts, tp, same_csr(cs, cp, n, nnz));
    }
    spmv_csr(n, cp, x, y2);
    printf("y = A x from the parallel CSR equals y from COO: %s\n",
           ok_str(close_doubles(y1, y2, n, 1e-12)));
    csr_free(cs); csr_free(cp);
    free(row); free(col); free(val); free(x); free(y1); free(y2); free(next); free(src);
    free(cnt); free(part);
    return 0;
}

/* Notes:
 * - CSR stores n+1 row pointers instead of nnz row indices; row r is the slice
 *   row_ptr[r] .. row_ptr[r+1]-1, so y[r] = sum val[p] * x[col_idx[p]] needs no
 *   search, and a parallel for over rows has one writer per y[r] (p076). COO is
 *   easy to build (append triplets) but its SpMV y[row[k]] += ... races in parallel.
 * - Conversion = counting sort by row: histogram, exclusive prefix sum, scatter.
 *   Each step has a race if done naively: cnt[row[k]]++ (two threads, same row),
 *   the running sum of the scan (each entry needs the previous one), and the
 *   cursor next[r]++ in the scatter (two threads, same slot).
 * - (a) avoids every atomic: thread t owns cnt[t][*] and its slots. Its cost is
 *   T*n extra memory and O(n) work per thread for the offsets, so it pays off when
 *   nnz is much larger than T*n. Blocks of COO entries must be the same in steps
 *   (1) and (3): here they are computed explicitly from t and nth.
 * - Blocked scan (OpenMP 4.5 has no scan directive; OpenMP 5.0 adds "inscan"):
 *   sum per block, scan the T block sums, add the block offset while scanning
 *   each block. Two passes, O(n) work, two barriers.
 * - (b) is shorter but the slot order inside a row depends on which thread wins
 *   each atomic, so the output would change from run to run; sorting each row by
 *   COO index (or by column, the usual canonical CSR) restores a unique answer.
 * - Duplicate (row, col) entries are kept as separate entries; SpMV adds them.
 *   To merge them, sort each row by column and add equal neighbours.
 * - Random row indices make the scatter a random-access, memory-latency-bound
 *   pass: expect modest speedups.
 */

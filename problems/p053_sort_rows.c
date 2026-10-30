/* P053: Sort every row (and every column) of a matrix in parallel
 * Problem: Given an R x C int matrix (row-major), (a) sort each row
 *   independently; (b) the same for a jagged matrix whose rows have different
 *   lengths; (c) sort each column independently. Parallelise with OpenMP,
 *   verify against the serial versions and report time and speedup for 1, 2,
 *   4 and 8 threads.
 * Solution: rows (and columns) are disjoint, so sorting them is embarrassingly
 *   parallel: parallel for over rows, each iteration calls qsort on its own row
 *   -> no shared writes, no races. (a) equal rows -> schedule(static).
 *   (b) row cost ~ L log L varies with the length L -> schedule(dynamic) hands
 *   out rows on demand; static is shown for comparison. (c) a column is not
 *   contiguous, so each thread allocates ITS OWN buffer inside the parallel
 *   region (private), copies column j into it, sorts it and copies it back;
 *   a single shared buffer would be a race.
 * Source: lab extension (Lab 9 sorting; Lab 8 matrix loops).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p053_sort_rows.c -o p053 -lm
 * Run:   ./p053 [R] [C]      (defaults R = C = 1000)
 */
#include "common.h"

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

/* ---------- (a) rows of equal length ---------- */
static void sort_rows_serial(int *m, long R, long C) {
    for (long r = 0; r < R; r++) qsort(m + r * C, (size_t)C, sizeof(int), cmp_int);
}
static void sort_rows_parallel(int *m, long R, long C) {
    #pragma omp parallel for schedule(static)
    for (long r = 0; r < R; r++) qsort(m + r * C, (size_t)C, sizeof(int), cmp_int);
}

/* ---------- (b) jagged rows: row r is d[start[r] .. start[r+1]) ---------- */
static void sort_jagged_serial(int *d, const long *start, long R) {
    for (long r = 0; r < R; r++)
        qsort(d + start[r], (size_t)(start[r + 1] - start[r]), sizeof(int), cmp_int);
}
static void sort_jagged_static(int *d, const long *start, long R) {
    #pragma omp parallel for schedule(static)
    for (long r = 0; r < R; r++)
        qsort(d + start[r], (size_t)(start[r + 1] - start[r]), sizeof(int), cmp_int);
}
static void sort_jagged_dynamic(int *d, const long *start, long R) {
    #pragma omp parallel for schedule(dynamic)
    for (long r = 0; r < R; r++)
        qsort(d + start[r], (size_t)(start[r + 1] - start[r]), sizeof(int), cmp_int);
}

/* ---------- (c) columns ---------- */
static void sort_cols_serial(int *m, long R, long C) {
    int *buf = xmalloc((size_t)R * sizeof(int));
    for (long j = 0; j < C; j++) {
        for (long i = 0; i < R; i++) buf[i] = m[i * C + j];
        qsort(buf, (size_t)R, sizeof(int), cmp_int);
        for (long i = 0; i < R; i++) m[i * C + j] = buf[i];
    }
    free(buf);
}
static void sort_cols_parallel(int *m, long R, long C) {
    #pragma omp parallel
    {
        int *buf = xmalloc((size_t)R * sizeof(int));     /* one private buffer per thread */
        #pragma omp for schedule(static)
        for (long j = 0; j < C; j++) {
            for (long i = 0; i < R; i++) buf[i] = m[i * C + j];
            qsort(buf, (size_t)R, sizeof(int), cmp_int);
            for (long i = 0; i < R; i++) m[i * C + j] = buf[i];
        }
        free(buf);
    }
}

int main(int argc, char *argv[]) {
    long R = arg_long(argc, argv, 1, 1000L);
    long C = arg_long(argc, argv, 2, 1000L);
    if (R < 1) R = 1;
    if (C < 1) C = 1;
    size_t bytes = (size_t)(R * C) * sizeof(int);
    int *m0 = xmalloc(bytes), *ref = xmalloc(bytes), *w = xmalloc(bytes);
    fill_int(m0, R * C, -1000000, 1000000, 53);
    omp_warmup();
    double ts, tp;
    printf("P053: sort rows / columns of a %ld x %ld matrix (qsort per row or column)\n", R, C);
    /* In-place sorts: each timed run first restores the input (memcpy is inside the timing). */

    memcpy(ref, m0, bytes); sort_rows_serial(ref, R, C);
    BEST_TIME(ts, { memcpy(w, m0, bytes); sort_rows_serial(w, R, C); });
    printf("\n(a) sort each row, parallel for schedule(static)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(w, m0, bytes); sort_rows_parallel(w, R, C); });
        table_row(R * C, THREADS[k], ts, tp, same_ints(w, ref, R * C));
    }

    /* Jagged matrix: row r has about 2*C*r/R elements (short rows first, long rows last). */
    long *start = xmalloc((size_t)(R + 1) * sizeof(long));
    start[0] = 0;
    for (long r = 0; r < R; r++) start[r + 1] = start[r] + 1 + 2 * C * r / R;
    long nj = start[R];
    int *j0 = xmalloc((size_t)nj * sizeof(int)), *jref = xmalloc((size_t)nj * sizeof(int));
    int *jw = xmalloc((size_t)nj * sizeof(int));
    fill_int(j0, nj, -1000000, 1000000, 5353);
    size_t jbytes = (size_t)nj * sizeof(int);
    memcpy(jref, j0, jbytes); sort_jagged_serial(jref, start, R);
    BEST_TIME(ts, { memcpy(jw, j0, jbytes); sort_jagged_serial(jw, start, R); });
    printf("\n(b) jagged rows (lengths 1 .. %ld, growing with r), schedule(static)\n",
           1 + 2 * C * (R - 1) / R);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(jw, j0, jbytes); sort_jagged_static(jw, start, R); });
        table_row(nj, THREADS[k], ts, tp, same_ints(jw, jref, nj));
    }
    printf("    same rows, schedule(dynamic)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(jw, j0, jbytes); sort_jagged_dynamic(jw, start, R); });
        table_row(nj, THREADS[k], ts, tp, same_ints(jw, jref, nj));
    }

    memcpy(ref, m0, bytes); sort_cols_serial(ref, R, C);
    BEST_TIME(ts, { memcpy(w, m0, bytes); sort_cols_serial(w, R, C); });
    printf("\n(c) sort each column (private column buffer per thread)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, { memcpy(w, m0, bytes); sort_cols_parallel(w, R, C); });
        table_row(R * C, THREADS[k], ts, tp, same_ints(w, ref, R * C));
    }
    free(m0); free(ref); free(w); free(start); free(j0); free(jref); free(jw);
    return 0;
}

/* Notes:
 * - Independent rows = no dependence between iterations; qsort is reentrant
 *   (no global state), so calling it from many threads is safe.
 * - (b) with static, the thread holding the last (longest) rows does far more
 *   work than the others; dynamic gives the next row to whichever thread is
 *   free. Sorting rows longest-first and using dynamic is even better.
 * - (c) buf must be private: a shared buffer would be overwritten by other
 *   threads mid-sort. Allocating it once per thread (not per column) avoids
 *   R x C mallocs. A VLA or `int *buf` declared inside the region also works.
 * - Column access has stride C (one cache line per element); an alternative is
 *   transpose -> sort rows -> transpose back.
 * - Few rows but long ones (R < threads): parallelise inside each row instead
 *   (parallel merge sort, P052).
 */

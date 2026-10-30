/* P088: Pascal's triangle (binomial coefficients) row by row
 * Problem: (Textbook Ex 24.4.) Compute the binomial coefficients C(r, c),
 *   0 <= c <= r <= R, in a triangular array, where row r comes from row r-1:
 *   C(r, 0) = C(r, r) = 1 and C(r, c) = C(r-1, c-1) + C(r-1, c). Work modulo
 *   M = 1000000007 so the numbers stay bounded. Store the triangle in one array:
 *   row r starts at offset r(r+1)/2, so C(r, c) = T[r(r+1)/2 + c]. Parallelise
 *   with OpenMP (rows are sequential, the entries of a row are parallel); also
 *   compute only row R with two row buffers, and build a small triangle with
 *   tasks and depend clauses. Verify against the serial code and against the
 *   closed form C(r, c) = r! / (c! (r-c)!).
 * Solution: row r needs the whole of row r-1 (sequential in r), but its entries
 *   only read row r-1, so they are independent: "omp for" over c inside one
 *   parallel region; its implicit barrier finishes row r before row r+1 starts.
 *   Single row R: keep two buffers, row r in buf[r % 2], reading buf[(r-1) % 2]
 *   (never update one row in place in parallel: entry c would read entry c-1
 *   that another thread may already have overwritten). Tasks: one task per
 *   entry with depend(in: the two entries above) depend(out: this entry); the
 *   runtime starts a task as soon as its inputs are done (no row barrier), but
 *   one addition per task is far too little work to pay for a task.
 *   Integers mod M -> results must match exactly.
 * Source: textbook Ex 24.4.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p088_pascal_triangle.c -o p088 -lm
 * Run:   ./p088 [R] [R1] [Rt]  (defaults R = 4000 full triangle, R1 = 10000 single
 *                               row, Rt = 300 triangle with tasks)
 */
#include "common.h"

#define MOD 1000000007L

static inline long off(long r) { return r * (r + 1) / 2; }     /* start of row r */

/* ---------- whole triangle, rows 0..R ---------- */

static void triangle_serial(long *T, long R) {
    T[0] = 1;
    for (long r = 1; r <= R; r++) {
        const long *prev = T + off(r - 1);
        long *row = T + off(r);
        for (long c = 0; c <= r; c++)
            row[c] = (c == 0 || c == r) ? 1 : (prev[c - 1] + prev[c]) % MOD;
    }
}

static void triangle_parallel(long *T, long R) {
    T[0] = 1;
    #pragma omp parallel
    for (long r = 1; r <= R; r++) {               /* every thread walks the rows */
        const long *prev = T + off(r - 1);
        long *row = T + off(r);
        #pragma omp for
        for (long c = 0; c <= r; c++)
            row[c] = (c == 0 || c == r) ? 1 : (prev[c - 1] + prev[c]) % MOD;
        /* implicit barrier: row r complete before row r+1 reads it */
    }
}

/* ---------- only row R, two row buffers ---------- */

static void row_serial(long *out, long R) {
    long *buf[2] = {xmalloc((size_t)(R + 1) * sizeof(long)), xmalloc((size_t)(R + 1) * sizeof(long))};
    buf[0][0] = 1;
    for (long r = 1; r <= R; r++) {
        const long *prev = buf[(r - 1) % 2];
        long *row = buf[r % 2];
        for (long c = 0; c <= r; c++)
            row[c] = (c == 0 || c == r) ? 1 : (prev[c - 1] + prev[c]) % MOD;
    }
    memcpy(out, buf[R % 2], (size_t)(R + 1) * sizeof(long));
    free(buf[0]); free(buf[1]);
}

static void row_parallel(long *out, long R) {
    long *buf[2] = {xmalloc((size_t)(R + 1) * sizeof(long)), xmalloc((size_t)(R + 1) * sizeof(long))};
    buf[0][0] = 1;
    #pragma omp parallel
    for (long r = 1; r <= R; r++) {
        const long *prev = buf[(r - 1) % 2];      /* read row r-1 ... */
        long *row = buf[r % 2];                   /* ... write row r into the other buffer */
        #pragma omp for
        for (long c = 0; c <= r; c++)
            row[c] = (c == 0 || c == r) ? 1 : (prev[c - 1] + prev[c]) % MOD;
    }
    memcpy(out, buf[R % 2], (size_t)(R + 1) * sizeof(long));
    free(buf[0]); free(buf[1]);
}

/* ---------- whole triangle with one task per entry and depend clauses ---------- */

static void triangle_tasks(long *T, long R) {
    #pragma omp parallel
    #pragma omp single
    for (long r = 0; r <= R; r++)
        for (long c = 0; c <= r; c++) {
            long k = off(r) + c;                  /* this entry */
            if (c == 0 || c == r) {
                #pragma omp task depend(out: T[k]) firstprivate(k)
                T[k] = 1;
            } else {
                long up = off(r - 1) + c;         /* C(r-1, c); C(r-1, c-1) is up - 1 */
                #pragma omp task depend(in: T[up - 1], T[up]) depend(out: T[k]) firstprivate(k, up)
                T[k] = (T[up - 1] + T[up]) % MOD;
            }
        }
    /* implicit barriers at the end of single and parallel: all tasks done */
}

/* ---------- closed form, for checking ---------- */

/* Exact C(r, c) for r <= 60 with 64-bit integers: C(r,k) = C(r,k-1)*(r-k+1)/k. */
static long binom_exact(long r, long c) {
    long v = 1;
    for (long k = 1; k <= c; k++) v = v * (r - k + 1) / k;
    return v;
}

static long powmod(long b, long e) {
    long res = 1;
    b %= MOD;
    while (e > 0) {
        if (e & 1) res = res * b % MOD;
        b = b * b % MOD;
        e >>= 1;
    }
    return res;
}

/* Check row R against C(R, c) = R! / (c! (R-c)!) mod M (Fermat inverse). */
static int check_row_closed_form(const long *row, long R) {
    long *fact = xmalloc((size_t)(R + 1) * sizeof(long));
    fact[0] = 1;
    for (long k = 1; k <= R; k++) fact[k] = fact[k - 1] * k % MOD;
    int ok = 1;
    for (long c = 0; c <= R; c++) {
        long denom = fact[c] * fact[R - c] % MOD;
        if (row[c] != fact[R] * powmod(denom, MOD - 2) % MOD) ok = 0;
    }
    free(fact);
    return ok;
}

int main(int argc, char *argv[]) {
    long R  = arg_long(argc, argv, 1, 4000);
    long R1 = arg_long(argc, argv, 2, 10000);
    long Rt = arg_long(argc, argv, 3, 300);
    if (R < 60) R = 60;                           /* the exact check below uses rows 0..60 */
    if (R1 < 1) R1 = 1;
    if (Rt < 1) Rt = 1;

    long *Ts = xmalloc((size_t)off(R + 1) * sizeof(long));
    long *Tp = xmalloc((size_t)off(R + 1) * sizeof(long));
    omp_warmup();
    triangle_parallel(Tp, 10);
    printf("P088: Pascal's triangle mod %ld\nrows 0..10 (parallel):\n", MOD);
    for (long r = 0; r <= 10; r++) {
        printf("%*s", (int)(2 * (10 - r)), "");
        for (long c = 0; c <= r; c++) printf("%4ld", Tp[off(r) + c]);
        printf("\n");
    }

    double ts, tp;
    BEST_TIME(ts, triangle_serial(Ts, R));
    int exact_ok = 1;
    for (long r = 0; r <= 60; r++)
        for (long c = 0; c <= r; c++)
            if (Ts[off(r) + c] != binom_exact(r, c) % MOD) exact_ok = 0;
    printf("\nrows 0..60 vs exact C(r,c) = r!/(c!(r-c)!) mod M: %s;  row %ld vs factorial formula: %s\n",
           ok_str(exact_ok), R, ok_str(check_row_closed_form(Ts + off(R), R)));
    printf("C(60, 30) = %ld\n", binom_exact(60, 30));

    printf("\nWhole triangle, rows 0..%ld (%ld entries): omp for over each row (check: equal to serial)\n",
           R, off(R + 1));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, triangle_parallel(Tp, R));
        table_row(R, THREADS[k], ts, tp, same_longs(Ts, Tp, off(R + 1)));
    }
    free(Ts); free(Tp);

    long *rs = xmalloc((size_t)(R1 + 1) * sizeof(long)), *rp = xmalloc((size_t)(R1 + 1) * sizeof(long));
    BEST_TIME(ts, row_serial(rs, R1));
    printf("\nOnly row %ld, two row buffers (check: equal to serial and to the factorial formula)\n", R1);
    int closed_ok = check_row_closed_form(rs, R1);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, row_parallel(rp, R1));
        table_row(R1, THREADS[k], ts, tp, closed_ok && same_longs(rs, rp, R1 + 1));
    }
    free(rs); free(rp);

    long *As = xmalloc((size_t)off(Rt + 1) * sizeof(long)), *At = xmalloc((size_t)off(Rt + 1) * sizeof(long));
    BEST_TIME(ts, triangle_serial(As, Rt));
    printf("\nTriangle rows 0..%ld with one task per entry + depend (%ld tasks; check: equal to serial)\n",
           Rt, off(Rt + 1));
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        for (long e = 0; e < off(Rt + 1); e++) At[e] = -1;
        BEST_TIME(tp, triangle_tasks(At, Rt));
        table_row(Rt, THREADS[k], ts, tp, same_longs(As, At, off(Rt + 1)));
    }
    free(As); free(At);
    return 0;
}

/* Notes:
 * - Dependence: row r needs row r-1 (sequential); entries of one row are
 *   independent. Expect speedup < 1 here: row r is r+1 cheap additions
 *   (~0.5 ns each, a few microseconds per row) but every row costs a barrier
 *   of similar or larger cost. Parallel rows only pay off for very long rows
 *   or much more work per entry (granularity).
 * - In-place single row (for c = r down to 1: row[c] += row[c-1]) is correct
 *   serially only because c goes DOWN; in parallel another thread may update
 *   row[c-1] first -> race. Two buffers (read one, write the other) fix it.
 * - Triangle storage: row r starts at r(r+1)/2, total (R+1)(R+2)/2 entries.
 * - Tasks with depend form a DAG (each entry waits for the two above): no
 *   global barriers, but one task per addition is mostly overhead. Use one
 *   task per block of a row to make tasks worth it.
 * - Mod M keeps values < M, so (a + b) never overflows 64 bits; check large
 *   rows with factorials and Fermat inverses: C = R! * (c!(R-c)!)^(M-2) mod M.
 */

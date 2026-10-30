/* P034: Is the array sorted? Count descents; is it a palindrome?
 * Problem: For an integer array a[N], use OpenMP to (1) check whether it is sorted in
 *   ascending (non-decreasing) order, (2) count the "descents" i with a[i] > a[i+1],
 *   and (3) check whether it is a palindrome (a[i] == a[N-1-i] for all i). Test a
 *   sorted array, one that is unsorted only at the end, a palindrome and a random
 *   array; verify against serial code and time the checks.
 * Solution: each test is a reduction over independent pairs. Sortedness:
 *   reduction(&&:sorted) (private copies start at 1 = true, combined with &&);
 *   descents in the same loop with reduction(+:descents). Iteration i only READS
 *   a[i] and a[i+1], so the pair that straddles two threads' blocks is still checked
 *   and there is no race. Palindrome: reduction(&&:palin) over i < N/2 comparing
 *   a[i] with a[N-1-i]. Consistency: sorted must equal (descents == 0).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p034_sorted_check.c -o p034 -lm
 * Run:   ./p034 [N]   (default N = 2e7)
 */
#include "common.h"

typedef struct { int sorted; long descents; int palin; } checks_t;

static checks_t checks_serial(const int *a, long n) {
    int sorted = 1, palin = 1;
    long descents = 0;
    for (long i = 0; i < n - 1; i++) {
        int up = a[i] <= a[i + 1];
        sorted = sorted && up;
        descents += !up;
    }
    for (long i = 0; i < n / 2; i++) palin = palin && (a[i] == a[n - 1 - i]);
    checks_t c = {sorted, descents, palin};
    return c;
}

static checks_t checks_parallel(const int *a, long n) {
    int sorted = 1, palin = 1;
    long descents = 0;
    #pragma omp parallel for reduction(&&:sorted) reduction(+:descents)
    for (long i = 0; i < n - 1; i++) {
        int up = a[i] <= a[i + 1];        /* reads a[i+1]: fine, nobody writes a */
        sorted = sorted && up;
        descents += !up;
    }
    #pragma omp parallel for reduction(&&:palin)
    for (long i = 0; i < n / 2; i++) palin = palin && (a[i] == a[n - 1 - i]);
    checks_t c = {sorted, descents, palin};
    return c;
}

static int same_checks(checks_t x, checks_t y) {
    return x.sorted == y.sorted && x.descents == y.descents && x.palin == y.palin &&
           x.sorted == (x.descents == 0);
}

/* Fill a[] with test pattern 'kind'. */
static void make(int *a, long n, int kind) {
    if (kind == 0) for (long i = 0; i < n; i++) a[i] = (int)(i / 3);                   /* sorted, with repeats */
    if (kind == 1) {                                                                    /* sorted except the end */
        for (long i = 0; i < n; i++) a[i] = (int)(i / 3);
        if (n >= 2) a[n - 1] = -1;
    }
    if (kind == 2) for (long i = 0; i < n; i++) a[i] = (int)((i < n - 1 - i ? i : n - 1 - i) % 1000);  /* palindrome */
    if (kind == 3) fill_int(a, n, 0, 999, 21);                                          /* random */
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    if (n < 0) n = 0;
    int *a = xmalloc((size_t)n * sizeof(int));
    const char *names[4] = {"sorted (i/3)", "unsorted at the end", "palindrome", "random"};
    omp_warmup();

    printf("P034: N = %ld\n", n);
    printf("  %-22s %7s %10s %11s %6s\n", "input", "sorted", "descents", "palindrome", "check");
    for (int kind = 0; kind < 4; kind++) {
        make(a, n, kind);
        checks_t s = checks_serial(a, n), p = checks_parallel(a, n);
        printf("  %-22s %7s %10ld %11s %6s\n", names[kind], s.sorted ? "yes" : "no", s.descents,
               s.palin ? "yes" : "no", ok_str(same_checks(s, p)));
    }

    make(a, n, 0);                          /* sorted input: the checks must scan everything */
    checks_t cs = {0, 0, 0}, cp = {0, 0, 0};
    double ts, tp;
    BEST_TIME(ts, cs = checks_serial(a, n));
    printf("Timing on the sorted array (both loops):\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, cp = checks_parallel(a, n));
        table_row(n, THREADS[k], ts, tp, same_checks(cs, cp));
    }
    free(a);
    return 0;
}

/* Notes:
 * - reduction(&&:x): private copies start at 1 (true) and are combined with &&;
 *   reduction(||:x) starts at 0. Both exist for C since OpenMP 3.1.
 * - Each iteration compares a[i] with a[i+1]; block boundaries are covered because
 *   the thread owning i reads a[i+1] even if another thread "owns" it (reads only).
 * - A reduction cannot stop early: the loop always scans all of a, even when the
 *   first pair is already out of order. For early exit use a flag or
 *   "#pragma omp cancel for" (see P036); break is not allowed in an omp for.
 * - Results are integers/booleans: parallel must match serial exactly; sorted ==
 *   (descents == 0) is a free consistency check.
 * - Loop bound i < n - 1 is safe for n = 0 only because n is a signed long.
 */

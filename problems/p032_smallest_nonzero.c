/* P032: Smallest nonzero element (min reduction and a user-defined reduction)
 * Problem: Given an array of nonnegative ints, return its smallest NONZERO element,
 *   or -1 if the array is empty or all its elements are zero. Parallelise with
 *   OpenMP in two ways: a built-in reduction(min:) and a user-defined reduction.
 *   Test a normal array, an all-zero array, an empty array and an array with a
 *   single nonzero element; verify against serial code and time both versions.
 * Solution: (A) reduction(min:m) with m starting at INT_MAX ("no nonzero yet") and
 *   only nonzero elements taking part; since INT_MAX could itself be the answer, a
 *   second reduction(+:nonzeros) tells "no nonzero at all" apart. (B) declare
 *   reduction(minnz : int : ...) with the operator minnz(x, y) = smaller of x and y
 *   ignoring zeros. Its identity is 0 ("nothing seen yet"), so private copies start at
 *   0 (initializer(omp_priv = 0)) and the result 0 means "no nonzero": no sentinel.
 * Source: textbook Exercise 20.4.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p032_smallest_nonzero.c -o p032 -lm
 * Run:   ./p032 [N]   (default N = 2e7)
 */
#include "common.h"

static int smallest_nonzero_serial(const int *a, long n) {
    int best = -1;
    for (long i = 0; i < n; i++)
        if (a[i] != 0 && (best == -1 || a[i] < best)) best = a[i];
    return best;
}

/* (A) built-in min reduction with an INT_MAX sentinel. */
static int smallest_nonzero_min(const int *a, long n) {
    int m = INT_MAX;
    long nonzeros = 0;
    #pragma omp parallel for reduction(min:m) reduction(+:nonzeros)
    for (long i = 0; i < n; i++) {
        if (a[i] != 0) {
            nonzeros++;
            if (a[i] < m) m = a[i];
        }
    }
    return nonzeros == 0 ? -1 : m;
}

/* (B) user-defined reduction. 0 means "no nonzero seen yet" and is the identity. */
static int min_nonzero2(int x, int y) {
    if (x == 0) return y;
    if (y == 0) return x;
    return x < y ? x : y;
}
#pragma omp declare reduction(minnz : int : omp_out = min_nonzero2(omp_out, omp_in)) initializer(omp_priv = 0)

static int smallest_nonzero_udr(const int *a, long n) {
    int m = 0;
    #pragma omp parallel for reduction(minnz:m)
    for (long i = 0; i < n; i++) m = min_nonzero2(m, a[i]);
    return m == 0 ? -1 : m;
}

/* Run all three versions on one test array and compare with the expected answer. */
static int run_test(const char *name, const int *a, long n, int expected) {
    int s = smallest_nonzero_serial(a, n);
    int r1 = smallest_nonzero_min(a, n);
    int r2 = smallest_nonzero_udr(a, n);
    int ok = s == expected && r1 == expected && r2 == expected;
    printf("  %-28s n = %-9ld serial %11d  min-red %11d  udr %11d  %s\n", name, n, s, r1, r2, ok_str(ok));
    return ok;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    if (n < 1) n = 1;
    int *a = xmalloc((size_t)n * sizeof(int));
    omp_warmup();

    printf("P032: smallest nonzero element (-1 if none)\n");
    /* normal: values 0..1000, about 5% zeros, a 7 planted at 2n/3 */
    fill_int(a, n, 0, 1000, 9);
    for (long i = 0; i < n; i++) if (a[i] < 50) a[i] = 0;
    a[2 * n / 3] = 7;
    run_test("normal (7 planted)", a, n, 7);
    for (long i = 0; i < n; i++) a[i] = 0;
    run_test("all zeros", a, n, -1);
    run_test("empty", a, 0, -1);
    a[n / 2] = 42;
    run_test("single nonzero (42)", a, n, 42);
    a[n / 2] = INT_MAX;
    run_test("single nonzero = INT_MAX", a, n, INT_MAX);

    /* timing on the normal array */
    fill_int(a, n, 0, 1000, 9);
    for (long i = 0; i < n; i++) if (a[i] < 50) a[i] = 0;
    a[2 * n / 3] = 7;
    int r0 = 0, r1 = 0, r2 = 0;
    double ts, t1, t2;
    BEST_TIME(ts, r0 = smallest_nonzero_serial(a, n));
    printf("Timing on the normal array, N = %ld: A = reduction(min:), B = declare reduction(minnz)\n", n);
    printf("%8s %12s %12s %8s %12s %8s %6s\n", "threads", "serial(s)", "A(s)", "speedA", "B(s)", "speedB", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(t1, r1 = smallest_nonzero_min(a, n));
        BEST_TIME(t2, r2 = smallest_nonzero_udr(a, n));
        printf("%8d %12.6f %12.6f %8.2f %12.6f %8.2f %6s\n", THREADS[k], ts, t1,
               t1 > 0 ? ts / t1 : 0.0, t2, t2 > 0 ? ts / t2 : 0.0, ok_str(r1 == r0 && r2 == r0));
    }
    free(a);
    return 0;
}

/* Notes:
 * - Plain reduction(min:) would return 0 (zeros are the minimum), so zeros must be
 *   skipped; a skipped-everything result must still be recognisable (-1).
 * - Sentinel pitfall: with INT_MAX as "none yet", an array whose only nonzero is
 *   INT_MAX looks empty. Counting nonzeros (or a UDR with a true identity) fixes it.
 * - declare reduction(name : type : combiner) initializer(...): omp_out/omp_in are
 *   two partial results, omp_priv is a private copy's start value. The operator must
 *   be associative and commutative, and omp_priv must be its identity (0 here, since
 *   min_nonzero2(0, y) = y).
 * - Results are integers: parallel must equal serial exactly.
 * - Empty array: the loop runs zero times, every version returns -1.
 */

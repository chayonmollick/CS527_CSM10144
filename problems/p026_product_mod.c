/* P026: Product of all elements modulo 1e9+7
 * Problem: Given an array a[N] of integers in 1..1e6, compute the product of all
 *   elements modulo p = 1e9+7 in parallel, and also log(product) = sum log(a[i]) as a
 *   floating-point check (how many digits the true product has). Verify against
 *   serial code.
 * Solution: reduction(*:r) cannot be used: the true product overflows at once and
 *   the built-in reduction cannot apply "% p" when combining. Two fixes:
 *   (A) manual reduction: each thread multiplies mod p into a private value over its
 *       share of the loop ("omp for nowait"), then the partial products are combined
 *       one thread at a time in a critical section;
 *   (B) a user-defined reduction:
 *       declare reduction(mulmod : long : omp_out = omp_out * omp_in % MOD)
 *       initializer(omp_priv = 1).
 *   Multiplication mod p is associative and commutative, so both give the exact
 *   serial answer. The log sum is an ordinary reduction(+:s).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p026_product_mod.c -o p026 -lm
 * Run:   ./p026 [N]   (default N = 1e7)
 */
#include "common.h"

#define MOD 1000000007L     /* prime; (MOD-1)^2 < 2^63 so a*b % MOD fits a 64-bit long */

static long prodmod_serial(const int *a, long n) {
    long r = 1;
    for (long i = 0; i < n; i++) r = r * a[i] % MOD;
    return r;
}

/* (A) private partial products combined in a critical section. */
static long prodmod_critical(const int *a, long n) {
    long r = 1;                                   /* shared result */
    #pragma omp parallel
    {
        long mine = 1;                            /* private partial product */
        #pragma omp for nowait
        for (long i = 0; i < n; i++) mine = mine * a[i] % MOD;
        #pragma omp critical
        r = r * mine % MOD;                       /* one thread at a time */
    }
    return r;
}

/* (B) user-defined reduction: combiner multiplies mod p, private copies start at 1. */
#pragma omp declare reduction(mulmod : long : omp_out = omp_out * omp_in % MOD) initializer(omp_priv = 1)

static long prodmod_udr(const int *a, long n) {
    long r = 1;
    #pragma omp parallel for reduction(mulmod:r)
    for (long i = 0; i < n; i++) r = r * a[i] % MOD;
    return r;
}

/* log10 of the TRUE product (no overflow: we add logarithms). */
static double logprod_serial(const int *a, long n) {
    double s = 0.0;
    for (long i = 0; i < n; i++) s += log10((double)a[i]);
    return s;
}
static double logprod_parallel(const int *a, long n) {
    double s = 0.0;
    #pragma omp parallel for reduction(+:s)
    for (long i = 0; i < n; i++) s += log10((double)a[i]);
    return s;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000L);
    int *a = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, 1, 1000000, 5);
    omp_warmup();

    long r0 = 0, rA = 0, rB = 0;
    double ts, tA, tB;
    BEST_TIME(ts, r0 = prodmod_serial(a, n));
    printf("P026: product mod %ld of N = %ld values in 1..1e6 = %ld\n", MOD, n, r0);
    printf("  A = private products + critical, B = declare reduction(mulmod)\n");
    printf("%8s %12s %12s %8s %12s %8s %6s\n", "threads", "serial(s)", "A(s)", "speedA", "B(s)", "speedB", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tA, rA = prodmod_critical(a, n));
        BEST_TIME(tB, rB = prodmod_udr(a, n));
        printf("%8d %12.6f %12.6f %8.2f %12.6f %8.2f %6s\n", THREADS[k], ts, tA,
               tA > 0 ? ts / tA : 0.0, tB, tB > 0 ? ts / tB : 0.0, ok_str(rA == r0 && rB == r0));
    }

    double l0 = 0, l = 0, tl;
    BEST_TIME(ts, l0 = logprod_serial(a, n));
    printf("log10(product) = %.6f: the true product has %.0f decimal digits\n", l0, n > 0 ? floor(l0) + 1 : 1.0);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tl, l = logprod_parallel(a, n));
        table_row(n, THREADS[k], ts, tl, close1(l0, l, 1e-10));
    }
    free(a);
    return 0;
}

/* Notes:
 * - reduction(*:r) multiplies the private copies at the end with plain *, so the
 *   product overflows (undefined for signed long) and "% MOD" is never applied to
 *   the combination; a custom combine step is needed (critical or declare reduction).
 * - Correct because (x*y) mod p = ((x mod p)*(y mod p)) mod p and the operation is
 *   associative + commutative: any grouping of the threads gives the same answer.
 * - In declare reduction: omp_in = another thread's partial, omp_out = the value
 *   being accumulated, omp_priv = a private copy's initial value (1 = identity).
 * - The critical section runs once per thread, not once per element, so it costs
 *   nothing; a critical inside the loop would serialise the whole loop.
 * - long must be 64-bit (Linux/macOS); on Windows use long long. Values must be
 *   < MOD so that a*b < 2^63.
 */

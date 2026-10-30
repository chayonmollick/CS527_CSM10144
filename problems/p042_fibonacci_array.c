/* P042: Fibonacci array F[0..N] mod 1e9+7 (breaking a recurrence)
 * Problem: Fill F[0..N] with the Fibonacci numbers modulo P = 1e9+7
 *   (F[0] = 0, F[1] = 1, F[i] = F[i-1] + F[i-2]). The recurrence is sequential;
 *   parallelise it with OpenMP in two ways: (a) compute every F(i)
 *   independently, (b) split the index range into blocks. Verify against the
 *   serial loop and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: fast doubling gives (F(n), F(n+1)) directly in O(log n) steps:
 *   F(2m) = F(m)*(2F(m+1) - F(m)),  F(2m+1) = F(m)^2 + F(m+1)^2  (all mod P).
 *   (a) parallel for over i, F[i] = fib_pair(i): no iteration reads another's
 *   result, so there is no dependence at all, but the total work is O(N log N)
 *   instead of O(N). (b) one parallel region, each thread takes a block
 *   [lo, hi), jumps to (F(lo), F(lo+1)) with fast doubling, then runs the cheap
 *   recurrence over its own block: O(N + p log N) work. Every thread writes
 *   only its own F[i]s -> race-free.
 * Source: lab extension (Lab 9 / q27 compute Fibonacci with tasks; this is the array version).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p042_fibonacci_array.c -o p042 -lm
 * Run:   ./p042 [N] [Na]     (defaults N = 1e7 for serial/blocked, Na = 1e6 for (a))
 */
#include "common.h"

#define MOD 1000000007LL

/* (F(n), F(n+1)) mod MOD by fast doubling, reading the bits of n from the top.
 * Values stay < MOD < 2^30, so a*a + b*b < 2^61 fits in long long. */
static void fib_pair(long n, long long *fn, long long *fn1) {
    long long a = 0, b = 1;                             /* (F(m), F(m+1)) with m = 0 */
    int top = 0;
    while (top < 62 && (n >> (top + 1)) != 0) top++;    /* highest set bit of n */
    for (int bit = top; bit >= 0; bit--) {
        long long f2m  = a * ((2 * b - a + MOD) % MOD) % MOD;   /* F(2m)   */
        long long f2m1 = (a * a + b * b) % MOD;                  /* F(2m+1) */
        if ((n >> bit) & 1) { a = f2m1; b = (f2m + f2m1) % MOD; } /* m -> 2m+1 */
        else                { a = f2m;  b = f2m1; }               /* m -> 2m   */
    }
    *fn = a; *fn1 = b;
}

static void fib_serial(long *F, long n) {               /* fills F[0..n] */
    F[0] = 0;
    if (n >= 1) F[1] = 1;
    for (long i = 2; i <= n; i++) {
        long s = F[i-1] + F[i-2];
        F[i] = s >= MOD ? s - MOD : s;
    }
}

/* (a) every element on its own: no dependence, O(log i) work each. */
static void fib_independent(long *F, long n) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= n; i++) {
        long long f, f1;
        fib_pair(i, &f, &f1);
        F[i] = (long)f;
    }
}

/* (b) jump to the start of my block, then iterate the recurrence. */
static void fib_blocked(long *F, long n) {
    long m = n + 1;                                     /* elements F[0..n] */
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = m * tid / nt, hi = m * (tid + 1) / nt;
        if (lo < hi) {
            long long a, b;
            fib_pair(lo, &a, &b);                       /* a = F(lo), b = F(lo+1) */
            for (long i = lo; i < hi; i++) {
                F[i] = (long)a;
                long long s = a + b;
                if (s >= MOD) s -= MOD;
                a = b; b = s;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    long n  = arg_long(argc, argv, 1, 10000000L);
    long na = arg_long(argc, argv, 2, 1000000L);
    if (n < 0) n = 0;
    if (na < 0) na = 0;
    if (na > n) na = n;
    long *ref = xmalloc((size_t)(n + 1) * sizeof(long));
    long *F   = xmalloc((size_t)(n + 1) * sizeof(long));
    omp_warmup();

    fib_serial(ref, n);
    long long f100, f101;
    fib_pair(100, &f100, &f101);
    printf("P042: Fibonacci mod 1e9+7, F(N) = %ld for N = %ld; F(100) = %lld (expect 687995182) %s\n",
           ref[n], n, f100, ok_str(f100 == 687995182LL));

    double ts, tp;
    BEST_TIME(ts, fib_serial(ref, na));
    printf("\n(a) each F(i) independently by fast doubling, parallel for, N = %ld\n", na);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(F, 0, (size_t)(na + 1) * sizeof(long));
        BEST_TIME(tp, fib_independent(F, na));
        table_row(na, THREADS[k], ts, tp, same_longs(F, ref, na + 1));
    }

    BEST_TIME(ts, fib_serial(ref, n));
    printf("\n(b) blocked: fast doubling to each block start, then iterate, N = %ld\n", n);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(F, 0, (size_t)(n + 1) * sizeof(long));
        BEST_TIME(tp, fib_blocked(F, n));
        table_row(n, THREADS[k], ts, tp, same_longs(F, ref, n + 1));
    }
    free(ref); free(F);
    return 0;
}

/* Notes:
 * - F[i] = F[i-1] + F[i-2] is a flow dependence of distance 1 and 2: a plain
 *   parallel for reads elements that are not computed yet (wrong results).
 * - Breaking the dependence = finding a way to get F(i) without F(i-1):
 *   fast doubling (or the matrix power [[1,1],[1,0]]^i) jumps straight to i.
 * - (a) trades work for parallelism: O(N log N) total vs O(N) serial, so even
 *   with 8 threads it is SLOWER than the serial loop. More parallel != faster.
 * - (b) is the practical version: only one O(log N) jump per thread, then the
 *   cheap recurrence -> O(N + p log N) work, near-linear speedup until memory
 *   bandwidth limits it. Same idea as the blocked scans (P037, P041).
 * - The modulus keeps the numbers in range (F(93) already overflows 64 bits);
 *   products of two values < 2^30 are < 2^60, so long long never overflows.
 */

/* P098: Sieve of Eratosthenes (simple parallel and segmented)
 * Problem: Count the primes <= N with the sieve of Eratosthenes (for every
 *   prime p <= sqrt(N), cross out p*p, p*p+p, p*p+2p, ... <= N; what is left is
 *   prime). Parallelise it in two ways: (a) the simple sieve with the marking
 *   loop of each prime done in parallel; (b) a segmented sieve: split [2, N]
 *   into blocks, every thread sieves its own blocks using the base primes
 *   <= sqrt(N). Verify the counts against the serial sieve and the known values
 *   (pi(1e7) = 664579, pi(1e8) = 5761455) and report time and speedup for 1, 2,
 *   4 and 8 threads.
 * Solution: (a) the loop over p stays serial (comp[p] must be final before we
 *   know p is prime - it is, because all smaller primes were done); for one p the
 *   multiples j = p*p, p*p+p, ... are distinct, so "#pragma omp parallel for" over j
 *   writes each comp[j] from exactly one thread: no race; the implicit barrier
 *   ends prime p. The final count is a reduction(+:count). But every prime sweeps
 *   the whole N-byte array, so it is memory-bound.
 *   (b) base primes <= sqrt(N) come from a small serial sieve. Blocks are
 *   independent: a thread sieves block [lo, hi] in its own private buffer (sized to
 *   fit in cache) starting each prime at its first multiple >= lo, counts the
 *   zeros, and adds to reduction(+:count). Nothing is shared but the read-only
 *   base primes. schedule(dynamic) hands out whole blocks.
 * Source: classic sieve exercise (lab extension; segmented sieve).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p098_sieve_primes.c -o p098 -lm
 * Run:   ./p098 [N]          (default N = 1e7; try 1e8 -> 5761455, about 5 s)
 */
#include "common.h"

#define SEG (1L << 17)          /* segment length in bytes (128 KB: stays in cache) */

/* Exact integer square root: largest r with r*r <= n. */
static long isqrt(long n) {
    long r = (long)sqrt((double)n);
    while (r * r > n) r--;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

/* comp[i] = 1 if i is composite; comp has n+1 bytes. */
static long sieve_serial(long n, unsigned char *comp) {
    memset(comp, 0, (size_t)n + 1);
    for (long p = 2; p * p <= n; p++)
        if (!comp[p])
            for (long j = p * p; j <= n; j += p) comp[j] = 1;
    long count = 0;
    for (long i = 2; i <= n; i++) count += !comp[i];
    return count;
}

/* (a) simple sieve, marking loop of each prime in parallel. */
static long sieve_simple_parallel(long n, unsigned char *comp) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= n; i++) comp[i] = 0;
    for (long p = 2; p * p <= n; p++) {
        if (comp[p]) continue;                  /* final: smaller primes are done */
        #pragma omp parallel for schedule(static)
        for (long j = p * p; j <= n; j += p) comp[j] = 1;
    }                                           /* barrier after every prime */
    long count = 0;
    #pragma omp parallel for reduction(+:count) schedule(static)
    for (long i = 2; i <= n; i++) count += !comp[i];
    return count;
}

/* (b) segmented sieve: every thread sieves whole blocks in a private buffer. */
static long sieve_segmented(long n) {
    if (n < 2) return 0;
    long r = isqrt(n);
    unsigned char *small = xmalloc((size_t)r + 1);          /* base primes <= sqrt(n) */
    long *primes = xmalloc((size_t)(r + 1) * sizeof(long)), np = 0;
    memset(small, 0, (size_t)r + 1);
    for (long p = 2; p <= r; p++) {
        if (small[p]) continue;
        primes[np++] = p;
        for (long j = p * p; j <= r; j += p) small[j] = 1;
    }
    long nseg = (n - 2) / SEG + 1, count = 0;               /* blocks cover [2, n] */
    #pragma omp parallel reduction(+:count)
    {
        unsigned char *seg = xmalloc(SEG);                  /* this thread's block */
        #pragma omp for schedule(dynamic)
        for (long s = 0; s < nseg; s++) {
            long lo = 2 + s * SEG, hi = lo + SEG - 1;
            if (hi > n) hi = n;
            memset(seg, 0, (size_t)(hi - lo + 1));
            for (long k = 0; k < np && primes[k] * primes[k] <= hi; k++) {
                long p = primes[k];
                long start = (lo + p - 1) / p * p;          /* first multiple >= lo */
                if (start < p * p) start = p * p;
                for (long j = start; j <= hi; j += p) seg[j - lo] = 1;
            }
            for (long i = lo; i <= hi; i++) count += !seg[i - lo];
        }
        free(seg);
    }
    free(small); free(primes);
    return count;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 10000000);
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    unsigned char *cs = xmalloc((size_t)n + 1), *cp = xmalloc((size_t)n + 1);
    omp_warmup();

    long rs = 0, rp = 0;
    double ts, tp;
    BEST_TIME(ts, rs = sieve_serial(n, cs));
    printf("P098: primes <= %ld: serial count = %ld", n, rs);
    const long known[10] = {0, 4, 25, 168, 1229, 9592, 78498, 664579, 5761455, 50847534};
    long p10 = 1;
    for (int e = 1; e <= 9; e++) {                    /* N a power of 10? */
        p10 *= 10;
        if (n == p10) printf(" (known pi(1e%d) = %ld, %s)", e, known[e], ok_str(rs == known[e]));
    }
    printf("\n");

    printf("(a) simple sieve, parallel for over the multiples of each prime\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = sieve_simple_parallel(n, cp));
        table_row(n, THREADS[k], ts, tp, rp == rs);
    }
    printf("(b) segmented sieve, %ld-byte blocks, parallel for over blocks"
           " (serial = simple)\n", SEG);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = sieve_segmented(n));
        table_row(n, THREADS[k], ts, tp, rp == rs);
    }
    free(cs); free(cp);
    return 0;
}

/* Notes:
 * - Only primes up to sqrt(N) are needed: a composite c <= N has a prime factor
 *   <= sqrt(c). Marking starts at p*p (smaller multiples have a smaller factor).
 * - (a) is race-free because one parallel for handles one prime (distinct j).
 *   Parallelising over primes instead would let two threads write the same
 *   byte (e.g. 12 by p = 2 and p = 3) - the same value, but formally a data race.
 * - (a) is memory-bound: every prime streams through the whole N-byte array, so
 *   adding threads soon stops helping. (b) keeps each block in cache while all
 *   primes cross it out, so it is faster even on 1 thread and scales better.
 * - Blocks cost about the same (later blocks have a few more active base
 *   primes), so static would work too; dynamic is a cheap safety net (blocks are
 *   big, so only nseg scheduling steps).
 * - Memory: (a) needs N bytes; (b) needs only sqrt(N) + one block per thread.
 */

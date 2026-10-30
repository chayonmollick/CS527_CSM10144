/* P046: Parallel LSD radix sort of unsigned 32-bit keys (8-bit digits)
 * Problem: Sort N unsigned 32-bit integers with an LSD radix sort using 8-bit
 *   digits (4 passes), parallelised with OpenMP. Verify that the output is
 *   sorted and identical to qsort's, and compare the time with qsort and with
 *   the serial radix sort for N = 1e5, 1e6 and 1e7 on 1, 2, 4, 8 threads.
 * Solution: each pass is a stable counting sort on one digit. Inside one
 *   parallel region each thread owns a static block [lo, hi) of the input:
 *   (1) count the digits of its block into a local histogram cnt[256];
 *   barrier; (2) single turns all histograms into write offsets, ordered by
 *   digit first and thread second: off[t][d] = #keys with a smaller digit +
 *   #keys with digit d in threads 0..t-1; implicit barrier; (3) each thread
 *   scatters its block in input order to dst[off[t][d]++]. Every (digit,
 *   thread) pair owns a disjoint output range -> no races; thread order +
 *   in-order scan inside a block -> stable, which LSD radix sort needs.
 *   A barrier ends each pass (dst becomes the next src).
 * Source: lab extension (counting sort of Lab 8 / practice q30, made stable and multi-pass).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p046_radix_sort.c -o p046 -lm
 * Run:   ./p046 [N]          (default: N = 1e5, 1e6, 1e7)
 */
#include "common.h"
#include <stdint.h>

#define DIGIT_BITS 8
#define NBUCKET    256                        /* 2^DIGIT_BITS */
#define NPASS      4                          /* 32 bits / 8 bits */

static inline int digit(uint32_t key, int shift) { return (int)((key >> shift) & (NBUCKET - 1)); }

static int cmp_u32(const void *x, const void *y) {
    uint32_t a = *(const uint32_t *)x, b = *(const uint32_t *)y;
    return (a > b) - (a < b);
}
static int is_sorted_u32(const uint32_t *a, long n) {
    for (long i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}

/* Passes ping-pong in -> tmp -> out -> tmp -> out (NPASS is even), so the
 * input is never modified and the result ends in out. */
static void radix_serial(const uint32_t *in, uint32_t *out, uint32_t *tmp, long n) {
    const uint32_t *src = in;
    uint32_t *dst = tmp;
    for (int pass = 0; pass < NPASS; pass++) {
        int shift = pass * DIGIT_BITS;
        long cnt[NBUCKET] = {0};
        for (long i = 0; i < n; i++) cnt[digit(src[i], shift)]++;
        long run = 0;                                     /* exclusive scan -> start of each digit */
        for (int d = 0; d < NBUCKET; d++) { long c = cnt[d]; cnt[d] = run; run += c; }
        for (long i = 0; i < n; i++) dst[cnt[digit(src[i], shift)]++] = src[i];
        src = dst;
        dst = (dst == tmp) ? out : tmp;
    }
}

static void radix_parallel(const uint32_t *in, uint32_t *out, uint32_t *tmp, long n) {
    long *off = xmalloc((size_t)omp_get_max_threads() * NBUCKET * sizeof(long));  /* off[t*256 + d] */
    #pragma omp parallel
    {
        int tid = omp_get_thread_num(), nt = omp_get_num_threads();
        long lo = n * tid / nt, hi = n * (tid + 1) / nt;
        long *my = off + (long)tid * NBUCKET;             /* my row of offsets */
        const uint32_t *src = in;                         /* private, identical in all threads */
        uint32_t *dst = tmp;

        for (int pass = 0; pass < NPASS; pass++) {
            int shift = pass * DIGIT_BITS;
            long cnt[NBUCKET] = {0};                      /* (1) local digit histogram */
            for (long i = lo; i < hi; i++) cnt[digit(src[i], shift)]++;
            for (int d = 0; d < NBUCKET; d++) my[d] = cnt[d];

            #pragma omp barrier                           /* all histograms written */
            #pragma omp single
            {                                             /* (2) scan in (digit, thread) order */
                long run = 0;
                for (int d = 0; d < NBUCKET; d++)
                    for (int t = 0; t < nt; t++) {
                        long c = off[(long)t * NBUCKET + d];
                        off[(long)t * NBUCKET + d] = run;
                        run += c;
                    }
            }                                             /* implicit barrier */

            for (long i = lo; i < hi; i++)                /* (3) stable scatter of my block */
                dst[my[digit(src[i], shift)]++] = src[i];

            #pragma omp barrier                           /* dst complete before it is read */
            src = dst;
            dst = (dst == tmp) ? out : tmp;
        }
    }
    free(off);
}

static void run_size(long n) {
    uint32_t *in  = xmalloc((size_t)n * sizeof(uint32_t)), *ref = xmalloc((size_t)n * sizeof(uint32_t));
    uint32_t *out = xmalloc((size_t)n * sizeof(uint32_t)), *tmp = xmalloc((size_t)n * sizeof(uint32_t));
    unsigned long long s = 46 + (unsigned long long)n;
    for (long i = 0; i < n; i++) {                        /* 32 random bits from two 31-bit draws */
        unsigned long hi = lcg_next(&s), lo = lcg_next(&s);
        in[i] = (uint32_t)((hi << 16) ^ lo);
    }

    memcpy(ref, in, (size_t)n * sizeof(uint32_t));        /* qsort reference, timed once */
    double t0 = omp_get_wtime();
    qsort(ref, (size_t)n, sizeof(uint32_t), cmp_u32);
    double tq = omp_get_wtime() - t0;

    double ts, tp;
    BEST_TIME(ts, radix_serial(in, out, tmp, n));
    int ok_serial = memcmp(out, ref, (size_t)n * sizeof(uint32_t)) == 0;
    printf("\nN = %ld: qsort %.6f s, serial radix %.6f s (%.1fx faster than qsort), serial %s\n",
           n, tq, ts, ts > 0 ? tq / ts : 0.0, ok_str(ok_serial));
    printf("%12s %8s %12s %12s %8s %9s %6s\n",
           "N", "threads", "serial(s)", "parallel(s)", "speedup", "vs qsort", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(out, 0, (size_t)n * sizeof(uint32_t));
        BEST_TIME(tp, radix_parallel(in, out, tmp, n));
        int ok = is_sorted_u32(out, n) && memcmp(out, ref, (size_t)n * sizeof(uint32_t)) == 0;
        double sp = tp > 0 ? ts / tp : 0.0, vq = tp > 0 ? tq / tp : 0.0;
        printf("%12ld %8d %12.6f %12.6f %8.2f %9.2f %6s\n", n, THREADS[k], ts, tp, sp, vq, ok_str(ok));
    }
    free(in); free(ref); free(out); free(tmp);
}

int main(int argc, char *argv[]) {
    omp_warmup();
    printf("P046: LSD radix sort, 32-bit keys, 8-bit digits (%d passes)\n", NPASS);
    printf("speedup = serial radix / parallel radix, vs qsort = qsort / parallel radix\n");
    if (argc > 1) {
        long n = arg_long(argc, argv, 1, 1000000L);
        run_size(n < 1 ? 1 : n);
    } else {
        const long sizes[] = {100000L, 1000000L, 10000000L};
        for (int i = 0; i < 3; i++) run_size(sizes[i]);
    }
    return 0;
}

/* Notes:
 * - O(passes * (N + 256)) = O(N) work vs O(N log N) for comparison sorts, which
 *   is why even the serial radix sort beats qsort by a large factor.
 * - LSD needs a STABLE counting sort per pass: keys equal in the current digit
 *   must keep the order given by the lower digits sorted in earlier passes.
 *   Offsets ordered (digit, then thread) + in-order scatter give stability.
 * - The count phase is race-free because counts are thread-local; the scatter is
 *   race-free because each (digit, thread) pair writes its own range.
 * - Barriers per pass: after counting (before single), after single (implicit),
 *   after scattering (dst becomes the next src). Three per pass.
 * - Scattered writes to 256 places per thread are cache-unfriendly; that, plus
 *   memory bandwidth (4 passes read + write N keys), limits the speedup.
 * - Signed ints: flip the sign bit (key ^ 0x80000000u) before sorting. Doubles:
 *   flip all bits of negatives and the sign bit of positives (64-bit keys).
 */

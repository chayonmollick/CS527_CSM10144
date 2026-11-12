/* 21_false_sharing.c - threads writing NEIGHBOURING memory slow each other down
 *
 * What: caches move memory in lines of 64 bytes (128 on Apple M chips). If thread 0
 *       writes sum[0] and thread 1 writes sum[1] in the SAME line, the line bounces
 *       between their caches on every write, although no variable is really shared:
 *       false sharing. The answer is right, it is only slow. Fixes: pad each thread's
 *       slot to a line of its own, or better, add into a local variable and write the
 *       slot once at the end (which is what reduction does for you).
 * When: per-thread arrays of counters or partial sums updated in a hot loop
 *       (practice/q02, problems/p012). Much more visible on x86 lab PCs than on a Mac.
 * Expected output (4 threads; the times VARY. These are from an Apple M4, where the
 * first two are close; on an x86 PC the adjacent slots are usually several times slower):
 *   adjacent slots (one cache line) : 0.0079 s
 *   padded slots (own cache lines)  : 0.0073 s
 *   local variable, one write       : 0.0015 s
 *   all three totals are 70000000: OK
 */
#include <omp.h>
#include <stdio.h>

#define N 20000000L
#define MAXT 64
#define LINE 128                            /* enough for 64- and 128-byte cache lines */

struct padded { long v; char pad[LINE - sizeof(long)]; };

static volatile long adjacent[MAXT];        /* volatile: really store on every update */
static volatile struct padded padded[MAXT];
static long local_sum[MAXT];

int main(void) {
    int p = omp_get_max_threads() < MAXT ? omp_get_max_threads() : MAXT;
    double t0 = omp_get_wtime();
    #pragma omp parallel num_threads(p)
    {
        int id = omp_get_thread_num();
        #pragma omp for
        for (long i = 0; i < N; i++) adjacent[id] += i & 7;      /* neighbours share a line */
    }
    double t1 = omp_get_wtime();
    #pragma omp parallel num_threads(p)
    {
        int id = omp_get_thread_num();
        #pragma omp for
        for (long i = 0; i < N; i++) padded[id].v += i & 7;      /* one line per thread */
    }
    double t2 = omp_get_wtime();
    #pragma omp parallel num_threads(p)
    {
        long s = 0;                         /* private, in a register */
        #pragma omp for
        for (long i = 0; i < N; i++) s += i & 7;
        local_sum[omp_get_thread_num()] = s;                     /* one write per thread */
    }
    double t3 = omp_get_wtime();

    long a = 0, b = 0, c = 0, expect = N / 8 * 28;   /* 0 + 1 + ... + 7 for every 8 values of i */
    for (int t = 0; t < p; t++) { a += adjacent[t]; b += padded[t].v; c += local_sum[t]; }
    printf("adjacent slots (one cache line) : %.4f s\n", t1 - t0);
    printf("padded slots (own cache lines)  : %.4f s\n", t2 - t1);
    printf("local variable, one write       : %.4f s\n", t3 - t2);
    int ok = a == expect && b == expect && c == expect;
    printf("all three totals are %ld: %s\n", expect, ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

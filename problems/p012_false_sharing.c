/* P012: False sharing: per-thread counters side by side vs padded vs local
 * Problem: Count the multiples of 3 in [0, N) in parallel, each thread counting into
 *   its own counter, cnt[id] += (i % 3 == 0) in every iteration:
 *   (a) a shared array cnt[thread_id] (counters adjacent in memory),
 *   (b) the same array padded so that every counter has its own cache line,
 *   (c) a local variable, written to the array once at the end. Verify that all totals
 *   are equal and time them for 1, 2, 4, 8 threads. Explain the difference.
 * Solution: there is no race in any version: thread t writes only cnt[t]. But in (a)
 *   several counters share one cache line, so every increment invalidates the line in
 *   the other cores' caches (false sharing) and the line ping-pongs between cores.
 *   (b) pads each counter to its own line; (c) keeps the counter in a register.
 *   Counters in (a)/(b) are volatile so that gcc really stores every update to memory
 *   (otherwise it would keep them in a register and hide the effect); the update is
 *   written branch-free so there is a store in every iteration, as in sum[id] += x[i].
 *   Speedups are relative to the same version on 1 thread (the serial loop is only the
 *   correctness reference: some CPUs treat a fixed-address counter specially).
 * Source: false sharing from the lectures / textbook ch. 25 (lab extension).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p012_false_sharing.c -o p012 -lm
 * Run:   ./p012 [N]          (default N = 1e8)
 */
#include "common.h"

#define MAXT 64
#define LINE 128   /* padding unit: a cache line is 64 B on x86 and 128 B on Apple M-series;
                      128 is safe on both (and beats x86's adjacent-line prefetcher) */

typedef struct { volatile long v; char pad[LINE - sizeof(long)]; } Padded;

static volatile long cnt_adj[MAXT];    /* (a) 8-byte counters next to each other */
static Padded cnt_pad[MAXT];           /* (b) one counter per cache line */
static long cnt_once[MAXT];            /* (c) written once per thread */

static volatile long cnt_serial;       /* global like the others: same memory traffic */

static long count_serial(long n) {
    cnt_serial = 0;
    for (long i = 0; i < n; i++) cnt_serial += (i % 3 == 0);
    return cnt_serial;
}

/* (a) adjacent counters: false sharing */
static long count_adjacent(long n) {
    int nt = 1;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        cnt_adj[id] = 0;
        #pragma omp single
        nt = omp_get_num_threads();        /* implied barrier: all counters are 0 now */
        #pragma omp for
        for (long i = 0; i < n; i++) cnt_adj[id] += (i % 3 == 0);
    }
    long total = 0;
    for (int t = 0; t < nt; t++) total += cnt_adj[t];
    return total;
}

/* (b) padded counters: same code, one cache line per counter */
static long count_padded(long n) {
    int nt = 1;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        cnt_pad[id].v = 0;
        #pragma omp single
        nt = omp_get_num_threads();
        #pragma omp for
        for (long i = 0; i < n; i++) cnt_pad[id].v += (i % 3 == 0);
    }
    long total = 0;
    for (int t = 0; t < nt; t++) total += cnt_pad[t].v;
    return total;
}

/* (c) local counter in a register, stored to the shared array once */
static long count_local(long n) {
    int nt = 1;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        long local = 0;
        #pragma omp single nowait
        nt = omp_get_num_threads();
        #pragma omp for
        for (long i = 0; i < n; i++) local += (i % 3 == 0);
        cnt_once[id] = local;              /* one write: false sharing is irrelevant */
    }
    long total = 0;
    for (int t = 0; t < nt; t++) total += cnt_once[t];
    return total;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 100000000L);
    if (n < 0) n = 0;
    omp_set_dynamic(0);
    omp_warmup();

    long expect = (n + 2) / 3;           /* multiples of 3 in [0, n): 0, 3, 6, ... */
    long rs = 0, ra = 0, rb = 0, rc = 0;
    double ts, ta, tb, tc, ta1 = 0, tb1 = 0, tc1 = 0;
    BEST_TIME(ts, rs = count_serial(n));
    printf("P012: count multiples of 3 below N = %ld: serial %ld (formula %ld), serial time %.4f s\n",
           n, rs, expect, ts);
    printf("address gap between neighbouring counters: (a) %d bytes, (b) %d bytes\n",
           (int)sizeof(long), (int)sizeof(Padded));
    printf("time in seconds (speedup = same version on 1 thread / on p threads);\n"
           "last column = false-sharing penalty, time (a) / time (b)\n");
    printf("%8s %18s %18s %18s %8s %6s\n", "threads", "(a) adjacent", "(b) padded", "(c) local", "(a)/(b)", "check");
    int all_ok = (rs == expect);
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(ta, ra = count_adjacent(n));
        BEST_TIME(tb, rb = count_padded(n));
        BEST_TIME(tc, rc = count_local(n));
        if (k == 0) { ta1 = ta; tb1 = tb; tc1 = tc; }      /* THREADS[0] == 1 */
        int ok = (ra == expect && rb == expect && rc == expect);
        all_ok &= ok;
        printf("%8d %10.4f (%5.2f) %10.4f (%5.2f) %10.4f (%5.2f) %8.2f %6s\n", THREADS[k],
               ta, ta1 / ta, tb, tb1 / tb, tc, tc1 / tc, ta / tb, ok_str(ok));
    }
    printf("\n(a) vs (b): same work, only the memory layout differs -> the gap is false sharing\n"
           "(none on 1 thread; grows with threads; largest on x86 with many cores/sockets).\n"
           "(c) is fastest: the counter stays in a register, no store per iteration at all.\n");
    printf("All totals correct: %s\n", ok_str(all_ok));
    return 0;
}

/* Notes:
 * - Caches keep whole lines (64 B on x86, 128 B on Apple M). A write by one core
 *   invalidates the line in all other cores, even if they use DIFFERENT bytes of it:
 *   that is false sharing. Results stay correct; only speed suffers.
 * - True sharing = threads really use the same variable (needs synchronisation);
 *   false sharing = different variables that happen to share a line (needs layout fix).
 * - Fixes: pad/align per-thread data to a line, give each thread a contiguous block
 *   (schedule(static) instead of chunk 1), or accumulate in a private variable and
 *   write once (best; that is what reduction does).
 * - volatile is only for the demo: it forces a load+store per increment the way
 *   real code that updates shared memory would. Without it gcc keeps the counter in
 *   a register and (a) looks as fast as (c).
 * - The size of the penalty depends on the machine: small (1.1-1.5x) when the cores
 *   share an L2 (Apple M clusters), usually several times on x86 desktops/servers and
 *   worst across sockets. It appears only with 2+ threads and frequent writes.
 */

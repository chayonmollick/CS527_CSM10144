/* P017: Producer-consumer with a bounded circular buffer
 * Problem: Producers put the numbers 1..N into a circular buffer of size B; consumers
 *   take them out and add them up. Implement it (a) for P producers and C consumers
 *   with critical sections, (b) for one producer section and one consumer section
 *   that synchronise through an atomic counter and flush. Check that the sum is
 *   N(N+1)/2 and that every item is consumed exactly once. Explain why a busy-wait
 *   loop needs flush / atomic reads.
 * Solution: (a) every buffer operation (test count, move head/tail, change count) is one
 *   critical(buffer) block; a thread that finds the buffer full/empty leaves the critical
 *   and tries again. Entering/leaving a critical implies a flush, so no extra flushes are
 *   needed. Consumers stop when the buffer is empty and all N items have been taken.
 *   (b) only count is shared by both sides: it is read/updated with atomic. The producer
 *   writes the slot, flushes, then increments count; the consumer sees count > 0,
 *   flushes, then reads the slot (and flushes again before decrementing, so the slot is
 *   not overwritten while being read). seen[item]++ is written by exactly one consumer
 *   per item (items are unique), so the check array is race-free.
 * Source: lab extension of textbook ch. 23 / 26 (locks, flush, memory model).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p017_producer_consumer.c -o p017 -lm
 * Run:   ./p017 [N] [B]      (defaults N = 1e5 items, B = 64 slots)
 */
#include "common.h"

#define MAXB 4096
static long buf[MAXB];
static long B = 64;                       /* buffer capacity actually used */

/* ---------- (a) P producers / C consumers, critical sections ---------- */
static long head, tail, count, taken;    /* all protected by critical(buffer) */

static int pc_critical(long n, int P, int C, long *sum_out, unsigned char *seen) {
    long sum = 0;
    int team = 0;
    head = tail = count = taken = 0;
    #pragma omp parallel num_threads(P + C) reduction(+:sum)
    {
        int id = omp_get_thread_num();
        #pragma omp single
        team = omp_get_num_threads();     /* implied barrier: everyone sees team */
        if (team != P + C) {
            /* too few threads: some role would be missing and the others would wait forever */
        } else if (id < P) {                                   /* producer number id */
            for (long item = id + 1; item <= n; item += P) {   /* items id+1, id+1+P, ... */
                int done = 0;
                while (!done) {                                /* retry while full */
                    #pragma omp critical(buffer)
                    {
                        if (count < B) { buf[head] = item; head = (head + 1) % B; count++; done = 1; }
                    }
                }
            }
        } else {                                               /* consumer */
            for (;;) {
                long item = 0;
                int finished = 0;
                #pragma omp critical(buffer)
                {
                    if (count > 0) { item = buf[tail]; tail = (tail + 1) % B; count--; taken++; }
                    else if (taken == n) finished = 1;         /* empty and nothing left to come */
                }
                if (finished) break;
                if (item > 0) { sum += item; seen[item]++; }
            }
        }
    }
    *sum_out = sum;
    return team == P + C;
}

/* ---------- (b) one producer + one consumer, atomic counter + flush ---------- */
static int cnt;                           /* items in the buffer: atomic access only */

static int pc_flush(long n, long *sum_out, unsigned char *seen) {
    long sum = 0;                          /* written only by the consumer section */
    int team = 0;
    cnt = 0;
    #pragma omp parallel num_threads(2)
    {
        #pragma omp single
        team = omp_get_num_threads();
        if (team == 2) {                   /* with 1 thread the producer would spin forever */
            #pragma omp sections
            {
                #pragma omp section
                {                                              /* PRODUCER */
                    long h = 0;
                    for (long item = 1; item <= n; item++) {
                        int c;
                        do {
                            #pragma omp atomic read
                            c = cnt;                           /* re-read memory every time */
                        } while (c == B);                      /* full: wait */
                        #pragma omp flush                      /* slot is free: consumer is done with it */
                        buf[h] = item;
                        h = (h + 1) % B;
                        #pragma omp flush                      /* item in memory BEFORE count says so */
                        #pragma omp atomic update
                        cnt++;
                    }
                }
                #pragma omp section
                {                                              /* CONSUMER */
                    long t = 0;
                    for (long k = 0; k < n; k++) {
                        int c;
                        do {
                            #pragma omp atomic read
                            c = cnt;
                        } while (c == 0);                      /* empty: wait */
                        #pragma omp flush                      /* see the item, not a stale copy */
                        long item = buf[t];
                        t = (t + 1) % B;
                        #pragma omp flush                      /* finish reading before freeing the slot */
                        #pragma omp atomic update
                        cnt--;
                        sum += item;
                        if (item >= 1 && item <= n) seen[item]++;
                    }
                }
            }
        }
    }
    *sum_out = sum;
    return team == 2;
}

/* sum must be n(n+1)/2 and every item 1..n seen exactly once */
static int check(long n, long sum, const unsigned char *seen) {
    if (sum != n * (n + 1) / 2) return 0;
    for (long i = 1; i <= n; i++) if (seen[i] != 1) return 0;
    return 1;
}

int main(int argc, char *argv[]) {
    /* 1e5, not 1e6: on Linux the critical version spends ~40 s on 1e6 items (two lock
     * round-trips per item plus retries whenever the buffer is full or empty). */
    long n = arg_long(argc, argv, 1, 100000L);
    B = arg_long(argc, argv, 2, 64);
    if (n < 0) n = 0;
    if (B < 1 || B > MAXB) { printf("B must be 1..%d\n", MAXB); return 1; }
    unsigned char *seen = xmalloc((size_t)(n + 1));
    omp_set_dynamic(0);
    omp_set_num_threads(8);
    omp_warmup();

    long expect = n * (n + 1) / 2, sum = 0;
    printf("P017: producer-consumer, items 1..%ld, buffer of %ld slots, expected sum %ld\n\n", n, B, expect);
    printf("%-34s %10s %16s %6s\n", "version", "time(s)", "sum", "check");
    int all_ok = 1;
    const int PC[3][2] = {{1, 1}, {2, 2}, {4, 4}};
    for (int k = 0; k < 3; k++) {
        memset(seen, 0, (size_t)(n + 1));
        double t0 = omp_get_wtime();
        int ran = pc_critical(n, PC[k][0], PC[k][1], &sum, seen);
        double t = omp_get_wtime() - t0;
        int ok = ran && check(n, sum, seen);
        all_ok &= ok;
        char label[64];
        snprintf(label, sizeof label, "(a) critical, %d prod / %d cons", PC[k][0], PC[k][1]);
        printf("%-34s %10.4f %16ld %6s\n", label, t, sum, ok_str(ok));
    }
    memset(seen, 0, (size_t)(n + 1));
    double t0 = omp_get_wtime();
    int ran = pc_flush(n, &sum, seen);
    double t = omp_get_wtime() - t0;
    int ok = ran && check(n, sum, seen);
    all_ok &= ok;
    printf("%-34s %10.4f %16ld %6s\n", "(b) atomic count + flush, 1 / 1", t, sum, ok_str(ok));

    printf("\nWhy not  while (cnt == B) ;  with a plain read? With no synchronisation inside the\n"
           "loop the compiler may load cnt ONCE into a register and spin forever, and the CPU may\n"
           "show this thread a stale buffer slot. atomic read forces a fresh read of cnt each\n"
           "time; flush orders 'write item' before 'count++' and 'see count' before 'read item'.\n");
    printf("All versions: sum correct and every item consumed exactly once: %s\n", ok_str(all_ok));
    free(seen);
    return 0;
}

/* Notes:
 * - A circular buffer needs: head (next write), tail (next read), count (or one empty
 *   slot) so that full and empty can be told apart; indices wrap with % B.
 * - Version (a) is the safe default: all shared state changes inside ONE named
 *   critical; it works for any number of producers and consumers.
 * - Never block inside the critical waiting for the buffer to change: the other side
 *   needs the same critical to change it -> deadlock. Leave and retry (or use locks
 *   and test_lock).
 * - Busy waiting burns a core; with more threads than cores it can crawl, since the
 *   thread you wait for may not be running. Keep P + C <= cores.
 * - Termination for many consumers: stop only when the buffer is empty AND all N items
 *   have been taken (a counter updated under the same critical).
 */

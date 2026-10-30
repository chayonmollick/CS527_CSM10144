/* P002: Summing thread numbers: the classic race and three fixes
 * Problem: The code  int tsum = 0;  #pragma omp parallel { tsum += omp_get_thread_num(); }
 *   should leave tsum = 0 + 1 + ... + (p-1) = p(p-1)/2. Show that it is wrong
 *   (a data race), then fix it with atomic, with critical and with a reduction
 *   clause. Verify each fix for p = 2, 4, 8 and 16 threads.
 * Solution: tsum += id is load / add / store; two threads can load the same old
 *   value and one update is lost. The race window is tiny, so the region is run
 *   many times with a barrier just before the update (all threads hit it together)
 *   and tsum is volatile (gcc must really load and store it). A second racy version
 *   widens the window by spinning between the load and the store. Fixes:
 *   atomic (one hardware read-modify-write), critical (one thread at a time),
 *   reduction(+:tsum) (private copies, combined once at the end).
 * Source: textbook Ex 17.3.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p002_tsum_race.c -o p002 -lm
 * Run:   ./p002 [TRIALS]     (default TRIALS = 2000 runs of each region per p)
 */
#include "common.h"

/* Textbook code, made visible: barrier lines all threads up, volatile forces real memory access. */
static int tsum_racy(void) {
    volatile int tsum = 0;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        #pragma omp barrier
        tsum += id;                       /* RACE: unsynchronised read-modify-write */
    }
    return tsum;
}

/* Same race with the load and the store pulled apart, as the hardware does it. */
static int tsum_racy_wide(void) {
    volatile int tsum = 0;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        #pragma omp barrier
        int old = tsum;                   /* load  */
        for (volatile int k = 0; k < 20000; k++) { }   /* other threads load the same old value */
        tsum = old + id;                  /* store: overwrites their updates */
    }
    return tsum;
}

static int tsum_atomic(void) {
    int tsum = 0;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        #pragma omp atomic
        tsum += id;
    }
    return tsum;
}

static int tsum_critical(void) {
    int tsum = 0;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        #pragma omp critical
        tsum += id;
    }
    return tsum;
}

static int tsum_reduction(void) {
    int tsum = 0;
    #pragma omp parallel reduction(+:tsum)
    {
        tsum += omp_get_thread_num();     /* each thread adds into its own private tsum (starts at 0) */
    }
    return tsum;
}

/* Run f trials times; return how many results differ from expect (and one wrong sample). */
static long count_wrong(int (*f)(void), long trials, int expect, int *sample) {
    long wrong = 0;
    *sample = expect;
    for (long t = 0; t < trials; t++) {
        int v = f();
        if (v != expect) { wrong++; *sample = v; }
    }
    return wrong;
}

int main(int argc, char *argv[]) {
    long trials = arg_long(argc, argv, 1, 2000);
    const int P[4] = {2, 4, 8, 16};
    int all_ok = 1;

    omp_set_dynamic(0);
    omp_warmup();
    printf("P002: tsum += omp_get_thread_num(), %ld trials per version and p\n", trials);
    printf("racy columns: number of WRONG trials (and one wrong value); fixes must never be wrong\n\n");
    printf("%4s %8s %18s %18s %8s %8s %8s\n",
           "p", "expect", "racy (wrong/val)", "racy-wide", "atomic", "critical", "reduct.");
    for (int k = 0; k < 4; k++) {
        int p = P[k], expect = p * (p - 1) / 2, s1, s2, s3, s4, s5;
        omp_set_num_threads(p);
        long w1 = count_wrong(tsum_racy, trials, expect, &s1);
        long w2 = count_wrong(tsum_racy_wide, trials / 10 + 1, expect, &s2);
        long w3 = count_wrong(tsum_atomic, trials, expect, &s3);
        long w4 = count_wrong(tsum_critical, trials, expect, &s4);
        long w5 = count_wrong(tsum_reduction, trials, expect, &s5);
        int ok = (w3 == 0 && w4 == 0 && w5 == 0);
        all_ok &= ok;
        char r1[64], r2[64];
        snprintf(r1, sizeof r1, "%ld (%d)", w1, s1);
        snprintf(r2, sizeof r2, "%ld/%ld (%d)", w2, trials / 10 + 1, s2);
        printf("%4d %8d %18s %18s %8s %8s %8s\n", p, expect, r1, r2,
               ok_str(w3 == 0), ok_str(w4 == 0), ok_str(w5 == 0));
    }
    printf("\nA racy count of 0 does NOT prove the code correct: the race is timing dependent.\n");
    printf("Fixes (atomic, critical, reduction) correct for every p and trial: %s\n", ok_str(all_ok));
    return 0;
}

/* Notes:
 * - Race = two threads access the same location, at least one writes, no
 *   synchronisation. tsum += id compiles to load, add, store; an update between
 *   another thread's load and store is lost, so tsum comes out too SMALL.
 * - atomic: one hardware read-modify-write (lock add / ldadd); cheapest fix for a
 *   single scalar update. critical: a lock around any block; more general, slower.
 * - reduction(+:tsum): every thread gets a private tsum = 0, the copies are added
 *   into the original at the end of the region. No synchronisation per update, so
 *   it is the right choice when every iteration updates the sum.
 * - Without volatile, gcc -O2 may keep tsum in a register or merge the update, so a
 *   race demo can print a wrong-but-constant value or hide the race altogether.
 * - With p > cores (p = 16 here) threads are time-sliced; a thread preempted between
 *   its load and store makes the race more likely, not less.
 */

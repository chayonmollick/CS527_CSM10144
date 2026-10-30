/* P015: Deadlock from inconsistent lock order, detected safely, and the fix
 * Problem: Two sections each need locks A and B. Section 1 takes A then B, section 2
 *   takes B then A. Show that this can deadlock, without ever hanging the program, and
 *   fix it. Check that the fixed version completes and the shared data is consistent.
 * Solution: deadlock = circular wait: section 1 holds A and waits for B while section 2
 *   holds B and waits for A. To demonstrate it safely, each section takes its first lock,
 *   waits (with a timeout) until the other also holds its first lock, then tries the
 *   second lock with omp_test_lock() in a loop with a time limit instead of blocking in
 *   omp_set_lock(). Giving up = "would have deadlocked". The fix: every thread acquires
 *   the locks in the SAME global order (A before B); then no cycle is possible. The fixed
 *   version moves money between two balances X (lock A) and Y (lock B) many times.
 * Source: textbook Ex 23.2.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p015_lock_deadlock.c -o p015 -lm
 * Run:   ./p015 [ITER]       (default ITER = 1e6 transfers per section)
 */
#include "common.h"

static omp_lock_t lockA, lockB;
static int holding;                  /* sections that hold their first lock (atomic) */

/* Spin until both sections hold their first lock, or give up after timeout seconds
 * (happens if one thread ran both sections one after the other). */
static int wait_for_other(double timeout) {
    double t0 = omp_get_wtime();
    for (;;) {
        int h;
        #pragma omp atomic read
        h = holding;
        if (h >= 2) return 1;
        if (omp_get_wtime() - t0 > timeout) return 0;
    }
}

/* omp_set_lock() with a time limit: 1 = got the lock, 0 = gave up. */
static int lock_with_timeout(omp_lock_t *l, double timeout, double *waited) {
    double t0 = omp_get_wtime();
    int got;
    while (!(got = omp_test_lock(l)) && omp_get_wtime() - t0 < timeout) { }
    *waited = omp_get_wtime() - t0;
    return got;
}

/* One section of the BAD version: take `first`, then try `second`. */
static void bad_section(omp_lock_t *first, omp_lock_t *second, int *got, double *waited, int *tid) {
    *tid = omp_get_thread_num();
    omp_set_lock(first);
    #pragma omp atomic
    holding++;
    wait_for_other(1.0);                       /* make the bad interleaving certain */
    *got = lock_with_timeout(second, 0.2, waited);
    if (*got) omp_unset_lock(second);
    omp_unset_lock(first);
}

/* FIXED version: both sections lock A then B. X is guarded by A, Y by B. */
static void transfers_fixed(long iter, long *X, long *Y) {
    #pragma omp parallel sections num_threads(2)
    {
        #pragma omp section
        for (long k = 0; k < iter; k++) {      /* move 1 from X to Y */
            omp_set_lock(&lockA);
            omp_set_lock(&lockB);
            *X -= 1; *Y += 1;
            omp_unset_lock(&lockB);
            omp_unset_lock(&lockA);
        }
        #pragma omp section
        for (long k = 0; k < iter; k++) {      /* move 2 from Y to X: SAME order A, B */
            omp_set_lock(&lockA);
            omp_set_lock(&lockB);
            *Y -= 2; *X += 2;
            omp_unset_lock(&lockB);
            omp_unset_lock(&lockA);
        }
    }
}

int main(int argc, char *argv[]) {
    long iter = arg_long(argc, argv, 1, 1000000L);
    if (iter < 0) iter = 0;
    omp_init_lock(&lockA);
    omp_init_lock(&lockB);
    omp_set_dynamic(0);
    omp_set_num_threads(2);
    omp_warmup();                              /* create the second thread now */

    printf("P015: two sections, two locks\n\n");
    printf("BAD: section 1 locks A then B, section 2 locks B then A (second lock tried for 0.2 s)\n");
    int got1 = -1, got2 = -1, tid1 = -1, tid2 = -1;
    double w1 = 0, w2 = 0;
    holding = 0;
    #pragma omp parallel sections num_threads(2)
    {
        #pragma omp section
        bad_section(&lockA, &lockB, &got1, &w1, &tid1);
        #pragma omp section
        bad_section(&lockB, &lockA, &got2, &w2, &tid2);
    }
    printf("  section 1 (thread %d): holds A, wants B: %s after %.3f s\n", tid1,
           got1 ? "got it" : "GAVE UP", w1);
    printf("  section 2 (thread %d): holds B, wants A: %s after %.3f s\n", tid2,
           got2 ? "got it" : "GAVE UP", w2);
    if (!got1 || !got2)
        printf("  -> circular wait detected: with omp_set_lock both threads would wait forever = DEADLOCK\n"
               "     (a section that 'got it' only did so because the other one gave up)\n");
    else
        printf("  -> no deadlock this run (the sections did not overlap, e.g. ran on one thread)\n");

    long x0 = 1000000, y0 = 1000000, X = x0, Y = y0;
    double t0 = omp_get_wtime();
    transfers_fixed(iter, &X, &Y);
    double t = omp_get_wtime() - t0;
    int ok = (X == x0 + iter) && (Y == y0 - iter) && (X + Y == x0 + y0);
    printf("\nFIXED: both sections lock A then B; %ld transfers each, finished in %.3f s\n", iter, t);
    printf("  X = %ld (expect %ld), Y = %ld (expect %ld), X + Y = %ld (expect %ld)\n",
           X, x0 + iter, Y, y0 - iter, X + Y, x0 + y0);
    printf("Fixed version completed and data consistent: %s\n", ok_str(ok));

    omp_destroy_lock(&lockA);
    omp_destroy_lock(&lockB);
    return 0;
}

/* Notes:
 * - Four conditions for deadlock: mutual exclusion, hold and wait, no preemption,
 *   circular wait. A global lock order breaks the circular wait.
 * - Other fixes: one lock for both resources (simpler, less parallel), or try-lock
 *   and back off (release what you hold and retry) - that can livelock without a
 *   random delay.
 * - omp_test_lock() never blocks: it returns nonzero and takes the lock if it was
 *   free, 0 otherwise. omp_set_lock() blocks until the lock is free.
 * - With many locks (e.g. one per bank account, P016) "same order" means: always lock
 *   the lower index first.
 * - critical sections with DIFFERENT names nested in opposite orders deadlock the same
 *   way; nesting a critical inside a critical of the SAME name deadlocks one thread.
 */

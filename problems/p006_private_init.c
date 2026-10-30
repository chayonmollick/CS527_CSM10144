/* P006: Giving every thread's private variable the same starting value
 * Problem: Every thread needs its own private copy of x that starts with the same
 *   value (42). Show three ways: firstprivate, single + copyprivate, and
 *   threadprivate + copyin; also plain assignment inside the region. Print the value
 *   each thread sees, say when each method is preferable, and check that all threads
 *   saw the value.
 * Solution: firstprivate copies the value the variable had before the region;
 *   copyprivate broadcasts the value ONE thread computed inside the region (the single
 *   has an implied barrier, after which every private x holds it); copyin copies the
 *   master's threadprivate (global, per-thread) variable to every thread's copy at the
 *   start of the region. Each thread writes only got[its id], so recording is race-free.
 * Source: textbook Ex 21.3.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p006_private_init.c -o p006 -lm
 * Run:   ./p006 [P]          (default P = 4 threads for the printed demo; 1, 2, 4, 8 are checked)
 */
#include "common.h"

#define MAXT 64
#define VALUE 42

static int got[MAXT];          /* got[t] = value thread t saw */
static int team;

/* A global with one copy PER THREAD; each copy starts at the static initialiser. */
static int tp = -1;
#pragma omp threadprivate(tp)

/* Stands for "read from input / expensive computation": done by one thread only. */
static int read_value(void) { return VALUE; }

static void note(int v) {      /* record what this thread sees */
    int id = omp_get_thread_num();
    if (id < MAXT) got[id] = v;
    if (id == 0) team = omp_get_num_threads();
}

/* Print got[0..team-1] and check it against expect(t) = base + step * t. */
static int show(const char *label, int base, int step, int verbose) {
    int ok = 1;
    for (int t = 0; t < team && t < MAXT; t++) if (got[t] != base + step * t) ok = 0;
    if (verbose) {
        printf("  %-38s", label);
        for (int t = 0; t < team && t < 8; t++) printf(" %3d", got[t]);
        printf("   %s\n", ok_str(ok));
    }
    return ok;
}

/* The four ways of starting every private copy at VALUE. */
static int four_ways(int verbose) {
    int ok = 1;

    int x = VALUE;                                   /* 1. firstprivate */
    #pragma omp parallel firstprivate(x)
    { note(x); x += 1000; }                          /* changes stay private */
    ok &= show("firstprivate(x):", VALUE, 0, verbose);
    ok &= (x == VALUE);                              /* original untouched */

    #pragma omp parallel                             /* 2. single + copyprivate */
    {
        int y;                                       /* private: declared in the region */
        #pragma omp single copyprivate(y)
        y = read_value();                            /* ONE thread computes it ... */
        note(y);                                     /* ... everyone has it after the single */
    }
    ok &= show("single copyprivate(y):", VALUE, 0, verbose);

    tp = VALUE;                                      /* 3. threadprivate + copyin: master's copy */
    #pragma omp parallel copyin(tp)
    { note(tp); }
    ok &= show("threadprivate tp + copyin(tp):", VALUE, 0, verbose);

    #pragma omp parallel                             /* 4. plain assignment in the region */
    {
        int z = read_value();                        /* every thread computes it itself */
        note(z);
    }
    ok &= show("int z = read_value(); inside region:", VALUE, 0, verbose);
    return ok;
}

int main(int argc, char *argv[]) {
    int p = (int)arg_long(argc, argv, 1, 4);
    if (p < 1 || p > MAXT) { printf("P must be 1..%d\n", MAXT); return 1; }
    omp_set_dynamic(0);               /* threadprivate persistence needs a fixed team */
    omp_set_num_threads(p);
    int ok = 1;

    printf("P006: private variables starting at %d, %d threads (values of threads 0..%d)\n\n",
           VALUE, p, p < 8 ? p - 1 : 7);

    printf("threadprivate WITHOUT copyin: each thread has its own copy\n");
    tp = VALUE;                       /* sets only the master's copy */
    #pragma omp parallel
    { note(tp); }
    {   /* expected: thread 0 sees 42, every other thread sees the initialiser -1 */
        int okw = (got[0] == VALUE);
        for (int t = 1; t < team; t++) if (got[t] != -1) okw = 0;
        printf("  %-38s", "parallel (no copyin): tp =");
        for (int t = 0; t < team && t < 8; t++) printf(" %3d", got[t]);
        printf("   %s\n", ok_str(okw));
        ok &= okw;
    }

    printf("\nThe four ways (every thread must see %d):\n", VALUE);
    ok &= four_ways(1);

    printf("\nthreadprivate values PERSIST between regions (same team, dynamic off):\n");
    #pragma omp parallel
    { tp = 100 + omp_get_thread_num(); }             /* region 1 sets per-thread values */
    #pragma omp parallel
    { note(tp); }                                    /* region 2, no copyin: still there */
    ok &= show("region 2 sees tp = 100 + thread:", 100, 1, 1);

    printf("\nThe four ways with 1, 2, 4, 8 threads:");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int okk = four_ways(0);
        printf("  p=%d %s", THREADS[k], ok_str(okk));
        ok &= okk;
    }
    printf("\nOverall check: %s\n", ok_str(ok));
    return 0;
}

/* Notes:
 * - firstprivate: value known BEFORE the region in a local variable. Simplest choice;
 *   each thread may then change its own copy.
 * - single + copyprivate: the value is produced INSIDE the region by one thread (read
 *   input, expensive call, random draw) and broadcast; the single's barrier is needed.
 * - threadprivate + copyin: for GLOBAL/static variables (per-thread scratch buffers,
 *   per-thread RNG state, counters used in called functions) that must persist across
 *   regions. Persistence is guaranteed only with dynamic threads off and the same
 *   number of threads (and no nesting) between the regions.
 * - Plain assignment inside the region: fine when the expression is cheap and gives
 *   the same result in every thread.
 * - private(x) does NOT initialise: each copy is garbage until assigned (and the
 *   original x is not updated afterwards unless lastprivate is used).
 */

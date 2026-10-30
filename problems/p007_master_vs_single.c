/* P007: master has no barrier: "#pragma omp master a = f();" before a worksharing loop
 * Problem: In
 *       #pragma omp parallel
 *       {  #pragma omp master
 *          a = f();
 *          #pragma omp for
 *          for (i = 0; i < N; i++) b[i] = a * i;  }
 *   explain why the loop can use a before it is set. Demonstrate the bug safely
 *   (f is slow, the program still terminates), then give correct versions (single;
 *   master + barrier) and check them.
 * Solution: master = "only thread 0 executes this" with NO barrier at entry or exit, and
 *   omp for has its barrier at the END, not the start. So threads 1..p-1 run straight
 *   into the loop and read a while thread 0 is still inside f(): their part of b uses
 *   the old value (here 0). single has an implied barrier at its end, so every thread
 *   waits until a is set; master + explicit barrier does the same. (a is volatile in
 *   the buggy version only so that the stale reads are really performed and visible.)
 * Source: textbook Ex 21.2.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p007_master_vs_single.c -o p007 -lm
 * Run:   ./p007 [N] [DELAY_MS]   (defaults N = 1e6, DELAY_MS = 20)
 */
#include "common.h"

static double delay_s;

/* The "expensive" function: busy-waits delay_s seconds, then returns 3. */
static double f(void) {
    double t0 = omp_get_wtime();
    while (omp_get_wtime() - t0 < delay_s) { }
    return 3.0;
}

static long count_wrong(const double *b, long n) {
    long w = 0;
    for (long i = 0; i < n; i++) if (b[i] != 3.0 * (double)i) w++;
    return w;
}

/* BUGGY: no barrier between the master block and the loop. */
static void buggy_master(double *b, long n) {
    volatile double a = 0.0;                 /* the value threads see before f() returns */
    #pragma omp parallel
    {
        #pragma omp master
        a = f();
        #pragma omp for
        for (long i = 0; i < n; i++) b[i] = a * (double)i;   /* may read a == 0 */
    }
}

/* CORRECT 1: single (one thread, any thread) + its implied barrier at the end. */
static void with_single(double *b, long n) {
    double a = 0.0;
    #pragma omp parallel
    {
        #pragma omp single
        a = f();
        /* implied barrier here: everybody waits for a */
        #pragma omp for
        for (long i = 0; i < n; i++) b[i] = a * (double)i;
    }
}

/* CORRECT 2: master + explicit barrier. */
static void with_master_barrier(double *b, long n) {
    double a = 0.0;
    #pragma omp parallel
    {
        #pragma omp master
        a = f();
        #pragma omp barrier
        #pragma omp for
        for (long i = 0; i < n; i++) b[i] = a * (double)i;
    }
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000000L);
    delay_s = arg_double(argc, argv, 2, 20.0) / 1000.0;
    if (n < 1) n = 1;
    double *b = xmalloc((size_t)n * sizeof(double));
    omp_set_dynamic(0);
    omp_warmup();

    printf("P007: master without barrier, N = %ld, f() takes %.0f ms and returns 3\n", n, delay_s * 1e3);
    printf("correct result: b[i] = 3*i; the buggy column counts elements computed with a == 0\n\n");
    printf("%8s %22s %16s %10s %16s\n", "threads", "buggy master: wrong", "b[N-1] (buggy)", "single", "master+barrier");
    int all_ok = 1;
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        buggy_master(b, n);
        long wrong = count_wrong(b, n);
        double last = b[n - 1];
        with_single(b, n);
        int ok1 = count_wrong(b, n) == 0;
        with_master_barrier(b, n);
        int ok2 = count_wrong(b, n) == 0;
        all_ok &= ok1 && ok2;
        printf("%8d %14ld (%4.1f%%) %16.0f %10s %16s\n", THREADS[k], wrong,
               100.0 * (double)wrong / (double)n, last, ok_str(ok1), ok_str(ok2));
    }
    printf("\nWith p threads about (p-1)/p of b is wrong: every chunk except thread 0's.\n");
    printf("(1 thread is always right; a 0 count for p > 1 only means the race was not hit.)\n");
    printf("Correct versions (single, master + barrier): %s\n", ok_str(all_ok));
    free(b);
    return 0;
}

/* Notes:
 * - Implied barriers: END of parallel, for, sections, single (unless nowait). NO barrier
 *   at the start of a worksharing loop, and none at all for master (or critical, atomic).
 * - single vs master: single = any one thread + barrier at the end (others wait);
 *   master = thread 0, no barrier, others skip ahead. Use master only for work nobody
 *   else depends on immediately (e.g. printing progress), or add a barrier.
 * - The buggy version is a data race on a (read while being written): the result is
 *   undefined, not just "0". Without volatile the compiler could even load a only once.
 * - single + copyprivate(a) is the way to broadcast a value computed into a PRIVATE a.
 * - OpenMP 5.1 renames master to "masked" (master is deprecated); same no-barrier rule.
 */

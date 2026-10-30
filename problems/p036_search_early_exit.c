/* P036: Parallel search with early exit
 * Problem: Decide whether a value occurs in an integer array a[N] (and where). Compare
 *   four OpenMP versions: (1) a full scan with reduction(min:index) giving the FIRST
 *   occurrence, (2) a shared "found" flag checked at the start of every iteration,
 *   (3) "#pragma omp cancel for" with "#pragma omp cancellation point for", and (4) a
 *   hand-written SPMD loop over chunks that stops early. Time them for a target near
 *   the start, in the middle, near the end and absent. Verify the existence answer,
 *   and that the full scan returns the first occurrence.
 * Solution: break is not allowed inside "omp for", so early exit needs other means.
 *   (1) every thread scans its block, min reduction keeps the smallest index: always
 *   scans all N elements. (2) the flag is written/read with omp atomic; iterations
 *   still run but skip the work once it is set, so it only helps if the work per
 *   iteration is large. (3) cancel for makes the threads leave the loop at their next
 *   cancellation point; it only works if OMP_CANCELLATION=true is set BEFORE the
 *   program starts (omp_get_cancellation() tells); otherwise it runs as a full scan.
 *   (4) thread t scans chunks t, t+T, t+2T, ... (all threads sweep from the front
 *   together) and checks the flag before each chunk, so it stops soon after any
 *   thread finds the value. (2)-(4) return AN occurrence, not necessarily the first.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p036_search_early_exit.c -o p036 -lm
 * Run:   OMP_CANCELLATION=true ./p036 [N]   (default N = 2e7; without the variable
 *        version 3 cannot exit early)
 */
#include "common.h"

#define CHUNK 4096           /* chunk size of the SPMD version and cancellation-check interval */

/* Serial: stop at the first match. */
static long find_serial(const int *a, long n, int target) {
    for (long i = 0; i < n; i++)
        if (a[i] == target) return i;
    return -1;
}

/* (1) Full scan, first occurrence via reduction(min:). n means "not found". */
static long find_fullscan(const int *a, long n, int target) {
    long first = n;
    #pragma omp parallel for reduction(min:first)
    for (long i = 0; i < n; i++)
        if (a[i] == target && i < first) first = i;
    return first < n ? first : -1;
}

/* (2) Shared flag: iterations after a hit still run, but skip the comparison. */
static long find_flag(const int *a, long n, int target) {
    long where = -1;
    int found = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        int stop;
        #pragma omp atomic read
        stop = found;
        if (stop) continue;                  /* cannot break out of an omp for */
        if (a[i] == target) {
            #pragma omp atomic write
            where = i;
            #pragma omp atomic write
            found = 1;
        }
    }
    return where;
}

/* (3) Cancellation: needs OMP_CANCELLATION=true, otherwise cancel is ignored.
 * Written as parallel + for (not the combined "parallel for", which gcc treats as a
 * nowait loop, where cancel for is not allowed). */
static long find_cancel(const int *a, long n, int target) {
    long where = -1;
    #pragma omp parallel
    {
        #pragma omp for
        for (long i = 0; i < n; i++) {
            if (a[i] == target) {
                #pragma omp atomic write
                where = i;
                #pragma omp cancel for               /* this thread leaves; tells the others */
            }
            if (i % CHUNK == 0) {
                #pragma omp cancellation point for   /* others notice the cancel here */
            }
        }
    }
    return where;
}

/* (4) Manual SPMD loop over chunks: an ordinary C loop, so break is allowed. */
static long find_manual(const int *a, long n, int target) {
    long where = -1;
    int found = 0;
    #pragma omp parallel
    {
        long t = omp_get_thread_num(), T = omp_get_num_threads();
        for (long start = t * CHUNK; start < n; start += T * CHUNK) {   /* chunks t, t+T, ... */
            int stop;
            #pragma omp atomic read
            stop = found;
            if (stop) break;
            long end = start + CHUNK < n ? start + CHUNK : n;
            for (long i = start; i < end; i++) {
                if (a[i] == target) {
                    #pragma omp atomic write
                    where = i;
                    #pragma omp atomic write
                    found = 1;
                    break;
                }
            }
        }
    }
    return where;
}

/* An early-exit answer is correct if it agrees on existence and points at the target. */
static int valid(const int *a, long idx, long expected, int target) {
    if (expected < 0) return idx == -1;
    return idx >= 0 && a[idx] == target;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    if (n < 1) n = 1;
    const int target = 2000000;                      /* data is in [0, 1e6]: absent unless planted */
    int *a = xmalloc((size_t)n * sizeof(int));
    fill_int(a, n, 0, 1000000, 23);
    omp_warmup();

    int cancel_on = omp_get_cancellation();
    printf("P036: search for %d in N = %ld ints; cancellation is %s\n", target, n, cancel_on ? "ON" : "OFF");
    if (!cancel_on)
        printf("  (cancel is ignored, version 3 scans everything: run as OMP_CANCELLATION=true ./p036)\n");

    const char *case_name[4] = {"near the start (1%)", "middle (50%)", "near the end (99%)", "absent"};
    long pos[4] = {n / 100, n / 2, n - 1 - n / 100, -1};
    for (int c = 0; c < 4; c++) {
        long p = pos[c];
        if (p >= 0) {
            a[p] = target;                           /* plant the target ...            */
            a[n - 1] = target;                       /* ... and a second, later copy    */
        }
        long rs = -1, r1 = -1, r2 = -1, r3 = -1, r4 = -1;
        double ts, t1, t2, t3, t4;
        BEST_TIME(ts, rs = find_serial(a, n, target));
        printf("Case: %s, first occurrence at %ld\n", case_name[c], rs);
        printf("%8s %11s %11s %11s %11s %11s %6s\n", "threads", "serial(ms)", "fullscan", "flag",
               "cancel", "manual", "check");
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            BEST_TIME(t1, r1 = find_fullscan(a, n, target));
            BEST_TIME(t2, r2 = find_flag(a, n, target));
            BEST_TIME(t3, r3 = find_cancel(a, n, target));
            BEST_TIME(t4, r4 = find_manual(a, n, target));
            int ok = rs == p && r1 == rs && valid(a, r2, rs, target) && valid(a, r3, rs, target) &&
                     valid(a, r4, rs, target);
            printf("%8d %11.3f %11.3f %11.3f %11.3f %11.3f %6s\n", THREADS[k], 1e3 * ts, 1e3 * t1,
                   1e3 * t2, 1e3 * t3, 1e3 * t4, ok_str(ok));
        }
        if (p >= 0) {                                /* restore ordinary data */
            a[p] = 0;
            a[n - 1] = 0;
        }
    }
    free(a);
    return 0;
}

/* Notes:
 * - break/return/goto out of an "omp for" are not allowed: the runtime has already
 *   divided the iterations and every thread must reach the closing barrier.
 * - Full scan + reduction(min:i) is the only version that guarantees the FIRST
 *   occurrence; its time does not depend on where the target is.
 * - Flag version: "if (found) continue" still visits every i; it only skips the work
 *   of each iteration, which here is a single compare, so it saves little. It pays
 *   off when each iteration does a lot of work. The flag must be read/written
 *   atomically (or be volatile) or the compiler may keep it in a register.
 * - cancel: OMP_CANCELLATION=true must be in the environment when the program
 *   starts (setting it in main is too late); check with omp_get_cancellation().
 *   Threads only stop at cancellation points, so check one every CHUNK iterations.
 * - With static blocks a target at 50% is the first element of a thread's block, so
 *   cancel finds it at once; the manual chunked sweep takes time ~ position / T.
 */

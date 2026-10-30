/* CS527 Computer Systems — OpenMP practice
 * Q23: A shared counter is incremented N times inside a parallel loop. Show the
 *      race condition, fix it four ways, verify each and compare their cost.
 *  - no synchronisation : lost updates -> a different wrong total on every run
 *  - critical           : lock around counter++ (all unnamed criticals share one lock)
 *  - atomic             : one hardware atomic add per increment
 *  - omp_lock_t         : explicit lock: omp_init_lock / set / unset / destroy
 *  - reduction(+:)      : private counter per thread, combined once at the end
 * The counters are `volatile` so that gcc -O2 really performs N increments. Without
 * it gcc rewrites the loop as `counter += my_iterations` (one add per thread), which
 * hides the race and makes the serial/reduction timings meaningless.
 * Usage: ./q23_counter_race [N]     (default N = 1e7)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

static long count_serial(long n) {
    volatile long counter = 0;
    for (long i = 0; i < n; i++) counter++;
    return counter;
}

/* BROKEN: counter++ is load, add, store. Two threads can load the same old value,
 * both add 1 and both store it back: one increment is lost. */
static long count_race(long n) {
    volatile long counter = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) counter++;
    return counter;
}

static long count_critical(long n) {
    volatile long counter = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp critical
        counter++;
    }
    return counter;
}

static long count_atomic(long n) {
    volatile long counter = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        #pragma omp atomic
        counter++;
    }
    return counter;
}

static long count_lock(long n) {
    volatile long counter = 0;
    omp_lock_t lock;                     /* shared: declared outside the region */
    omp_init_lock(&lock);                /* must be initialised before use */
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        omp_set_lock(&lock);             /* wait until free, then take it */
        counter++;
        omp_unset_lock(&lock);           /* release */
    }
    omp_destroy_lock(&lock);
    return counter;
}

static long count_reduction(long n) {
    volatile long counter = 0;
    #pragma omp parallel for reduction(+:counter)
    for (long i = 0; i < n; i++) counter++;   /* private copy, starts at 0 */
    return counter;
}

typedef long (*count_fn)(long);

int main(int argc, char **argv) {
    long n = (argc > 1) ? atol(argv[1]) : 10000000L;   /* 1e7 */
    if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }

    printf("Q23: shared counter incremented N = %ld times\n", n);
    count_serial(n);                                   /* warm-up run, not timed */
    double t0 = omp_get_wtime();
    long ref = count_serial(n);
    double ts = omp_get_wtime() - t0;
    printf("serial counter = %ld  %s   (%.4f s)\n", ref, ref == n ? "OK" : "WRONG", ts);

    omp_set_num_threads(4);
    printf("\n[1] NO synchronisation (BROKEN), 4 threads, 5 runs:\n");
    for (int r = 0; r < 5; r++) {
        long c = count_race(n);
        printf("    run %d: counter = %-10ld lost %ld updates (%.1f%%) -> %s\n",
               r + 1, c, n - c, 100.0 * (n - c) / n, c == n ? "OK (lucky)" : "WRONG");
    }
    omp_set_num_threads(1);
    long c1 = count_race(n);
    printf("    same code with 1 thread: counter = %ld -> %s (nobody to race with)\n",
           c1, c1 == n ? "OK" : "WRONG");

    const char *name[4] = {"critical", "atomic", "omp_lock_t", "reduction"};
    count_fn fn[4] = {count_critical, count_atomic, count_lock, count_reduction};

    omp_set_num_threads(4);
    printf("\n[2] Correct versions, 4 threads:\n");
    for (int m = 0; m < 4; m++) {
        long c = fn[m](n);
        printf("    %-10s counter = %ld  %s\n", name[m], c, c == n ? "OK" : "WRONG");
    }

    printf("\n[3] Timing (s) and speedup = T_serial / T_parallel, T_serial = %.4f s\n", ts);
    const int threads[] = {1, 2, 4, 8};
    double tlast[4] = {0};
    int all_ok = 1;
    printf("%8s", "threads");
    for (int m = 0; m < 4; m++) printf(" %19s", name[m]);
    printf("\n");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        printf("%8d", threads[ti]);
        for (int m = 0; m < 4; m++) {
            double p0 = omp_get_wtime();
            long c = fn[m](n);
            double tp = omp_get_wtime() - p0;
            all_ok = all_ok && c == n;
            tlast[m] = tp;
            printf("  %8.4f (%7.2fx)", tp, tp > 0 ? ts / tp : 0.0);
        }
        printf("\n");
    }
    printf("all timed runs gave counter == N: %s\n", all_ok ? "OK" : "WRONG");
    printf("cost per increment with 8 threads:");
    for (int m = 0; m < 4; m++) printf("  %s %.2f ns", name[m], tlast[m] / n * 1e9);
    printf("\n(serial: %.2f ns per increment)\n", ts / n * 1e9);
    printf("note: reduction beats serial even on 1 thread: its private copy is a plain\n"
           "register, while the volatile serial counter is loaded and stored every time.\n");
    return 0;
}

/* Viva notes:
 * - Race: counter++ is three steps (load, add, store); interleaving two threads loses
 *   updates, so the total is too small and changes from run to run.
 * - Cost: reduction << atomic < critical ~= lock. Reduction touches only a private
 *   register in the loop and synchronises once per thread at the end. atomic is one
 *   hardware read-modify-write (lock xadd / ldadd), but the counter's cache line still
 *   bounces between cores. critical and omp_lock_t are a lock acquire + release per
 *   increment (more instructions, and waiting threads may sleep and be woken).
 * - With synchronisation per increment, adding threads makes it SLOWER than serial:
 *   all threads queue for one memory location, so there is no parallel work at all.
 * - critical vs omp_lock_t: same idea; a lock is a variable, so you can have many
 *   (e.g. one per bin/bucket), pass them around, and use omp_test_lock() to try
 *   without blocking. Always init before use and destroy after.
 * - atomic only applies to a single simple update of a scalar; use critical for
 *   multi-statement updates (e.g. updating a value and its index together).
 */

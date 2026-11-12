/* 12_schedule_static_dynamic_guided.c - loop schedules for iterations of uneven cost
 *
 * What: schedule(static)     one contiguous block per thread, fixed in advance (no cost).
 *       schedule(static, c)  chunks of c iterations dealt out in turn: t0, t1, t2, t3, t0...
 *       schedule(dynamic, c) a thread that is free takes the next c iterations: balances
 *                            uneven work, costs a little per chunk.
 *       schedule(guided)     like dynamic, but the chunks start big and shrink.
 *       schedule(runtime)    whatever OMP_SCHEDULE or omp_set_schedule() says (used
 *                            below to try every schedule with one loop).
 * When: static when every iteration costs the same (vector add); dynamic or guided when
 *       they differ. Testing i for primality costs more for big i, so with static the
 *       thread with the last block works longest while the others wait for it.
 *       static,1 is even worse here: with 4 threads, threads 0 and 2 get only even i
 *       (rejected after one division) and threads 1 and 3 do almost all the work. A
 *       cyclic schedule balances only if the cost does not follow i mod p.
 * Expected output (4 threads; the times and the dynamic map VARY between runs;
 * a tiny loop like this one may even go entirely to one thread):
 *   static,2  : 0 0 1 1 2 2 3 3 0 0 1 1 2 2 3 3
 *   dynamic,2 : 2 2 0 0 3 3 1 1 2 2 0 0 3 3 1 1   (varies)
 *   primes below 400000 (serial): 33860
 *   schedule(static     ) 0.0035 s  count 33860
 *   schedule(static,1   ) 0.0060 s  count 33860
 *   schedule(dynamic,100) 0.0027 s  count 33860
 *   schedule(guided     ) 0.0027 s  count 33860
 *   all four schedules found the serial count: OK
 */
#include <omp.h>
#include <stdio.h>

#define N 400000

static int is_prime(int n) {                /* trial division: the cost grows with n */
    if (n < 2) return 0;
    for (int d = 2; d * d <= n; d++)
        if (n % d == 0) return 0;
    return 1;
}

int main(void) {
    int map[16];
    #pragma omp parallel for schedule(static, 2)
    for (int i = 0; i < 16; i++) map[i] = omp_get_thread_num();
    printf("static,2  :");
    for (int i = 0; i < 16; i++) printf(" %d", map[i]);
    #pragma omp parallel for schedule(dynamic, 2)
    for (int i = 0; i < 16; i++) map[i] = omp_get_thread_num();
    printf("\ndynamic,2 :");
    for (int i = 0; i < 16; i++) printf(" %d", map[i]);
    printf("   (varies)\n");

    int serial = 0;
    for (int i = 0; i < N; i++) serial += is_prime(i);
    printf("primes below %d (serial): %d\n", N, serial);

    const char *name[4] = {"static", "static,1", "dynamic,100", "guided"};
    omp_sched_t kind[4] = {omp_sched_static, omp_sched_static, omp_sched_dynamic, omp_sched_guided};
    int chunk[4] = {0, 1, 100, 0}, ok = 1;  /* chunk 0 = that schedule's default */
    for (int k = 0; k < 4; k++) {
        omp_set_schedule(kind[k], chunk[k]);
        int count = 0;
        double t = omp_get_wtime();
        #pragma omp parallel for schedule(runtime) reduction(+:count)
        for (int i = 0; i < N; i++) count += is_prime(i);
        t = omp_get_wtime() - t;
        printf("schedule(%-11s) %.4f s  count %d\n", name[k], t, count);
        ok = ok && count == serial;
    }
    printf("all four schedules found the serial count: %s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

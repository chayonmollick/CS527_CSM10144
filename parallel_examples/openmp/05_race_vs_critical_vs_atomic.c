/* 05_race_vs_critical_vs_atomic.c - a shared counter: data race, critical, atomic
 *
 * What: count++ is three steps: load, add, store. Two threads can load the same old
 *       value, and then one increment is lost: a DATA RACE (wrong, different every run).
 *       critical:  one thread at a time runs the block (any code; the slowest fix).
 *       atomic:    one memory update done indivisibly (x++, x += e, x = x op e).
 *       reduction: private counters, added once at the end (the fastest; see 03).
 * When: critical for an update of several statements (a max AND its index, a list);
 *       atomic for a single "x op= expr"; reduction whenever the loop computes one
 *       sum / count / max. For rare updates all three are cheap.
 * Expected output (4 threads; the race value and all the times VARY between runs):
 *   no synchronisation: 255093    (lost updates: usually well below 1000000)
 *   critical          : 1000000   0.0673 s
 *   atomic            : 1000000   0.0068 s
 *   reduction         : 1000000   0.0000 s
 *   critical, atomic and reduction are correct: OK
 */
#include <omp.h>
#include <stdio.h>

#define N 1000000L

int main(void) {
    volatile long racy = 0;                 /* volatile: really load and store every time */
    #pragma omp parallel for
    for (long i = 0; i < N; i++)
        racy++;                             /* DATA RACE: updates get lost */

    long c1 = 0, c2 = 0, c3 = 0;
    double t1 = omp_get_wtime();
    #pragma omp parallel for
    for (long i = 0; i < N; i++) {
        #pragma omp critical
        c1++;
    }
    double t2 = omp_get_wtime();
    #pragma omp parallel for
    for (long i = 0; i < N; i++) {
        #pragma omp atomic
        c2++;
    }
    double t3 = omp_get_wtime();
    #pragma omp parallel for reduction(+:c3)
    for (long i = 0; i < N; i++)
        c3++;
    double t4 = omp_get_wtime();

    printf("no synchronisation: %-9ld (lost updates: usually well below %ld)\n", (long)racy, N);
    printf("critical          : %-9ld %.4f s\n", c1, t2 - t1);
    printf("atomic            : %-9ld %.4f s\n", c2, t3 - t2);
    printf("reduction         : %-9ld %.4f s\n", c3, t4 - t3);
    int ok = c1 == N && c2 == N && c3 == N;
    printf("critical, atomic and reduction are correct: %s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

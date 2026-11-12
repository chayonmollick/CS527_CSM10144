/* 09_single_master_barrier.c - single, master and barrier inside one parallel region
 *
 * What: single  - ONE thread (whichever gets there first) runs the block; the others
 *                 wait at its end (implicit barrier) unless nowait is given.
 *       master  - only thread 0 runs the block, and there is NO barrier: the others
 *                 go straight on.
 *       barrier - every thread waits there until the whole team has arrived.
 * When: single for set-up inside a region (read n, allocate, initialise shared data);
 *       master for printing / timing; barrier between two phases when phase 2 reads
 *       what OTHER threads wrote in phase 1 (without it, phase 2 reads stale values).
 * Expected output (4 threads; only the number of the single thread varies):
 *   single ran on thread 2 (any thread), master on thread 0 (always 0)
 *   phase 1: each of the 4 threads added its share of 1..1000
 *   phase 2: after the barrier every thread read all 4 partial sums and got 500500
 *   OK
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int p = omp_get_max_threads(), n = 0, team = 0, single_id = -1, master_id = -1;
    long *part = calloc(p, sizeof *part), *seen = calloc(p, sizeof *seen);
    if (!part || !seen) return 1;

    #pragma omp parallel num_threads(p)
    {
        int t = omp_get_thread_num();
        #pragma omp single
        {                                   /* set-up, done once ... */
            n = 1000;
            team = omp_get_num_threads();
            single_id = t;
        }                                   /* ... and everyone waits here: n is ready */
        for (int i = t + 1; i <= n; i += team)
            part[t] += i;                   /* phase 1: thread t adds t+1, t+1+team, ... */
        #pragma omp barrier                 /* phase 2 reads OTHER threads' part[] */
        for (int k = 0; k < team; k++)
            seen[t] += part[k];
        #pragma omp master
        master_id = t;                      /* thread 0 only; nobody waits for it */
    }

    int ok = single_id >= 0 && master_id == 0;
    for (int t = 0; t < team; t++) ok = ok && seen[t] == 500500;
    printf("single ran on thread %d (any thread), master on thread %d (always 0)\n", single_id, master_id);
    printf("phase 1: each of the %d threads added its share of 1..1000\n", team);
    printf("phase 2: after the barrier every thread read all %d partial sums and got %ld\n", team, seen[0]);
    printf("%s\n", ok ? "OK" : "WRONG");
    free(part); free(seen);
    return ok ? 0 : 1;
}

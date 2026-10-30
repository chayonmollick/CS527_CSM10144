/* CS527 Computer Systems — OpenMP practice
 * Q20: Write an OpenMP "hello world" in which every thread prints its id and the
 *      team size. Show the runtime-library query functions and explain how the
 *      number of threads in a region is decided.
 *  - omp_get_thread_num / omp_get_num_threads inside vs outside a parallel region
 *  - omp_get_max_threads, omp_get_num_procs, omp_in_parallel
 *  - omp_get_wtime / omp_get_wtick for wall-clock timing
 *  - num_threads(n) clause vs omp_set_num_threads() vs OMP_NUM_THREADS (precedence)
 *  - if(cond) clause: a false condition runs the region with a team of 1
 * Usage: ./q20_hello_threads [threads]     (default 4)
 * Try:   OMP_NUM_THREADS=3 ./q20_hello_threads   and compare section 1.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXT 256

int main(int argc, char **argv) {
    int nt = (argc > 1) ? atoi(argv[1]) : 4;
    if (nt < 1 || nt > MAXT) nt = 4;

    printf("== 1. Sequential part (before any parallel region) ==\n");
    const char *env = getenv("OMP_NUM_THREADS");
    printf("OMP_NUM_THREADS env var : %s\n", env ? env : "(not set)");
    printf("omp_get_num_procs()     = %d  (processors available to the program)\n",
           omp_get_num_procs());
    printf("omp_get_max_threads()   = %d  (team size the NEXT region would get:\n"
           "                               OMP_NUM_THREADS if set, else usually #procs)\n",
           omp_get_max_threads());
    printf("omp_get_num_threads()   = %d  (outside a region the team is just this thread)\n",
           omp_get_num_threads());
    printf("omp_get_thread_num()    = %d\n", omp_get_thread_num());
    printf("omp_in_parallel()       = %d\n", omp_in_parallel());

    printf("\n== 2. omp_set_num_threads(%d), then #pragma omp parallel ==\n", nt);
    omp_set_num_threads(nt);
    printf("omp_get_max_threads() now = %d\n", omp_get_max_threads());
    printf("Hello lines below come out in a NONDETERMINISTIC order (threads race to printf):\n");
    int seen[MAXT] = {0};
    int team = 0;
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();      /* declared inside -> private */
        int n   = omp_get_num_threads();
        printf("  Hello from thread %d of %d  (omp_in_parallel() = %d)\n",
               tid, n, omp_in_parallel());
        seen[tid] = 1;                       /* each thread writes its own slot: no race */
        if (tid == 0) team = n;
    }
    int all = (team == nt);
    for (int t = 0; t < team; t++) all = all && seen[t];
    printf("every id 0..%d said hello exactly once, team size %d: %s\n",
           team - 1, team, all ? "OK" : "WRONG");

    printf("\n== 3. num_threads(n) clause: overrides omp_set_num_threads() for ONE region ==\n");
    #pragma omp parallel num_threads(2)
    {
        if (omp_get_thread_num() == 0)
            printf("region with num_threads(2)  : team size = %d\n", omp_get_num_threads());
    }
    #pragma omp parallel
    {
        if (omp_get_thread_num() == 0)
            printf("next region without clause  : team size = %d  (back to omp_set_num_threads(%d))\n",
                   omp_get_num_threads(), nt);
    }
    printf("Precedence (highest first): if(false) -> 1 thread > num_threads(n) clause\n"
           "  > omp_set_num_threads(n) call > OMP_NUM_THREADS env var > default (#procs)\n");

    printf("\n== 4. if(cond) clause: parallelise only when it is worth it ==\n");
    long sizes[2] = {100, 1000000};
    for (int k = 0; k < 2; k++) {
        long n = sizes[k];
        #pragma omp parallel if(n > 10000)
        {
            if (omp_get_thread_num() == 0)
                printf("if(n > 10000) with n = %-7ld: team size = %d, omp_in_parallel() = %d -> %s\n",
                       n, omp_get_num_threads(), omp_in_parallel(),
                       omp_get_num_threads() == 1 ? "region ran SERIALLY" : "region ran in parallel");
        }
    }
    #pragma omp parallel if(0) num_threads(8)
    {
        if (omp_get_thread_num() == 0)
            printf("if(0) num_threads(8)        : team size = %d  (if beats num_threads)\n",
                   omp_get_num_threads());
    }

    printf("\n== 5. Timing with omp_get_wtime() ==\n");
    printf("omp_get_wtick() = %.3e s  (timer resolution)\n", omp_get_wtick());
    const int reps = 1000;
    long hits = 0;
    double t0 = omp_get_wtime();             /* wall-clock seconds from some fixed point */
    for (int r = 0; r < reps; r++) {
        #pragma omp parallel
        {
            #pragma omp atomic
            hits++;                          /* tiny body (gcc deletes an empty region) */
        }
    }
    double t1 = omp_get_wtime();
    printf("%d near-empty parallel regions took %.6f s -> about %.2f us of fork/join\n"
           "overhead each (%ld thread-entries counted)\n",
           reps, t1 - t0, (t1 - t0) / reps * 1e6, hits);
    printf("(always time the DIFFERENCE of two omp_get_wtime() calls; do not use clock(),\n"
           " which adds up CPU time of all threads and makes parallel code look slower)\n");
    return 0;
}

/* Viva notes:
 * - Outside a region: omp_get_num_threads() = 1, omp_get_thread_num() = 0,
 *   omp_in_parallel() = 0. omp_get_max_threads() = the size the next region gets.
 * - Team size precedence: if(false) -> 1 > num_threads(n) clause > omp_set_num_threads()
 *   > OMP_NUM_THREADS > implementation default (number of cores). The clause lasts for
 *   one region only; omp_set_num_threads() lasts until changed.
 * - if(cond) makes the region "inactive" (team of 1) — used to avoid fork/join overhead
 *   for small inputs; omp_in_parallel() returns 0 inside an inactive region.
 * - Hello lines appear in a different order on each run: the OS schedules threads
 *   independently, so output order between threads is never guaranteed.
 * - Variables declared inside the region (tid) are private; those declared before it
 *   are shared by default. omp_get_wtime() returns wall-clock time in seconds.
 */

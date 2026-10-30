/* P001: Hello threads and the OpenMP runtime functions
 * Problem: Write a program with a parallel region in which every thread prints
 *   "hello" with its thread number. Print omp_get_num_threads(), omp_get_thread_num(),
 *   omp_get_max_threads(), omp_get_num_procs() and omp_in_parallel() before, inside
 *   and after the region. Run the region with 1, 2, 4, 8 and 2 x (number of cores)
 *   threads. Can you ask for more threads than there are cores?
 * Solution: the runtime functions answer "who am I / how big is my team" only
 *   inside a region; outside it the program is a team of one. omp_set_num_threads(p)
 *   sets the team size of the next regions (dynamic adjustment is switched off so
 *   the request is obeyed exactly). Each thread counts its hello with an atomic
 *   increment and marks seen[tid], so the check is race-free.
 * Source: textbook Ex 17.1, 17.2, 18.1, 18.3, 18.4, 19.2.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p001_hello_threads.c -o p001 -lm
 * Run:   ./p001 [P]          (default: teams of 1, 2, 4, 8, 2*cores; P adds one more team size)
 */
#include "common.h"

#define MAXT 1024                    /* largest team we record */

/* Print the five runtime queries with a label (works inside and outside regions). */
static void show_runtime(const char *where) {
    printf("  %-28s num_threads=%-3d thread_num=%-3d max_threads=%-3d num_procs=%-3d in_parallel=%d\n",
           where, omp_get_num_threads(), omp_get_thread_num(),
           omp_get_max_threads(), omp_get_num_procs(), omp_in_parallel());
}

/* One parallel region with p threads. Returns 1 if every thread said hello exactly once. */
static int hello_team(int p) {
    int seen[MAXT] = {0};
    int hellos = 0, team = -1;
    int print_each = (p <= 8);        /* keep the output short for big teams */

    omp_set_num_threads(p);
    #pragma omp parallel
    {
        int id = omp_get_thread_num();  /* private: declared inside the region */
        int nt = omp_get_num_threads();
        if (print_each) printf("    hello from thread %d of %d\n", id, nt);
        #pragma omp atomic
        hellos++;                       /* shared counter: must be atomic */
        if (id < MAXT) seen[id]++;      /* each thread writes only its own slot */
        #pragma omp master
        {
            team = nt;
            show_runtime("inside (master thread):");
        }
    }
    /* implicit barrier at the end of the region: all hellos are done here */

    int ok = (team == p) && (hellos == team);
    for (int t = 0; t < team && t < MAXT; t++) if (seen[t] != 1) ok = 0;
    printf("  asked for %d threads: team size %d, hellos counted %d  -> %s\n",
           p, team, hellos, ok_str(ok));
    return ok;
}

int main(int argc, char *argv[]) {
    int procs = omp_get_num_procs();
    int extra = (int)arg_long(argc, argv, 1, 0);
    int sizes[6] = {1, 2, 4, 8, 2 * procs, extra};
    int nsizes = extra > 0 ? 6 : 5;
    int all_ok = 1;

    omp_set_dynamic(0);              /* the runtime may not shrink the team we ask for */
    printf("P001: hello threads (%d processors reported by omp_get_num_procs)\n", procs);
    printf("Before any parallel region (a team of one: thread 0 of 1, not in parallel):\n");
    show_runtime("before:");

    for (int k = 0; k < nsizes; k++) {
        if (sizes[k] > MAXT) { printf("  skip %d threads (> MAXT)\n", sizes[k]); continue; }
        printf("\nParallel region with omp_set_num_threads(%d):\n", sizes[k]);
        all_ok &= hello_team(sizes[k]);
    }

    /* The num_threads clause overrides omp_set_num_threads for one region only. */
    int team3 = 0;
    #pragma omp parallel num_threads(3)
    {
        #pragma omp single
        team3 = omp_get_num_threads();
    }
    printf("\nnum_threads(3) clause on one region -> team of %d (%s); "
           "max_threads afterwards still %d\n", team3, ok_str(team3 == 3), omp_get_max_threads());
    all_ok &= (team3 == 3);

    printf("\nAfter the regions (back to a team of one):\n");
    show_runtime("after:");
    printf("\n%d threads (2 x cores) ran even though there are only %d cores: "
           "threads are time-shared (oversubscribed), not an error.\n", 2 * procs, procs);
    printf("Overall check: %s\n", ok_str(all_ok));
    return 0;
}

/* Notes:
 * - Outside a parallel region omp_get_num_threads() is 1 and omp_get_thread_num() is 0;
 *   omp_get_max_threads() is the team size the NEXT region would get.
 * - Precedence of team size: num_threads clause > omp_set_num_threads() > OMP_NUM_THREADS
 *   env variable > implementation default (usually the number of cores).
 * - More threads than cores is allowed: the OS time-slices them; it only slows a
 *   compute-bound program (context switches, barriers wait for descheduled threads).
 * - The hello lines come out in a different order every run: thread scheduling is
 *   not deterministic, and printf from several threads may even interleave.
 * - Variables declared inside the region (id, nt) are private; hellos is shared, so
 *   hellos++ needs atomic (it is a read-modify-write).
 * - omp_in_parallel() is 0 inside a 1-thread region: it reports whether an ACTIVE
 *   region (team size > 1) encloses the call, not just any parallel construct.
 */

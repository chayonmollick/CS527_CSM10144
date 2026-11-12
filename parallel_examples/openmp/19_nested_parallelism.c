/* 19_nested_parallelism.c - a parallel region inside a parallel region
 *
 * What: a thread that meets another "omp parallel" becomes the master of a new inner
 *       team - but only if nesting is enabled; by default the inner region gets a team
 *       of 1. Enable 2 levels with omp_set_max_active_levels(2); the OpenMP 4.5 runtime
 *       of gcc 9 and 10 (maybe the lab PC) also needs omp_set_nested(1), which newer
 *       runtimes call deprecated - hence the #if below.
 *       omp_get_level() = number of enclosing parallel regions;
 *       omp_get_active_level() = how many of them have more than one thread;
 *       omp_get_ancestor_thread_num(1) = my thread number in the outer team.
 * When: rarely: e.g. 2 independent jobs that are each a parallel loop. Threads in use
 *       = outer x inner, which easily exceeds the cores. Tasks are usually better.
 * Expected output (the ORDER of the lines varies; the counts do not):
 *   nesting on:
 *   outer thread 0, inner thread 0 of 2 (level 2, active levels 2)    <- 4 such lines
 *   ...
 *   nesting off:
 *   outer thread 1, inner thread 0 of 1 (level 2, active levels 1)    <- 2 such lines
 *   ...
 *   inner threads: 4 with nesting (2 x 2), 2 without (2 x 1): OK
 */
#include <omp.h>
#include <stdio.h>

static int run(void) {                      /* 2 outer threads, each opens a team of 2 */
    int count = 0;
    #pragma omp parallel num_threads(2)
    {
        #pragma omp parallel num_threads(2)
        {
            printf("outer thread %d, inner thread %d of %d (level %d, active levels %d)\n",
                   omp_get_ancestor_thread_num(1), omp_get_thread_num(),
                   omp_get_num_threads(), omp_get_level(), omp_get_active_level());
            #pragma omp atomic
            count++;
        }
    }
    return count;
}

int main(void) {
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 11
    omp_set_nested(1);                      /* gcc 9/10 (OpenMP 4.5 runtime) also needs this */
#endif
    omp_set_max_active_levels(2);           /* allow 2 levels of active parallel regions */
    printf("nesting on:\n");
    int on = run();

    omp_set_max_active_levels(1);           /* inner regions now get a team of 1 */
    printf("nesting off:\n");
    int off = run();

    int ok = on == 4 && off == 2;
    printf("inner threads: %d with nesting (2 x 2), %d without (2 x 1): %s\n", on, off, ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

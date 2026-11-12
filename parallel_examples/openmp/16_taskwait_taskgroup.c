/* 16_taskwait_taskgroup.c - waiting for tasks: taskwait vs taskgroup
 *
 * What: taskwait  - waits for the CHILD tasks of the current task only, not for the
 *                   tasks those children create (grandchildren may still be running).
 *       taskgroup - "#pragma omp taskgroup { ... }" waits at its end for every task
 *                   created inside it AND all their descendants (OpenMP 4.0).
 *       A barrier, or the end of the parallel region, also waits for all tasks.
 * When: taskwait in recursive code where each level waits for its own children (15);
 *       taskgroup when tasks create further tasks that nobody waits for one by one
 *       (tree walks, graph search) and you need ALL of them finished before going on.
 * Expected output (4 threads; the taskwait count VARIES between runs):
 *   after taskwait : 1 of 2047 tasks finished (only the direct child is certain)
 *   after taskgroup: 2047 of 2047 tasks finished
 *   taskgroup waited for every descendant: OK
 */
#include <omp.h>
#include <stdio.h>

#define DEPTH 10                            /* a binary tree of 2^(DEPTH+1) - 1 = 2047 tasks */

static long finished = 0;

static void spawn(int depth) {              /* creates two child tasks and does NOT wait */
    if (depth > 0) {
        #pragma omp task
        spawn(depth - 1);
        #pragma omp task
        spawn(depth - 1);
    }
    double s = 0.0;                         /* a little work of its own */
    for (int k = 1; k <= 2000; k++) s += 1.0 / k;
    #pragma omp atomic
    finished += (s > 0.0);                  /* counts 1 */
}

int main(void) {
    long total = (1L << (DEPTH + 1)) - 1, after_wait = 0, after_group = 0;
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp task
        spawn(DEPTH);
        #pragma omp taskwait                /* waits for that one child task only */
        #pragma omp atomic read
        after_wait = finished;
    }                                       /* end of region: all tasks are done here */

    finished = 0;
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp taskgroup
        {
            #pragma omp task
            spawn(DEPTH);
        }                                   /* waits for the task AND all its descendants */
        #pragma omp atomic read
        after_group = finished;
    }
    printf("after taskwait : %ld of %ld tasks finished (only the direct child is certain)\n", after_wait, total);
    printf("after taskgroup: %ld of %ld tasks finished\n", after_group, total);
    printf("taskgroup waited for every descendant: %s\n", after_group == total ? "OK" : "WRONG");
    return after_group == total ? 0 : 1;
}

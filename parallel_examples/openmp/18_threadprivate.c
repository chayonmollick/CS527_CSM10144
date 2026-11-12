/* 18_threadprivate.c - a global variable with one copy per thread
 *
 * What: "#pragma omp threadprivate(counter)" after a global (or static) variable gives
 *       every thread its own copy for the whole program. A thread's copy keeps its
 *       value from one parallel region to the next (same team size, dynamic off).
 *       copyin(counter) on a parallel region copies the master's value into every copy.
 * When: per-thread state kept in globals: a random seed per thread, call counters, a
 *       scratch buffer used by functions called inside parallel regions. (The OpenMP
 *       simulator extension e22 uses it for the current-instruction globals.) private()
 *       cannot do this: it covers one construct and only variables named in it.
 * Expected output (4 threads; the same every run):
 *   region 2 sees the values set in region 1: 0 10 20 30
 *   copyin(counter) gives every thread the master's value: 77 77 77 77
 *   OK
 */
#include <omp.h>
#include <stdio.h>

#define MAXT 256

static int counter = 0;                     /* a global ...                */
#pragma omp threadprivate(counter)          /* ... with one copy per thread */

int main(void) {
    omp_set_dynamic(0);                     /* the copies persist only if the team stays the same */
    int p = omp_get_max_threads() < MAXT ? omp_get_max_threads() : MAXT;
    int seen[MAXT] = {0}, ok = 1;

    #pragma omp parallel num_threads(p)
    counter = 10 * omp_get_thread_num();    /* region 1: each thread sets ITS copy */

    #pragma omp parallel num_threads(p)
    seen[omp_get_thread_num()] = counter;   /* region 2: the values are still there */

    printf("region 2 sees the values set in region 1:");
    for (int t = 0; t < p; t++) { printf(" %d", seen[t]); ok = ok && seen[t] == 10 * t; }

    counter = 77;                           /* outside a region: the master's (thread 0's) copy */
    #pragma omp parallel num_threads(p) copyin(counter)
    seen[omp_get_thread_num()] = counter;   /* every copy starts as 77 */

    printf("\ncopyin(counter) gives every thread the master's value:");
    for (int t = 0; t < p; t++) { printf(" %d", seen[t]); ok = ok && seen[t] == 77; }
    printf("\n%s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

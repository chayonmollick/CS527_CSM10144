/* 01_hello_threads.c - parallel region, thread id, team size, num_threads clause
 *
 * What: "#pragma omp parallel" starts a team of threads and EVERY thread runs the block.
 *       omp_get_thread_num() = this thread's id 0..p-1, omp_get_num_threads() = p.
 *       Team size, first rule that applies: num_threads(n) clause on the region >
 *       omp_set_num_threads(n) call > OMP_NUM_THREADS variable > number of cores.
 * When: the start of every OpenMP program. Variables declared inside the block are
 *       private (one per thread); variables declared before it are shared.
 * Expected output (OMP_NUM_THREADS=4; the ORDER of the thread lines varies between runs):
 *   outside a parallel region: thread 0 of 1
 *   hello from thread 1 of 4
 *   hello from thread 3 of 4
 *   hello from thread 0 of 4
 *   hello from thread 2 of 4
 *   num_threads(2): thread 0 of 2
 *   num_threads(2): thread 1 of 2
 *   threads 0..3 each said hello once; num_threads(2) gave 2 threads: OK
 */
#include <omp.h>
#include <stdio.h>

#define MAXT 256

int main(void) {
    printf("outside a parallel region: thread %d of %d\n",
           omp_get_thread_num(), omp_get_num_threads());

    int hello[MAXT] = {0};                  /* shared; each thread writes only its own slot */
    int team = 0;
    #pragma omp parallel
    {
        int id = omp_get_thread_num();      /* declared inside the block: private */
        int p = omp_get_num_threads();
        printf("hello from thread %d of %d\n", id, p);
        if (id < MAXT) hello[id]++;
        if (id == 0) team = p;              /* only thread 0 writes team: no race */
    }                                       /* implicit barrier: every thread is done here */

    int two = 0;
    #pragma omp parallel num_threads(2)     /* this region only: a team of 2 */
    {
        printf("num_threads(2): thread %d of %d\n", omp_get_thread_num(), omp_get_num_threads());
        if (omp_get_thread_num() == 0) two = omp_get_num_threads();
    }

    int ok = team == omp_get_max_threads() && two == 2;
    for (int t = 0; t < team && t < MAXT; t++) ok = ok && hello[t] == 1;
    printf("threads 0..%d each said hello once; num_threads(2) gave %d threads: %s\n",
           team - 1, two, ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

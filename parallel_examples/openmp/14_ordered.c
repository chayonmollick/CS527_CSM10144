/* 14_ordered.c - ordered: compute in parallel, but do one part in loop order
 *
 * What: a loop with the "ordered" clause may contain "#pragma omp ordered { ... }".
 *       That block runs for i = 0, 1, 2, ... strictly in sequence, although the
 *       iterations are spread over the threads; the rest of each iteration runs in
 *       parallel.
 * When: compute in parallel but print, write to a file or append to a list in the
 *       original order. Keep the ordered block small: threads queue up for it.
 * Expected output (4 threads; the thread numbers VARY, the order of i never does):
 *   i =  0  f(i) = 15.268195  computed by thread 1
 *   i =  1  f(i) = 15.181184  computed by thread 0
 *   ...
 *   i = 11  f(i) = 12.783291  computed by thread 3
 *   the ordered block ran for i = 0, 1, ..., 11 in that order: OK
 */
#include <omp.h>
#include <stdio.h>

#define N 12

static double f(int i) {                    /* work whose cost depends on i */
    double s = 0.0;
    for (long k = 1; k <= 200000L * (N - i); k++) s += 1.0 / k;
    return s;
}

int main(void) {
    int order[N], next = 0;
    #pragma omp parallel for ordered schedule(dynamic, 1)
    for (int i = 0; i < N; i++) {
        double v = f(i);                    /* in parallel; finishes in any order */
        int t = omp_get_thread_num();
        #pragma omp ordered
        {                                   /* one at a time, in loop order: i = 0, 1, 2, ... */
            printf("i = %2d  f(i) = %.6f  computed by thread %d\n", i, v, t);
            order[next++] = i;
        }
    }
    int ok = next == N;
    for (int k = 0; k < next; k++) ok = ok && order[k] == k;
    printf("the ordered block ran for i = 0, 1, ..., %d in that order: %s\n", N - 1, ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

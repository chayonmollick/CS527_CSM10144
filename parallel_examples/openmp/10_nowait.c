/* 10_nowait.c - nowait: drop the barrier at the end of a worksharing loop
 *
 * What: every "omp for" (and single, sections) ends with an implicit barrier.
 *       "#pragma omp for nowait" removes it: a thread that has finished its part of
 *       loop 1 starts loop 2 at once instead of waiting for the slowest thread.
 * When: the next loop does not read anything the previous loop wrote (loops 1 and 2
 *       here are independent). Keep (or add) a barrier before a loop that reads
 *       elements OTHER threads wrote: loop 3 reads b[n-1-i].
 *       (Allowed exception: both loops schedule(static) over the same range and
 *       iteration i only reads element i - then the same thread wrote it.)
 * Expected output (the same every run):
 *   loops 1 and 2 ran with nowait, loop 3 after a barrier
 *   d[i] = b[n-1-i] + c[i] for 1000000 elements matches the serial loops (0 wrong): OK
 */
#include <stdio.h>
#include <stdlib.h>

#define N 1000000

int main(void) {
    int *a = malloc(N * sizeof *a), *b = malloc(N * sizeof *b);
    int *c = malloc(N * sizeof *c), *d = malloc(N * sizeof *d);
    if (!a || !b || !c || !d) return 1;
    for (int i = 0; i < N; i++) a[i] = i % 1000;

    #pragma omp parallel
    {
        #pragma omp for nowait              /* loop 2 does not use b: no need to wait */
        for (int i = 0; i < N; i++) b[i] = 2 * a[i];
        #pragma omp for nowait
        for (int i = 0; i < N; i++) c[i] = a[i] + 1;
        #pragma omp barrier                 /* loop 3 reads b and c written by other threads */
        #pragma omp for
        for (int i = 0; i < N; i++) d[i] = b[N - 1 - i] + c[i];
    }

    long wrong = 0;
    for (int i = 0; i < N; i++)             /* serial answer: 2 a[n-1-i] + a[i] + 1 */
        if (d[i] != 2 * a[N - 1 - i] + a[i] + 1) wrong++;
    printf("loops 1 and 2 ran with nowait, loop 3 after a barrier\n");
    printf("d[i] = b[n-1-i] + c[i] for %d elements matches the serial loops (%ld wrong): %s\n",
           N, wrong, wrong == 0 ? "OK" : "WRONG");
    free(a); free(b); free(c); free(d);
    return wrong == 0 ? 0 : 1;
}

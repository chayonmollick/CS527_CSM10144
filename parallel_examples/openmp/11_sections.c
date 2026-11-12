/* 11_sections.c - sections: different jobs at the same time
 *
 * What: "#pragma omp parallel sections" holds several "#pragma omp section" blocks.
 *       Each block runs ONCE, on some thread; different blocks run in parallel, and
 *       everyone waits at the end. More sections than threads: some threads run
 *       several; fewer: the extra threads have nothing to do.
 * When: a FEW different, independent jobs - sum, maximum and count of the same array,
 *       or reading two files. Not for one big loop (use parallel for) and not for
 *       recursion (use tasks, 15). The time is that of the slowest section.
 * Expected output (4 threads; which thread ran each section VARIES between runs):
 *   section 1 (sum)   ran on thread 3: 499500000
 *   section 2 (max)   ran on thread 0: 999
 *   section 3 (evens) ran on thread 1: 500000
 *   all three equal the serial loop: OK
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 1000000

int main(void) {
    int *a = malloc(N * sizeof *a);
    if (!a) return 1;
    for (int i = 0; i < N; i++) a[i] = (int)((i * 7L) % 1000);

    long sum = 0, evens = 0;
    int mx = a[0], who[3] = {-1, -1, -1};
    #pragma omp parallel sections           /* each variable is written by ONE section only */
    {
        #pragma omp section
        {
            for (int i = 0; i < N; i++) sum += a[i];
            who[0] = omp_get_thread_num();
        }
        #pragma omp section
        {
            for (int i = 1; i < N; i++) if (a[i] > mx) mx = a[i];
            who[1] = omp_get_thread_num();
        }
        #pragma omp section
        {
            for (int i = 0; i < N; i++) evens += a[i] % 2 == 0;
            who[2] = omp_get_thread_num();
        }
    }

    long s_sum = 0, s_evens = 0;            /* serial check, all three in one loop */
    int s_mx = a[0];
    for (int i = 0; i < N; i++) {
        s_sum += a[i];
        if (a[i] > s_mx) s_mx = a[i];
        s_evens += a[i] % 2 == 0;
    }
    printf("section 1 (sum)   ran on thread %d: %ld\n", who[0], sum);
    printf("section 2 (max)   ran on thread %d: %d\n", who[1], mx);
    printf("section 3 (evens) ran on thread %d: %ld\n", who[2], evens);
    int ok = sum == s_sum && mx == s_mx && evens == s_evens;
    printf("all three equal the serial loop: %s\n", ok ? "OK" : "WRONG");
    free(a);
    return ok ? 0 : 1;
}

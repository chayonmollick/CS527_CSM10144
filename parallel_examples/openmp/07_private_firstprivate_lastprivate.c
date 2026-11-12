/* 07_private_firstprivate_lastprivate.c - private, firstprivate, lastprivate
 *
 * What: private(x)      every thread gets its own x, NOT initialised (garbage until set);
 *                       the x outside the region is unchanged afterwards.
 *       firstprivate(x) own copy, initialised with the value x had before the region.
 *       lastprivate(x)  own copy; after the loop, x = the value from the sequentially
 *                       LAST iteration (i = n-1), whichever thread ran it.
 * When: private for scratch variables declared outside the region (a temp, the j of an
 *       inner loop); firstprivate for a start value every thread needs (offset, seed,
 *       scale); lastprivate when the code after the loop needs the last iteration's value.
 *       Trap: lastprivate is NOT "the last value assigned": if the final iteration does
 *       not assign x, x is undefined after the loop.
 * Expected output (4 threads; the same every run):
 *   private     : the copies were set to 100..103; x after the region = 10
 *   firstprivate: every copy started at 10: yes; x after the region = 10
 *   lastprivate : last = 9801 (from i = 99: 99 * 99)
 *   OK
 */
#include <omp.h>
#include <stdio.h>

#define MAXT 256

int main(void) {
    int x = 10, ok = 1;
    int copy[MAXT] = {0};

    #pragma omp parallel private(x)
    {
        int id = omp_get_thread_num();
        x = 100 + id;                       /* the private x has no value until it is set */
        if (id < MAXT) copy[id] = x;
    }
    printf("private     : the copies were set to 100..%d; x after the region = %d\n",
           100 + omp_get_max_threads() - 1, x);
    ok = ok && x == 10 && copy[0] == 100;

    int started_at_10 = 1;
    #pragma omp parallel firstprivate(x) reduction(&&:started_at_10)
    {
        started_at_10 = (x == 10);          /* every copy starts with the outer value */
        x += omp_get_thread_num();          /* changes only this thread's copy */
    }
    printf("firstprivate: every copy started at 10: %s; x after the region = %d\n",
           started_at_10 ? "yes" : "no", x);
    ok = ok && started_at_10 && x == 10;

    int last = -1;
    #pragma omp parallel for lastprivate(last)
    for (int i = 0; i < 100; i++)
        last = i * i;                       /* after the loop: the value from i = 99 */
    printf("lastprivate : last = %d (from i = 99: 99 * 99)\n", last);
    ok = ok && last == 99 * 99;

    printf("%s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

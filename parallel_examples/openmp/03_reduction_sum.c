/* 03_reduction_sum.c - reduction(+:sum): add up a loop without a race
 *
 * What: each thread gets a PRIVATE copy of sum that starts at 0 (the identity of +).
 *       At the end of the loop the copies are added together AND to the value sum had
 *       before the loop. There is no lock inside the loop, so it is fast.
 *       Operators: + * - & | ^ && || max min. Several variables: reduction(+:a, b, c).
 * When: the loop folds everything into one value: a sum, a count, a product, a
 *       logical AND. Without it, "sum += x" from many threads is a data race (05).
 *       A double sum is added in a different order, so its last digits can differ
 *       from the serial loop: compare doubles with a tolerance, integers exactly.
 * Expected output (the same every run, apart from the last digits of the doubles):
 *   sum of 1..10000000          = 50000005000000 (serial 50000005000000)
 *   sum of 1/k^2, k=1..10000000 = 1.644933967 (serial 1.644933967; pi^2/6 = 1.644934067)
 *   multiples of 3 or 5         = 4666667 (serial 4666667)
 *   the reduction adds to the old value: 1000 + (1 + 2 + ... + 10) = 1055
 *   all equal the serial loops: OK
 */
#include <math.h>
#include <stdio.h>

#define N 10000000L

int main(void) {
    long long sum = 0;
    double inv2 = 0.0;
    long count = 0;
    #pragma omp parallel for reduction(+:sum, inv2, count)
    for (long k = 1; k <= N; k++) {
        sum += k;                           /* each thread adds into its own copies */
        inv2 += 1.0 / ((double)k * k);
        if (k % 3 == 0 || k % 5 == 0) count++;
    }

    long long s_sum = 0;                    /* the same loop, serial */
    double s_inv2 = 0.0;
    long s_count = 0;
    for (long k = 1; k <= N; k++) {
        s_sum += k;
        s_inv2 += 1.0 / ((double)k * k);
        if (k % 3 == 0 || k % 5 == 0) s_count++;
    }

    int r = 1000;                           /* not reset: the copies are added to 1000 */
    #pragma omp parallel for reduction(+:r)
    for (int i = 1; i <= 10; i++) r += i;

    printf("sum of 1..%ld          = %lld (serial %lld)\n", N, sum, s_sum);
    printf("sum of 1/k^2, k=1..%ld = %.9f (serial %.9f; pi^2/6 = 1.644934067)\n", N, inv2, s_inv2);
    printf("multiples of 3 or 5         = %ld (serial %ld)\n", count, s_count);
    printf("the reduction adds to the old value: 1000 + (1 + 2 + ... + 10) = %d\n", r);
    int ok = sum == s_sum && count == s_count && fabs(inv2 - s_inv2) < 1e-9 * s_inv2 && r == 1055;
    printf("all equal the serial loops: %s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

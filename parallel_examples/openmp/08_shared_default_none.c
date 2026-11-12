/* 08_shared_default_none.c - shared variables and default(none)
 *
 * What: shared(v): one v for all threads - the default for variables declared before
 *       the region. Many threads READING it is fine; many WRITING it is a data race
 *       unless each thread writes different elements (b[i] here) or they synchronise.
 *       default(none): no default at all. Every variable from outside the region must
 *       be listed as shared / private / firstprivate / reduction, or it does not compile:
 *           error: 'count' not specified in enclosing 'parallel'
 * When: default(none) is the safest habit in the test: it makes you decide for every
 *       variable. The loop index of the omp for and the variables declared inside the
 *       region are private anyway and need no clause.
 * Expected output (the same every run):
 *   a[i] > 500 for 4990 of 10000 elements; b[i] = a[i]^2 there, else 0; sum of b = 2913944495
 *   serial: 4990 elements, sum 2913944495, 0 wrong b[i]
 *   matches the serial loop: OK
 */
#include <stdio.h>

#define N 10000

int main(void) {
    int a[N], b[N];
    for (int i = 0; i < N; i++) a[i] = (i * 37) % 1001;    /* values 0..1000 */

    int n = N, limit = 500;
    long count = 0, total = 0;
    #pragma omp parallel for default(none) shared(a, b, n) firstprivate(limit) reduction(+:count, total)
    for (int i = 0; i < n; i++) {
        int sq = a[i] * a[i];               /* declared inside: private */
        b[i] = a[i] > limit ? sq : 0;       /* each i writes its own b[i]: no race */
        if (a[i] > limit) count++;
        total += b[i];
    }

    long s_count = 0, s_total = 0, wrong = 0;
    for (int i = 0; i < N; i++) {           /* serial check */
        int want = a[i] > 500 ? a[i] * a[i] : 0;
        if (b[i] != want) wrong++;
        s_count += a[i] > 500;
        s_total += want;
    }
    printf("a[i] > %d for %ld of %d elements; b[i] = a[i]^2 there, else 0; sum of b = %ld\n",
           limit, count, N, total);
    printf("serial: %ld elements, sum %ld, %ld wrong b[i]\n", s_count, s_total, wrong);
    int ok = count == s_count && total == s_total && wrong == 0;
    printf("matches the serial loop: %s\n", ok ? "OK" : "WRONG");
    return ok ? 0 : 1;
}

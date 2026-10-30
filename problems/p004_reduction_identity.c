/* P004: What value does a reduction variable start with inside the region?
 * Problem: Show that in reduction(op:x) every thread's private copy of x starts at the
 *   identity of op (+:0, *:1, max:INT_MIN, min:INT_MAX, &&:1, ||:0, &:~0, |:0, ^:0),
 *   whatever value x had before the region, and that the original value of x IS
 *   combined into the result at the end. Check every operator.
 * Solution: x is set to a non-identity value before each region. Inside, every thread
 *   first records its private copy into seen[thread] (its own slot: no race), then
 *   combines a contribution of its own. After the region we check (1) all private
 *   copies started at the identity and (2) final x = original op all contributions.
 * Source: textbook Ex 20.3.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p004_reduction_identity.c -o p004 -lm
 * Run:   ./p004 [P]          (default P = 4 threads for the printed table; 1, 2, 4, 8 are checked)
 */
#include "common.h"

#define MAXT 64
static long long seen[MAXT];   /* seen[t] = private copy of thread t at region start */
static int team;               /* team size of the last region */
static int verbose;            /* print the table (only for the demo thread count) */

/* Called first thing inside each region: store this thread's starting private copy. */
static void record(long long v) {
    int id = omp_get_thread_num();
    if (id < MAXT) seen[id] = v;
    if (id == 0) team = omp_get_num_threads();
}

/* Check one operator: every private copy == identity and final == expected. */
static int report(const char *op, long long before, long long identity,
                  long long final, long long expect, int hex) {
    int ok = (final == expect);
    for (int t = 0; t < team && t < MAXT; t++) if (seen[t] != identity) ok = 0;
    if (verbose) {
        const char *f = hex ? " 0x%llx" : " %lld";
        printf("%-4s", op);
        printf(hex ? " before=0x%-6llx" : " before=%-8lld", before);
        printf(" copies started at:");
        for (int t = 0; t < team && t < 8; t++) printf(f, seen[t]);
        printf(hex ? "  final=0x%llx" : "  final=%lld", final);
        printf(hex ? " (expect 0x%llx)" : " (expect %lld)", expect);
        printf("  %s\n", ok_str(ok));
    }
    return ok;
}

/* All operators with the current number of threads. Returns 1 if all checks pass. */
static int run_all(void) {
    int ok = 1;
    {   int x = 100;                                     /* + : identity 0 */
        #pragma omp parallel reduction(+:x)
        { record(x); x += omp_get_thread_num() + 1; }
        ok &= report("+", 100, 0, x, 100 + team * (team + 1) / 2, 0);
    }
    {   int x = 100;                                     /* - : identity 0, partials are ADDED */
        #pragma omp parallel reduction(-:x)
        { record(x); x -= omp_get_thread_num() + 1; }
        ok &= report("-", 100, 0, x, 100 - team * (team + 1) / 2, 0);
    }
    {   long long x = 3;                                 /* * : identity 1 */
        #pragma omp parallel reduction(*:x)
        { record(x); x *= 2; }
        ok &= report("*", 3, 1, x, 3LL << team, 0);
    }
    {   int x = 1000;                                    /* max : identity INT_MIN */
        #pragma omp parallel reduction(max:x)
        { record(x); int v = 10 * omp_get_thread_num(); if (v > x) x = v; }
        ok &= report("max", 1000, INT_MIN, x, 1000, 0);  /* 1000 > every contribution */
    }
    {   int x = -5;                                      /* min : identity INT_MAX */
        #pragma omp parallel reduction(min:x)
        { record(x); int v = omp_get_thread_num(); if (v < x) x = v; }
        ok &= report("min", -5, INT_MAX, x, -5, 0);      /* -5 < every contribution */
    }
    {   int x = 0;                                       /* && : identity 1 (true) */
        #pragma omp parallel reduction(&&:x)
        { record(x); x = x && 1; }
        ok &= report("&&", 0, 1, x, 0, 0);               /* original false wins */
    }
    {   int x = 1;                                       /* || : identity 0 (false) */
        #pragma omp parallel reduction(||:x)
        { record(x); x = x || 0; }
        ok &= report("||", 1, 0, x, 1, 0);               /* original true wins */
    }
    {   unsigned x = 0xFFu;                              /* & : identity ~0 (all bits set) */
        #pragma omp parallel reduction(&:x)
        { record(x); x &= ~(1u << omp_get_thread_num()); }
        ok &= report("&", 0xFF, ~0u, x, 0xFFu & ~((1u << team) - 1), 1);
    }
    {   unsigned x = 0x100u;                             /* | : identity 0 */
        #pragma omp parallel reduction(|:x)
        { record(x); x |= 1u << omp_get_thread_num(); }
        ok &= report("|", 0x100, 0, x, 0x100u | ((1u << team) - 1), 1);
    }
    {   unsigned x = 0x5u;                               /* ^ : identity 0 */
        #pragma omp parallel reduction(^:x)
        { record(x); x ^= 1u << omp_get_thread_num(); }
        ok &= report("^", 0x5, 0, x, 0x5u ^ ((1u << team) - 1), 1);
    }
    return ok;
}

int main(int argc, char *argv[]) {
    int p = (int)arg_long(argc, argv, 1, 4);
    if (p < 1 || p > 30) { printf("P must be 1..30 (bit tricks use 1u << thread)\n"); return 1; }
    omp_set_dynamic(0);

    printf("P004: reduction identities with %d threads\n", p);
    printf("(x is set to a NON-identity value before each region; copies shown for threads 0..%d)\n\n",
           p < 8 ? p - 1 : 7);
    omp_set_num_threads(p);
    verbose = 1;
    int ok = run_all();

    verbose = 0;
    printf("\nSame checks with 1, 2, 4, 8 threads:");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int okk = run_all();
        printf("  p=%d %s", THREADS[k], ok_str(okk));
        ok &= okk;
    }
    printf("\nOverall check: %s\n", ok_str(ok));
    return 0;
}

/* Notes:
 * - Inside the region x is a NEW private variable per thread, initialised to the
 *   operator's identity; the value x had before the region is not visible there.
 * - At the end: x_original = x_original op copy_0 op copy_1 ... ; so the value before
 *   the region DOES matter (e.g. max with 1000 stays 1000, && with false stays false).
 * - reduction(-:x) also starts at 0 and the partial results are ADDED (OpenMP <= 5.1),
 *   so x -= v in each thread gives x_original - sum(v).
 * - max/min for C (OpenMP 3.1+): the identity is the smallest/largest value of the type
 *   (INT_MIN / INT_MAX for int; for double gcc uses -inf / +inf).
 * - The combination order is unspecified: fine for integers, but floating-point
 *   + and * may give slightly different last bits from run to run.
 */

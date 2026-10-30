/* P005: Find the LAST index that satisfies a condition (and the lastprivate trap)
 * Problem: Given int a[N], find the largest index i with cond(a[i]) (here a[i] % 100 == 7),
 *   or -1 if there is none. Parallelise the loop  for (i...) if (cond(a[i])) result = i;
 *   Show that a shared result is a race, give a critical-section fix, explain why
 *   lastprivate(result) with "if (cond) result = i;" is NOT correct, and give a correct
 *   fast version. Verify against the serial result and time the correct versions.
 * Solution: (1) shared result: threads overwrite each other; the final value is the
 *   last WRITE in time, not the largest index. (2) critical + "keep the larger index":
 *   correct, but one lock per match. (3) lastprivate copies out the private value of
 *   the thread that ran the sequentially LAST iteration (i = N-1); that thread's copy
 *   only knows matches in its own chunk (or is unset), so it fails when the last
 *   chunk has no match (dataset B). (4) reduction(max:result): each thread keeps the
 *   largest matching index it saw, the maxima are combined at the end: correct for
 *   any schedule, no locks.
 * Source: textbook Ex 19.13.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p005_lastprivate_search.c -o p005 -lm
 * Run:   ./p005 [N]          (default N = 2e7)
 */
#include "common.h"

static int cond(int x) { return x % 100 == 7; }

/* Serial reference: scan everything, remember the last match (same work as the parallel loops). */
static long last_serial(const int *a, long n) {
    long result = -1;
    for (long i = 0; i < n; i++) if (cond(a[i])) result = i;
    return result;
}

/* WRONG: data race on the shared result; the final value is whoever wrote last. */
static long last_racy(const int *a, long n) {
    long result = -1;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) if (cond(a[i])) result = i;
    return result;
}

/* WRONG in general: lastprivate takes the copy of the thread that ran i = n-1.
 * firstprivate makes every copy start at -1 (without it the copy is uninitialised). */
static long last_lastprivate_naive(const int *a, long n) {
    long result = -1;
    #pragma omp parallel for firstprivate(result) lastprivate(result)
    for (long i = 0; i < n; i++) if (cond(a[i])) result = i;
    return result;
}

/* Correct: critical section, keep the larger index (threads arrive in any order). */
static long last_critical(const int *a, long n) {
    long result = -1;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        if (cond(a[i])) {
            #pragma omp critical
            { if (i > result) result = i; }
        }
    }
    return result;
}

/* Correct and fast: private maximum per thread, combined once at the end. */
static long last_reduction(const int *a, long n) {
    long result = -1;
    #pragma omp parallel for reduction(max:result)
    for (long i = 0; i < n; i++)
        if (cond(a[i]) && i > result) result = i;   /* "i > result" makes it schedule-proof */
    return result;
}

/* Where lastprivate IS right: x is assigned in EVERY iteration. */
static int lastprivate_every_iteration(const int *a, long n) {
    int x = -1;
    #pragma omp parallel for lastprivate(x)
    for (long i = 0; i < n; i++) x = a[i] % 100;
    return x;                                       /* == a[n-1] % 100, as in serial code */
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 20000000L);
    if (n < 1) n = 1;
    int *A = xmalloc((size_t)n * sizeof(int));   /* random: matches everywhere (~1%) */
    int *B = xmalloc((size_t)n * sizeof(int));   /* matches only near the start      */
    fill_int(A, n, 0, 999999, 5);
    for (long i = 0; i < n; i++) B[i] = cond(A[i]) ? A[i] + 1 : A[i];   /* remove all matches */
    B[n / 20] = 7;
    B[n / 10] = 107;                              /* answer for B is n/10 */
    omp_set_dynamic(0);
    omp_warmup();

    long ansA = last_serial(A, n), ansB = last_serial(B, n);
    printf("P005: last index with a[i] %% 100 == 7, N = %ld\n", n);
    printf("Dataset A (random, ~1%% matches): serial answer %ld\n", ansA);
    printf("Dataset B (matches only at N/20 and N/10): serial answer %ld\n\n", ansB);

    printf("Part 1: the wrong ways (failures are expected; they show the bug)\n");
    printf("%8s %26s %24s %24s\n", "threads", "racy shared: wrong runs", "naive lastprivate A", "naive lastprivate B");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int wrong = 0;
        for (int r = 0; r < 5; r++) if (last_racy(A, n) != ansA) wrong++;
        long la = last_lastprivate_naive(A, n), lb = last_lastprivate_naive(B, n);
        printf("%8d %22d / 5 %16ld %-7s %16ld %-7s\n", THREADS[k], wrong,
               la, la == ansA ? "right" : "WRONG", lb, lb == ansB ? "right" : "WRONG");
    }
    printf("(naive lastprivate is right on A only by luck: the last chunk happens to contain a match)\n\n");

    printf("Part 2: correct versions, timed on dataset A, also checked on dataset B\n");
    double ts, tc, tr;
    long rc = 0, rr = 0;
    int all_ok = 1;
    BEST_TIME(ts, ansA = last_serial(A, n));
    printf("%8s %12s %12s %8s %12s %8s %6s\n",
           "threads", "serial(s)", "critical(s)", "speedup", "max-red(s)", "speedup", "check");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tc, rc = last_critical(A, n));
        BEST_TIME(tr, rr = last_reduction(A, n));
        int ok = rc == ansA && rr == ansA &&
                 last_critical(B, n) == ansB && last_reduction(B, n) == ansB;
        all_ok &= ok;
        printf("%8d %12.6f %12.6f %8.2f %12.6f %8.2f %6s\n",
               THREADS[k], ts, tc, ts / tc, tr, ts / tr, ok_str(ok));
    }

    int x = lastprivate_every_iteration(A, n);
    int lp_ok = (x == A[n - 1] % 100);
    all_ok &= lp_ok;
    printf("\nPart 3: lastprivate(x) with x = a[i] %% 100 set in EVERY iteration: x = %d, "
           "a[N-1] %% 100 = %d  %s\n", x, A[n - 1] % 100, ok_str(lp_ok));
    printf("Correct versions (critical, reduction(max), lastprivate used properly): %s\n", ok_str(all_ok));
    free(A); free(B);
    return 0;
}

/* Notes:
 * - lastprivate(x) = "x after the loop has the value it would have after the
 *   sequentially last iteration" - it copies the private x of the thread that ran
 *   i = N-1. It is only right if x is assigned in that iteration (i.e. every iteration).
 * - Plain lastprivate without firstprivate: a thread whose chunk has no match copies
 *   out an UNINITIALISED value. firstprivate+lastprivate on the same variable is legal.
 * - reduction(max:result) works because "largest matching index" is an associative,
 *   commutative combination of per-thread answers (identity LONG_MIN, original -1 kept).
 * - The critical version is correct but serialises every match; cost grows with the
 *   number of matches (try % 2 == 1). Keep "if (i > result)" inside the critical.
 * - Best serial method: scan BACKWARDS and break at the first match. With matches
 *   near the end that beats any full parallel scan; the parallel scan wins when the
 *   answer may be far from the end (or there may be none).
 */

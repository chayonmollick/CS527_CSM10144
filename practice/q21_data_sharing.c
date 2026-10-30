/* CS527 Computer Systems — OpenMP practice
 * Q21: Demonstrate the OpenMP data-sharing attribute clauses. For each one print
 *      the variable's value before the region, inside it (per thread) and after it.
 *  - shared        : one copy, seen and updated by every thread
 *  - private       : new UNINITIALISED copy per thread, thrown away at the end
 *  - firstprivate  : private copy initialised from the original (copy-in)
 *  - lastprivate   : original receives the value of the sequentially LAST iteration
 *  - default(none) : the compiler forces you to scope every variable explicitly
 *  - loop index of an `omp for` is private automatically (an inner index is NOT, in C)
 *  - threadprivate + copyin : a per-thread global that persists between regions
 *  - reduction     : private copies start at the operator's identity (0 for +, 1 for *)
 * "Inside" values are stored per thread and printed after the region, in thread order.
 * Usage: ./q21_data_sharing [threads]     (default 4)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAXT 64

static int tp = 0;                       /* global: one copy PER THREAD because of: */
#pragma omp threadprivate(tp)

/* 1. shared: every thread works on the SAME variable (same address). */
static void demo_shared(void) {
    int x = 0, *orig = &x;
    int same[MAXT], nt = 0;
    printf("\n[1] shared(x)\n    before: x = %d\n", x);
    #pragma omp parallel shared(x)
    {
        int t = omp_get_thread_num();
        same[t] = (&x == orig);
        #pragma omp atomic
        x += 1;                          /* all threads update the one x */
        if (t == 0) nt = omp_get_num_threads();
    }
    for (int t = 0; t < nt; t++)
        printf("    inside thread %d: &x is the original's address? %s\n", t, same[t] ? "yes" : "no");
    printf("    after : x = %d  (each of the %d threads added 1 to the SAME x) %s\n",
           x, nt, x == nt ? "OK" : "WRONG");
}

/* 2. private: each thread gets its own new, uninitialised p. */
static void demo_private(void) {
    int p = 100, *orig = &p;
    int val[MAXT], same[MAXT], nt = 0;
    printf("\n[2] private(p)\n    before: p = %d\n", p);
    #pragma omp parallel private(p)
    {
        int t = omp_get_thread_num();
        /* p's value on entry is UNDEFINED (garbage, not 100). Reading it before
         * assigning is a bug (gcc -Wall warns), so we assign first. */
        p = 10 * t;
        val[t] = p;
        same[t] = (&p == orig);
        if (t == 0) nt = omp_get_num_threads();
    }
    for (int t = 0; t < nt; t++)
        printf("    inside thread %d: p = %-3d (assigned here; value on entry undefined), "
               "&p is the original's address? %s\n", t, val[t], same[t] ? "yes" : "no");
    printf("    after : p = %d  (original untouched, private copies discarded) %s\n",
           p, p == 100 ? "OK" : "WRONG");
}

/* 3. firstprivate: private copy initialised with the original's value. */
static void demo_firstprivate(void) {
    int f = 100;
    int entry[MAXT], val[MAXT], nt = 0;
    printf("\n[3] firstprivate(f)\n    before: f = %d\n", f);
    #pragma omp parallel firstprivate(f)
    {
        int t = omp_get_thread_num();
        entry[t] = f;                    /* = 100, copied in */
        f += t;                          /* changes this thread's copy only */
        val[t] = f;
        if (t == 0) nt = omp_get_num_threads();
    }
    int ok = (f == 100);
    for (int t = 0; t < nt; t++) {
        printf("    inside thread %d: f on entry = %d, after f += %d -> %d\n", t, entry[t], t, val[t]);
        ok = ok && entry[t] == 100;
    }
    printf("    after : f = %d  (copy-in only; changes are NOT copied back) %s\n", f, ok ? "OK" : "WRONG");
}

/* 4. lastprivate: after the loop the original holds the value assigned in the
 *    sequentially last iteration (i = n-1), whichever thread ran it. */
static void demo_lastprivate(void) {
    const int n = 10;
    int last = -1, who[10];
    printf("\n[4] lastprivate(last) on: for i = 0..%d { last = i*i; }  schedule(static,2)\n", n - 1);
    printf("    before: last = %d\n", last);
    #pragma omp parallel for lastprivate(last) schedule(static, 2)
    for (int i = 0; i < n; i++) {
        last = i * i;
        who[i] = omp_get_thread_num();
    }
    printf("    inside: iterations ran on threads:");
    for (int i = 0; i < n; i++) printf(" %d", who[i]);
    printf("\n    after : last = %d  (= 9*9 from iteration i=9, run by thread %d,\n"
           "            NOT the last thread to finish) %s\n",
           last, who[n - 1], last == (n - 1) * (n - 1) ? "OK" : "WRONG");
}

/* 5. default(none): nothing is scoped implicitly. Delete `shared(a, n)` and gcc
 *    refuses to compile: "'a' not specified in enclosing 'parallel'". */
static long sum_default_none(const int *a, long n) {
    long sum = 0;
    #pragma omp parallel for default(none) shared(a, n) reduction(+:sum)
    for (long i = 0; i < n; i++)        /* i: loop index, predetermined private */
        sum += a[i];
    return sum;
}

static void demo_default_none(void) {
    int a[100];
    for (int i = 0; i < 100; i++) a[i] = i + 1;
    long s = sum_default_none(a, 100);
    printf("\n[5] default(none) shared(a, n) reduction(+:sum)\n"
           "    sum of 1..100 = %ld %s\n"
           "    (every variable used inside must be listed; good habit to catch\n"
           "     accidental sharing. In gcc >= 9 even const variables must be listed.)\n",
           s, s == 5050 ? "OK" : "WRONG");
}

/* 6. The index of a loop under `omp for` is private even if declared outside. */
static void demo_loop_index(void) {
    int i = -7, owner[12], nt = omp_get_max_threads();
    printf("\n[6] loop index: `int i` declared OUTSIDE, then  parallel for lastprivate(i)\n");
    printf("    before: i = %d\n", i);
    #pragma omp parallel for lastprivate(i)
    for (i = 0; i < 12; i++)             /* each thread has its own i */
        owner[i] = omp_get_thread_num();
    printf("    inside: owner of i = 0..11:");
    for (int k = 0; k < 12; k++) printf(" %d", owner[k]);
    printf("\n            (default static schedule: contiguous blocks for %d threads)\n", nt);
    printf("    after : i = %d  (lastprivate(i) gives the value after a sequential loop;\n"
           "            WITHOUT lastprivate the value of i after the loop is unspecified) %s\n",
           i, i == 12 ? "OK" : "WRONG");
    printf("    NOTE: in C only the index of the loop directly under `omp for` is private.\n"
           "    An inner `for (j = ...)` with j declared before the region is SHARED -> race.\n"
           "    Write `for (int j = ...)` or add private(j).\n");
}

/* 7. threadprivate global + copyin. */
static void demo_threadprivate(void) {
    int a[MAXT], b[MAXT], c[MAXT], nt = 0;
    omp_set_dynamic(0);                  /* values persist only if the team size is fixed */
    tp = 42;                             /* sets the initial (master) thread's copy only */
    printf("\n[7] threadprivate(tp) global, initialised to 0; master sets tp = 42\n");
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        a[t] = tp;                       /* master: 42, others: their own copy = 0 */
        if (t == 0) nt = omp_get_num_threads();
    }
    #pragma omp parallel copyin(tp)
    {
        int t = omp_get_thread_num();
        b[t] = tp;                       /* copyin: everyone starts with master's 42 */
        tp = 100 + t;
    }
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        c[t] = tp;                       /* still 100+t: persisted from the last region */
    }
    int ok = (tp == 100);
    for (int t = 0; t < nt; t++) {
        printf("    thread %d: region A (no copyin) tp = %-3d | region B copyin(tp) tp = %d, "
               "sets %d | region C tp = %d\n", t, a[t], b[t], 100 + t, c[t]);
        ok = ok && b[t] == 42 && c[t] == 100 + t && a[t] == (t == 0 ? 42 : 0);
    }
    printf("    after : master's tp = %d (the master's copy IS thread 0's copy) %s\n",
           tp, ok ? "OK" : "WRONG");
}

/* 8. reduction: each private copy starts at the identity of the operator; the
 *    original value IS included when the copies are combined at the end. */
static void demo_reduction(void) {
    long s = 1000, p = 2;
    int mx = -5, mn = 50, nt = 0;
    long s_in[MAXT], p_in[MAXT];
    int mx_in[MAXT], mn_in[MAXT];
    printf("\n[8] reduction(+:s) reduction(*:p) reduction(max:mx) reduction(min:mn)\n");
    printf("    before: s = %ld, p = %ld, mx = %d, mn = %d\n", s, p, mx, mn);
    #pragma omp parallel reduction(+:s) reduction(*:p) reduction(max:mx) reduction(min:mn)
    {
        int t = omp_get_thread_num();
        s_in[t] = s; p_in[t] = p; mx_in[t] = mx; mn_in[t] = mn;   /* identities */
        s += t + 1;                       /* thread t contributes t+1 */
        p *= t + 1;
        if (10 * t > mx) mx = 10 * t;
        if (10 * t + 5 < mn) mn = 10 * t + 5;
        if (t == 0) nt = omp_get_num_threads();
    }
    int ok = 1;
    for (int t = 0; t < nt; t++) {
        printf("    thread %d private copies on entry: s = %ld, p = %ld, mx = %d, mn = %d\n",
               t, s_in[t], p_in[t], mx_in[t], mn_in[t]);
        ok = ok && s_in[t] == 0 && p_in[t] == 1 && mx_in[t] == INT_MIN && mn_in[t] == INT_MAX;
    }
    long es = 1000, ep = 2;
    for (int t = 0; t < nt; t++) { es += t + 1; ep *= t + 1; }
    ok = ok && s == es && p == ep && mx == 10 * (nt - 1) && mn == 5;
    printf("    after : s = %ld (1000 + 1+2+..+%d), p = %ld (2 * %d!), mx = %d, mn = %d %s\n",
           s, nt, p, nt, mx, mn, ok ? "OK" : "WRONG");
    printf("    identities: + - | ^ || -> 0,  * && -> 1,  & -> ~0 (all bits 1),\n"
           "                max -> smallest value of the type (INT_MIN), min -> largest (INT_MAX)\n");
}

int main(int argc, char **argv) {
    int nt = (argc > 1) ? atoi(argv[1]) : 4;
    if (nt < 1 || nt > MAXT) nt = 4;
    omp_set_num_threads(nt);
    printf("Q21: data-sharing clauses with %d threads\n", nt);
    demo_shared();
    demo_private();
    demo_firstprivate();
    demo_lastprivate();
    demo_default_none();
    demo_loop_index();
    demo_threadprivate();
    demo_reduction();
    return 0;
}

/* Viva notes:
 * - Default scoping: variables declared before the region are shared; variables
 *   declared inside it, and the index of the loop under `omp for`, are private.
 * - private gives an uninitialised copy (original untouched); firstprivate copies the
 *   value IN; lastprivate copies the last iteration's value OUT (both may be combined).
 * - threadprivate is for globals/statics: each thread keeps its copy across regions
 *   (if the team size stays fixed and dynamic threads are off); copyin initialises all
 *   copies from the master's, copyprivate (on single) broadcasts from one thread.
 * - reduction(op:x): each thread gets a private x set to op's identity, works on it
 *   without synchronisation, and the copies are combined with the original at the end.
 * - default(none) is a safety tool: forgetting to make a temporary private is the most
 *   common OpenMP race; default(none) turns it into a compile error.
 */

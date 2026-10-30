/* CS527 Computer Systems — OpenMP practice
 * Q22: Demonstrate the OpenMP synchronisation constructs with small labelled examples.
 *  - barrier                    : no thread continues until the whole team arrives
 *  - single (+ copyprivate)     : one thread runs the block; copyprivate broadcasts a value
 *  - master (5.1: masked)       : only thread 0 runs it, and there is NO implied barrier
 *  - implicit barrier at the end of `omp for` and `single`; removed with `nowait`
 *  - ordered                    : loop bodies run in parallel, the ordered part in i order
 *  - critical(name)             : differently named critical sections use different locks
 *  - atomic update / read / write / capture
 * Work is simulated with spin(), a busy-wait on omp_get_wtime(), so timings are visible.
 * Usage: ./q22_sync_constructs [threads]     (default 4)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXT 64

static void spin(double sec) {           /* busy-wait for `sec` seconds */
    double t0 = omp_get_wtime();
    while (omp_get_wtime() - t0 < sec) { }
}

static double ms_since(double t0) { return (omp_get_wtime() - t0) * 1e3; }

/* 1. barrier */
static void demo_barrier(void) {
    printf("\n[1] barrier: thread t works t*20 ms, prints 'phase 1', waits at the barrier,\n"
           "    then prints 'phase 2'. Every phase-1 line comes before every phase-2 line;\n"
           "    the order of the phase-2 lines is nondeterministic.\n");
    double t0 = omp_get_wtime();
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        spin(0.020 * t);
        printf("    thread %d: phase 1 done at %3.0f ms\n", t, ms_since(t0));
        #pragma omp barrier
        printf("    thread %d: phase 2 at %3.0f ms (after barrier)\n", t, ms_since(t0));
    }
}

/* 2. single + copyprivate */
static void demo_single(void) {
    printf("\n[2] single copyprivate(cfg): ONE thread (whichever arrives first) runs the block;\n"
           "    copyprivate copies its private cfg into every other thread's private cfg.\n");
    int got[MAXT], nt = 0, ok = 1;
    #pragma omp parallel
    {
        int cfg = 0;                     /* private (declared inside); 0 in every thread */
        #pragma omp single copyprivate(cfg)
        {
            cfg = 42;                    /* e.g. "read the input once" */
            printf("    single block run by thread %d: sets its cfg = %d\n",
                   omp_get_thread_num(), cfg);
        }                                /* implicit barrier + broadcast happen here */
        got[omp_get_thread_num()] = cfg;
        if (omp_get_thread_num() == 0) nt = omp_get_num_threads();
    }
    for (int t = 0; t < nt; t++) {
        printf("    thread %d: cfg = %d after the single\n", t, got[t]);
        ok = ok && got[t] == 42;
    }
    printf("    every thread received cfg = 42: %s\n", ok ? "OK" : "WRONG");
}

/* 3. master: thread 0 only, others do NOT wait */
static void demo_master(void) {
    printf("\n[3] master: only thread 0 runs it (50 ms of work). No implied barrier, so the\n"
           "    other threads pass it immediately. (OpenMP 5.1 renames it `masked`, which also\n"
           "    takes filter(tid); `masked` needs gcc >= 12.)\n");
    double pass[MAXT], t0 = omp_get_wtime();
    int nt = 0;
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        #pragma omp master
        {
            spin(0.050);
            nt = omp_get_num_threads();
        }
        pass[t] = ms_since(t0);          /* time this thread got past the master block */
    }
    for (int t = 0; t < nt; t++)
        printf("    thread %d passed the master block at %3.0f ms%s\n",
               t, pass[t], t == 0 ? "  (it ran the block)" : "");
}

/* 4. implicit barrier at the end of `omp for` / `single` vs nowait */
static void demo_nowait(void) {
    int nt = omp_get_max_threads();
    double done1[MAXT], start2_bar[MAXT], start2_now[MAXT];
    double s_bar[MAXT], s_now[MAXT];
    double t0;

    /* loop 1 has nt iterations; with schedule(static,1) thread t runs iteration t,
     * which takes t*20 ms. Then each thread notes when it reached the code after it. */
    t0 = omp_get_wtime();
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        #pragma omp for schedule(static, 1)
        for (int i = 0; i < nt; i++) { spin(0.020 * i); done1[t] = ms_since(t0); }
                                         /* <- implicit barrier here */
        start2_bar[t] = ms_since(t0);
    }
    t0 = omp_get_wtime();
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        #pragma omp for schedule(static, 1) nowait
        for (int i = 0; i < nt; i++) spin(0.020 * i);
                                         /* <- no barrier: go straight on */
        start2_now[t] = ms_since(t0);
    }
    printf("\n[4a] end of `omp for`: thread t's iteration takes t*20 ms\n");
    printf("    %6s %16s %24s %22s\n", "thread", "loop done (ms)", "next stmt, barrier (ms)",
           "next stmt, nowait (ms)");
    for (int t = 0; t < nt; t++)
        printf("    %6d %16.0f %24.0f %22.0f\n", t, done1[t], start2_bar[t], start2_now[t]);
    printf("    with the implicit barrier everyone waits for the slowest thread;\n"
           "    with nowait each thread continues as soon as its own iterations are done.\n");

    t0 = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single
        spin(0.050);
        s_bar[omp_get_thread_num()] = ms_since(t0);
    }
    t0 = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single nowait
        spin(0.050);
        s_now[omp_get_thread_num()] = ms_since(t0);
    }
    printf("\n[4b] end of `single` (the single block takes 50 ms)\n");
    printf("    %6s %22s %22s\n", "thread", "passed, barrier (ms)", "passed, nowait (ms)");
    for (int t = 0; t < nt; t++)
        printf("    %6d %22.0f %22.0f\n", t, s_bar[t], s_now[t]);
    printf("    (with nowait only the thread that ran the single shows ~50 ms;\n"
           "     the region's own end barrier still waits for it)\n");
}

/* 5. ordered */
static void demo_ordered(void) {
    const int n = 8;
    printf("\n[5a] parallel for WITHOUT ordered, iteration i takes (8-i)*5 ms:\n"
           "     lines come out in completion order, NOT in i order (nondeterministic)\n");
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < n; i++) {
        spin(0.005 * (n - i));
        printf("    i = %d  (thread %d)\n", i, omp_get_thread_num());
    }

    printf("\n[5b] parallel for ordered: same work done in parallel, then the\n"
           "     `#pragma omp ordered` block runs strictly in i = 0,1,2,... order\n");
    int seq[8], k = 0;
    double t0 = omp_get_wtime();
    #pragma omp parallel for ordered schedule(dynamic, 1)
    for (int i = 0; i < n; i++) {
        spin(0.005 * (n - i));           /* parallel part */
        long sq = (long)i * i;
        #pragma omp ordered
        {
            printf("    i = %d  i*i = %2ld  (thread %d)\n", i, sq, omp_get_thread_num());
            seq[k++] = i;                /* only one thread at a time is in here */
        }
    }
    double tpar = ms_since(t0);
    int ok = 1;
    for (int i = 0; i < n; i++) ok = ok && seq[i] == i;
    printf("    ordered output in sequence: %s; took %.0f ms (all-serial work would be %d ms)\n",
           ok ? "OK" : "WRONG", tpar, 5 * n * (n + 1) / 2);
}

/* 6. named critical sections */
static void demo_critical(void) {
    const int reps = 3;
    const double hold = 0.005;           /* each update holds the lock 5 ms */
    int a1 = 0, b1 = 0, a2 = 0, b2 = 0, nt = omp_get_max_threads();
    double t0 = omp_get_wtime();
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        for (int r = 0; r < reps; r++) {
            if (t < nt / 2) {
                #pragma omp critical
                { a1++; spin(hold); }
            } else {
                #pragma omp critical
                { b1++; spin(hold); }
            }
        }
    }
    double t_unnamed = ms_since(t0);
    t0 = omp_get_wtime();
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        for (int r = 0; r < reps; r++) {
            if (t < nt / 2) {
                #pragma omp critical(list_a)
                { a2++; spin(hold); }
            } else {
                #pragma omp critical(list_b)
                { b2++; spin(hold); }
            }
        }
    }
    double t_named = ms_since(t0);
    int ea = (nt / 2) * reps, eb = (nt - nt / 2) * reps;
    printf("\n[6] critical: half the threads update counter A, half update counter B,\n"
           "    %d updates each, every update holding the lock %.0f ms\n", reps, hold * 1e3);
    printf("    two UNNAMED critical  : A = %d, B = %d, time %4.0f ms  (one global lock:\n"
           "                            A and B updates also exclude each other)\n", a1, b1, t_unnamed);
    printf("    critical(list_a) / critical(list_b): A = %d, B = %d, time %4.0f ms  (two locks:\n"
           "                            an A update and a B update can overlap)\n", a2, b2, t_named);
    printf("    counts correct: %s\n",
           (a1 == ea && b1 == eb && a2 == ea && b2 == eb) ? "OK" : "WRONG");
}

/* 7. the four forms of atomic */
static void demo_atomic(void) {
    int total = 0, next_ticket = 0, last_writer = -1, nt = 0;
    int ticket[MAXT], seen[MAXT];
    #pragma omp parallel
    {
        int t = omp_get_thread_num();

        #pragma omp atomic update
        total += t + 1;                  /* read-modify-write as one indivisible step */

        int my;
        #pragma omp atomic capture
        my = next_ticket++;              /* fetch-and-add: old value returned to `my` */
        ticket[t] = my;

        #pragma omp atomic write
        last_writer = t;                 /* indivisible store */

        #pragma omp barrier
        int v;
        #pragma omp atomic read
        v = last_writer;                 /* indivisible load: never a torn value */
        seen[t] = v;
        if (t == 0) nt = omp_get_num_threads();
    }
    printf("\n[7] atomic\n");
    printf("    update : total += t+1 by every thread -> total = %d (expected %d) %s\n",
           total, nt * (nt + 1) / 2, total == nt * (nt + 1) / 2 ? "OK" : "WRONG");
    int used[MAXT] = {0}, ok = 1;
    printf("    capture: my = next_ticket++ gives each thread a unique ticket:");
    for (int t = 0; t < nt; t++) {
        printf(" t%d->%d", t, ticket[t]);
        if (ticket[t] < 0 || ticket[t] >= nt || used[ticket[t]]++) ok = 0;
    }
    printf("  %s\n", ok ? "OK" : "WRONG");
    ok = 1;
    for (int t = 0; t < nt; t++) ok = ok && seen[t] == last_writer;
    printf("    write  : every thread stores last_writer = t; the final value is %d\n"
           "             (which thread wins is nondeterministic)\n", last_writer);
    printf("    read   : after a barrier all threads read the same last_writer: %s\n",
           ok ? "OK" : "WRONG");
}

int main(int argc, char **argv) {
    int nt = (argc > 1) ? atoi(argv[1]) : 4;
    if (nt < 2 || nt > MAXT) nt = 4;
    omp_set_num_threads(nt);
    printf("Q22: synchronisation constructs with %d threads\n", nt);
    demo_barrier();
    demo_single();
    demo_master();
    demo_nowait();
    demo_ordered();
    demo_critical();
    demo_atomic();
    return 0;
}

/* Viva notes:
 * - Implicit barriers: end of parallel, for, sections, single. NOT at the start of
 *   for/single, and NOT for master/masked or critical. `nowait` removes the barrier at
 *   the end of for/sections/single (never the one at the end of parallel).
 * - single vs master: single = any one thread + implicit barrier (+ copyprivate allowed);
 *   master = thread 0 only, no barrier (so add `#pragma omp barrier` if others need its
 *   result). OpenMP 5.1 deprecates master in favour of `masked [filter(tid)]`.
 * - critical vs atomic: critical protects any block with a lock (all unnamed criticals
 *   share ONE lock, names give separate locks); atomic protects one simple update
 *   (x op= expr, x++, v = x, x = expr, v = x++) using a hardware instruction -> cheaper.
 * - atomic capture (v = x++) is fetch-and-add: the standard way to hand out unique
 *   indices/tickets. Only the access to x is atomic, not the evaluation of expr.
 * - ordered needs the `ordered` clause on the loop; it serialises only the ordered
 *   block, so the rest of each iteration still overlaps (e.g. ordered output).
 */

/* CS527 Computer Systems — OpenMP practice
 * Q26: Parallelise a loop whose iteration i costs ~i (triangular workload). Compare
 *      the loop schedules, use schedule(runtime) with omp_set_schedule(), and show
 *      the load imbalance of a static schedule per thread.
 *  - schedule(static)     : one contiguous block of ~N/T iterations per thread
 *  - schedule(static,1)   : round robin (cyclic), chunk 1, fixed in advance
 *  - schedule(dynamic,c)  : threads take the next chunk of c iterations when free
 *  - schedule(guided)     : dynamic with chunks shrinking from ~remaining/T down to 1
 *  - schedule(runtime)    : kind and chunk taken from omp_set_schedule() / OMP_SCHEDULE
 * Usage: ./q26_schedule_runtime [N]     (default N = 30000)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

/* iteration i does i steps of work -> total work ~ N^2/2, the last rows are heaviest */
static long row_work(long i) {
    long s = 0;
    for (long j = 0; j < i; j++) s += (i * j + j) % 7;
    return s;
}

static void tri_serial(long n, long *r) {
    for (long i = 0; i < n; i++) r[i] = row_work(i);
}
static void tri_static(long n, long *r) {
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) r[i] = row_work(i);
}
static void tri_static1(long n, long *r) {
    #pragma omp parallel for schedule(static, 1)
    for (long i = 0; i < n; i++) r[i] = row_work(i);
}
static void tri_dynamic1(long n, long *r) {
    #pragma omp parallel for schedule(dynamic, 1)
    for (long i = 0; i < n; i++) r[i] = row_work(i);
}
static void tri_dynamic64(long n, long *r) {
    #pragma omp parallel for schedule(dynamic, 64)
    for (long i = 0; i < n; i++) r[i] = row_work(i);
}
static void tri_guided(long n, long *r) {
    #pragma omp parallel for schedule(guided)
    for (long i = 0; i < n; i++) r[i] = row_work(i);
}
static void tri_runtime(long n, long *r) {      /* schedule decided by omp_set_schedule() */
    #pragma omp parallel for schedule(runtime)
    for (long i = 0; i < n; i++) r[i] = row_work(i);
}

/* schedule(runtime) loop that also records, per thread, how many iterations it ran
 * and how long it was busy (nowait: a thread stops the clock when it runs out). */
static void tri_stats(long n, long *r, long *iters, double *busy) {
    #pragma omp parallel
    {
        int t = omp_get_thread_num();
        long cnt = 0;
        double t0 = omp_get_wtime();
        #pragma omp for schedule(runtime) nowait
        for (long i = 0; i < n; i++) { r[i] = row_work(i); cnt++; }
        busy[t] = omp_get_wtime() - t0;
        iters[t] = cnt;
    }
}

static const char *kind_name(omp_sched_t k) {
    switch (k) {
        case omp_sched_static:  return "static";
        case omp_sched_dynamic: return "dynamic";
        case omp_sched_guided:  return "guided";
        case omp_sched_auto:    return "auto";
        default:                return "other";
    }
}

static int same(const long *x, const long *y, long n) {
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}

typedef struct {
    const char *name;
    void (*fn)(long, long *);
    int use_runtime;          /* 1: call omp_set_schedule(kind, chunk) first */
    omp_sched_t kind;
    int chunk;
} Sched;

int main(int argc, char **argv) {
    long n = (argc > 1) ? atol(argv[1]) : 30000L;
    if (n <= 0) { fprintf(stderr, "usage: %s [N]\n", argv[0]); return 1; }
    long *ref = malloc((size_t)n * sizeof(long));
    long *r   = malloc((size_t)n * sizeof(long));
    if (!ref || !r) { fprintf(stderr, "alloc failed\n"); return 1; }

    printf("Q26: triangular loop, N = %ld (iteration i costs ~i steps)\n", n);
    tri_serial(n, ref);                                  /* warm-up run, not timed */
    double t0 = omp_get_wtime();
    tri_serial(n, ref);
    double ts = omp_get_wtime() - t0;
    printf("serial: %.4f s\n", ts);

    /* --- 1. per-thread load: static vs dynamic, 4 threads, via schedule(runtime) --- */
    const int T = 4;
    omp_set_num_threads(T);
    omp_sched_t kinds[2] = {omp_sched_static, omp_sched_dynamic};
    int chunks[2] = {0, 1};                    /* chunk < 1 means "default" */
    long it[2][64];
    double busy[2][64];
    for (int k = 0; k < 2; k++) {
        omp_set_schedule(kinds[k], chunks[k]);
        omp_sched_t got; int gc;
        omp_get_schedule(&got, &gc);
        printf("%somp_set_schedule(omp_sched_%s, %d) -> omp_get_schedule() = (%s, %d)\n",
               k == 0 ? "\n" : "", kind_name(kinds[k]), chunks[k], kind_name(got), gc);
        tri_stats(n, r, it[k], busy[k]);
        if (!same(ref, r, n)) printf("    WRONG result\n");
    }
    printf("\n[1] Per-thread load with %d threads (same schedule(runtime) loop):\n", T);
    printf("    %6s | %10s %10s | %10s %10s\n", "thread", "static it", "busy(s)", "dynamic it", "busy(s)");
    double mx[2] = {0, 0}, sum[2] = {0, 0};
    for (int t = 0; t < T; t++) {
        printf("    %6d | %10ld %10.4f | %10ld %10.4f\n", t, it[0][t], busy[0][t], it[1][t], busy[1][t]);
        for (int k = 0; k < 2; k++) {
            sum[k] += busy[k][t];
            if (busy[k][t] > mx[k]) mx[k] = busy[k][t];
        }
    }
    printf("    imbalance = max busy / mean busy: static %.2f, dynamic %.2f  (1.00 = perfect)\n",
           mx[0] / (sum[0] / T), mx[1] / (sum[1] / T));
    printf("    static: equal iteration COUNTS, unequal WORK (thread t's block costs ~2t+1 units);\n"
           "    dynamic: unequal counts, but every thread is busy for about the same time.\n");

    /* --- 2. timing table for all schedules --- */
    const Sched S[] = {
        {"static",              tri_static,    0, omp_sched_static,  0},
        {"static,1",            tri_static1,   0, omp_sched_static,  0},
        {"dynamic,1",           tri_dynamic1,  0, omp_sched_static,  0},
        {"dynamic,64",          tri_dynamic64, 0, omp_sched_static,  0},
        {"guided",              tri_guided,    0, omp_sched_static,  0},
        {"runtime(static)",     tri_runtime,   1, omp_sched_static,  0},
        {"runtime(dynamic,16)", tri_runtime,   1, omp_sched_dynamic, 16},
        {"runtime(guided,4)",   tri_runtime,   1, omp_sched_guided,  4},
    };
    const int ns = (int)(sizeof S / sizeof S[0]);
    const int threads[] = {1, 2, 4, 8};
    printf("\n[2] Time in s (speedup = T_serial / T_parallel), T_serial = %.4f s\n", ts);
    printf("%-20s", "schedule");
    for (int ti = 0; ti < 4; ti++) printf("   %4d thread%s     ", threads[ti], threads[ti] > 1 ? "s" : " ");
    printf(" check\n");
    for (int s = 0; s < ns; s++) {
        if (S[s].use_runtime) omp_set_schedule(S[s].kind, S[s].chunk);
        printf("%-20s", S[s].name);
        int ok = 1;
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            double p0 = omp_get_wtime();
            S[s].fn(n, r);
            double tp = omp_get_wtime() - p0;
            ok = ok && same(ref, r, n);
            printf("  %8.4f (%6.2fx)", tp, tp > 0 ? ts / tp : 0.0);
        }
        printf("  %s\n", ok ? "OK" : "WRONG");
    }
    printf("best possible speedup of plain static here = T^2/(2T-1) (the last block holds\n"
           "the most work): 2 threads -> 1.33x, 4 -> 2.29x, 8 -> 4.27x\n");

    free(ref); free(r);
    return 0;
}

/* Viva notes:
 * - static: iterations split into equal contiguous blocks before the loop starts; zero
 *   scheduling overhead, best when every iteration costs the same. With cost ~i the
 *   last thread gets (2T-1)/T^2 of the work -> imbalance; others idle at the barrier.
 * - static,1 (cyclic) interleaves cheap and expensive iterations, so it balances a
 *   smoothly growing cost with no runtime overhead (but poor locality for arrays).
 * - dynamic,c: idle threads grab the next c iterations from a shared queue -> balances
 *   irregular work, but each grab costs synchronisation; small c = best balance and
 *   most overhead, larger c amortises it. Default chunk is 1.
 * - guided: like dynamic but chunks start large (~remaining/threads) and shrink to the
 *   given minimum -> few grabs early, fine balancing at the end.
 * - runtime: the loop reads the schedule at run time from omp_set_schedule(kind, chunk)
 *   or the OMP_SCHEDULE env var (e.g. OMP_SCHEDULE="dynamic,16" ./a.out), so you can
 *   tune without recompiling. auto lets the compiler/runtime choose.
 */

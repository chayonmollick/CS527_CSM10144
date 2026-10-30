/* CS527 Computer Systems — OpenMP practice
 * Q27: Compute the n-th Fibonacci number recursively using OpenMP tasks.
 *      Compare a task per call (no cutoff) with a cutoff below which the
 *      recursion runs serially, and report speedup over the serial code.
 *  - serial recursion fib(n) = fib(n-1) + fib(n-2)
 *  - tasks, no cutoff: every call spawns two tasks (shared(x), shared(y)) + taskwait
 *  - tasks with cutoff: spawn tasks only while n >= CUTOFF, plain serial below
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define CUTOFF   20     /* below this n the task version calls fib_serial() */
#define NOCUT_N  25     /* the no-cutoff version is run only up to this n   */

static long fib_serial(long n) {
    if (n < 2) return n;
    return fib_serial(n - 1) + fib_serial(n - 2);
}

/* One task per call: correct, but each task does almost no work, so the
 * task create/schedule overhead dominates (~2*fib(n) tasks). */
static long fib_task_nocut(long n) {
    if (n < 2) return n;
    long x, y;
    #pragma omp task shared(x)          /* x lives on this stack frame...   */
    x = fib_task_nocut(n - 1);
    #pragma omp task shared(y)
    y = fib_task_nocut(n - 2);
    #pragma omp taskwait                /* ...so wait before reading x, y */
    return x + y;
}

/* Same recursion, but small sub-problems are done serially. */
static long fib_task_cutoff(long n) {
    if (n < CUTOFF) return fib_serial(n);
    long x, y;
    #pragma omp task shared(x)
    x = fib_task_cutoff(n - 1);
    #pragma omp task shared(y)
    y = fib_task_cutoff(n - 2);
    #pragma omp taskwait
    return x + y;
}

/* Tasks need a team: one thread (single) starts the recursion, the rest of
 * the team picks up the tasks it creates. */
static long run_tasks(long n, int use_cutoff) {
    long r = 0;
    #pragma omp parallel
    #pragma omp single
    r = use_cutoff ? fib_task_cutoff(n) : fib_task_nocut(n);
    return r;
}

static void time_table(const char *title, long n, int use_cutoff) {
    const int threads[] = {1, 2, 4, 8};
    double t0 = omp_get_wtime();
    long ref = fib_serial(n);
    double ts = omp_get_wtime() - t0;

    printf("\n%s: fib(%ld) = %ld, serial %.6f s\n", title, n, ref, ts);
    printf("%8s %12s %12s %9s %11s %7s\n",
           "threads", "serial(s)", "parallel(s)", "speedup", "efficiency", "check");
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime();
        long r = run_tasks(n, use_cutoff);
        double tp = omp_get_wtime() - p0;
        double sp = tp > 0 ? ts / tp : 0.0;
        printf("%8d %12.6f %12.6f %9.2f %11.2f %7s\n",
               threads[ti], ts, tp, sp, sp / threads[ti], r == ref ? "OK" : "WRONG");
    }
}

int main(int argc, char **argv) {
    /* default: n = 35 and 40 (gcc -O2 makes serial fib(35) only ~10 ms) */
    long sizes[2] = {35, 40}; int nsizes = 2;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    long n = sizes[0];
    if (n < 0 || n > 92) { fprintf(stderr, "n must be in 0..92\n"); return 1; }
    long n_small = n < NOCUT_N ? n : NOCUT_N;

    printf("Q27: Fibonacci with OpenMP tasks (cutoff = %d)\n", CUTOFF);
    for (int si = 0; si < nsizes; si++)
        time_table("[tasks WITH cutoff]", sizes[si], 1);
    time_table("[tasks WITHOUT cutoff] (small n only: one task per call)", n_small, 0);
    printf("\nWith no cutoff fib(%ld) creates about %ld tasks, each doing ~1 addition,\n"
           "so it is slower than the serial code even on 8 threads.\n",
           n_small, 2 * fib_serial(n_small + 1));
    return 0;
}

/* Viva notes:
 * - Why shared(x)? Local variables referenced in a task are firstprivate by
 *   default, so without shared(x) the child would write its own copy and the
 *   parent would read garbage. taskwait is needed so the children have finished
 *   (and x, y are still alive on the parent's stack) before x + y is computed.
 * - Why parallel + single? Tasks are executed by the threads of the enclosing
 *   team; single makes exactly one thread start the recursion, the others sit
 *   at single's implicit barrier and execute queued tasks.
 * - Cutoff: task creation costs ~microseconds; fib(n<20) costs less than that.
 *   A cutoff keeps the number of tasks small (~thousands) with enough work each.
 * - final(n < CUTOFF): once true, all descendant tasks become "included" (run
 *   immediately by the creating thread) -- a cutoff the runtime enforces, but
 *   the task constructs are still executed, so an explicit if/serial call is
 *   cheaper. mergeable lets an included task share the parent's data env.
 * - if(n >= CUTOFF) clause: the task is still created but runs undeferred
 *   (immediately) when false; similar effect, also still some overhead.
 * - Recursive fib is a load-imbalanced tree (left subtree is bigger); tasks
 *   handle that via dynamic work distribution, a parallel for cannot.
 */

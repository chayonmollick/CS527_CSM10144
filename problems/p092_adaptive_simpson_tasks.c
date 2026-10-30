/* P092: Adaptive Simpson quadrature with tasks
 * Problem: Compute I = integral from 0 to 1 of f(x) = sqrt(x) + 1/(1 + 1000(x - 0.3)^2)
 *   with adaptive Simpson quadrature to a tolerance eps: apply Simpson's rule on
 *   [a,b] and on its two halves; if the two estimates differ by more than
 *   15*eps, split the interval and recurse on each half with eps/2 (so the
 *   method refines only near the sharp peak at x = 0.3 and the sqrt corner at
 *   x = 0). Parallelise the recursion with OpenMP tasks (left and right halves
 *   become tasks above a depth cutoff). Compare with the exact value
 *   2/3 + (atan(0.7 sqrt(1000)) + atan(0.3 sqrt(1000))) / sqrt(1000) and with the
 *   serial result, and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: the two halves of an interval are independent sub-problems (each
 *   gets its endpoints, f values and eps by value), so each half can be a task.
 *   parallel + single starts the recursion; below depth TASK_DEPTH the
 *   recursion is serial (cutoff). Two ways to combine the results:
 *   (a) each task returns its value into a local of the parent (shared(l),
 *       shared(r)); the parent does taskwait and returns l + r. Same tree and
 *       same order of additions as the serial code, so the result is identical.
 *   (b) no waiting inside the recursion: an accepted piece is added into one
 *       shared total with #pragma omp atomic; the barrier at the end of the
 *       parallel region waits for all tasks. Balances better with gcc (see
 *       Notes); the order of additions varies, so compare with a tolerance.
 * Source: recursive divide-and-conquer with tasks (lecture slides on tasks).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p092_adaptive_simpson_tasks.c -o p092 -lm
 * Run:   ./p092 [eps] [W]   (defaults eps = 1e-10, W = 10000: f costs about W sin())
 */
#include "common.h"

#define MAX_DEPTH 60    /* never split deeper than this (stops runaway recursion) */
#define TASK_DEPTH 20   /* depths 0..TASK_DEPTH-1 create tasks; deeper is serial */

static long W = 10000;  /* simulated cost of one f evaluation (0 = plain f) */

/* The integrand: a sqrt corner at x = 0 and a sharp peak at x = 0.3.
 * The loop only simulates an expensive f (as in real quadrature problems,
 * where f is costly); 0.0 * busy adds exactly nothing to the value. */
static double f(double x) {
    double busy = 0.0;
    for (long k = 1; k <= W; k++) busy += sin(k * x);
    return sqrt(x) + 1.0 / (1.0 + 1000.0 * (x - 0.3) * (x - 0.3)) + 0.0 * busy;
}

static double exact_value(void) {
    double s = sqrt(1000.0);
    return 2.0 / 3.0 + (atan(0.7 * s) + atan(0.3 * s)) / s;
}

/* Simpson's rule on [a,b] with fa = f(a), fm = f(mid), fb = f(b). */
static double simpson(double a, double b, double fa, double fm, double fb) {
    return (b - a) / 6.0 * (fa + 4.0 * fm + fb);
}

/* Serial adaptive Simpson on [a,b]; whole = Simpson estimate on [a,b]. */
static double asr_serial(double a, double b, double fa, double fm, double fb,
                         double whole, double eps, int depth) {
    double m = 0.5 * (a + b), lm = 0.5 * (a + m), rm = 0.5 * (m + b);
    double flm = f(lm), frm = f(rm);
    double left = simpson(a, m, fa, flm, fm), right = simpson(m, b, fm, frm, fb);
    double delta = left + right - whole;
    if (depth >= MAX_DEPTH || fabs(delta) <= 15.0 * eps)
        return left + right + delta / 15.0;           /* accept (Richardson) */
    return asr_serial(a, m, fa, flm, fm, left, eps / 2, depth + 1)
         + asr_serial(m, b, fm, frm, fb, right, eps / 2, depth + 1);
}

/* (a) Same recursion; the halves are tasks, the parent waits and adds. */
static double asr_task(double a, double b, double fa, double fm, double fb,
                       double whole, double eps, int depth) {
    if (depth >= TASK_DEPTH)                           /* cutoff: serial below */
        return asr_serial(a, b, fa, fm, fb, whole, eps, depth);
    double m = 0.5 * (a + b), lm = 0.5 * (a + m), rm = 0.5 * (m + b);
    double flm = f(lm), frm = f(rm);
    double left = simpson(a, m, fa, flm, fm), right = simpson(m, b, fm, frm, fb);
    double delta = left + right - whole;
    if (depth >= MAX_DEPTH || fabs(delta) <= 15.0 * eps)
        return left + right + delta / 15.0;
    double l = 0.0, r = 0.0;
    #pragma omp task shared(l)
    l = asr_task(a, m, fa, flm, fm, left, eps / 2, depth + 1);
    #pragma omp task shared(r)
    r = asr_task(m, b, fm, frm, fb, right, eps / 2, depth + 1);
    #pragma omp taskwait                               /* l and r are ready */
    return l + r;
}

/* (b) Same recursion; accepted pieces are added to *total atomically. */
static void asr_atomic(double a, double b, double fa, double fm, double fb,
                       double whole, double eps, int depth, double *total) {
    double v;
    if (depth >= TASK_DEPTH) {                         /* cutoff: serial below */
        v = asr_serial(a, b, fa, fm, fb, whole, eps, depth);
    } else {
        double m = 0.5 * (a + b), lm = 0.5 * (a + m), rm = 0.5 * (m + b);
        double flm = f(lm), frm = f(rm);
        double left = simpson(a, m, fa, flm, fm), right = simpson(m, b, fm, frm, fb);
        double delta = left + right - whole;
        if (depth < MAX_DEPTH && fabs(delta) > 15.0 * eps) {   /* split: 2 tasks */
            #pragma omp task
            asr_atomic(a, m, fa, flm, fm, left, eps / 2, depth + 1, total);
            #pragma omp task
            asr_atomic(m, b, fm, frm, fb, right, eps / 2, depth + 1, total);
            return;                                    /* no wait needed */
        }
        v = left + right + delta / 15.0;
    }
    #pragma omp atomic
    *total += v;
}

static double integrate_serial(double a, double b, double eps) {
    double fa = f(a), fm = f(0.5 * (a + b)), fb = f(b);
    return asr_serial(a, b, fa, fm, fb, simpson(a, b, fa, fm, fb), eps, 0);
}

static double integrate_tasks(double a, double b, double eps) {
    double fa = f(a), fm = f(0.5 * (a + b)), fb = f(b), result = 0.0;
    #pragma omp parallel
    #pragma omp single
    result = asr_task(a, b, fa, fm, fb, simpson(a, b, fa, fm, fb), eps, 0);
    return result;
}

static double integrate_atomic(double a, double b, double eps) {
    double fa = f(a), fm = f(0.5 * (a + b)), fb = f(b), total = 0.0;
    #pragma omp parallel
    #pragma omp single
    asr_atomic(a, b, fa, fm, fb, simpson(a, b, fa, fm, fb), eps, 0, &total);
    /* the barrier at the end of the parallel region waits for every task */
    return total;
}

int main(int argc, char *argv[]) {
    double eps = arg_double(argc, argv, 1, 1e-10);
    W = arg_long(argc, argv, 2, 10000);
    omp_warmup();

    double exact = exact_value(), rs = 0, rp = 0, ts, tp;
    BEST_TIME(ts, rs = integrate_serial(0.0, 1.0, eps));
    printf("P092: adaptive Simpson on [0,1], eps = %g, W = %ld, task cutoff depth %d\n",
           eps, W, TASK_DEPTH);
    printf("exact = %.15f  serial = %.15f  |error| = %.2e  %s\n", exact, rs,
           fabs(rs - exact), ok_str(fabs(rs - exact) <= 100.0 * eps + 1e-12));

    printf("(a) tasks return l and r, taskwait, l + r   (N column = cutoff depth)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = integrate_tasks(0.0, 1.0, eps));
        table_row(TASK_DEPTH, THREADS[k], ts, tp, close1(rs, rp, 1e-12));
    }
    printf("    value %.15f\n", rp);
    printf("(b) tasks add accepted pieces with atomic, no taskwait\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = integrate_atomic(0.0, 1.0, eps));
        table_row(TASK_DEPTH, THREADS[k], ts, tp, close1(rs, rp, 1e-12));
    }
    printf("    value %.15f\n", rp);
    return 0;
}

/* Notes:
 * - Why tasks and not parallel for: the intervals are only known while
 *   recursing (they crowd around x = 0.3 and x = 0), so work is created on the
 *   fly and the runtime hands tasks to idle threads.
 * - shared(l), shared(r): locals of the parent are firstprivate in a task by
 *   default, so the child would write a copy; taskwait makes sure l and r are
 *   written before the parent reads them (and before its stack frame is gone).
 * - (a) scales poorly with gcc: a thread waiting in taskwait only runs its OWN
 *   child tasks, so in a lopsided tree threads sit waiting. (b) has no nested
 *   waits: idle threads at the final barrier run any task. The price is an
 *   atomic per piece and an addition order that changes from run to run.
 * - Cutoff: tasks only above depth TASK_DEPTH, so tiny intervals do not pay
 *   task overhead. With a cheap f (W = 0) the whole job takes microseconds and
 *   no version can speed it up: tasks pay off only when f is expensive.
 * - eps is halved on each split so the total error stays about eps; delta/15 is
 *   the Richardson correction (Simpson's error falls 16x when h halves).
 */

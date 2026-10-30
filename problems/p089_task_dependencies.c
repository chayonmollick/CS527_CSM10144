/* P089: Task dependencies with depend(in/out) (OpenMP 4.0)
 * Problem: (Textbook 24.7.1.) (a) Four steps on arrays form a small DAG:
 *   A: a[i] = 0.001*i;   B: b[i] = sin(a[i]) (needs A);   C: c[i] = cos(a[i])
 *   (needs A);   D: d[i] = b[i]^2 + c[i]^2 (needs B and C; so d[i] = 1).
 *   Run each step as an OpenMP task and express the order with
 *   "depend(in: ...) depend(out: ...)" so that B and C can run at the same
 *   time; compare with taskwait. (b) Fill a Fibonacci array f[0..n-1] with
 *   one task per entry, task i computing f[i] = f[i-1] + f[i-2]. Show that
 *   WITHOUT depend clauses the tasks run in any order and the result is
 *   wrong, and that WITH depend it is always correct. Verify against serial.
 * Solution: a task with depend(out: x) must finish before any later-created
 *   task with depend(in: x) or depend(out: x) starts; tasks with only
 *   depend(in: x) on the same x may run together. So
 *     A: out(a)   B: in(a) out(b)   C: in(a) out(c)   D: in(b, c) out(d)
 *   gives exactly the DAG A -> {B, C} -> D. The dependence items here are the
 *   array sections a[0:n], b[0:n], ... (always the same section per array).
 *   Fibonacci: task i has depend(in: f[i-1], f[i-2]) depend(out: f[i]), a
 *   chain. Without depend, a task may read f[i-1] before it is written; to
 *   show this SAFELY (no data race = no undefined behaviour) that version
 *   reads and writes f with "omp atomic read/write" and starts from f = -1,
 *   so a too-early task simply uses a wrong value and the error is visible.
 *   Tasks are created by one thread (single); the barrier at the end of the
 *   parallel region waits for all of them.
 * Source: textbook 24.7.1 (task dependencies); lab extension.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p089_task_dependencies.c -o p089 -lm
 * Run:   ./p089 [N] [F]      (defaults N = 2e6 array length, F = 90 Fibonacci entries)
 */
#include "common.h"

/* ---------- (a) the DAG A -> {B, C} -> D ---------- */

static void step_A(double *a, long n) {
    for (long i = 0; i < n; i++) a[i] = 0.001 * (double)i;
}
static void step_B(const double *a, double *b, long n) {
    for (long i = 0; i < n; i++) b[i] = sin(a[i]);
}
static void step_C(const double *a, double *c, long n) {
    for (long i = 0; i < n; i++) c[i] = cos(a[i]);
}
static void step_D(const double *b, const double *c, double *d, long n) {
    for (long i = 0; i < n; i++) d[i] = b[i] * b[i] + c[i] * c[i];
}

static void dag_serial(double *a, double *b, double *c, double *d, long n) {
    step_A(a, n); step_B(a, b, n); step_C(a, c, n); step_D(b, c, d, n);
}

/* start[s] / stop[s] = global order numbers when step s (0=A..3=D) began / ended. */
static void dag_depend(double *a, double *b, double *c, double *d, long n, int *start, int *stop) {
    int clock = 0;
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp task depend(out: a[0:n]) shared(clock)
        {
            #pragma omp atomic capture
            start[0] = ++clock;
            step_A(a, n);
            #pragma omp atomic capture
            stop[0] = ++clock;
        }
        #pragma omp task depend(in: a[0:n]) depend(out: b[0:n]) shared(clock)
        {
            #pragma omp atomic capture
            start[1] = ++clock;
            step_B(a, b, n);
            #pragma omp atomic capture
            stop[1] = ++clock;
        }
        #pragma omp task depend(in: a[0:n]) depend(out: c[0:n]) shared(clock)
        {
            #pragma omp atomic capture
            start[2] = ++clock;
            step_C(a, c, n);
            #pragma omp atomic capture
            stop[2] = ++clock;
        }
        #pragma omp task depend(in: b[0:n], c[0:n]) depend(out: d[0:n]) shared(clock)
        {
            #pragma omp atomic capture
            start[3] = ++clock;
            step_D(b, c, d, n);
            #pragma omp atomic capture
            stop[3] = ++clock;
        }
    }   /* implicit barrier: all four tasks done */
}

/* Same DAG with taskwait: wait for A, then run B and C, wait, then D. */
static void dag_taskwait(double *a, double *b, double *c, double *d, long n) {
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp task
        step_A(a, n);
        #pragma omp taskwait
        #pragma omp task
        step_B(a, b, n);
        #pragma omp task
        step_C(a, c, n);
        #pragma omp taskwait
        step_D(b, c, d, n);
    }
}

/* ---------- (b) Fibonacci chain ---------- */

static void fib_serial(long *f, long n) {
    f[0] = 0;
    if (n > 1) f[1] = 1;
    for (long i = 2; i < n; i++) f[i] = f[i - 1] + f[i - 2];
}

static void fib_depend(long *f, long n) {
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp task depend(out: f[0])
        f[0] = 0;
        if (n > 1) {
            #pragma omp task depend(out: f[1])
            f[1] = 1;
        }
        for (long i = 2; i < n; i++) {
            #pragma omp task depend(in: f[i - 1], f[i - 2]) depend(out: f[i]) firstprivate(i)
            f[i] = f[i - 1] + f[i - 2];
        }
    }
}

/* A little work so that tasks overlap in time (makes ordering problems visible). */
static long busy(long i) {
    volatile long s = 0;
    for (long k = 0; k < 2000 + 300 * (i % 7); k++) s += k;
    return s;
}

/* WITHOUT depend: tasks may run in any order. Atomic reads/writes keep this
 * free of data races; f[i] = -1 means "not computed yet". */
static void fib_nodepend(long *f, long n) {
    f[0] = 0;
    if (n > 1) f[1] = 1;
    for (long i = 2; i < n; i++) f[i] = -1;
    #pragma omp parallel
    #pragma omp single
    for (long i = 2; i < n; i++) {
        #pragma omp task firstprivate(i)
        {
            long x, y;
            #pragma omp atomic read
            x = f[i - 1];
            #pragma omp atomic read
            y = f[i - 2];
            (void)busy(i);
            #pragma omp atomic write
            f[i] = x + y;
        }
    }
}

static long count_wrong(const long *x, const long *y, long n) {
    long w = 0;
    for (long i = 0; i < n; i++) w += (x[i] != y[i]);
    return w;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 2000000);
    long F = arg_long(argc, argv, 2, 90);
    if (n < 1) n = 1;
    if (F < 1) F = 1;
    if (F > 92) F = 92;                           /* fib(92) is the last one in 64 bits */

    double *a = xmalloc((size_t)n * sizeof(double)), *b = xmalloc((size_t)n * sizeof(double));
    double *c = xmalloc((size_t)n * sizeof(double)), *ds = xmalloc((size_t)n * sizeof(double));
    double *dp = xmalloc((size_t)n * sizeof(double));
    int start[4], stop[4];
    omp_warmup();

    printf("P089: task dependencies\n(a) DAG  A -> {B, C} -> D  on arrays of %ld doubles\n", n);
    double ts, tp;
    BEST_TIME(ts, dag_serial(a, b, c, ds, n));
    double err = 0.0;
    for (long i = 0; i < n; i++) if (fabs(ds[i] - 1.0) > err) err = fabs(ds[i] - 1.0);
    printf("serial: max |d[i] - 1| = %.1e (sin^2 + cos^2 = 1)  %s\n", err, ok_str(err < 1e-12));

    omp_set_num_threads(4);
    dag_depend(a, b, c, dp, n, start, stop);
    const char *nm = "ABCD";
    printf("one run with 4 threads, order numbers (start-stop):");
    for (int s = 0; s < 4; s++) printf("  %c %d-%d", nm[s], start[s], stop[s]);
    int order_ok = start[1] > stop[0] && start[2] > stop[0] && start[3] > stop[1] && start[3] > stop[2];
    int overlap = start[1] < stop[2] && start[2] < stop[1];
    printf("\nA before B and C, D after both: %s;  B and C overlapped in time: %s\n",
           ok_str(order_ok), overlap ? "yes" : "no (possible, e.g. on a busy machine)");

    printf("\nDAG with depend (check: d equal to serial, order respected)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(dp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, dag_depend(a, b, c, dp, n, start, stop));
        int ok = start[1] > stop[0] && start[2] > stop[0] && start[3] > stop[1] && start[3] > stop[2];
        table_row(n, THREADS[k], ts, tp, ok && close_doubles(ds, dp, n, 0.0));
    }
    printf("DAG with taskwait instead of depend (check: d equal to serial)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        memset(dp, 0, (size_t)n * sizeof(double));
        BEST_TIME(tp, dag_taskwait(a, b, c, dp, n));
        table_row(n, THREADS[k], ts, tp, close_doubles(ds, dp, n, 0.0));
    }
    free(a); free(b); free(c); free(ds); free(dp);

    long fs[92], fd[92], fn[92];
    fib_serial(fs, F);
    printf("\n(b) Fibonacci f[0..%ld] with one task per entry (serial f[%ld] = %ld)\n",
           F - 1, F - 1, fs[F - 1]);
    printf("%8s %26s %34s\n", "threads", "WITH depend", "WITHOUT depend (atomic, safe)");
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        int ok = 1;
        for (int rep = 0; rep < 20; rep++) {      /* run 20 times: must be right every time */
            for (long i = 0; i < F; i++) fd[i] = -1;
            fib_depend(fd, F);
            if (count_wrong(fs, fd, F) != 0) ok = 0;
        }
        fib_nodepend(fn, F);
        long wrong = count_wrong(fs, fn, F);
        printf("%8d %26s %22ld wrong entries\n", THREADS[k], ok ? "OK (20 of 20 runs)" : "WRONG", wrong);
    }
    printf("(without depend the count varies from run to run; it can even be 0 by luck)\n");
    return 0;
}

/* Notes:
 * - depend(out: x): later tasks that use x (in or out) wait for this one.
 *   depend(in: x): waits for earlier out(x) tasks; several in(x) tasks may run
 *   concurrently. Only sibling tasks (same parent) are ordered this way.
 * - Dependences are matched by the ADDRESS of the list item: use the same
 *   item (e.g. a[0:n] or a[0]) for the same data in every task.
 * - taskwait waits for ALL child tasks so far: simpler but coarser (here the
 *   same DAG; in general it waits for unrelated tasks too).
 * - Without depend (or taskwait) the order of tasks is unspecified; reading
 *   f[i-1] while another task writes it is a data race (undefined behaviour) -
 *   the demo uses atomic read/write to stay well-defined.
 * - Critical path: A + max(B, C) + D, so the DAG speedup is at most
 *   (A+B+C+D) / (A + max(B,C) + D) ~ 2 here, whatever the thread count.
 *   Which thread runs a ready task, and when, is up to the runtime: with
 *   gcc's libgomp and 2 threads the depend version often runs B and C one
 *   after the other (the idle thread is not woken when they become ready);
 *   taskwait creates B and C after A, which wakes it. Results stay correct.
 */

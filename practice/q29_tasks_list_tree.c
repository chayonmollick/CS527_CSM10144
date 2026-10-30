/* CS527 Computer Systems — OpenMP practice
 * Q29: Use OpenMP tasks for irregular parallelism: (a) process every node of
 *      a linked list in parallel, (b) sum the values in a binary tree,
 *      (c) parallelise a loop with taskloop, (d) wait for nested tasks with
 *      taskgroup. Verify each against the serial result.
 *  - (a) list: one producer thread walks the list, one task per node (firstprivate(p));
 *        alternative: copy node pointers to an array, then parallel for
 *  - (b) tree sum: task per subtree with shared(l), shared(r) + taskwait, depth cutoff
 *  - (c) taskloop grainsize(g) over an array (OpenMP 4.5, gcc >= 6), effect of g
 *  - (d) taskgroup waits for ALL descendant tasks (taskwait: only direct children)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WORK        2000   /* sin() calls per list node in (a)          */
#define DEPTH_CUTOFF 10    /* (b) tasks only for the top 10 tree levels  */

static unsigned long long rng = 42;            /* deterministic, serial only */
static unsigned next_rand(void) {
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng >> 33);
}

static void header(void) {
    printf("%8s %12s %12s %9s %11s %7s\n",
           "threads", "serial(s)", "parallel(s)", "speedup", "efficiency", "check");
}
static void row(int t, double ts, double tp, int ok) {
    double sp = tp > 0 ? ts / tp : 0.0;
    printf("%8d %12.6f %12.6f %9.2f %11.2f %7s\n",
           t, ts, tp, sp, sp / t, ok ? "OK" : "WRONG");
}

/* ------------------------- (a) linked list ------------------------- */
typedef struct node { long value; double result; struct node *next; } node_t;

static double work(long v) {                  /* some per-node work */
    double s = 0.0;
    for (int k = 1; k <= WORK; k++) s += sin((double)v * 1e-3 + k);
    return s;
}

static node_t *list_build(long n) {
    node_t *head = NULL;
    for (long i = 0; i < n; i++) {
        node_t *p = malloc(sizeof(node_t));
        if (!p) { fprintf(stderr, "alloc failed\n"); exit(1); }
        p->value = next_rand() % 100000; p->result = 0.0; p->next = head;
        head = p;
    }
    return head;
}
static void list_serial(node_t *head) {
    for (node_t *p = head; p; p = p->next) p->result = work(p->value);
}
/* The while loop itself cannot be a "parallel for" (unknown trip count, no
 * index), so one thread walks it and hands each node to a task. */
static void list_tasks(node_t *head) {
    #pragma omp parallel
    #pragma omp single
    {
        node_t *p = head;
        while (p) {
            #pragma omp task firstprivate(p)   /* capture the CURRENT node */
            p->result = work(p->value);
            p = p->next;
        }
    }   /* implicit barrier: all tasks done */
}
/* Alternative: count + copy pointers to an array, then a normal parallel for. */
static void list_array(node_t *head, long n) {
    node_t **arr = malloc((size_t)n * sizeof(node_t *));
    if (!arr) { fprintf(stderr, "alloc failed\n"); exit(1); }
    long k = 0;
    for (node_t *p = head; p; p = p->next) arr[k++] = p;
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) arr[i]->result = work(arr[i]->value);
    free(arr);
}
static int list_check(node_t *head, const double *ref) {
    long i = 0;
    for (node_t *p = head; p; p = p->next, i++) if (p->result != ref[i]) return 0;
    return 1;
}
static void list_clear(node_t *head) { for (node_t *p = head; p; p = p->next) p->result = -1; }

/* ------------------------- (b) binary tree ------------------------- */
typedef struct tnode { long val; struct tnode *left, *right; } tnode_t;

/* balanced tree over pool[lo..hi] (nodes come from one array) */
static tnode_t *tree_build(tnode_t *pool, long lo, long hi) {
    if (lo > hi) return NULL;
    long mid = lo + (hi - lo) / 2;
    pool[mid].left  = tree_build(pool, lo, mid - 1);
    pool[mid].right = tree_build(pool, mid + 1, hi);
    return &pool[mid];
}
static long tree_sum_serial(const tnode_t *t) {
    if (!t) return 0;
    return t->val + tree_sum_serial(t->left) + tree_sum_serial(t->right);
}
static long tree_sum_task(const tnode_t *t, int depth) {
    if (!t) return 0;
    if (depth >= DEPTH_CUTOFF) return tree_sum_serial(t);   /* cutoff */
    long l, r;
    #pragma omp task shared(l)
    l = tree_sum_task(t->left, depth + 1);
    #pragma omp task shared(r)
    r = tree_sum_task(t->right, depth + 1);
    #pragma omp taskwait                     /* children done -> l, r valid */
    return t->val + l + r;
}
static long tree_sum_parallel(const tnode_t *root) {
    long s = 0;
    #pragma omp parallel
    #pragma omp single
    s = tree_sum_task(root, 0);
    return s;
}

/* ------------------------- (c) taskloop ---------------------------- */
static double f(double x) { return sqrt(x) * sin(x); }

static void loop_serial(const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = f(x[i]);
}
/* taskloop splits the iterations into tasks of ~grain iterations each; it is
 * called by ONE thread (single), the team executes the tasks. */
static void loop_taskloop(const double *x, double *y, long n, long grain) {
    #pragma omp parallel
    #pragma omp single
    #pragma omp taskloop grainsize(grain)
    for (long i = 0; i < n; i++) y[i] = f(x[i]);
}
static int same_array(const double *a, const double *b, long n) {
    for (long i = 0; i < n; i++) if (fabs(a[i] - b[i]) > 1e-12) return 0;
    return 1;
}

/* ------------------------- (d) taskgroup --------------------------- */
/* Creates 2^depth leaf tasks; it does NOT wait for its children itself. */
static void spawn(long *count, int depth) {
    if (depth == 0) {
        volatile double w = work(depth);      /* a little work per leaf */
        (void)w;
        #pragma omp atomic
        (*count)++;
        return;
    }
    #pragma omp task
    spawn(count, depth - 1);
    #pragma omp task
    spawn(count, depth - 1);
}

int main(int argc, char **argv) {
    long nlist = 20000L, ntree = 1L << 22, nloop = 10000000L;
    if (argc > 1) nlist = ntree = nloop = atol(argv[1]);
    if (nlist < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    const int threads[] = {1, 2, 4, 8};
    printf("Q29: Tasks for lists, trees, taskloop, taskgroup\n");

    /* (a) ------------------------------------------------------------ */
    node_t *head = list_build(nlist);
    double *ref = malloc((size_t)nlist * sizeof(double));
    if (!ref) { fprintf(stderr, "alloc failed\n"); return 1; }
    list_serial(head);                        /* warm-up (page faults etc.) */
    double t0 = omp_get_wtime(); list_serial(head); double ts = omp_get_wtime() - t0;
    { long i = 0; for (node_t *p = head; p; p = p->next) ref[i++] = p->result; }

    printf("\n(a) linked list, %ld nodes: one task per node (single producer)\n", nlist);
    header();
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]); list_clear(head);
        double p0 = omp_get_wtime(); list_tasks(head); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, list_check(head, ref));
    }
    printf("(a2) same list: pointers copied to an array + parallel for\n");
    header();
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]); list_clear(head);
        double p0 = omp_get_wtime(); list_array(head, nlist); double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, list_check(head, ref));
    }
    while (head) { node_t *nx = head->next; free(head); head = nx; }
    free(ref);

    /* (b) ------------------------------------------------------------ */
    tnode_t *pool = malloc((size_t)ntree * sizeof(tnode_t));
    if (!pool) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < ntree; i++) pool[i].val = next_rand() % 1000;
    tnode_t *root = tree_build(pool, 0, ntree - 1);
    t0 = omp_get_wtime(); long sref = tree_sum_serial(root); ts = omp_get_wtime() - t0;

    printf("\n(b) binary tree sum, %ld nodes, tasks for depth < %d: sum = %ld\n",
           ntree, DEPTH_CUTOFF, sref);
    header();
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime(); long s = tree_sum_parallel(root);
        double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, s == sref);
    }
    free(pool);

    /* (c) ------------------------------------------------------------ */
    double *x  = malloc((size_t)nloop * sizeof(double));
    double *ys = malloc((size_t)nloop * sizeof(double));
    double *yp = malloc((size_t)nloop * sizeof(double));
    if (!x || !ys || !yp) { fprintf(stderr, "alloc failed\n"); return 1; }
    for (long i = 0; i < nloop; i++) x[i] = (double)(next_rand() % 10000);
    t0 = omp_get_wtime(); loop_serial(x, ys, nloop); ts = omp_get_wtime() - t0;

    long grain = (nloop + 99) / 100;          /* ~100 tasks (see note below) */
    printf("\n(c) taskloop grainsize(%ld) (~100 tasks), y[i] = sqrt(x)*sin(x), N = %ld\n",
           grain, nloop);
    header();
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        double p0 = omp_get_wtime(); loop_taskloop(x, yp, nloop, grain);
        double tp = omp_get_wtime() - p0;
        row(threads[ti], ts, tp, same_array(ys, yp, nloop));
    }
    printf("grainsize effect at 8 threads (gcc runs the taskloop serially if it\n"
           "would create more than 64*threads = 512 tasks):\n");
    printf("%10s %8s %12s %9s %7s\n", "grainsize", "tasks", "time(s)", "speedup", "check");
    omp_set_num_threads(8);
    const long ntasks[] = {10000, 1000, 500, 100, 8};
    for (int gi = 0; gi < 5; gi++) {
        long g = (nloop + ntasks[gi] - 1) / ntasks[gi];
        double p0 = omp_get_wtime(); loop_taskloop(x, yp, nloop, g);
        double tp = omp_get_wtime() - p0;
        printf("%10ld %8ld %12.6f %9.2f %7s\n", g, (nloop + g - 1) / g, tp,
               tp > 0 ? ts / tp : 0.0, same_array(ys, yp, nloop) ? "OK" : "WRONG");
    }
    free(x); free(ys); free(yp);

    /* (d) ------------------------------------------------------------ */
    const int depth = 12;
    long count1 = 0, count2 = 0, seen_taskwait = 0, seen_taskgroup = 0;
    omp_set_num_threads(8);
    #pragma omp parallel
    #pragma omp single
    {
        spawn(&count1, depth);
        #pragma omp taskwait          /* waits only for spawn()'s 2 children */
        #pragma omp atomic read
        seen_taskwait = count1;       /* grandchildren may still be running */

        #pragma omp taskgroup
        {
            spawn(&count2, depth);
        }                             /* waits for ALL descendants */
        #pragma omp atomic read
        seen_taskgroup = count2;
    }   /* implicit barrier: every task (incl. count1's) is finished here */
    printf("\n(d) %d leaf tasks spawned recursively (no taskwait inside):\n", 1 << depth);
    printf("    count after taskwait  = %ld (children only; usually < %d)\n",
           seen_taskwait, 1 << depth);
    printf("    count after taskgroup = %ld  %s\n", seen_taskgroup,
           seen_taskgroup == (1L << depth) ? "OK" : "WRONG");
    printf("    taskwait run's count after the parallel region = %ld  %s\n", count1,
           count1 == (1L << depth) ? "OK" : "WRONG");
    return 0;
}

/* Viva notes:
 * - Linked list: a while loop has no iteration count, so "parallel for" is
 *   impossible. One thread (single) walks the list and creates a task per node;
 *   firstprivate(p) captures the current pointer value at task creation (it is
 *   the default for p anyway, but write it -- it is the point of the example).
 *   Alternative: copy pointers to an array (O(n) serial pass) + parallel for.
 * - Tree sum: shared(l), shared(r) so the children write the parent's
 *   variables; taskwait before l + r. Depth cutoff (2^10 tasks) avoids one
 *   task per node, which would be all overhead.
 * - taskloop (4.5): like "for", but iterations become tasks, so it composes
 *   with other tasks and balances load. grainsize(g) = ~g iterations per task,
 *   num_tasks(k) = k tasks. Too small grain -> overhead. gcc/libgomp gotcha:
 *   if a taskloop would make > 64*nthreads tasks it runs them all serially in
 *   the encountering thread (same throttle makes a "task" run immediately when
 *   > 64*nthreads tasks are queued) -- keep the task count moderate.
 * - taskwait waits for the CHILD tasks of the current task only; taskgroup
 *   waits for the children AND all their descendants (needed in (d) because
 *   spawn() returns without waiting). The implicit barrier at the end of a
 *   parallel region also waits for every task.
 * - Default data sharing in tasks: variables shared in the enclosing context
 *   stay shared; locals of the creating function are firstprivate.
 */

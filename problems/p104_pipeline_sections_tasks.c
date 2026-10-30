/* P104: Three-stage pipeline with task dependences
 * Problem: Data arrives in B blocks of S values and goes through 3 stages:
 *   stage 1 generates block b, stage 2 transforms it, stage 3 reduces it to a
 *   block sum r[b] and outputs the running total out[b] = out[b-1] + r[b].
 *   Block b's stage s needs block b's stage s-1, and each stage handles the
 *   blocks in order (stage s of block b-1 must finish before stage s of block b).
 *   (a) Run it as a pipeline with OpenMP tasks and depend clauses, so different
 *   stages of different blocks overlap. (b) Compare with running the stages one
 *   after another, each as a parallel for over the values of one block.
 *   (c) Also build the pipeline with parallel sections (one section per stage).
 *   Verify out[] against the serial code and report time and speedup.
 * Solution: (a) one thread (single) creates 3 tasks per block; the runtime starts
 *   a task when its dependences are met:
 *     stage 1: depend(inout: tok[0])                    depend(out: done1[b])
 *     stage 2: depend(inout: tok[1]) depend(in: done1[b]) depend(out: done2[b])
 *     stage 3: depend(inout: tok[2]) depend(in: done2[b])
 *   done1[b], done2[b] are per-block sentinels (stage s-1 -> stage s of block b);
 *   tok[s] is a per-stage sentinel: inout on the same item chains the stage-s
 *   tasks in creation order, i.e. block order. So while stage 3 works on block
 *   b, stage 2 can work on b+1 and stage 1 on b+2: at most 3 tasks run at
 *   once, the speedup is at most 3 (number of stages). Each block has its own
 *   buffers X[b], Y[b], so overlapping stages never touch the same data.
 *   (b) data parallelism inside each stage: per block, parallel for over the S
 *   values for stages 1 and 2, and a reduction(+:sum) for stage 3; this scales
 *   with the threads but pays a fork/join per stage per block.
 *   (c) parallel sections num_threads(3): section s runs stage s for b = 0..B-1;
 *   it waits (spins) until stage s-1 has finished block b, read from a progress
 *   counter written with atomic write after a flush (producer) and read with
 *   atomic read followed by a flush (consumer), so the block data is visible.
 * Source: lecture slides on tasks (depend clauses, OpenMP 4.0+), pipeline pattern.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p104_pipeline_sections_tasks.c -o p104 -lm
 * Run:   ./p104 [B] [S]      (defaults B = 64 blocks, S = 16384 values per block)
 */
#include "common.h"

#define WORK 4          /* sin() calls per value per stage: makes the stages cost alike */

static double work(double v) {
    for (int r = 0; r < WORK; r++) v = sin(v) + 1.0;
    return v;
}

/* Stage 1: generate block b into x (value depends on the global index). */
static void stage1(long b, long S, double *x) {
    for (long i = 0; i < S; i++) x[i] = work((double)(b * S + i) * 1e-6);
}
/* Stage 2: transform x into y. */
static void stage2(long S, const double *x, double *y) {
    for (long i = 0; i < S; i++) y[i] = work(x[i]);
}
/* Stage 3: reduce y and output the running total (needs out[b-1]). */
static void stage3(long b, long S, const double *y, double *out) {
    double sum = 0.0;
    for (long i = 0; i < S; i++) sum += work(y[i]);
    out[b] = (b > 0 ? out[b - 1] : 0.0) + sum;
}

static void pipeline_serial(long B, long S, double *X, double *Y, double *out) {
    for (long b = 0; b < B; b++) {
        stage1(b, S, X + b * S);
        stage2(S, X + b * S, Y + b * S);
        stage3(b, S, Y + b * S, out);
    }
}

/* (a) tasks with depend: per-block sentinels done1/done2, per-stage tok[]. */
static void pipeline_tasks(long B, long S, double *X, double *Y, double *out) {
    /* sentinels: only their addresses are used, as dependence labels */
    char *done1 = xmalloc((size_t)B), *done2 = xmalloc((size_t)B), *tok = xmalloc(3);
    #pragma omp parallel
    #pragma omp single
    {
        for (long b = 0; b < B; b++) {
            #pragma omp task depend(inout: tok[0]) depend(out: done1[b])
            stage1(b, S, X + b * S);
            #pragma omp task depend(inout: tok[1]) depend(in: done1[b]) depend(out: done2[b])
            stage2(S, X + b * S, Y + b * S);
            #pragma omp task depend(inout: tok[2]) depend(in: done2[b])
            stage3(b, S, Y + b * S, out);
        }
        #pragma omp taskwait    /* the creating thread also runs tasks while it waits */
    }   /* barrier at the end of single/parallel: every task has finished */
    free(done1); free(done2); free(tok);
}

/* (b) stage after stage, each stage a parallel for over one block's values. */
static void pipeline_parallel_for(long B, long S, double *X, double *Y, double *out) {
    for (long b = 0; b < B; b++) {
        double *x = X + b * S, *y = Y + b * S, sum = 0.0;
        #pragma omp parallel for schedule(static)
        for (long i = 0; i < S; i++) x[i] = work((double)(b * S + i) * 1e-6);
        #pragma omp parallel for schedule(static)
        for (long i = 0; i < S; i++) y[i] = work(x[i]);
        #pragma omp parallel for schedule(static) reduction(+:sum)
        for (long i = 0; i < S; i++) sum += work(y[i]);
        out[b] = (b > 0 ? out[b - 1] : 0.0) + sum;
    }
}

/* (c) one section per stage; doneK = number of blocks stage K has finished. */
static void pipeline_sections(long B, long S, double *X, double *Y, double *out) {
    long done1 = 0, done2 = 0;
    #pragma omp parallel sections num_threads(3)
    {
        #pragma omp section
        for (long b = 0; b < B; b++) {                 /* stage 1 */
            stage1(b, S, X + b * S);
            #pragma omp flush                          /* X[b] visible before the flag */
            #pragma omp atomic write
            done1 = b + 1;
        }
        #pragma omp section
        for (long b = 0; b < B; b++) {                 /* stage 2 */
            long ready;
            do {
                #pragma omp atomic read
                ready = done1;
            } while (ready <= b);                      /* wait for stage 1 of block b */
            #pragma omp flush                          /* now X[b] can be read */
            stage2(S, X + b * S, Y + b * S);
            #pragma omp flush
            #pragma omp atomic write
            done2 = b + 1;
        }
        #pragma omp section
        for (long b = 0; b < B; b++) {                 /* stage 3 */
            long ready;
            do {
                #pragma omp atomic read
                ready = done2;
            } while (ready <= b);
            #pragma omp flush
            stage3(b, S, Y + b * S, out);
        }
    }
}

int main(int argc, char *argv[]) {
    long B = arg_long(argc, argv, 1, 64);
    long S = arg_long(argc, argv, 2, 16384);
    if (B < 1 || S < 1) { fprintf(stderr, "B and S must be >= 1\n"); return 1; }
    double *X = xmalloc((size_t)(B * S) * sizeof(double));      /* X[b*S ..]: block b */
    double *Y = xmalloc((size_t)(B * S) * sizeof(double));
    double *os = xmalloc((size_t)B * sizeof(double)), *op = xmalloc((size_t)B * sizeof(double));
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, pipeline_serial(B, S, X, Y, os));
    printf("P104: 3-stage pipeline, B = %ld blocks of S = %ld values; out[B-1] = %.6f\n",
           B, S, os[B - 1]);
    printf("(a) tasks + depend (pipeline: at most 3 stages at once)   [N column = B*S]\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, pipeline_tasks(B, S, X, Y, op));
        table_row(B * S, THREADS[k], ts, tp, close_doubles(os, op, B, 1e-12));
    }
    printf("(b) stages one after another, parallel for inside each stage\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, pipeline_parallel_for(B, S, X, Y, op));
        table_row(B * S, THREADS[k], ts, tp, close_doubles(os, op, B, 1e-12));
    }
    BEST_TIME(tp, pipeline_sections(B, S, X, Y, op));
    printf("(c) parallel sections, 3 threads (one per stage): %.6f s, speedup %.2f, %s\n",
           tp, ts / tp, ok_str(close_doubles(os, op, B, 1e-12)));
    free(X); free(Y); free(os); free(op);
    return 0;
}

/* Notes:
 * - Pipeline speedup <= number of stages (3), and less if the stages are
 *   unbalanced (the slowest stage sets the pace); filling and draining the
 *   pipeline costs about 2 extra stage times. More threads than stages do not help.
 * - Without tok[s] (the per-stage chain), stage-1 tasks of all blocks could run
 *   at once: fine for a stateless stage, wrong for one that must see blocks in
 *   order (stage 3 here reads out[b-1]). Drop tok[s] for stages that are
 *   stateless to get more parallelism.
 * - depend items are just addresses used as labels: done1[b], tok[0] are never
 *   read or written; tasks with depend(in) on the same item may run together,
 *   depend(out/inout) waits for all earlier tasks that named that item.
 * - (b) uses all threads inside every stage (good when S is large), but has
 *   3B fork/joins and a barrier after every stage; it sums in a different order,
 *   so compare with a tolerance ((a) and (c) give the same bits as serial).
 * - The taskwait at the end of single matters with gcc: without it the thread
 *   that created the tasks sat idle at the barrier (observed: with 2 threads one
 *   thread ran every task). In taskwait it runs its ready child tasks itself.
 * - (c) hard-wires one thread per stage and busy-waits: it needs all 3 sections
 *   running at once (num_threads(3)); with fewer threads it relies on the
 *   sections being handed out in order, else it deadlocks. Tasks + depend
 *   express the same pipeline with no flags, no spinning and any thread count.
 */

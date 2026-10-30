/* P003: Nested parallel regions
 * Problem: Write a function that contains a parallel region (2 threads). Call it
 *   from serial code, and from inside another parallel region (3 threads), first
 *   with nested parallelism disabled and then enabled. In each thread print
 *   omp_get_level(), omp_get_active_level(), omp_get_ancestor_thread_num() and the
 *   team sizes, and check how many threads run at each nesting level.
 * Solution: a parallel region met inside an active region creates a new team only
 *   if nesting is allowed (max-active-levels >= 2); otherwise it runs with a team of
 *   one (inactive: the level still goes up, the ACTIVE level does not). Each thread
 *   records what it saw into its own slot rec[k] (k from an atomic capture) and
 *   adds itself to a per-level counter with atomic, so the bookkeeping is race-free.
 *   Records are printed after the regions, sorted, so the output is readable.
 * Source: textbook Ex 18.2.
 * Build: gcc -O2 -Wall -Wextra -fopenmp p003_nested_parallel.c -o p003 -lm
 * Run:   ./p003 [OUTER] [INNER]   (defaults OUTER = 3, INNER = 2 threads)
 */
#include "common.h"

#define MAXREC 4096

typedef struct { int level, active, outer_id, inner_id, inner_team; } Rec;
static Rec rec[MAXREC];
static int nrec;              /* number of records used (atomic capture) */
static int per_level[8];      /* threads seen at nesting level L (atomic) */

/* The function of the exercise: it opens its own parallel region. */
static void work(int inner) {
    #pragma omp parallel num_threads(inner)
    {
        Rec r;
        r.level      = omp_get_level();            /* parallel regions enclosing us   */
        r.active     = omp_get_active_level();     /* ... that have more than 1 thread */
        r.outer_id   = r.level >= 2 ? omp_get_ancestor_thread_num(r.level - 1) : 0;
        r.inner_id   = omp_get_thread_num();       /* number within the INNERMOST team */
        r.inner_team = omp_get_team_size(r.level); /* = omp_get_num_threads() here    */
        int k;
        #pragma omp atomic capture
        k = nrec++;
        if (k < MAXREC) rec[k] = r;
        #pragma omp atomic
        per_level[r.level]++;
    }
}

/* Nesting on/off. OpenMP 4.5 (gcc 9) needs omp_set_nested; OpenMP 5 uses
 * omp_set_max_active_levels (omp_set_nested is deprecated there). Call both. */
static void set_nesting(int on) {
    omp_set_nested(on);
    omp_set_max_active_levels(on ? 2 : 1);
}

static int cmp_rec(const void *x, const void *y) {
    const Rec *a = x, *b = y;
    if (a->outer_id != b->outer_id) return a->outer_id - b->outer_id;
    return a->inner_id - b->inner_id;
}

/* Print the records of one case and check the per-level thread counts. */
static int report(int expect1, int expect2) {
    int n = nrec < MAXREC ? nrec : MAXREC;
    qsort(rec, (size_t)n, sizeof(Rec), cmp_rec);
    for (int k = 0; k < n; k++)
        printf("  outer thread %d -> inner thread %d of %d   level=%d active_level=%d\n",
               rec[k].outer_id, rec[k].inner_id, rec[k].inner_team, rec[k].level, rec[k].active);
    int ok = (per_level[1] == expect1 && per_level[2] == expect2);
    printf("  threads per level: level 1 = %d (expect %d), level 2 = %d (expect %d)  -> %s\n",
           per_level[1], expect1, per_level[2], expect2, ok_str(ok));
    nrec = 0;
    memset(per_level, 0, sizeof per_level);
    return ok;
}

/* Outer region: every outer thread counts itself at level 1, then calls work(). */
static void outer_region(int outer, int inner) {
    #pragma omp parallel num_threads(outer)
    {
        #pragma omp atomic
        per_level[1]++;
        work(inner);
    }
}

int main(int argc, char *argv[]) {
    int outer = (int)arg_long(argc, argv, 1, 3);
    int inner = (int)arg_long(argc, argv, 2, 2);
    int ok = 1;
    if (outer < 1 || inner < 1 || outer * inner > MAXREC) { printf("bad sizes\n"); return 1; }

    omp_set_dynamic(0);           /* teams get exactly the size asked for */
    printf("P003: nested parallelism, outer team %d, inner team %d\n", outer, inner);
    printf("Defaults: omp_get_nested() = %d, omp_get_max_active_levels() = %d, "
           "omp_get_level() in serial code = %d\n\n",
           omp_get_nested(), omp_get_max_active_levels(), omp_get_level());

    printf("Case 1: work() called from serial code (one level: nesting setting irrelevant)\n");
    set_nesting(0);
    work(inner);
    ok &= report(inner, 0);

    printf("\nCase 2: nesting DISABLED (max_active_levels = 1), work() called by each outer thread\n");
    set_nesting(0);
    outer_region(outer, inner);
    /* inner regions are inactive (1 thread each); exception: a 1-thread outer region
     * is itself inactive, so the inner one is then the first ACTIVE level */
    ok &= report(outer, outer > 1 ? outer : inner);

    printf("\nCase 3: nesting ENABLED (max_active_levels = 2), work() called by each outer thread\n");
    set_nesting(1);
    outer_region(outer, inner);
    ok &= report(outer, outer * inner);    /* every outer thread forks a team of `inner` */

    printf("\nCase 3 runs %d x %d = %d threads at level 2: each outer thread is thread 0\n"
           "(the master) of its own inner team, so only %d new threads are created.\n",
           outer, inner, outer * inner, outer * (inner - 1));
    printf("Overall check: %s\n", ok_str(ok));
    return 0;
}

/* Notes:
 * - Nesting disabled: an inner parallel region inside an active one gets a team of
 *   ONE thread. omp_get_level() still counts it (2), omp_get_active_level() does not (1).
 * - omp_get_thread_num() is the number in the innermost team (0..inner-1), so it is
 *   not unique across the program; use omp_get_ancestor_thread_num(level) to know
 *   which outer thread you belong to (unique id = outer_id * inner + inner_id).
 * - Each outer thread becomes thread 0 (the master) of its own inner team, so case 3
 *   uses outer * inner threads in total, not outer + outer * inner.
 * - Enable with OMP_MAX_ACTIVE_LEVELS=2 / omp_set_max_active_levels(2) (OpenMP 5) or
 *   OMP_NESTED=true / omp_set_nested(1) (OpenMP <= 4.5); gcc 9 needs the latter.
 * - Nesting oversubscribes quickly (outer * inner threads): useful when the outer loop
 *   has too few iterations to keep all cores busy, otherwise prefer one flat level.
 */

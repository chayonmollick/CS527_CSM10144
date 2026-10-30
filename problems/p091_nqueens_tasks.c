/* P091: N-Queens solution count (parallel for and tasks)
 * Problem: Count all the ways to place N queens on an N x N chess board so that
 *   no two queens attack each other (same column or same diagonal), for
 *   N = 8..13. Use backtracking with bitmasks. Parallelise it with OpenMP in two
 *   ways: (a) a parallel for over the columns of the first-row queen, (b) tasks,
 *   one per placement of the queens in the first two rows. Verify against the
 *   known counts (8 -> 92, 10 -> 724, 12 -> 14200, 13 -> 73712) and the serial
 *   result, and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: the search tree below different first-row (or first-two-row)
 *   placements are independent subtrees, so they can be searched in parallel;
 *   each subtree only reads its own copies of the masks (passed by value), so
 *   there is nothing shared except the count. (a) parallel for over the first
 *   column with reduction(+:count); subtrees have very different sizes, so
 *   schedule(dynamic,1) hands out one column at a time. (b) parallel + single
 *   starts the recursion; while row < TASK_ROWS every safe column becomes a
 *   task that writes its count into its own slot sub[c]; taskwait, then add the
 *   slots. Below the cutoff the search is plain serial recursion.
 * Source: classic backtracking exercise (task parallelism, irregular trees).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p091_nqueens_tasks.c -o p091 -lm
 * Run:   ./p091 [N]          (default N = 13, 1 <= N <= 20)
 */
#include "common.h"

#define TASK_ROWS 2     /* rows 0..TASK_ROWS-1 spawn tasks; deeper rows are serial */
#define MAXN 20

/* Known solution counts, known[n] for n = 0..16 (0 = not used). */
static const long known[17] = {0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680,
                               14200, 73712, 365596, 2279184, 14772512};

/* Bitmask backtracking. Bit c of cols = column c is taken; d1 / d2 = squares
 * of the current row attacked along the two diagonals. Masks are passed by
 * value, so every call (and every task) has its own copy. */
static long nq_serial(int n, unsigned cols, unsigned d1, unsigned d2) {
    unsigned all = (1u << n) - 1;
    if (cols == all) return 1;                     /* a queen in every row */
    long count = 0;
    unsigned avail = all & ~(cols | d1 | d2);      /* safe squares in this row */
    while (avail) {
        unsigned bit = avail & (~avail + 1);       /* lowest safe square */
        avail ^= bit;
        count += nq_serial(n, cols | bit, ((d1 | bit) << 1) & all, (d2 | bit) >> 1);
    }
    return count;
}

/* (a) parallel for over the column of the queen in row 0. */
static long nq_parallel_for(int n) {
    long count = 0;
    #pragma omp parallel for reduction(+:count) schedule(dynamic, 1)
    for (int c = 0; c < n; c++) {
        unsigned bit = 1u << c, all = (1u << n) - 1;
        count += nq_serial(n, bit, (bit << 1) & all, bit >> 1);
    }
    return count;
}

/* (b) tasks: one task per safe square while row < TASK_ROWS. */
static long nq_task(int n, int row, unsigned cols, unsigned d1, unsigned d2) {
    if (row >= TASK_ROWS) return nq_serial(n, cols, d1, d2);   /* cutoff */
    unsigned all = (1u << n) - 1;
    if (cols == all) return 1;
    long sub[MAXN] = {0};                          /* sub[c] = count below column c */
    for (int c = 0; c < n; c++) {
        unsigned bit = 1u << c;
        if ((cols | d1 | d2) & bit) continue;      /* square attacked */
        #pragma omp task shared(sub) firstprivate(c, bit)
        sub[c] = nq_task(n, row + 1, cols | bit, ((d1 | bit) << 1) & all, (d2 | bit) >> 1);
    }
    #pragma omp taskwait                           /* all children done before adding */
    long count = 0;
    for (int c = 0; c < n; c++) count += sub[c];
    return count;
}

static long nq_tasks(int n) {
    long count = 0;
    #pragma omp parallel
    #pragma omp single
    count = nq_task(n, 0, 0, 0, 0);
    return count;
}

int main(int argc, char *argv[]) {
    int n = (int)arg_long(argc, argv, 1, 13);
    if (n < 1 || n > MAXN) { fprintf(stderr, "N must be in 1..%d\n", MAXN); return 1; }
    omp_warmup();

    /* Check both parallel versions against the known counts for N = 8..13. */
    printf("P091: N-Queens, known counts check (parallel versions, %d threads)\n",
           omp_get_max_threads());
    for (int m = 8; m <= 13; m++) {
        long a = nq_parallel_for(m), b = nq_tasks(m);
        printf("  N = %2d: known %6ld  for %6ld  tasks %6ld  %s\n",
               m, known[m], a, b, ok_str(a == known[m] && b == known[m]));
    }

    long rs = 0, rp = 0;
    double ts, tp;
    BEST_TIME(ts, rs = nq_serial(n, 0, 0, 0));
    printf("N = %d: serial count = %ld", n, rs);
    if (n <= 16) printf(" (known %ld, %s)", known[n], ok_str(rs == known[n]));
    printf("\n");

    printf("(a) parallel for over row-0 column, reduction(+:count), schedule(dynamic,1)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = nq_parallel_for(n));
        table_row(n, THREADS[k], ts, tp, rp == rs);
    }
    printf("(b) tasks for rows 0..%d (cutoff), taskwait, then sum\n", TASK_ROWS - 1);
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, rp = nq_tasks(n));
        table_row(n, THREADS[k], ts, tp, rp == rs);
    }
    return 0;
}

/* Notes:
 * - Why tasks: the search tree is irregular (subtree sizes are not known in
 *   advance), which is what tasks and dynamic scheduling are for.
 * - (a) has only N independent pieces of very different size, so with 8
 *   threads and N = 13 some threads idle; (b) makes about N^2 tasks, which
 *   balances better. More task rows = more, smaller tasks (more overhead).
 * - No race: masks are passed by value (each task has its own copy); each task
 *   writes only its own sub[c] slot, and sub is read only after taskwait.
 * - shared(sub) is needed: a local array referenced in a task is firstprivate by
 *   default, so without it the child would write into a private copy.
 * - Symmetry trick (not used, for clarity): count only first-row columns
 *   c < N/2, double it, and add the middle column for odd N.
 */

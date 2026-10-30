/* P094: Level-synchronous parallel BFS
 * Problem: Given an undirected graph with n vertices in CSR format (the
 *   neighbours of u are adj[off[u]] .. adj[off[u+1]-1]) and a source vertex s,
 *   compute dist[v] = number of edges on a shortest path from s to v (-1 if v
 *   cannot be reached) by breadth-first search. Parallelise it level by level:
 *   all vertices of the current frontier are expanded in parallel, every
 *   unvisited neighbour must be claimed exactly once and appended to the next
 *   frontier. Verify the distances against a serial BFS and report time and
 *   speedup for 1, 2, 4 and 8 threads.
 * Solution: levels are processed one after another (level L+1 needs the whole
 *   level L); within a level the frontier is split with parallel for
 *   schedule(dynamic, 64) (vertex degrees differ). Two threads may find the same
 *   unvisited neighbour v, so the claim is an atomic exchange on visited[v]
 *   (#pragma omp atomic capture: old = visited[v]; visited[v] = 1): only the
 *   thread that sees old == 0 owns v, writes dist[v] and appends v.
 *   (a) append with an atomic index: #pragma omp atomic capture pos = nsize++.
 *   (b) each thread appends to its own buffer; at the end of the level it
 *       reserves room in the next frontier with ONE atomic capture and copies
 *       its buffer there (far fewer atomics on the shared counter).
 * Source: graph-algorithms lab extension (level-synchronous BFS).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p094_bfs_level_sync.c -o p094 -lm
 * Run:   ./p094 [n] [m]      (defaults n = 1e6 vertices, m = 4e6 random edges)
 */
#include "common.h"

/* Random undirected graph with n vertices and m edges, stored in CSR form:
 * the edge {u,v} appears as v in u's list and as u in v's list. */
static void make_graph(long n, long m, long **off_out, int **adj_out) {
    int *eu = xmalloc((size_t)m * sizeof(int)), *ev = xmalloc((size_t)m * sizeof(int));
    unsigned long long s = 12345;
    for (long e = 0; e < m; e++) {
        eu[e] = (int)(lcg_next(&s) % (unsigned long)n);
        ev[e] = (int)(lcg_next(&s) % (unsigned long)n);
    }
    long *off = calloc((size_t)n + 1, sizeof(long));
    if (!off) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (long e = 0; e < m; e++) { off[eu[e] + 1]++; off[ev[e] + 1]++; }   /* degrees */
    for (long u = 0; u < n; u++) off[u + 1] += off[u];                     /* prefix sum */
    int *adj = xmalloc((size_t)(2 * m) * sizeof(int));
    long *pos = xmalloc((size_t)n * sizeof(long));
    memcpy(pos, off, (size_t)n * sizeof(long));
    for (long e = 0; e < m; e++) { adj[pos[eu[e]]++] = ev[e]; adj[pos[ev[e]]++] = eu[e]; }
    free(eu); free(ev); free(pos);
    *off_out = off; *adj_out = adj;
}

static void bfs_serial(long n, const long *off, const int *adj, int src, int *dist) {
    int *queue = xmalloc((size_t)n * sizeof(int));
    for (long v = 0; v < n; v++) dist[v] = -1;
    long head = 0, tail = 0;
    dist[src] = 0; queue[tail++] = src;
    while (head < tail) {
        int u = queue[head++];
        for (long e = off[u]; e < off[u + 1]; e++) {
            int v = adj[e];
            if (dist[v] == -1) { dist[v] = dist[u] + 1; queue[tail++] = v; }
        }
    }
    free(queue);
}

/* Claim v: returns 1 for exactly one caller (the first one), 0 for all others. */
static int claim(int *visited, int v) {
    int old;
    #pragma omp atomic read
    old = visited[v];                  /* cheap test first: usually already visited */
    if (old) return 0;
    #pragma omp atomic capture
    { old = visited[v]; visited[v] = 1; }      /* atomic exchange */
    return old == 0;
}

/* (a) next frontier filled through one shared atomic index. */
static void bfs_atomic_index(long n, const long *off, const int *adj, int src, int *dist) {
    int *visited = calloc((size_t)n, sizeof(int));
    int *frontier = xmalloc((size_t)n * sizeof(int)), *next = xmalloc((size_t)n * sizeof(int));
    if (!visited) { fprintf(stderr, "out of memory\n"); exit(1); }
    #pragma omp parallel for
    for (long v = 0; v < n; v++) dist[v] = -1;
    visited[src] = 1; dist[src] = 0;
    frontier[0] = src;
    long fsize = 1;
    for (int level = 0; fsize > 0; level++) {
        long nsize = 0;
        #pragma omp parallel for schedule(dynamic, 64)
        for (long f = 0; f < fsize; f++) {
            int u = frontier[f];
            for (long e = off[u]; e < off[u + 1]; e++) {
                int v = adj[e];
                if (claim(visited, v)) {           /* only one thread gets here per v */
                    dist[v] = level + 1;
                    long pos;
                    #pragma omp atomic capture
                    pos = nsize++;
                    next[pos] = v;
                }
            }
        }                                          /* implicit barrier: level done */
        int *tmp = frontier; frontier = next; next = tmp;
        fsize = nsize;
    }
    free(visited); free(frontier); free(next);
}

/* (b) per-thread buffers, one atomic per thread per level to reserve space. */
static void bfs_local_buffers(long n, const long *off, const int *adj, int src, int *dist) {
    int *visited = calloc((size_t)n, sizeof(int));
    int *frontier = xmalloc((size_t)n * sizeof(int)), *next = xmalloc((size_t)n * sizeof(int));
    if (!visited) { fprintf(stderr, "out of memory\n"); exit(1); }
    #pragma omp parallel for
    for (long v = 0; v < n; v++) dist[v] = -1;
    visited[src] = 1; dist[src] = 0;
    frontier[0] = src;
    long fsize = 1;
    for (int level = 0; fsize > 0; level++) {
        long nsize = 0;
        #pragma omp parallel
        {
            long cap = 1024, cnt = 0;
            int *buf = xmalloc((size_t)cap * sizeof(int));   /* this thread's finds */
            #pragma omp for schedule(dynamic, 64) nowait
            for (long f = 0; f < fsize; f++) {
                int u = frontier[f];
                for (long e = off[u]; e < off[u + 1]; e++) {
                    int v = adj[e];
                    if (claim(visited, v)) {
                        dist[v] = level + 1;
                        if (cnt == cap) {                    /* grow the buffer */
                            cap *= 2;
                            buf = realloc(buf, (size_t)cap * sizeof(int));
                            if (!buf) { fprintf(stderr, "out of memory\n"); exit(1); }
                        }
                        buf[cnt++] = v;
                    }
                }
            }
            long start;
            #pragma omp atomic capture
            { start = nsize; nsize += cnt; }                  /* reserve cnt slots */
            memcpy(next + start, buf, (size_t)cnt * sizeof(int));
            free(buf);
        }                                          /* end of region: level done */
        int *tmp = frontier; frontier = next; next = tmp;
        fsize = nsize;
    }
    free(visited); free(frontier); free(next);
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 1000000);
    long m = arg_long(argc, argv, 2, 4000000);
    if (n < 1 || n > INT_MAX || m < 0) {
        fprintf(stderr, "need 1 <= n <= INT_MAX, m >= 0\n");
        return 1;
    }
    long *off; int *adj;
    make_graph(n, m, &off, &adj);
    int *ds = xmalloc((size_t)n * sizeof(int)), *dp = xmalloc((size_t)n * sizeof(int));
    int src = 0;
    omp_warmup();

    double ts, tp;
    BEST_TIME(ts, bfs_serial(n, off, adj, src, ds));
    long reached = 0; int maxd = 0;
    for (long v = 0; v < n; v++) if (ds[v] >= 0) { reached++; if (ds[v] > maxd) maxd = ds[v]; }
    printf("P094: BFS from vertex %d, n = %ld, m = %ld: %ld reached, max distance %d\n",
           src, n, m, reached, maxd);

    printf("(a) atomic exchange to claim, atomic index to append\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, bfs_atomic_index(n, off, adj, src, dp));
        table_row(n, THREADS[k], ts, tp, same_ints(ds, dp, n));
    }
    printf("(b) atomic exchange to claim, per-thread buffers + one atomic per thread\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, bfs_local_buffers(n, off, adj, src, dp));
        table_row(n, THREADS[k], ts, tp, same_ints(ds, dp, n));
    }
    free(off); free(adj); free(ds); free(dp);
    return 0;
}

/* Notes:
 * - Race 1: two frontier vertices share an unvisited neighbour v. With a plain
 *   "if (!visited[v]) visited[v] = 1" both threads could pass the test and add
 *   v twice. The atomic exchange lets exactly one of them see old == 0.
 *   (OpenMP 4.5 has no compare-and-swap; #pragma omp critical also works but
 *   serialises every claim.)
 * - Race 2: appending to the next frontier: pos = nsize++ must be an atomic
 *   capture (read + increment together) or two threads get the same slot.
 * - dist[v] needs no atomic: only the claiming thread writes it, and it is read
 *   only after the level's barrier.
 * - The barrier at the end of every level is essential: level L+1 starts only
 *   when level L's frontier is complete. Order inside a frontier may differ from
 *   the serial queue, but every vertex still gets the same (shortest) distance.
 * - Speedup is limited: BFS does little work per memory access (random reads of
 *   adj/visited), and the first and last levels have tiny frontiers.
 */

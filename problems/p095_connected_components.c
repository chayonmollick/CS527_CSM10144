/* P095: Connected components by parallel label propagation
 * Problem: An undirected graph with n vertices is given as an edge list
 *   (eu[e], ev[e]), e = 0..m-1. Find its connected components by label
 *   propagation: every vertex starts with its own number as label; repeatedly
 *   set label[u] = label[v] = min(label[u], label[v]) over all edges until no
 *   label changes. Then label[v] = smallest vertex of v's component. Parallelise
 *   the sweeps with OpenMP, verify against a serial union-find, print the number
 *   of components and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: sweeps are sequential (repeat until no change); one sweep is a
 *   parallel for whose "did anything change?" flag is a reduction(||:changed).
 *   Two ways to make one sweep race-free:
 *   (a) double buffer (Jacobi): build the adjacency lists (CSR) once; in a sweep
 *       every vertex v computes new[v] = min(old[v], old[u] for neighbours u)
 *       reading only old[] and writing only its own new[v]; swap buffers.
 *   (b) in place on the edge list (monotone atomic min): read both labels with
 *       atomic read; lower the bigger one inside a critical section that
 *       re-checks (label = min(label, val)) - labels only ever decrease, so a
 *       stale read just means one more sweep. Fewer sweeps, but each decrease
 *       pays for the critical section.
 *   Timing baseline: the same propagation run serially (in place). The check
 *   uses a serial union-find that links the bigger root under the smaller, so
 *   every root is the smallest vertex of its component (same canonical label).
 * Source: graph-algorithms lab extension (label propagation / union-find).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p095_connected_components.c -o p095 -lm
 * Run:   ./p095 [n] [m]      (defaults n = 5e5 vertices, m = 7.5e5 random edges)
 */
#include "common.h"

static void make_edges(long n, long m, int *eu, int *ev) {
    unsigned long long s = 2024;
    for (long e = 0; e < m; e++) {
        eu[e] = (int)(lcg_next(&s) % (unsigned long)n);
        ev[e] = (int)(lcg_next(&s) % (unsigned long)n);
    }
}

/* CSR adjacency (each edge stored in both directions), for version (a). */
static void make_csr(long n, long m, const int *eu, const int *ev, long **off_out, int **adj_out) {
    long *off = xmalloc((size_t)(n + 1) * sizeof(long));
    for (long v = 0; v <= n; v++) off[v] = 0;
    for (long e = 0; e < m; e++) { off[eu[e] + 1]++; off[ev[e] + 1]++; }
    for (long v = 0; v < n; v++) off[v + 1] += off[v];
    int *adj = xmalloc((size_t)(2 * m) * sizeof(int));
    long *pos = xmalloc((size_t)n * sizeof(long));
    memcpy(pos, off, (size_t)n * sizeof(long));
    for (long e = 0; e < m; e++) { adj[pos[eu[e]]++] = ev[e]; adj[pos[ev[e]]++] = eu[e]; }
    free(pos);
    *off_out = off; *adj_out = adj;
}

/* ---- serial reference: union-find ---- */
static int find(int *parent, int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];          /* path halving */
        x = parent[x];
    }
    return x;
}

static void cc_union_find(long n, long m, const int *eu, const int *ev, int *label) {
    for (long v = 0; v < n; v++) label[v] = (int)v;         /* label[] = parent[] */
    for (long e = 0; e < m; e++) {
        int a = find(label, eu[e]), b = find(label, ev[e]);
        if (a < b) label[b] = a;                /* smaller root wins */
        else if (b < a) label[a] = b;
    }
    for (long v = 0; v < n; v++) label[v] = find(label, (int)v);   /* = min vertex */
}

/* ---- serial label propagation (timing baseline); returns the sweep count ---- */
static int cc_serial(long n, long m, const int *eu, const int *ev, int *label) {
    for (long v = 0; v < n; v++) label[v] = (int)v;
    int sweeps = 0, changed;
    do {
        changed = 0;
        for (long e = 0; e < m; e++) {
            int u = eu[e], v = ev[e];
            if (label[u] < label[v])      { label[v] = label[u]; changed = 1; }
            else if (label[v] < label[u]) { label[u] = label[v]; changed = 1; }
        }
        sweeps++;
    } while (changed);
    return sweeps;
}

/* ---- (a) Jacobi sweeps over CSR with two buffers; returns the sweep count ---- */
static int cc_jacobi(long n, const long *off, const int *adj, int *label) {
    int *old = label, *nw = xmalloc((size_t)n * sizeof(int));
    #pragma omp parallel for
    for (long v = 0; v < n; v++) old[v] = (int)v;
    int sweeps = 0, changed;
    do {
        changed = 0;
        #pragma omp parallel for reduction(||:changed) schedule(static)
        for (long v = 0; v < n; v++) {
            int best = old[v];
            for (long e = off[v]; e < off[v + 1]; e++)
                if (old[adj[e]] < best) best = old[adj[e]];
            nw[v] = best;                       /* only this thread writes nw[v] */
            if (best != old[v]) changed = 1;
        }
        int *t = old; old = nw; nw = t;         /* new labels become old */
        sweeps++;
    } while (changed);
    if (old != label) { memcpy(label, old, (size_t)n * sizeof(int)); free(old); }
    else free(nw);
    return sweeps;
}

/* label[x] = min(label[x], val); returns 1 if it lowered the label. */
static int lower_label(int *label, int x, int val) {
    int done = 0;
    #pragma omp critical(cc_lower)
    {
        if (val < label[x]) {                   /* re-check: it may have dropped already */
            #pragma omp atomic write
            label[x] = val;
            done = 1;
        }
    }
    return done;
}

/* ---- (b) in-place sweeps over the edge list with a monotone atomic min ---- */
static int cc_edges_inplace(long n, long m, const int *eu, const int *ev, int *label) {
    #pragma omp parallel for
    for (long v = 0; v < n; v++) label[v] = (int)v;
    int sweeps = 0, changed;
    do {
        changed = 0;
        #pragma omp parallel for reduction(||:changed) schedule(static)
        for (long e = 0; e < m; e++) {
            int u = eu[e], v = ev[e], a, b;
            #pragma omp atomic read
            a = label[u];
            #pragma omp atomic read
            b = label[v];
            /* not "changed = changed || lower(..)": || would skip the call */
            if (a < b)      { if (lower_label(label, v, a)) changed = 1; }
            else if (b < a) { if (lower_label(label, u, b)) changed = 1; }
        }
        sweeps++;
    } while (changed);
    return sweeps;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 500000);
    long m = arg_long(argc, argv, 2, 750000);
    if (n < 1 || n > INT_MAX || m < 0) {
        fprintf(stderr, "need 1 <= n <= INT_MAX, m >= 0\n");
        return 1;
    }
    int *eu = xmalloc((size_t)m * sizeof(int)), *ev = xmalloc((size_t)m * sizeof(int));
    make_edges(n, m, eu, ev);
    long *off; int *adj;
    make_csr(n, m, eu, ev, &off, &adj);
    int *ls = xmalloc((size_t)n * sizeof(int)), *lp = xmalloc((size_t)n * sizeof(int));
    omp_warmup();

    double tu, ts, tp;
    int sweeps = 0;
    BEST_TIME(tu, cc_union_find(n, m, eu, ev, ls));     /* reference labels */
    BEST_TIME(ts, sweeps = cc_serial(n, m, eu, ev, lp));
    long comps = 0, biggest_root = 0;
    long *size = calloc((size_t)n, sizeof(long));
    if (!size) { fprintf(stderr, "out of memory\n"); return 1; }
    for (long v = 0; v < n; v++) { size[ls[v]]++; if (ls[v] == v) comps++; }
    for (long v = 0; v < n; v++) if (size[v] > size[biggest_root]) biggest_root = v;
    printf("P095: connected components, n = %ld, m = %ld: %ld components, largest %ld vertices\n",
           n, m, comps, size[biggest_root]);
    free(size);
    printf("serial union-find %.6f s (check); serial label propagation %.6f s, %d sweeps, %s\n",
           tu, ts, sweeps, ok_str(same_ints(ls, lp, n)));

    printf("(a) Jacobi label propagation on CSR, double buffer\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, sweeps = cc_jacobi(n, off, adj, lp));
        table_row(n, THREADS[k], ts, tp, same_ints(ls, lp, n));
    }
    printf("    %d sweeps\n", sweeps);
    printf("(b) in-place label propagation on the edge list, atomic min via critical\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, sweeps = cc_edges_inplace(n, m, eu, ev, lp));
        table_row(n, THREADS[k], ts, tp, same_ints(ls, lp, n));
    }
    printf("    %d sweeps (last run; varies with timing)\n", sweeps);
    free(eu); free(ev); free(off); free(adj); free(ls); free(lp);
    return 0;
}

/* Notes:
 * - Why a fixed point is the answer: labels only decrease and are always a
 *   vertex of the same component; when no edge can lower a label, the two ends
 *   of every edge agree, so each component has one label = its smallest vertex.
 * - (a) is race-free by construction (read old, write own new[v]); the number of
 *   sweeps is about the graph's diameter (slow on long chains), and about twice
 *   the in-place count, since a new label is only seen in the next sweep.
 * - (b) without atomics/critical: two threads lowering label[x] to 5 and 7
 *   could finish in the order 5, 7 - the label goes UP (lost update). The
 *   critical section re-checks val < label[x] before writing (atomic min).
 * - reduction(||:changed): each thread has a private flag (initially 0), OR-ed
 *   together at the end; the do-while needs the barrier after every sweep.
 * - Union-find is O(m alpha(n)) and hard to parallelise (finds and links
 *   conflict); label propagation does more work but every sweep is parallel.
 * - A named critical is ONE lock for all vertices, so (b) gets slower with
 *   more threads; an array of omp_lock_t (lock x % NLOCKS) would let threads
 *   lower different labels at the same time.
 */

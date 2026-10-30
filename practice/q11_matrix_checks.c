/* CS527 Computer Systems — OpenMP practice
 * Q11: For N x N matrices check in parallel whether A == B, whether A is
 *      symmetric (A == A^T) and whether A is the identity matrix; also compute
 *      C = A - B and C = k*A.
 *  - checks   : logical AND reduction  reduction(&&:ok), parallel over rows
 *               (symmetric scans only the upper triangle j > i, so rows have
 *                unequal work -> schedule(dynamic,16))
 *  - C = A - B, C = k*A : collapse(2), every C element written by one iteration
 * Tested on matrices that pass and on copies with ONE wrong element near the end.
 * Matrices are flattened row-major: A[i][j] = A[(long)i*n + j].
 * Usage: ./q11 [N]   (N >= 2)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- serial reference versions (return as soon as a mismatch is seen) ---------- */
static int equal_serial(const double *A, const double *B, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (A[i*n + j] != B[i*n + j]) return 0;
    return 1;
}
static int symmetric_serial(const double *A, long n) {
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++)
            if (A[i*n + j] != A[j*n + i]) return 0;
    return 1;
}
static int identity_serial(const double *A, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            if (A[i*n + j] != (i == j ? 1.0 : 0.0)) return 0;
    return 1;
}
static void sub_serial(const double *A, const double *B, double *C, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) C[i*n + j] = A[i*n + j] - B[i*n + j];
}
static void scale_serial(double k, const double *A, double *C, long n) {
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) C[i*n + j] = k * A[i*n + j];
}

/* ---------- parallel versions ---------- */
/* Each thread ANDs into a private ok (initialised to 1 = true); the private
 * results are ANDed together at the end. No early exit: break is not allowed. */
static int equal_parallel(const double *A, const double *B, long n) {
    int ok = 1;
    #pragma omp parallel for reduction(&&:ok)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            ok = ok && (A[i*n + j] == B[i*n + j]);
    return ok;
}
/* only the upper triangle: row i has n-i-1 comparisons, so use a dynamic schedule */
static int symmetric_parallel(const double *A, long n) {
    int ok = 1;
    #pragma omp parallel for schedule(dynamic, 16) reduction(&&:ok)
    for (long i = 0; i < n; i++)
        for (long j = i + 1; j < n; j++)
            ok = ok && (A[i*n + j] == A[j*n + i]);
    return ok;
}
static int identity_parallel(const double *A, long n) {
    int ok = 1;
    #pragma omp parallel for reduction(&&:ok)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            ok = ok && (A[i*n + j] == (i == j ? 1.0 : 0.0));
    return ok;
}
static void sub_parallel(const double *A, const double *B, double *C, long n) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) C[i*n + j] = A[i*n + j] - B[i*n + j];
}
static void scale_parallel(double k, const double *A, double *C, long n) {
    #pragma omp parallel for collapse(2)
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++) C[i*n + j] = k * A[i*n + j];
}

static int close_mat(const double *X, const double *Y, long n2) {
    for (long i = 0; i < n2; i++)
        if (fabs(X[i] - Y[i]) > 1e-9 * (1.0 + fabs(X[i]))) return 0;
    return 1;
}
static double speedup(double ts, double tp) { return tp > 0 ? ts / tp : 0.0; }

/* prints one test line; OK only if serial and parallel agree with the expected answer */
static void report(const char *what, int expect, int s, int p) {
    printf("  %-34s: expect %d  serial %d  parallel %d  %s\n",
           what, expect, s, p, (s == expect && p == expect) ? "OK" : "WRONG");
}

static int run(long n) {
    long n2 = n * n;
    const double k = 3.5;
    double *A  = malloc((size_t)n2 * sizeof(double));
    double *B  = malloc((size_t)n2 * sizeof(double));
    double *I  = malloc((size_t)n2 * sizeof(double));
    double *Ds = malloc((size_t)n2 * sizeof(double));   /* serial   A - B */
    double *Ks = malloc((size_t)n2 * sizeof(double));   /* serial   k * A */
    double *Cp = malloc((size_t)n2 * sizeof(double));   /* parallel result */
    if (!A || !B || !I || !Ds || !Ks || !Cp) {
        fprintf(stderr, "alloc failed (N=%ld)\n", n);
        free(A); free(B); free(I); free(Ds); free(Ks); free(Cp); return 1;
    }
    /* A is symmetric by construction (formula symmetric in i,j); B = copy of A */
    for (long i = 0; i < n; i++)
        for (long j = 0; j < n; j++)
            A[i*n + j] = (double)((i + j) % 17) + 0.5 * (double)((i * j) % 11);
    memcpy(B, A, (size_t)n2 * sizeof(double));
    for (long i = 0; i < n2; i++) I[i] = 0.0;
    for (long i = 0; i < n; i++) I[i*n + i] = 1.0;

    printf("Q11: matrix checks with reduction(&&:ok) (N = %ld)\n", n);
    omp_set_num_threads(8);
    printf("correctness (8 threads):\n");
    report("A == B   (B = copy of A)",       1, equal_serial(A, B, n),  equal_parallel(A, B, n));
    report("A symmetric",                    1, symmetric_serial(A, n), symmetric_parallel(A, n));
    report("I is identity",                  1, identity_serial(I, n),  identity_parallel(I, n));

    /* one wrong element near the end: B[n-1][n-2] and I[n-1][n-2] */
    long bad = (n - 1) * n + (n - 2);
    B[bad] += 1.0;
    I[bad]  = 1e-12;
    report("A == B   (B has 1 bad element)", 0, equal_serial(A, B, n),  equal_parallel(A, B, n));
    report("B symmetric (1 bad element)",    0, symmetric_serial(B, n), symmetric_parallel(B, n));
    report("I is identity (1 bad element)",  0, identity_serial(I, n),  identity_parallel(I, n));

    sub_serial(A, B, Ds, n);    sub_parallel(A, B, Cp, n);
    printf("  %-34s: %s\n", "C = A - B", close_mat(Ds, Cp, n2) ? "OK" : "WRONG");
    scale_serial(k, A, Ks, n);  scale_parallel(k, A, Cp, n);
    printf("  %-34s: %s\n", "C = k*A", close_mat(Ks, Cp, n2) ? "OK" : "WRONG");

    /* timing on the PASSING inputs (worst case: every element must be looked at) */
    B[bad] = A[bad];
    I[bad] = 0.0;
    double t0 = omp_get_wtime(); int e0 = equal_serial(A, B, n);      double ts_eq = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); int s0 = symmetric_serial(A, n);            double ts_sy = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); int i0 = identity_serial(I, n);             double ts_id = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); sub_serial(A, B, Ds, n);                    double ts_sb = omp_get_wtime() - t0;
    t0 = omp_get_wtime(); scale_serial(k, A, Ks, n);                  double ts_sc = omp_get_wtime() - t0;
    printf("\nserial(s): equal %.6f  symm %.6f  ident %.6f  sub %.6f  scale %.6f\n",
           ts_eq, ts_sy, ts_id, ts_sb, ts_sc);
    printf("%8s %10s %6s %10s %6s %10s %6s %10s %6s %10s %6s %6s\n", "threads",
           "equal(s)", "sp", "symm(s)", "sp", "ident(s)", "sp", "sub(s)", "sp", "scale(s)", "sp", "check");
    const int threads[] = {1, 2, 4, 8};
    for (int ti = 0; ti < 4; ti++) {
        omp_set_num_threads(threads[ti]);
        t0 = omp_get_wtime(); int e = equal_parallel(A, B, n);        double te = omp_get_wtime() - t0;
        t0 = omp_get_wtime(); int s = symmetric_parallel(A, n);       double tsy = omp_get_wtime() - t0;
        t0 = omp_get_wtime(); int id = identity_parallel(I, n);       double tid = omp_get_wtime() - t0;
        int ok = e == e0 && s == s0 && id == i0 && e && s && id;
        t0 = omp_get_wtime(); sub_parallel(A, B, Cp, n);              double tsb = omp_get_wtime() - t0;
        ok = ok && close_mat(Ds, Cp, n2);
        t0 = omp_get_wtime(); scale_parallel(k, A, Cp, n);            double tsc = omp_get_wtime() - t0;
        ok = ok && close_mat(Ks, Cp, n2);
        printf("%8d %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %10.6f %6.2f %6s\n",
               threads[ti], te, speedup(ts_eq, te), tsy, speedup(ts_sy, tsy), tid, speedup(ts_id, tid),
               tsb, speedup(ts_sb, tsb), tsc, speedup(ts_sc, tsc), ok ? "OK" : "WRONG");
    }
    printf("\n");
    free(A); free(B); free(I); free(Ds); free(Ks); free(Cp);
    return 0;
}

int main(int argc, char **argv) {
    long sizes[] = {1000, 3000};             /* 6 matrices x 3000^2 doubles = 430 MB */
    int nsizes = 2;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    if (sizes[0] < 2) { fprintf(stderr, "usage: %s [N>=2]\n", argv[0]); return 1; }
    for (int s = 0; s < nsizes; s++)
        if (run(sizes[s])) return 1;
    return 0;
}

/* Viva notes:
 * - reduction(&&:ok): each thread gets a private ok = 1 (identity of &&); the
 *   private values are ANDed into the shared ok at the end. Writing `ok = 0` to a
 *   shared ok without a reduction is a benign-looking but real data race.
 *   reduction(&:ok) or reduction(min:ok) work too; ||-reduction for "any".
 * - Why no `break`: an omp for must be a canonical loop whose trip count is known
 *   when the loop starts (iterations are handed out in advance), so jumping out of
 *   it is not allowed (compile error). The serial code returns early; the parallel
 *   one scans everything, which is why the timings use passing inputs.
 * - Early exit IS possible with cancellation (run with OMP_CANCELLATION=true,
 *   otherwise cancel is ignored and the loop just finishes). Use two pragmas:
 *   GCC warns about `cancel for` inside a combined `parallel for` (implicit nowait).
 *       int ok = 1;
 *       #pragma omp parallel
 *       #pragma omp for
 *       for (long i = 0; i < n2; i++) {
 *           if (A[i] != B[i]) {
 *               #pragma omp atomic write
 *               ok = 0;
 *               #pragma omp cancel for
 *           }
 *           #pragma omp cancellation point for
 *       }
 * - Symmetric check: (i,j) and (j,i) are only READ, so parallel reads are fine;
 *   the triangle j > i gives unequal rows -> dynamic schedule; A[j*n+i] is a
 *   column access (strided), so it scales worse than equal/identity.
 * - C = A - B, C = k*A: collapse(2) makes one N*N iteration space; each C element
 *   is written by exactly one iteration -> no race. Memory-bound: low speedup.
 *   Cost of collapse(2) in GCC: the fused loop recomputes/wraps (i,j) each step,
 *   so the inner loop is no longer a plain vectorisable loop -> at 1 thread it is
 *   ~2-3x slower than serial. When N >> threads, `parallel for` on i alone (as in
 *   the checks above) is enough; collapse helps only when N is small.
 */

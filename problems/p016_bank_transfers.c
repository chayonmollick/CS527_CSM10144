/* P016: Bank transfers with one lock per account
 * Problem: There are M accounts and T transfers (from, to, amount). Execute the
 *   transfers in parallel using one omp_lock_t per account. Avoid deadlock and skip
 *   transfers with from == to. Check that the total money is unchanged and that the
 *   final balances equal the serial result. Compare the time with one global critical.
 * Solution: a transfer locks both of its accounts. Two transfers i->j and j->i taking
 *   "from first" would deadlock, so locks are always acquired in index order (lower
 *   account first): no cycle of waiting threads is possible. from == to is skipped (it
 *   would try to take the same lock twice and block forever). Transfers on different
 *   accounts run in parallel; the global critical serialises all of them. Each transfer
 *   is two integer additions, which commute, so any order gives the serial balances.
 *   Every transfer also does W units of "processing" while holding its locks (stands
 *   for validation/logging; the amount moved depends on it, deterministically per k).
 *   With W = 0 the locked work is ~1 ns and lock overhead dominates every version.
 * Source: lab extension of textbook ch. 23 (locks).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p016_bank_transfers.c -o p016 -lm
 * Run:   ./p016 [T] [M] [W]  (defaults T = 5e4 transfers, M = 1000 accounts, W = 1000)
 */
#include "common.h"

#define INIT_BALANCE 1000L

static int W = 1000;                       /* processing work per transfer (inside the locks) */

static void reset(long *bal, long m) { for (long a = 0; a < m; a++) bal[a] = INIT_BALANCE; }

/* Simulated processing done while the accounts are locked. A pure function of the
 * transfer number k (so serial and parallel agree); returns 0 or 1 extra unit. */
static long processing(long k) {
    unsigned long long h = (unsigned long long)k;
    for (int r = 0; r < W; r++) h = h * 6364136223846793005ULL + 1442695040888963407ULL;
    return (long)(h >> 63);
}

static void bank_serial(long *bal, long m, const int *from, const int *to, const int *amt, long t) {
    reset(bal, m);
    for (long k = 0; k < t; k++) {
        if (from[k] == to[k]) continue;
        long a = amt[k] + processing(k);
        bal[from[k]] -= a;
        bal[to[k]]   += a;
    }
}

/* One lock per account, always taken in increasing account index. */
static void bank_locks(long *bal, long m, omp_lock_t *lk,
                       const int *from, const int *to, const int *amt, long t) {
    reset(bal, m);
    #pragma omp parallel for
    for (long k = 0; k < t; k++) {
        int f = from[k], g = to[k];
        if (f == g) continue;                  /* same lock twice would block forever */
        int lo = f < g ? f : g, hi = f < g ? g : f;
        omp_set_lock(&lk[lo]);                 /* global order: lower index first */
        omp_set_lock(&lk[hi]);
        long a = amt[k] + processing(k);
        bal[f] -= a;
        bal[g] += a;
        omp_unset_lock(&lk[hi]);
        omp_unset_lock(&lk[lo]);
    }
}

/* One global lock: correct and simple, but only one transfer at a time. */
static void bank_critical(long *bal, long m, const int *from, const int *to, const int *amt, long t) {
    reset(bal, m);
    #pragma omp parallel for
    for (long k = 0; k < t; k++) {
        int f = from[k], g = to[k];
        if (f == g) continue;
        #pragma omp critical
        {
            long a = amt[k] + processing(k);
            bal[f] -= a;
            bal[g] += a;
        }
    }
}

static long total(const long *bal, long m) {
    long s = 0;
    for (long a = 0; a < m; a++) s += bal[a];
    return s;
}

int main(int argc, char *argv[]) {
    long t = arg_long(argc, argv, 1, 50000L);
    long m = arg_long(argc, argv, 2, 1000L);
    W = (int)arg_long(argc, argv, 3, 1000);
    if (t < 0) t = 0;
    if (m < 1) m = 1;
    int *from = xmalloc((size_t)t * sizeof(int));
    int *to   = xmalloc((size_t)t * sizeof(int));
    int *amt  = xmalloc((size_t)t * sizeof(int));
    fill_int(from, t, 0, (int)m - 1, 1);
    fill_int(to,   t, 0, (int)m - 1, 2);
    fill_int(amt,  t, 1, 100, 3);
    long *bs = xmalloc((size_t)m * sizeof(long));   /* serial balances   */
    long *bp = xmalloc((size_t)m * sizeof(long));   /* parallel balances */
    omp_lock_t *lk = xmalloc((size_t)m * sizeof(omp_lock_t));
    for (long a = 0; a < m; a++) omp_init_lock(&lk[a]);
    omp_set_dynamic(0);
    omp_warmup();

    double ts, tl, tc;
    BEST_TIME(ts, bank_serial(bs, m, from, to, amt, t));
    long expect_total = m * INIT_BALANCE;
    printf("P016: %ld transfers between %ld accounts, W = %d, total money %ld, serial time %.4f s\n",
           t, m, W, expect_total, ts);
    printf("check = total unchanged AND every balance equals the serial one, for both versions\n");
    printf("%8s %12s %13s %8s %13s %8s %6s\n",
           "threads", "serial(s)", "acct-locks(s)", "speedup", "critical(s)", "speedup", "check");
    int all_ok = (total(bs, m) == expect_total);
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tl, bank_locks(bp, m, lk, from, to, amt, t));
        int ok = total(bp, m) == expect_total && same_longs(bp, bs, m);
        BEST_TIME(tc, bank_critical(bp, m, from, to, amt, t));
        ok &= total(bp, m) == expect_total && same_longs(bp, bs, m);
        all_ok &= ok;
        printf("%8d %12.4f %13.4f %8.2f %13.4f %8.2f %6s\n",
               THREADS[k], ts, tl, ts / tl, tc, ts / tc, ok_str(ok));
    }
    printf("Money conserved and balances match serial: %s\n", ok_str(all_ok));

    for (long a = 0; a < m; a++) omp_destroy_lock(&lk[a]);
    free(lk); free(bs); free(bp); free(from); free(to); free(amt);
    return 0;
}

/* Notes:
 * - Deadlock needs a cycle: thread 1 holds acct 5 and waits for 9, thread 2 holds 9 and
 *   waits for 5. Locking min(from,to) first makes every thread climb the same ladder,
 *   so a cycle cannot form.
 * - Fine-grained locks (one per account) let transfers on disjoint accounts run in
 *   parallel; the global critical allows one transfer at a time. With few accounts
 *   (try M = 2) contention makes the two similar.
 * - Keep the locked region short in real code; here the processing is inside on purpose,
 *   to show that only fine-grained locks let that work proceed in parallel.
 * - The parallel result equals the serial one exactly only because the updates commute.
 *   A rule like "skip if balance < amount" makes the outcome order-dependent.
 * - Locks must be initialised (omp_init_lock) before and destroyed after use; an
 *   omp_lock_t must not be locked twice by the same thread (use omp_nest_lock_t for that).
 */

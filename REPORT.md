# CS527 OpenMP Lab — Report

**Machine:** _(fill in: CPU model, physical cores / logical threads, RAM, OS, gcc version)_
**Thread-count method:** `omp_set_num_threads()` swept in-code over {1, 2, 4, 8}.
**Build:** `gcc -O2 -Wall -Wextra -fopenmp`.

Each program prints the table shown below; paste the measured numbers into the
blank cells. The written answers do not depend on the specific hardware.

---

## Task 1 — Vector Addition

Timing (program prints this table):

| N | threads | serial (s) | parallel (s) | speedup |
|------:|:---:|:---:|:---:|:---:|
| 10^7 | 1 | | | |
| 10^7 | 2 | | | |
| 10^7 | 4 | | | |
| 10^7 | 8 | | | |
| 10^8 | 1/2/4/8 | | | |

**Q: At what array size does parallelization start paying off? Why not for small N?**
Parallelism helps only once the per-element work outweighs the fixed cost of
opening a parallel region (waking the thread team, splitting the loop,
barrier at the end) — on the order of microseconds. For small `N` (e.g. 10^5)
that fixed cost is comparable to, or larger than, the addition itself, so the
parallel version is the same speed or slower. The crossover is typically around
`N ≈ 10^5–10^6` on a multicore desktop; above that the loop body dominates and
speedup appears.

**Q: Does speedup keep increasing linearly with thread count?**
No. Vector addition is **memory-bandwidth bound**: it does one add per two loads
and a store, so almost no arithmetic per byte moved. Once enough threads are
running to saturate the memory controllers (often by 2–4 threads), adding more
threads cannot move data any faster, and speedup plateaus well below the
thread-count line. It is limited by bandwidth, not cores.

---

## Task 2 — Sum of Array Elements

| N | threads | serial (s) | reduction (s) | speedup |
|------:|:---:|:---:|:---:|:---:|
| 10^7 | 1/2/4/8 | | | |
| 10^8 | 1/2/4/8 | | | |

Naive (broken) run, 8 threads — paste the 5 differing results: _____

**Q: Why does the naive version give wrong/inconsistent answers?**
`sum += a[i]` is a read–modify–write: load `sum`, add `a[i]`, store `sum`. With
no synchronisation, threads interleave these steps on the **shared** `sum`. If
two threads both read the same old value before either stores, one update
overwrites the other — a lost update. Which updates are lost depends on timing,
so the total is both wrong and different from run to run. This is a textbook
data race on a shared variable.

**Q: Would `#pragma omp critical` around `sum += a[i]` fix correctness? Performance?**
Yes — `critical` serialises the update, so no updates are lost and the answer is
correct. But it serialises *every single iteration* behind one lock: the threads
spend their time queuing for the lock instead of adding, plus lock overhead, so
it is typically **slower than the plain serial loop**. `reduction(+:sum)` is the
right tool: each thread accumulates into a private partial sum with no
contention, and the partials are combined once at the end — correct *and*
scalable.

---

## Task 3 — Maximum/Minimum with Index

| N | threads | manual (max,idx) time (s) |
|------:|:---:|:---:|
| 10^7 | 1/2/4/8 | |
| 10^8 | 1/2/4/8 | |

critical vs manual at N=2·10^6:

| threads | critical (s) | manual (s) |
|:---:|:---:|:---:|
| 1/2/4/8 | | |

**Q: Why can't you directly write `reduction(max: best_idx)` to get the index?**
A `reduction` combines values with one associative/commutative operator. We are
not maximising the index — we are maximising the *value* and want the index that
*accompanies* the winning value. `reduction(max:)` on the value throws away where
it came from, and `reduction(max:)` on the index would just return the largest
index, which is meaningless. The quantity being combined is a **pair**
`(value, index)` with a custom "take the pair with the larger value" rule, which
a plain scalar `max` reduction cannot express.

**Q: critical vs manual per-thread reduction — which is faster and why?**
The manual per-thread reduction is far faster. In the critical version every
iteration enters a lock to compare/update the shared best, serialising the whole
loop (and adding lock overhead) — it barely beats, and can lose to, serial. The
manual version lets each thread scan its chunk into a *private* `(best_val,
best_idx)` with no synchronisation in the hot loop; only a final combine over
`#threads` partial results touches shared state. No contention in the inner loop
means it actually scales with cores.

---

## Task 4 — Matrix Addition (outer vs `collapse(2)`)

| N | threads | serial (s) | outer (s) | collapse (s) | sp_outer | sp_collapse |
|------:|:---:|:---:|:---:|:---:|:---:|:---:|
| 8    | 1/2/4/8 | | | | | |
| 4096 | 1/2/4/8 | | | | | |

**Q: For small N (=8), does `collapse(2)` help vs parallelizing only the outer loop?**
Yes. Parallelizing only the outer loop exposes just `N = 8` independent
iterations to share among the threads. With 8 threads that is one row each (and
with more threads some sit idle); there is almost no slack for load balancing,
and each "chunk" is tiny relative to the region's fixed overhead. `collapse(2)`
fuses the two loops into a single iteration space of `N·N = 64` independent
iterations, giving the scheduler 8× more work units to distribute — better
balance and utilisation. So for small, "tall-and-thin" outer loops, `collapse`
clearly helps.

**Q: For large N (=4096), does `collapse(2)` still make a measurable difference?**
Little to none. The outer loop alone already offers 4096 independent iterations
— far more than the thread count — so every thread is kept busy and well
balanced without fusing. `collapse(2)` adds a small per-iteration index-division
cost and, since matrix addition is memory-bandwidth bound anyway, the two
versions land within noise of each other.

---

## Task 5 — Matrix–Matrix Multiplication

| N | threads | serial (s) | outer (s) | sp | eff | collapse (s) | sp | eff |
|------:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 256  | 1/2/4/8 | | | | | | | |
| 512  | 1/2/4/8 | | | | | | | |
| 1024 | 1/2/4/8 | | | | | | | |

Bonus (`i,k,j` order): serial baseline vs `ijk` ______, parallel scaling ______.

**Q: Why is it safe to parallelize the `i` loop even though the `k` loop sums into `C[i][j]`?**
Splitting the `i` loop gives each thread a disjoint set of **rows** of `C`. Two
threads never touch the same `C[i][j]`, and the `k`-summation accumulates into a
thread-local `sum` (written to `C[i][j]` once at the end), which is owned solely
by the thread handling row `i`. No shared element is written by more than one
thread, so there is no race.

**Q: What happens if you parallelize the `k` loop directly with a plain `parallel for`?**
`k` is the reduction loop: every iteration does `C[i][j] += A[i][k]*B[k][j]` on
the **same** `C[i][j]`. Splitting `k` across threads makes them concurrently
read-modify-write that one location — the same lost-update data race as Task 2 —
so `C[i][j]` comes out wrong and non-deterministic. It would need
`reduction(+:sum)` (or per-thread partials), which is exactly why we do **not**
collapse `k`.

**Q: `parallel for` on `i` alone vs `collapse(2)` on `(i,j)` — when does collapse matter most?**
When the number of outer iterations `N` is small relative to the thread count
(or not a clean multiple of it). With `i` alone you get `N` work units; if
`N ≲ #threads` some threads starve or the split is lopsided. `collapse(2)`
exposes `N²` units and restores balance. For `N` much larger than the thread
count (e.g. 1024 on 8 threads) the `i` loop already saturates the team and
`collapse` makes little difference.

**Bonus — `i,k,j` vs `i,j,k`:** In `ikj`, the inner loop walks `B[k][j]` and
`C[i][j]` along rows (stride-1, cache-friendly); in `ijk` the access to
`B[k][j]` strides down a column, causing a cache miss per step. So `ikj`
usually has a **faster serial baseline** and stays bandwidth-friendly in
parallel. (It is still the naive algorithm — no blocking — just a better access
pattern.)

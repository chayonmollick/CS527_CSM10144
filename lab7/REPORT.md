# CS527 OpenMP Lab — Report

**Machine:** Apple M4, 10 cores (4 performance + 6 efficiency), 16 GB RAM, macOS 15.8; GNU gcc 15.2.0 (Homebrew)
**Thread-count method:** `omp_set_num_threads()` swept in-code over {1, 2, 4, 8}.
**Build:** `gcc -O2 -Wall -Wextra -fopenmp`.

All tables below were measured on the machine above (Task 1: best of 3 runs; the
others: one run each). Every parallel result matched its serial version. Runs
with more than 4 threads also use the slower efficiency cores, so efficiency drops
after 4 threads on every task.

---

## Task 1 — Vector Addition

Timing (best of 3 runs; the output arrays are touched before timing so page faults aren't counted):

| N | threads | serial (s) | parallel (s) | speedup |
|------:|:---:|:---:|:---:|:---:|
| 10^5 | 1 | 0.000140 | 0.000136 | 1.03 |
| 10^5 | 2 | 0.000140 | 0.000131 | 1.07 |
| 10^5 | 4 | 0.000140 | 0.000116 | 1.21 |
| 10^5 | 8 | 0.000140 | 0.000124 | 1.13 |
| 10^6 | 1 | 0.000681 | 0.000685 | 0.99 |
| 10^6 | 2 | 0.000681 | 0.000325 | 2.10 |
| 10^6 | 4 | 0.000681 | 0.000418 | 1.63 |
| 10^6 | 8 | 0.000681 | 0.000374 | 1.82 |
| 10^7 | 1 | 0.002977 | 0.002904 | 1.03 |
| 10^7 | 2 | 0.002977 | 0.002498 | 1.19 |
| 10^7 | 4 | 0.002977 | 0.002581 | 1.15 |
| 10^7 | 8 | 0.002977 | 0.002554 | 1.17 |
| 10^8 | 1 | 0.028547 | 0.027608 | 1.03 |
| 10^8 | 2 | 0.028547 | 0.025460 | 1.12 |
| 10^8 | 4 | 0.028547 | 0.031665 | 0.90 |
| 10^8 | 8 | 0.028547 | 0.077642 | 0.37 |

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

**Observed.** On this machine the effect is extreme. The serial loop already moves
2.4 GB in 0.0285 s at N = 10^8 (about 84 GB/s), close to the M4's memory bandwidth,
so extra threads have almost no bandwidth left to use. Speedup stays at about
1.1–1.2x for 10^7 and 10^8, and 8 threads at 10^8 is even slower (0.37x).
N = 10^5 takes about 0.14 ms in total, the same order as starting the team, so
there is no gain. Only N = 10^6 (a 24 MB working set, largely cache-resident)
profits, at 2.1x with 2 threads. On a typical x86 lab PC a single core reaches far
less than the full memory bandwidth, so a speedup of about 2–4x before the plateau
is expected there.

---

## Task 2 — Sum of Array Elements

| N | threads | serial (s) | reduction (s) | speedup |
|------:|:---:|:---:|:---:|:---:|
| 10^7 | 1 | 0.002678 | 0.002675 | 1.00 |
| 10^7 | 2 | 0.002678 | 0.001379 | 1.94 |
| 10^7 | 4 | 0.002678 | 0.001399 | 1.91 |
| 10^7 | 8 | 0.002678 | 0.001042 | 2.57 |
| 10^8 | 1 | 0.027119 | 0.026518 | 1.02 |
| 10^8 | 2 | 0.027119 | 0.013588 | 2.00 |
| 10^8 | 4 | 0.027119 | 0.010932 | 2.48 |
| 10^8 | 8 | 0.027119 | 0.008257 | 3.28 |

Naive (broken) runs, 8 threads, N = 10^8 (correct sum 550000000):
- plain `long sum` (the handout's code): 68750000, 68750000, 68750000, 68750000, 68750000
- `volatile long sum`: 84256392, 93233135, 89417323, 98457079, 96814711

`critical` vs `reduction`, N = 10^6, 8 threads: 0.019954 s vs 0.000125 s
(critical about 160x slower; both correct).

**Q: Why does the naive version give wrong/inconsistent answers?**
`sum += a[i]` is a read–modify–write: load `sum`, add `a[i]`, store `sum`. With
no synchronisation, threads interleave these steps on the **shared** `sum`. If
two threads both read the same old value before either stores, one update
overwrites the other — a lost update. Which updates are lost depends on timing,
so the total is both wrong and different from run to run. This is a textbook
data race on a shared variable.

**Observed.** Compiled with `-O2`, the plain version printed 68750000 on all five
runs, exactly 1/8 of the correct total. gcc keeps each thread's `sum` in a
register and stores it once at the end, so the last thread's store overwrites
the others. The result is wrong but repeatable. With `volatile long sum` every
iteration really loads and stores the shared variable, the lost updates depend
on timing, and the five totals all differ (see above). Both are the same race;
the optimiser only changes how it shows.

**Q: Would `#pragma omp critical` around `sum += a[i]` fix correctness? Performance?**
Yes — `critical` serialises the update, so no updates are lost and the answer is
correct. But it serialises *every single iteration* behind one lock: the threads
spend their time queuing for the lock instead of adding, plus lock overhead, so
it is typically **slower than the plain serial loop**. `reduction(+:sum)` is the
right tool: each thread accumulates into a private partial sum with no
contention, and the partials are combined once at the end — correct *and*
scalable.

**Observed.** The reduction reaches 3.3x at 8 threads for N = 10^8. It is
memory-bound (one add per 8 bytes loaded), so it flattens after about 4 threads.
`critical` is about 160x slower than the reduction and about 75x slower than
the plain serial loop (serial takes about 0.0003 s for 10^6 elements).

---

## Task 3 — Maximum/Minimum with Index

| N | threads | manual (max,idx) (s) | manual (min,idx) (s) |
|------:|:---:|:---:|:---:|
| 10^7 | 1 | 0.005195 | 0.004988 |
| 10^7 | 2 | 0.002591 | 0.002575 |
| 10^7 | 4 | 0.001370 | 0.001310 |
| 10^7 | 8 | 0.001026 | 0.000990 |
| 10^8 | 1 | 0.051255 | 0.050277 |
| 10^8 | 2 | 0.026286 | 0.025378 |
| 10^8 | 4 | 0.013266 | 0.012917 |
| 10^8 | 8 | 0.009383 | 0.009185 |

critical vs manual at N=2·10^6:

| threads | critical (s) | manual (s) | max and min agree |
|:---:|:---:|:---:|:---:|
| 1 | 0.008347 | 0.001027 | yes |
| 2 | 0.018278 | 0.000533 | yes |
| 4 | 0.026378 | 0.000296 | yes |
| 8 | 0.032305 | 0.000300 | yes |

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

**Observed.** The manual version scales 5.5x at 8 threads (N = 10^8: 0.051 s →
0.0094 s). The critical version gets *slower* as threads are added
(0.0083 s → 0.032 s) and is 8–110x slower than manual. The maximum value
occurs more than once in the first 2·10^6 elements. Threads reach equal values
in no fixed order, so both the critical and the manual versions break ties
toward the smaller index (manual uses `schedule(static)` so thread t owns the
t-th block). That way they return the same index as the serial scan.

---

## Task 4 — Matrix Addition (outer vs `collapse(2)`)

| N | threads | serial (s) | outer (s) | collapse (s) | sp_outer | sp_collapse |
|------:|:---:|:---:|:---:|:---:|:---:|:---:|
| 8 | 1 | 0.000859 | 0.005605 | 0.005327 | 0.15 | 0.16 |
| 8 | 2 | 0.000859 | 0.106543 | 0.091255 | 0.01 | 0.01 |
| 8 | 4 | 0.000859 | 0.259396 | 0.260210 | 0.00 | 0.00 |
| 8 | 8 | 0.000859 | 0.584743 | 0.558978 | 0.00 | 0.00 |
| 4096 | 1 | 0.062388 | 0.059223 | 0.068524 | 1.05 | 0.91 |
| 4096 | 2 | 0.062388 | 0.049574 | 0.048067 | 1.26 | 1.30 |
| 4096 | 4 | 0.062388 | 0.050595 | 0.048129 | 1.23 | 1.30 |
| 4096 | 8 | 0.062388 | 0.049891 | 0.048794 | 1.25 | 1.28 |

Times are totals over 20000 calls (N = 8) and 11 calls (N = 4096); every call opens a new parallel region.

**Q: For small N (=8), does `collapse(2)` help vs parallelizing only the outer loop?**
Yes. Parallelizing only the outer loop exposes just `N = 8` independent
iterations to share among the threads. With 8 threads that is one row each (and
with more threads some sit idle); there is almost no slack for load balancing,
and each "chunk" is tiny relative to the region's fixed overhead. `collapse(2)`
fuses the two loops into a single iteration space of `N·N = 64` independent
iterations, giving the scheduler 8× more work units to distribute — better
balance and utilisation. So for small, "tall-and-thin" outer loops, `collapse`
helps in principle.

**Observed.** In time, both versions are swamped by fork/join overhead. One call
does 64 additions (about 43 ns serially) while opening a parallel region costs
microseconds. Both parallel versions are about 6.5x slower than serial on
1 thread and about 680x slower on 8, and they are indistinguishable from each
other. The advantage of `collapse(2)` shows up only when there are more threads
than outer iterations (more than 8 here) or each element is expensive. A matrix
this small should simply not be parallelised.

**Q: For large N (=4096), does `collapse(2)` still make a measurable difference?**
Little to none. The outer loop alone already offers 4096 independent iterations
— far more than the thread count — so every thread is kept busy and well
balanced without fusing. `collapse(2)` adds a small per-iteration index-division
cost and, since matrix addition is memory-bandwidth bound anyway, the two
versions land within noise of each other.

**Observed.** Both reach about 1.25–1.3x and stop improving after 2 threads
(memory-bound, as in Task 1), with outer and collapse within noise of each other.

---

## Task 5 — Matrix–Matrix Multiplication

| N | threads | serial (s) | outer (s) | sp | eff | collapse (s) | sp | eff |
|------:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 256 | 1 | 0.0109 | 0.0110 | 0.99 | 0.99 | 0.0116 | 0.95 | 0.95 |
| 256 | 2 | 0.0109 | 0.0058 | 1.88 | 0.94 | 0.0060 | 1.83 | 0.91 |
| 256 | 4 | 0.0109 | 0.0040 | 2.72 | 0.68 | 0.0043 | 2.54 | 0.64 |
| 256 | 8 | 0.0109 | 0.0029 | 3.82 | 0.48 | 0.0029 | 3.74 | 0.47 |
| 512 | 1 | 0.1119 | 0.1051 | 1.07 | 1.07 | 0.1051 | 1.07 | 1.07 |
| 512 | 2 | 0.1119 | 0.0557 | 2.01 | 1.00 | 0.0565 | 1.98 | 0.99 |
| 512 | 4 | 0.1119 | 0.0369 | 3.03 | 0.76 | 0.0351 | 3.19 | 0.80 |
| 512 | 8 | 0.1119 | 0.0274 | 4.09 | 0.51 | 0.0272 | 4.11 | 0.51 |
| 1024 | 1 | 0.8977 | 0.8966 | 1.00 | 1.00 | 0.8993 | 1.00 | 1.00 |
| 1024 | 2 | 0.8977 | 0.4763 | 1.88 | 0.94 | 0.5206 | 1.72 | 0.86 |
| 1024 | 4 | 0.8977 | 0.3012 | 2.98 | 0.75 | 0.2899 | 3.10 | 0.77 |
| 1024 | 8 | 0.8977 | 0.2574 | 3.49 | 0.44 | 0.2755 | 3.26 | 0.41 |

Bonus (`i,k,j` order, parallel over `i`, 8 threads; the program times ikj only at 8 threads):
0.0014 s / 0.0089 s / 0.0686 s for N = 256 / 512 / 1024, against
0.0029 / 0.0274 / 0.2574 s for `ijk` on 8 threads. That is
2.1x, 3.1x and 3.8x faster, and at N = 1024 it is
13x faster than serial `ijk`.

**Observed.** Both parallel versions give about 1.9–2.0x on 2 threads
(efficiency about 0.94–1.0) and 3.3–4.1x on 8 (efficiency 0.41–0.51). Outer and
collapse are within noise of each other at every size, because N ≥ 256 is far
more than 8 threads. Scaling is weakest at N = 1024: the `ijk` order walks a
column of B for every C element, and those cache misses make it memory-bound.

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

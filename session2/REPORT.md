# CS527 OpenMP Lab 8 (Session 2) — Report

**Machine:** _(CPU, cores/threads, RAM, OS, gcc version)_
**Threads:** swept in-code with `omp_set_num_threads()` over {1,2,4,8}.
**Build:** `gcc -O2 -Wall -Wextra -fopenmp`. Fill the blank cells from a run on the lab machine.

---

## Task 1 — Array Search
Each program prints: threads | serial(s) | parallel(s) | speedup. (timing scans for an absent value so the whole array is traversed.)

| threads | serial(s) | parallel(s) | speedup |
|:---:|:---:|:---:|:---:|
| 1/2/4/8 | | | |

- **Existence + index:** the parallel version uses `reduction(min:found)` with a
  sentinel of `n`; after the loop, `found==n` means "absent", otherwise `found`
  is the smallest matching index (so it agrees exactly with serial search).
- A full scan is memory-bandwidth / branch bound, so expect modest, sub-linear
  speedup. If only *existence* is needed, an early-exit variant using
  `#pragma omp cancel for` can stop once any thread finds a match; it is faster
  when the target appears early but gives up a deterministic index.

## Task 2 — Parallel Sort (odd-even transposition)
| N | threads | serial(s) | parallel(s) | speedup |
|:---:|:---:|:---:|:---:|:---:|
| 2000 / 8000 / 20000 | 1/2/4/8 | | | |

**Synchronisation required (explanation).** The sort runs in `N` phases. An even
phase compare-exchanges the disjoint pairs (0,1),(2,3),…; an odd phase does
(1,2),(3,4),…. Because the pairs in one phase touch non-overlapping elements,
they can be done fully in parallel with **no** race. But phase `p+1` reads
elements that phase `p` may have just swapped, so all swaps of phase `p` must
finish first — a **barrier between phases**. The implicit barrier at the end of
each `#pragma omp parallel for` provides exactly this, so no extra locking is
needed inside a phase. Speedup is limited: the algorithm is `O(N^2)` work and
each phase only has `N/2` independent compare-exchanges separated by a barrier.

## Task 3 — Histogram (values 0..9)
| threads | per-thread-local (s) | array-reduction (s) |
|:---:|:---:|:---:|
| 1/2/4/8 | | |

Naive shared-bin run (8 threads): paste the "total counted" values showing lost updates: ______

**Preventing the race.** The naive version has every thread do `h[a[i]]++` on the
same shared bins — a read-modify-write race, so counts are lost and the totals
come out below `N`. Two correct fixes: (A) each thread fills a **private**
10-bin histogram with no sharing, then one `critical` combine adds the partials
(10 additions per thread — negligible); (B) `reduction(+:h[:BINS])`, which does
the same private-copy-then-combine automatically. `#pragma omp atomic` on the
increment also fixes correctness but serialises every update and is much slower.

## Task 4 — Matrix Transpose
| N | threads | serial(s) | outer(s) | collapse(s) | sp_outer | sp_coll |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 1024 / 4096 | 1/2/4/8 | | | | | |

Out-of-place transpose writes each `B[j][i]` from exactly one iteration, so there
is no race and no synchronisation is needed. It is memory-bound: one of the two
sides (read of `A` row-wise, write of `B` column-wise) is strided and cache-
unfriendly, so speedup saturates with memory bandwidth. With `N` (1024/4096) far
larger than the thread count, the outer loop alone already balances well;
`collapse(2)` gives little extra here (it mainly helps when the outer dimension
is small relative to the thread count).

## Task 5 — 2D Convolution (3x3 averaging, valid)
5x5 sanity check: `O[0][0] = 70.0` (matches the lab example). ✓

| N | threads | serial(s) | parallel(s) | speedup | match |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 2048 / 4096 | 1/2/4/8 | | | | |

Convolution is embarrassingly parallel: every output pixel is computed
independently from a read-only 3x3 window of the input, writing a distinct
output location — no race, so `collapse(2)` over the output grid is safe and
needs no synchronisation. Each pixel does 9 multiply-adds, enough arithmetic per
byte to scale better than the pure memory-bound kernels, until it too becomes
bandwidth-limited at high thread counts.

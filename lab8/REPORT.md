# CS527 OpenMP Lab 8 (Session 2) — Report

**Machine:** Apple M4, 10 cores, 16 GB RAM, macOS 15.7.9 (arm64)
**Compiler:** GNU gcc 15.2.0 (Homebrew), `-O2 -Wall -Wextra -fopenmp`
**Threads:** swept in-code with `omp_set_num_threads()` over {1, 2, 4, 8}.

All timings below are measured on the machine above. Correctness is checked
against the serial version in every program.

---

## Task 1 — Array Search

Timing scans for an **absent** value so the whole array (N = 10^8) is traversed.

| threads | serial (s) | parallel (s) | speedup |
|:---:|:---:|:---:|:---:|
| 1 | 0.012406 | 0.048744 | 0.25 |
| 2 | 0.012406 | 0.024454 | 0.51 |
| 4 | 0.012406 | 0.012771 | 0.97 |
| 8 | 0.012406 | 0.009630 | 1.29 |

- **Existence + index:** the parallel version uses `reduction(min:found)` with a
  sentinel of `n`; `found==n` means "absent", otherwise `found` is the smallest
  matching index, so it agrees exactly with serial search (verified for a unique
  element, a duplicated one, and an absent one).
- **Observed:** a full scan is memory-bandwidth bound and the min-reduction adds
  per-iteration overhead, so the parallel version is *slower* than serial until
  ~4 threads and reaches only 1.29x at 8. If only existence is needed, an
  early-exit variant with `#pragma omp cancel for` can stop as soon as a match is
  found (faster for early hits, but gives up the deterministic index).

## Task 2 — Parallel Sort (odd-even transposition)

| N | threads | serial bubble (s) | parallel (s) | speedup |
|:---:|:---:|:---:|:---:|:---:|
| 2000  | 1 | 0.004018 | 0.001897 | 2.12 |
| 2000  | 2 | 0.004018 | 0.017110 | 0.23 |
| 2000  | 4 | 0.004018 | 0.025802 | 0.16 |
| 2000  | 8 | 0.004018 | 0.061751 | 0.07 |
| 8000  | 1 | 0.039404 | 0.016977 | 2.32 |
| 8000  | 2 | 0.039404 | 0.066053 | 0.60 |
| 8000  | 4 | 0.039404 | 0.117959 | 0.33 |
| 8000  | 8 | 0.039404 | 0.263497 | 0.15 |
| 20000 | 1 | 0.263320 | 0.104152 | 2.53 |
| 20000 | 2 | 0.263320 | 0.206374 | 1.28 |
| 20000 | 4 | 0.263320 | 0.325582 | 0.81 |
| 20000 | 8 | 0.263320 | 0.632794 | 0.42 |

**Synchronisation required (explanation).** The sort runs in `N` phases. An even
phase compare-exchanges the disjoint pairs (0,1),(2,3),…; an odd phase does
(1,2),(3,4),…. Within a phase the pairs touch non-overlapping elements, so they
run in parallel with **no race**. But phase `p+1` reads elements phase `p` may
have just swapped, so all swaps of phase `p` must finish first — a **barrier
between phases**, provided automatically by the implicit barrier at the end of
each `#pragma omp parallel for`. No extra locking is needed inside a phase.

**Observed.** The 1-thread "parallel" column already beats serial bubble by
~2.1–2.5x, because odd-even does ~N²/2 compare-exchanges versus bubble's ~N².
Adding threads makes it *slower*: there are `N` phases, each ending in a barrier,
and at these sizes the per-phase barrier/fork cost outweighs the `N/2` units of
work in the phase. This is the classic limitation of fine-grained,
barrier-synchronised parallelism — it would only pay off for much larger `N`,
where the O(N²) work itself becomes impractical.

## Task 3 — Histogram (values 0..9)

Naive shared-bin version (8 threads) loses most updates — totals counted ranged
~13–26 million out of 100 million. Both correct methods match serial exactly.

| threads | per-thread-local (s) | array-reduction (s) |
|:---:|:---:|:---:|
| 1 | 0.028892 | 0.027270 |
| 2 | 0.014150 | 0.013718 |
| 4 | 0.009429 | 0.009355 |
| 8 | 0.006908 | 0.006401 |

**Preventing the race.** The naive version has every thread do `h[a[i]]++` on the
same shared bins — a read-modify-write race, so counts are lost and the total
falls far below `N`. Two correct fixes: (A) each thread fills a **private** 10-bin
histogram with no sharing, then one `critical` combine adds the partials (10
additions per thread — negligible); (B) `reduction(+:h[:BINS])`, which does the
same private-copy-then-combine automatically. `#pragma omp atomic` on the
increment also fixes correctness but serialises every update and is much slower.

**Observed.** Both correct methods scale ~4x at 8 threads (memory-bound: one
increment per element). The array-reduction form is marginally faster and far
less code than the manual local-array version.

## Task 4 — Matrix Transpose

| N | threads | serial (s) | outer (s) | collapse (s) | sp_outer | sp_coll |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 1024 | 1 | 0.006339 | 0.004642 | 0.004399 | 1.37 | 1.44 |
| 1024 | 2 | 0.006339 | 0.002246 | 0.002123 | 2.82 | 2.99 |
| 1024 | 4 | 0.006339 | 0.001181 | 0.001086 | 5.37 | 5.84 |
| 1024 | 8 | 0.006339 | 0.000984 | 0.000880 | 6.44 | 7.20 |
| 4096 | 1 | 0.113207 | 0.078665 | 0.088216 | 1.44 | 1.28 |
| 4096 | 2 | 0.113207 | 0.070192 | 0.064847 | 1.61 | 1.75 |
| 4096 | 4 | 0.113207 | 0.041982 | 0.043550 | 2.70 | 2.60 |
| 4096 | 8 | 0.113207 | 0.034813 | 0.036475 | 3.25 | 3.10 |

Out-of-place transpose writes each `B[j][i]` from exactly one iteration, so there
is no race and no synchronisation is needed.

**Observed.** At N=1024 the matrices fit cache well and it scales to ~7x; at
N=4096 the strided column-wise writes to `B` thrash cache and the kernel becomes
memory-bound, so it saturates near ~3x. `collapse(2)` is within noise of the
outer loop (slightly better at 1024, slightly worse at 4096) — expected, since
1024/4096 outer iterations already far exceed 8 threads, so fusing adds little
beyond a small index-arithmetic overhead.

## Task 5 — 2D Convolution (3x3 averaging, valid)

5x5 sanity check: `O[0][0] = 70.0` — matches the lab example. ✓

| N | threads | serial (s) | parallel (s) | speedup | match |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 2048 | 1 | 0.029202 | 0.018944 | 1.54 | yes |
| 2048 | 2 | 0.029202 | 0.007196 | 4.06 | yes |
| 2048 | 4 | 0.029202 | 0.003412 | 8.56 | yes |
| 2048 | 8 | 0.029202 | 0.002668 | 10.95 | yes |
| 4096 | 1 | 0.060140 | 0.060903 | 0.99 | yes |
| 4096 | 2 | 0.060140 | 0.027403 | 2.19 | yes |
| 4096 | 4 | 0.060140 | 0.015391 | 3.91 | yes |
| 4096 | 8 | 0.060140 | 0.011513 | 5.22 | yes |

Convolution is embarrassingly parallel: every output pixel is computed
independently from a read-only 3x3 window, writing a distinct output location —
no race, so `collapse(2)` over the output grid is safe with no synchronisation.

**Observed.** Nine multiply-adds per pixel give enough arithmetic per byte to
scale well — ~11x at N=2048 (the best scaling of the five kernels; cache reuse of
overlapping windows helps at 8 threads). At N=4096 the larger working set pushes
it toward memory bandwidth and it reaches ~5x.

---

### Summary
Compute-dense, independent kernels (convolution) scale best; pure
memory-streaming kernels (search, histogram, large transpose) saturate on memory
bandwidth at a few×; and barrier-synchronised fine-grained work (odd-even sort)
does not benefit from threads at these sizes despite being correct.

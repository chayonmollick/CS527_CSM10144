# CS527 OpenMP Lab 9 (Session 3) — Report

**Machine:** Apple M4, 10 cores (4 performance + 6 efficiency), 16 GB RAM, macOS 15.8 (arm64)
**Compiler:** GNU gcc 15.2.0 (Homebrew), `-O2 -Wall -Wextra -fopenmp` (also builds and passes under Linux gcc 9.5 and 12.5)
**Threads:** set in code with `omp_set_num_threads()`, swept over {1, 2, 4, 8}.
**Timing:** `omp_get_wtime()`, each value the best of 3 runs; speedup S = T_serial / T_parallel, efficiency E = S / threads.

All numbers below come from `./run_benchmarks.sh` on the machine above (raw
output in `results/`). Every parallel result was checked against the serial
one; every row printed `yes`.

A note on 8 threads: this CPU has only 4 performance cores, so runs with more than 4
threads also use the slower efficiency cores. Efficiency therefore drops after 4
threads on every task, independent of the algorithm.

---

## Task 1 — Merge sort with tasks

`merge_sort_parallel` spawns the two halves as `#pragma omp task` with
`shared(a, tmp)` and `firstprivate` bounds, waits with `#pragma omp taskwait`,
then merges. It is started from `#pragma omp parallel` + `#pragma omp single`.
Subarrays shorter than the cutoff (default 2048) use the serial merge sort. The
`tmp` array is allocated once before sorting. Each call only touches
`tmp[left..right]`, the same disjoint range it sorts, so tasks never share
elements. Correctness: the output is checked to be sorted and byte-identical
(`memcmp`) to the serial output.

| N | tasks | threads | serial (s) | parallel (s) | speedup | efficiency | correct |
|---:|---:|:---:|---:|---:|---:|---:|:---:|
| 10^5 | 126 | 1 | 0.007127 | 0.005848 | 1.22 | 1.22 | yes |
| 10^5 | 126 | 2 | 0.007127 | 0.002945 | 2.42 | 1.21 | yes |
| 10^5 | 126 | 4 | 0.007127 | 0.001946 | 3.66 | 0.92 | yes |
| 10^5 | 126 | 8 | 0.007127 | 0.001410 | 5.05 | 0.63 | yes |
| 10^6 | 1022 | 1 | 0.056389 | 0.056923 | 0.99 | 0.99 | yes |
| 10^6 | 1022 | 2 | 0.056389 | 0.031049 | 1.82 | 0.91 | yes |
| 10^6 | 1022 | 4 | 0.056389 | 0.022136 | 2.55 | 0.64 | yes |
| 10^6 | 1022 | 8 | 0.056389 | 0.015198 | 3.71 | 0.46 | yes |
| 10^7 | 16382 | 1 | 0.683344 | 0.684971 | 1.00 | 1.00 | yes |
| 10^7 | 16382 | 2 | 0.683344 | 0.364625 | 1.87 | 0.94 | yes |
| 10^7 | 16382 | 4 | 0.683344 | 0.251626 | 2.72 | 0.68 | yes |
| 10^7 | 16382 | 8 | 0.683344 | 0.165079 | 4.14 | 0.52 | yes |

Cutoff study (N = 10^7, 8 threads, serial = 0.684 s):

| cutoff | tasks | parallel (s) | speedup |
|---:|---:|---:|---:|
| 16 | 2097150 | 0.788262 | 0.87 |
| 256 | 131070 | 0.221694 | 3.09 |
| 2048 | 16382 | 0.177302 | 3.86 |
| 16384 | 2046 | 0.170367 | 4.02 |
| 131072 | 254 | 0.171787 | 3.98 |
| 1048576 | 30 | 0.172058 | 3.98 |

**Q: What is the difference between OpenMP tasks and `parallel for`?**
`parallel for` is a worksharing construct. It needs a loop whose iteration count
is known when the loop starts, splits those iterations among the team
(statically or dynamically), and ends with an implicit barrier. Tasks are units of work
that a thread *creates on the fly*, possibly recursively and in unknown number.
They go into a task pool and any idle thread of the team runs them. That makes
tasks the natural fit for divide-and-conquer (merge sort, quicksort), trees,
linked lists and other irregular work that a single loop cannot express. Tasks
are synchronised explicitly with `taskwait` / `taskgroup` (or a barrier).

**Q: Why is `taskwait` required before merging the two halves?**
Creating a task does not run it: when the parent reaches `merge`, the two child
tasks may not have started or may be half done. `merge` reads both halves and
uses the same `tmp` range the children use, so it may only run after both are
completely sorted. `taskwait` blocks the parent until its child tasks have
finished. Each child does its own `taskwait` before returning, so the whole
subtree is done. Without it, the merge reads partially sorted data and the
output is wrong.

**Q: How does the cutoff threshold affect performance?**
It sets how many tasks are created: about 2·N/cutoff (the `tasks` column). With
cutoff 16 there are 2.1 million tiny tasks. Creating, queueing and waiting on
each one costs more than sorting 16 elements, so the "parallel" sort is *slower
than serial* (0.87x). At 256 it reaches 3.1x, and from 2048 upward the time
plateaus around 4x. Very large cutoffs create too few tasks to balance the work
over the threads: at a cutoff ≥ N there are 0 tasks and the sort is serial.
Here even 30 tasks were enough for 8 threads because the halves are equal in
size. A good cutoff gives tens of tasks per thread, each doing thousands of
elements of work (2048–131072 here).

**Q: Why can parallel merge sort become slower than serial for small arrays?**
For N = 10^5 the whole sort takes about 7 ms, so fixed costs matter: waking the
team, creating and scheduling the tasks, and the `taskwait` synchronisations.
The merges near the top of the recursion are also serial: the final merge of all N
elements runs on one thread whatever the thread count. When the array is small,
these fixed and serial parts are a large share of the time. With a small
cutoff the task overhead alone makes it slower than serial (the cutoff = 16
row: 0.87x even with 8 threads). Here 10^5 still gained 5x at 8 threads
because the default cutoff keeps the task count to 126. The 1.22 for 1 thread
at 10^5 is noise: a 6–7 ms run is sensitive to which core type macOS runs it
on, and the larger sizes show the expected 1.00.

**Q: How does task creation overhead affect scalability?**
Every task costs an allocation, a push to and pop from a shared task queue
(contention grows with the number of threads) and its share of the `taskwait`
synchronisation. Too many tasks turn that overhead into the dominant cost. Even
with ideal tasks, the merge step limits scaling. Total work is O(N log N), but
the merges along the critical path cost N + N/2 + N/4 + … ≈ 2N, which runs
serially. The available parallelism is therefore only about log₂ N (≈ 23 for
10^7). Speedup saturates near 4x at 8 threads (efficiency 0.52), and memory
bandwidth during merging and the efficiency cores reduce it further. Making
the merge itself parallel is the way past this ceiling.

---

## Task 2 — static vs dynamic vs guided scheduling

Each element gets the handout's expensive update `x = x * 1.0000001 + 0.0000001`
repeated `work[i]` times. Three workloads are compared:
- **uniform:** every element costs the same.
- **sawtooth:** the handout's `(i % 1000)` pattern.
- **ramp:** cost grows linearly with `i`, so late iterations are heavy.

The handout's literal `(i % 1000) * 100` costs about 50 000 steps per element,
which would run for minutes at N = 10^7. The work is therefore scaled so every N
does the same total of 2·10^8 steps. The average work per element is in the
`avg work` column, and the program's 2nd argument overrides it. Results are
bit-identical to serial for every schedule (`ok`).

**Uniform workload**

| N | threads | avg work | serial (s) | static (s) | S | E | dynamic,100 (s) | S | E | guided (s) | S | E |
|---:|:---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10^5 | 1 | 2000 | 0.16298 | 0.15903 | 1.02 | 1.02 | 0.15855 | 1.03 | 1.03 | 0.15838 | 1.03 | 1.03 |
| 10^5 | 2 | 2000 | 0.16298 | 0.08128 | 2.01 | 1.00 | 0.08171 | 1.99 | 1.00 | 0.08252 | 1.97 | 0.99 |
| 10^5 | 4 | 2000 | 0.16298 | 0.04201 | 3.88 | 0.97 | 0.04192 | 3.89 | 0.97 | 0.04192 | 3.89 | 0.97 |
| 10^5 | 8 | 2000 | 0.16298 | 0.02832 | 5.75 | 0.72 | 0.02639 | 6.17 | 0.77 | 0.02671 | 6.10 | 0.76 |
| 10^6 | 1 | 200 | 0.10851 | 0.10642 | 1.02 | 1.02 | 0.10650 | 1.02 | 1.02 | 0.10496 | 1.03 | 1.03 |
| 10^6 | 2 | 200 | 0.10851 | 0.05394 | 2.01 | 1.01 | 0.05440 | 1.99 | 1.00 | 0.05426 | 2.00 | 1.00 |
| 10^6 | 4 | 200 | 0.10851 | 0.02816 | 3.85 | 0.96 | 0.02772 | 3.91 | 0.98 | 0.02752 | 3.94 | 0.99 |
| 10^6 | 8 | 200 | 0.10851 | 0.02074 | 5.23 | 0.65 | 0.01917 | 5.66 | 0.71 | 0.01948 | 5.57 | 0.70 |
| 10^7 | 1 | 20 | 0.06080 | 0.05717 | 1.06 | 1.06 | 0.05866 | 1.04 | 1.04 | 0.05927 | 1.03 | 1.03 |
| 10^7 | 2 | 20 | 0.06080 | 0.02909 | 2.09 | 1.04 | 0.02969 | 2.05 | 1.02 | 0.03004 | 2.02 | 1.01 |
| 10^7 | 4 | 20 | 0.06080 | 0.01502 | 4.05 | 1.01 | 0.01543 | 3.94 | 0.98 | 0.01540 | 3.95 | 0.99 |
| 10^7 | 8 | 20 | 0.06080 | 0.01023 | 5.94 | 0.74 | 0.01167 | 5.21 | 0.65 | 0.00939 | 6.48 | 0.81 |

**Sawtooth workload (handout pattern)**

| N | threads | avg work | serial (s) | static (s) | S | E | dynamic,100 (s) | S | E | guided (s) | S | E |
|---:|:---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10^5 | 1 | 2000 | 0.15922 | 0.15710 | 1.01 | 1.01 | 0.15633 | 1.02 | 1.02 | 0.15455 | 1.03 | 1.03 |
| 10^5 | 2 | 2000 | 0.15922 | 0.08049 | 1.98 | 0.99 | 0.08132 | 1.96 | 0.98 | 0.08169 | 1.95 | 0.97 |
| 10^5 | 4 | 2000 | 0.15922 | 0.04180 | 3.81 | 0.95 | 0.04190 | 3.80 | 0.95 | 0.04188 | 3.80 | 0.95 |
| 10^5 | 8 | 2000 | 0.15922 | 0.02785 | 5.72 | 0.71 | 0.02644 | 6.02 | 0.75 | 0.02647 | 6.01 | 0.75 |
| 10^6 | 1 | 200 | 0.11491 | 0.11310 | 1.02 | 1.02 | 0.11268 | 1.02 | 1.02 | 0.11149 | 1.03 | 1.03 |
| 10^6 | 2 | 200 | 0.11491 | 0.05781 | 1.99 | 0.99 | 0.05802 | 1.98 | 0.99 | 0.05819 | 1.97 | 0.99 |
| 10^6 | 4 | 200 | 0.11491 | 0.02972 | 3.87 | 0.97 | 0.02937 | 3.91 | 0.98 | 0.02945 | 3.90 | 0.98 |
| 10^6 | 8 | 200 | 0.11491 | 0.02115 | 5.43 | 0.68 | 0.02001 | 5.74 | 0.72 | 0.02018 | 5.70 | 0.71 |
| 10^7 | 1 | 20 | 0.06455 | 0.05842 | 1.11 | 1.11 | 0.06149 | 1.05 | 1.05 | 0.05847 | 1.10 | 1.10 |
| 10^7 | 2 | 20 | 0.06455 | 0.02956 | 2.18 | 1.09 | 0.03037 | 2.13 | 1.06 | 0.02946 | 2.19 | 1.10 |
| 10^7 | 4 | 20 | 0.06455 | 0.01514 | 4.26 | 1.07 | 0.01580 | 4.09 | 1.02 | 0.01528 | 4.22 | 1.06 |
| 10^7 | 8 | 20 | 0.06455 | 0.01088 | 5.93 | 0.74 | 0.01176 | 5.49 | 0.69 | 0.00968 | 6.67 | 0.83 |

**Ramp workload (cost grows with i)**

| N | threads | avg work | serial (s) | static (s) | S | E | dynamic,100 (s) | S | E | guided (s) | S | E |
|---:|:---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10^5 | 1 | 2000 | 0.15875 | 0.15709 | 1.01 | 1.01 | 0.15653 | 1.01 | 1.01 | 0.15710 | 1.01 | 1.01 |
| 10^5 | 2 | 2000 | 0.15875 | 0.12385 | 1.28 | 0.64 | 0.08058 | 1.97 | 0.99 | 0.08149 | 1.95 | 0.97 |
| 10^5 | 4 | 2000 | 0.15875 | 0.07317 | 2.17 | 0.54 | 0.04248 | 3.74 | 0.93 | 0.04191 | 3.79 | 0.95 |
| 10^5 | 8 | 2000 | 0.15875 | 0.04226 | 3.76 | 0.47 | 0.02650 | 5.99 | 0.75 | 0.02645 | 6.00 | 0.75 |
| 10^6 | 1 | 200 | 0.11356 | 0.11236 | 1.01 | 1.01 | 0.11335 | 1.00 | 1.00 | 0.11183 | 1.02 | 1.02 |
| 10^6 | 2 | 200 | 0.11356 | 0.08879 | 1.28 | 0.64 | 0.05754 | 1.97 | 0.99 | 0.05764 | 1.97 | 0.99 |
| 10^6 | 4 | 200 | 0.11356 | 0.05645 | 2.01 | 0.50 | 0.02948 | 3.85 | 0.96 | 0.02989 | 3.80 | 0.95 |
| 10^6 | 8 | 200 | 0.11356 | 0.03328 | 3.41 | 0.43 | 0.02061 | 5.51 | 0.69 | 0.02095 | 5.42 | 0.68 |
| 10^7 | 1 | 20 | 0.05667 | 0.05530 | 1.02 | 1.02 | 0.05814 | 0.97 | 0.97 | 0.05577 | 1.02 | 1.02 |
| 10^7 | 2 | 20 | 0.05667 | 0.04189 | 1.35 | 0.68 | 0.03036 | 1.87 | 0.93 | 0.02782 | 2.04 | 1.02 |
| 10^7 | 4 | 20 | 0.05667 | 0.02413 | 2.35 | 0.59 | 0.01575 | 3.60 | 0.90 | 0.01446 | 3.92 | 0.98 |
| 10^7 | 8 | 20 | 0.05667 | 0.01454 | 3.90 | 0.49 | 0.01242 | 4.56 | 0.57 | 0.00935 | 6.06 | 0.76 |

**Chunk-size study** — N = 10^6, 8 threads, `schedule(runtime)` with the kind and
chunk set by `omp_set_schedule()`:

| kind | chunk | uniform (s) | speedup | ramp (s) | speedup |
|---|---:|---:|---:|---:|---:|
| dynamic | 1 | 0.05496 | 1.96 | 0.05597 | 2.05 |
| dynamic | 10 | 0.02032 | 5.31 | 0.02172 | 5.29 |
| dynamic | 100 | 0.02018 | 5.35 | 0.02107 | 5.45 |
| dynamic | 1000 | 0.01904 | 5.67 | 0.02013 | 5.70 |
| dynamic | 10000 | 0.01905 | 5.66 | 0.02091 | 5.49 |
| dynamic | 125000 | 0.01989 | 5.42 | 0.03149 | 3.65 |
| static | 1 | 0.02063 | 5.23 | 0.02454 | 4.68 |
| static | 10 | 0.02246 | 4.80 | 0.02206 | 5.20 |
| static | 100 | 0.02125 | 5.08 | 0.02391 | 4.80 |
| static | 1000 | 0.02396 | 4.50 | 0.02486 | 4.62 |
| static | 10000 | 0.02363 | 4.57 | 0.02622 | 4.38 |
| static | 125000 | 0.02456 | 4.39 | 0.03597 | 3.19 |

**Q: What is the difference between static, dynamic and guided scheduling?**
- `schedule(static)` divides the iterations among the threads *before* the loop
  runs: one contiguous block of about N/p per thread, or round-robin chunks of c
  with `static, c`. It has no run-time cost and good locality, but it cannot
  react if some blocks take longer.
- `schedule(dynamic, c)` hands out chunks of c iterations at run time: whenever
  a thread finishes a chunk it takes the next one from a shared counter. Load
  balances itself, at the cost of one synchronised grab per chunk. The default
  c is 1.
- `schedule(guided[, c])` is dynamic with shrinking chunks. Each grab takes
  about remaining/threads iterations (never fewer than c). It starts with big
  chunks (few grabs) and finishes with small ones (good balance at the end).

**Q: Which strategy performs best for uniform workloads? For uneven workloads?**
*Uniform:* all three are close (S ≈ 5.2–6.5 at 8 threads, no consistent winner).
Static needs no balancing and has zero overhead, so it is the right default.
Dynamic,100 is measurably slower at N = 10^7 (5.21 vs 5.94), where each
iteration is cheap and the grabs show up.
*Uneven:* it depends on the *shape* of the imbalance:
- **Sawtooth (the handout's pattern):** static is still fine. Each static block
  of N/p elements contains thousands of complete 0…999 periods, so every thread
  gets almost the same total work. All three schedules tie.
- **Ramp:** static gives the last thread the heaviest block. With p threads the
  last block holds (2p−1)/p² of the work, which caps static's speedup at
  p²/(2p−1) = 1.33, 2.29 and 4.27 for 2, 4 and 8 threads. Measured at
  N = 10^6: 1.28, 2.01, 3.41 (efficiency 0.43 at 8 threads). Dynamic and guided
  rebalance at run time and reach about 5.5x. With the cheapest iterations
  (N = 10^7, avg 20 steps), guided wins clearly: 6.06 vs 4.56 for dynamic,100.
  It gets dynamic's balance with far fewer grabs.

**Q: Why can dynamic scheduling introduce additional overhead?**
Every chunk handed out is an atomic update of a shared iteration counter in the
runtime. The cache line holding the counter bounces between cores, and threads
queue behind each other when they finish at the same time. The cost is paid once
per chunk, so with a small chunk and cheap iterations it dominates: dynamic,1
reaches only about 2x at 8 threads (chunk-size table) while static reaches 5.2x
on the same uniform loop. Tiny chunks also scatter neighbouring iterations over
threads. That loses cache locality and makes threads write adjacent elements of
the same cache line (false sharing on the output array).

**Q: How does the chunk size affect performance?**
It trades overhead against balance:
- **Small chunks:** many grabs. Chunk 1 is overhead-bound at about 2x.
- **Chunks of 10–10 000:** the best region, 5.3–5.7x on both workloads.
- **Large chunks:** too few chunks to balance. At chunk = N/p = 125 000 each
  thread gets exactly one chunk, which is the same as plain static, and the ramp
  drops back to 3.65x.

The same holds for `static, c`. Small round-robin chunks spread the ramp's heavy
iterations evenly (static,1: 4.68x vs 3.19x at static,125000). They cost no
scheduling overhead, but they do cost locality. A good rule is to pick c so there
are many more chunks than threads (10–100 per thread) and each chunk is much
more work than one grab.

---

## Task 3 — Even/odd count

Values are signed pseudo-random ints, roughly half even. In C, `a[i] % 2` is −1
for a negative odd number, so the test is `== 0` / `else`, never `== 1`.

| N | threads | serial (s) | reduction (s) | S | E | atomic (s) | S | E | local + atomic (s) | S | correct |
|---:|:---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|:---:|
| 10^5 | 1 | 0.000230 | 0.000234 | 0.98 | 0.98 | 0.000271 | 0.85 | 0.85 | 0.000246 | 0.93 | yes |
| 10^5 | 2 | 0.000230 | 0.000127 | 1.81 | 0.91 | 0.000375 | 0.61 | 0.31 | 0.000149 | 1.54 | yes |
| 10^5 | 4 | 0.000230 | 0.000095 | 2.42 | 0.61 | 0.000664 | 0.35 | 0.09 | 0.000097 | 2.37 | yes |
| 10^5 | 8 | 0.000230 | 0.000100 | 2.30 | 0.29 | 0.002652 | 0.09 | 0.01 | 0.000114 | 2.02 | yes |
| 10^6 | 1 | 0.002572 | 0.002574 | 1.00 | 1.00 | 0.002902 | 0.89 | 0.89 | 0.002590 | 0.99 | yes |
| 10^6 | 2 | 0.002572 | 0.001321 | 1.95 | 0.97 | 0.003876 | 0.66 | 0.33 | 0.001320 | 1.95 | yes |
| 10^6 | 4 | 0.002572 | 0.000687 | 3.74 | 0.94 | 0.006573 | 0.39 | 0.10 | 0.000708 | 3.63 | yes |
| 10^6 | 8 | 0.002572 | 0.000457 | 5.63 | 0.70 | 0.030675 | 0.08 | 0.01 | 0.000474 | 5.43 | yes |
| 10^7 | 1 | 0.026231 | 0.026135 | 1.00 | 1.00 | 0.029127 | 0.90 | 0.90 | 0.025926 | 1.01 | yes |
| 10^7 | 2 | 0.026231 | 0.013263 | 1.98 | 0.99 | 0.038564 | 0.68 | 0.34 | 0.013239 | 1.98 | yes |
| 10^7 | 4 | 0.026231 | 0.006704 | 3.91 | 0.98 | 0.066426 | 0.39 | 0.10 | 0.006720 | 3.90 | yes |
| 10^7 | 8 | 0.026231 | 0.004101 | 6.40 | 0.80 | 0.268616 | 0.10 | 0.01 | 0.004162 | 6.30 | yes |
| 10^8 | 1 | 0.263900 | 0.262831 | 1.00 | 1.00 | 0.292797 | 0.90 | 0.90 | 0.261885 | 1.01 | yes |
| 10^8 | 2 | 0.263900 | 0.133827 | 1.97 | 0.99 | 0.398722 | 0.66 | 0.33 | 0.133029 | 1.98 | yes |
| 10^8 | 4 | 0.263900 | 0.067620 | 3.90 | 0.98 | 0.696376 | 0.38 | 0.09 | 0.067158 | 3.93 | yes |
| 10^8 | 8 | 0.263900 | 0.038479 | 6.86 | 0.86 | 2.859597 | 0.09 | 0.01 | 0.039788 | 6.63 | yes |

**Q: How can multiple variables be included in a reduction clause?**
List them after one operator, `reduction(+:even, odd)`. Use one clause per
operator when they differ: `reduction(+:sum) reduction(max:mx)`. Each listed
variable gets a private copy per thread, initialised to the operator's identity
(0 for `+`). Threads update only their own copies, and the copies are combined
into the original variables once, at the end of the loop.

**Q: Why is the reduction approach generally faster than using atomic updates?**
With `reduction` the hot loop updates thread-private counters held in registers.
There is no sharing at all, and only p combines happen at the end. With
`atomic`, *every* iteration does an atomic read-modify-write on a shared
variable. `even` and `odd` sit on the same cache line, so the line ping-pongs
between cores on every increment and the updates are serialised. The table shows
it: atomic is only 10% slower than serial on 1 thread (uncontended), but it gets
*slower* as threads are added. At N = 10^8 with 8 threads it takes 2.86 s
against 0.038 s for the reduction, 74x slower and 11x slower than serial. The
local + atomic version does what `reduction` does internally (private counters,
then one atomic add per thread) and matches reduction's speed.

**Q: Why must the sum of the even and odd counts equal N?**
Every integer is either even or odd, never both. Each iteration increments
exactly one of the two counters, and a worksharing loop executes every iteration
exactly once. The two counts therefore partition the N elements. That makes
`even + odd == N` a cheap correctness check: a race that lost updates (for
example plain `even++` on shared counters) would make the sum come out below N.
The program checks it for every version.

**Q: Does increasing the number of threads always improve performance?**
No:
- **Small N:** at N = 10^5 the reduction gives 2.42x at 4 threads but only 2.30x
  at 8. The loop takes about 0.2 ms, so the fork/join cost of the extra threads
  outweighs their share of the work.
- **Contention:** the atomic version gets slower with every thread added
  (0.29 s, 0.40 s, 0.70 s, 2.86 s at N = 10^8).
- **Memory-bound loops** stop scaling once memory bandwidth is saturated.
- **Hardware:** more threads than (performance) cores adds slower cores or
  time-slicing instead of real parallelism.

With a large N and a race-free reduction, scaling is good: 6.9x at 8 threads for
N = 10^8.

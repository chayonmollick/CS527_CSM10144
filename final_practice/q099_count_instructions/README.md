# Q099 — Count all processors' instructions: critical, atomic or per-processor

## Question
With the processors running in parallel (each round's slices in an OpenMP parallel
for), count the total number of instructions executed by all processors in three ways:
a shared counter updated inside `#pragma omp critical`, a shared counter updated with
`#pragma omp atomic`, and one counter per processor added up at the end (like a
reduction). Print all three totals when the simulation ends. Which are correct, and
which is fastest? Measure it.

## Answer
The base is the minimal parallel round from Q098: `threadprivate` for the
current-instruction globals, `critical(frames)`, `critical(log_file)`, and
`#pragma omp parallel for` over the round. Then `execute()` counts every instruction
three ways:

| Way | Code | Why it is correct | Cost |
|---|---|---|---|
| 1 | `#pragma omp critical(count)` around `count_critical++` | a lock: one thread at a time | lock + unlock per instruction; threads queue for the same lock |
| 2 | `#pragma omp atomic` on `count_atomic++` | one indivisible read-add-write in hardware (e.g. ARM `LDADD`, x86 `LOCK ADD`) | no lock, but the counter's cache line moves to the core of every thread that adds |
| 3 | `count_proc[p]++`, summed in `scheduler()` at the end | each task runs once per round, so processor `p` is run by **one** thread at a time and nothing else touches `count_proc[p]`. The barrier at the end of each round makes it visible to the next | a plain add, no synchronisation |

Way 3 is what `reduction(+:x)` does: private partial sums, combined once at the end.

**All three are correct.** Every run prints the same total, equal to the sum of the
per-task counts (40020 for the test workload, 4N + 5 per task). Without protection,
`total++` is a load, an add and a store. Two threads interleave them, and an update is
lost. The benchmark shows only 1,528,203 of 4,000,000 counted.

**Fastest:** the per-processor counters, then atomic, then critical. Inside the
simulation the difference is invisible. Each thread runs only 10 instructions between
`usleep` calls, so the lock is rarely contended. While preparing this answer, a build
with no counter and builds with only one of the three ways all ran 4 × 8006
instructions on 4 threads in 32–34 ms. So `./sim -k N` times each way on its own:
every thread adds 1 N times in a tight loop. With 4 threads and N = 10^6:
- critical: 72 ms;
- atomic: 44 ms;
- per-processor: 1.2 ms, 36× faster than atomic and 60× faster than critical.

Even with 1 thread an uncontended lock costs about 4.5 ns per update. Use `atomic` for a
single shared update that cannot be made private, and `critical` for an update of
several statements (like the free-frame list).

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (globals) | `extern long count_critical, count_atomic, count_proc[NP]`; `count_benchmark()` |
| `processor.cpp` | `execute()` | the three increments: `critical(count)`, `atomic`, `count_proc[p]++` |
| `processor.cpp` | `count_benchmark()` (new) | `-k N`: the three ways and an unprotected counter, each in its own parallel region, timed with `omp_get_wtime()` |
| `os.cpp` | `scheduler()` | parallel round (as Q098); at the end sum `count_proc[]` and print the three totals |
| `processor.h`, `processor.cpp`, `os.cpp` | | `threadprivate`, `critical(log_file)`, `critical(frames)`, as in Q098 |
| `main.cpp` | `main()`, `usage()` | option `-k N` |
| `Makefile` | | OpenMP as in e22: `g++ -fopenmp`; on macOS Homebrew `g++-<N>`, else clang++ + libomp |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`.

## Test
`./test.sh` runs 4 tasks of `count_loop.txt` (N = 1000, 2000, 3000, 4000 from
`n1000.byte` … `n4000.byte`) with 1 and 4 threads, then 10 more times with 4 threads.
All three totals must equal 40020 on every run, as must the sum of the per-task counts.
Then it runs the `-k` benchmark with 1 and 4 threads. The base tests pass 33 of 33 with
1 thread; with 4 threads 32 pass, because only the round-robin log order differs.

Timings are machine-specific: Apple M4 (4 performance + 6 efficiency cores), macOS,
Homebrew GCC 15, with other jobs running.
```
  ok    builds without warnings
  time  4 tasks (40020 instructions), 1 thread(s): 0.068 s (machine-specific, not checked)
        [os] instructions executed by all processors: critical 40020, atomic 40020, per-processor sum 40020
  ok    1 thread(s): critical, atomic and per-processor all = 40020
  ok    1 thread(s): the per-task counts add up to the same total
  ok    1 thread(s): results 3N = 3000 6000 9000 12000
  time  4 tasks (40020 instructions), 4 thread(s): 0.054 s (machine-specific, not checked)
        [os] instructions executed by all processors: critical 40020, atomic 40020, per-processor sum 40020
  ok    4 thread(s): critical, atomic and per-processor all = 40020
  ok    4 thread(s): the per-task counts add up to the same total
  ok    4 thread(s): results 3N = 3000 6000 9000 12000
  ok    10 more runs with 4 threads: no update ever lost
        counting benchmark: 1 thread(s) x 1000000 = 1000000 increments each way
          no protection      1000000            1.043 ms
          critical           1000000            4.495 ms
          atomic             1000000            1.947 ms
          per-processor      1000000            0.266 ms
  ok    benchmark, 1 thread(s): critical, atomic, per-processor all = 1000000
        counting benchmark: 4 thread(s) x 1000000 = 4000000 increments each way
          no protection      1528203  WRONG     1.871 ms
          critical           4000000           72.143 ms
          atomic             4000000           43.647 ms
          per-processor      4000000            1.192 ms
  ok    benchmark, 4 thread(s): critical, atomic, per-processor all = 4000000
  note  'no protection' with 4 threads is a data race: usually WRONG, not checked
  ok    the old programs still work (sum_array = 45)

11 passed, 0 failed
```

## Viva
- **Why does `count_proc[p]++` need no synchronisation?** A round runs each task once, so processor `p` is in one iteration of the parallel for and is touched by one thread. Between rounds, the implicit barrier (with its flush) hands the value to whichever thread runs `p` next.
- **atomic vs critical?** `atomic` covers one simple update (`x++`, `x += e`) and becomes a single hardware instruction. `critical` locks a whole block of statements. With 4 threads both slow down a lot, because every update has to fetch the same cache line from another core.
- **Why is the unprotected total always too small, never too big?** Each lost update is a store that overwrote another thread's increment, and no interleaving adds an extra 1.
- **False sharing:** `count_proc[NP]` is 4 longs (32 bytes), which usually share one 64-byte cache line, so 4 cores writing their own element still pass the line around. The benchmark spaces its counters 64 bytes apart for this reason.
- **An even simpler reduction?** `process_instructions()` already returns how many instructions it ran. `#pragma omp parallel for reduction(+:total)` with `total += process_instructions(...)` counts once per slice instead of once per instruction.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `count_loop.txt`, `n1000.byte`, `n2000.byte`, `n3000.byte`, `n4000.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

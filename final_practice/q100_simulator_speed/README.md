# Q100 — Measure the simulator's speed with 1 and 4 threads

## Question
With each round's time slices running in parallel (OpenMP), measure how fast the
simulator is: simulated instructions per second, with 1 and with 4 threads, on a heavy
workload of several long-running tasks. Report the speedup and explain why it is less
than 4. Then make the time slice a command-line option, repeat the measurement for
slices of 10, 100, 1000 and 10000 instructions, and explain the trend.

## Answer
**Constructs:**
- The base is the minimal parallel round from Q098: `#pragma omp parallel for` over the round, `threadprivate` for the current-instruction globals, `critical(frames)`, `critical(log_file)`.
- `scheduler()` reads `omp_get_wtime()` (wall-clock seconds) at its start and end, adds up the tasks' instruction counts, and prints `[os] speed: N instructions in T s = R million instructions/s (P thread(s), time slice S)`.
- `-s N` sets a variable `time_slice`, which the round uses instead of the constant `TIME_SLICE`.

Measured on this machine (one run, below; machine-specific): 0.68 million instructions/s
with 1 thread and 1.35 with 4 at the default slice of 10, a **speedup of 2.0**.

**Why not 4, with 10-instruction slices.** One slice is about 0.3 µs of work, but the
overheads are much larger, as a round's cost shows:
- **1 thread:** 4 × (`usleep(10)` ≈ 14 µs + 0.3 µs) ≈ 57 µs.
- **4 threads:** a fork/join of about 13 µs (an empty `parallel for` with 4 threads; libgomp on macOS) + one 14 µs sleep, because the four sleeps overlap, + the serial bookkeeping ≈ 29 µs.

So the limit is 57 / 29 ≈ 2, and most of the gain comes from **sleeping at the same
time**, not from computing at the same time. By Amdahl's law, a speedup of 2 on
4 threads means a third of the time behaves as serial.

**TIME_SLICE trend.** Longer slices mean fewer rounds per instruction, so fewer sleeps
and fork/joins. The 1-thread speed rises 45× (0.68 → 30.9 million/s). But the speedup
**falls** to 1.1× at 10000: now the 4 threads really compute at the same time, and they
slow each other down by **false sharing**. `PC[NP]`, `steps[NP]`, `FlagZ/N/C/V[NP]` and
`end_of_simulation[NP]` keep the 4 processors' values next to each other, in the same
cache lines. Every instruction writes `PC[p]` and `steps[p]`, so those lines bounce
between the cores. The proof:
- the same work as **4 separate processes**, which share no memory, takes 0.094 s, against 0.271 s for 1 thread (2.9×), while 4 threads in one process take 0.203 s (1.3×);
- while preparing this answer, padding those arrays to one 64-byte cache line per processor raised the 4-thread speedup for 4 big tasks at slice 10000 from 1.1× to 2.3×. It is not in the patch, because it changes every declaration.

Other limits: the serial part of each round (finishing, re-queueing, `shell()`,
`admit_waiting()`); tasks that finish at different times; only 4 performance cores on
this Apple M4 (+6 efficiency cores); other programs running.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `scheduler()` | parallel round (as Q098) using `time_slice`; `omp_get_wtime()` at start and end; prints the speed line |
| `os.cpp`, `os.h` | (globals) | `int time_slice = TIME_SLICE;`, `extern` in `os.h` |
| `main.cpp` | `main()`, `usage()` | option `-s N` (N > 0, else the usage message and exit code 1) |
| `processor.h`, `processor.cpp`, `os.cpp` | | `threadprivate`, `critical(log_file)`, `critical(frames)`, as in Q098 |
| `Makefile` | | OpenMP as in e22: `g++ -fopenmp`; on macOS Homebrew `g++-<N>`, else clang++ + libomp |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`.

## Test
`./test.sh` runs 4 tasks of `spin.txt` (N = 20000 from `spin_data.byte`: 80005
instructions each, result 60000). It uses slices of 10, 100, 1000 and 10000, each with 1
and 4 threads, best of 3 runs, and checks every run's results and total
(24 runs × 4 tasks). Then 4 big tasks (`big_data.byte`, N = 500000) run as 1 thread, as
4 threads, and as 4 separate processes. The base tests pass 33 of 33 with 1 thread; with
4 threads 32 pass, because only the round-robin log order differs.

Timings are machine-specific: Apple M4 (4 performance + 6 efficiency cores), macOS,
Homebrew GCC 15, with other jobs running. Runs vary by ±30%; the trend is stable.
```
  ok    builds without warnings
  ok    the default time slice is 10
  ok    -s 0 is rejected
  TIME_SLICE experiment: 4 tasks x 80005 instructions, best of 3 runs, machine-specific
      slice   1 thread  4 threads   speedup    (million simulated instructions per second)
         10       0.68       1.35      2.00
        100       5.73      11.10      1.94
       1000      22.17      33.61      1.52
      10000      30.91      34.59      1.12
  ok    all 24 runs: every task stored 60000, 320020 instructions in all
  ok    4 big tasks with 4 threads: each stored 1500000
  ok    4 separate processes: each stored 1500000
  time  4 x 2000005 instructions, slice 10000: 1 thread 0.271 s, 4 threads 0.203 s, 4 processes 0.094 s
  ok    the old programs still work (sum_array = 45)

7 passed, 0 failed
```

## Viva
- **Why `omp_get_wtime()` and not `clock()`?** `clock()` is CPU time summed over all threads, so it hides any speedup. The wall-clock time is what the user waits for.
- **Speedup and efficiency at slice 10?** S = T1 / T4 = 2.0; E = S / 4 = 50%.
- **Why does the speedup drop for long slices, when the overheads matter less?** The threads now execute at the same time and keep stealing the cache lines of `PC[]`, `steps[]` and the flags from each other (false sharing). Separate processes, with no shared memory, do scale.
- **How would you get closer to 4×?**
  - Keep each processor's state in one 64-byte-aligned struct.
  - Drop the `usleep`, which is only there to watch the run.
  - Use longer slices.
  - Keep one parallel region for the whole run, with `#pragma omp barrier` / `single` per round, instead of a fork/join every round.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `big_data.byte`, `spin.txt`, `spin_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

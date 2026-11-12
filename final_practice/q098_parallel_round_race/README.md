# Q098 — Run each round's time slices in parallel (minimal) and show the race

## Question
The scheduler gives every ready task one time slice per round, but runs the slices one
after another. Make the smallest change that runs the slices of a round at the same
time with OpenMP, one thread per task. Find every shared variable that becomes a data
race and protect it with `threadprivate` or `critical`. Show that the results are the
same as with one thread, and demonstrate what goes wrong when a protection is removed.

## Answer
**Construct:** in `scheduler()`, copy the round (the ready queue) into a vector and run
`process_instructions(p, TIME_SLICE)` for its tasks in a `#pragma omp parallel for`.
After the loop's implicit barrier (every slice finished, its writes visible), finishing
and re-queueing run serially in the old order. The slices are independent: a task owns
its processor `p`, and `Register`, `PC`, flags, `steps` and `exec_count` are `[NP]`
arrays indexed by `p`. It also owns its frames and its 10 page-table bytes in frame 0.

The shared state that races, and its fix:

| Shared state | Why it races | Fix |
|---|---|---|
| `proc_id, opcode, dest, src1, src2` (processor.h), `instr_pc` | One copy for "the instruction being executed", used by every thread | `#pragma omp threadprivate(...)`: one copy per thread |
| `freePages[]` | A data-page fault calls `getFreePage()` (find a 0, set it to 1); two threads can take the same frame | `#pragma omp critical(frames)` on that call only. The `memset` and page-table write after it touch only this task's new frame |
| `fd_log` | `print` does `fprintf` + `fflush` from several threads | `#pragma omp critical(log_file)` |

There is no race on `memory[]` (the frames are disjoint, and so are the page-table
bytes), on `tasks` (only read during the loop), or in the loader and shell, which run
between rounds.

**Race demonstration** (the test deletes the two `threadprivate` lines and builds again):

| Step | Thread A runs processor 0 | Thread B runs processor 1 |
|---|---|---|
| 1 | `proc_id = 0` | |
| 2 | | `proc_id = 1` (the only copy) |
| 3 | `fetch()` reads `PC[proc_id]`: processor 1's PC | |
| 4 | | `fetch()` overwrites `opcode, dest, src1, src2` |
| 5 | `execute()` runs B's instruction on `Register[1]` | `execute()` runs it again |

So processor 1 executes some instructions twice and processor 0 none. The sums come out
wrong, and tasks stop after the wrong number of instructions. With `BNE` loops the
counter can jump past its limit and loop for ever, which is why `long_loop.txt` uses
`BLT`. With one thread nothing interleaves, so the broken build is still right.

Removing `critical(frames)` instead gave **0 wrong runs out of 100** here (3 tasks
faulting at once): the window is a few nanoseconds wide. A race that the tests miss is
still a bug, so it must be found by reasoning about shared data.

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (globals) | `#pragma omp threadprivate(proc_id, opcode, dest, src1, src2)` |
| `processor.cpp` | (globals) | `#pragma omp threadprivate(instr_pc)` |
| `processor.cpp` | `execute()`, `OP_PRINT` | `#pragma omp critical(log_file)` around the write and flush |
| `os.cpp` | `getPhysicalAddress()` | `#pragma omp critical(frames)` on `frame = getFreePage();` |
| `os.cpp` | `scheduler()` | `round` = copy of the ready queue; `#pragma omp parallel for` over it; then finish or re-queue serially |
| `Makefile` | | OpenMP as in e22: `g++ -fopenmp`; on macOS Homebrew `g++-<N>`, else clang++ + libomp |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`.

## Test
`./test.sh` runs 4 × `long_loop.txt` (8006 instructions each, result 6000) with 1 and 4
threads, then 10 more times with 4 threads. It then runs a mixed workload (vectors,
`print`, page faults) and 10 runs of 3 tasks page-faulting at once. Last comes the race
demonstration, whose result is printed but not checked, because a race does not show on
every run. The base tests also pass: `OMP_NUM_THREADS=1 SIM=$PWD/sim
../minicomputer/tests/run_tests.sh` gives 33 of 33. With 4 threads 32 pass; the
round-robin log-order check fails, because only the order inside a round changes.

Timings are machine-specific: Apple M4 (4 performance + 6 efficiency cores), macOS,
Homebrew GCC 15, with other jobs running.
```
  ok    builds without warnings
  time  4 x long_loop, 1 thread(s): 0.057 s (machine-specific, not checked)
  ok    1 thread(s): all 4 tasks computed 6000
  ok    1 thread(s): every task ran 8006 instructions
  time  4 x long_loop, 4 thread(s): 0.034 s (machine-specific, not checked)
  ok    4 thread(s): all 4 tasks computed 6000
  ok    4 thread(s): every task ran 8006 instructions
  ok    10 more runs with 4 threads: none wrong
  ok    4 threads: fir out[15] = 744
  ok    4 threads: array_add C[15] = 131
  ok    4 threads: sum_array_print = 45
  ok    4 threads: pages (3 page faults) = 21
  ok    4 threads: log.txt has all 9 print lines
  ok    10 runs x 3 tasks with simultaneous page faults all = 21
  ok    without threadprivate, 1 thread is still right (no second thread)
  race  without threadprivate, 4 threads: 5 of 5 runs wrong; last run: 5250,3,6000,6000,
        instruction counts: 9 7008 8006 8006 (8006 expected)
  ok    the old programs still work (sum_array = 45)

14 passed, 0 failed
```

## Viva
- **Why `threadprivate` and not `private`?** `private(proc_id)` on the loop would only affect uses written inside the loop in `scheduler()`. `fetch()` and `execute()` in processor.cpp would still use the single global. `threadprivate` makes the global itself one-per-thread everywhere.
- **Why is the critical section only around `getFreePage()`?** Once a frame is marked taken it belongs to this task, so clearing it and writing the page-table entry need no lock. A short critical section means less waiting.
- **Why does the race vanish with 1 thread, and why run 10 times?** A race needs two threads touching the same data at the same moment. One thread never interleaves, and with several threads the interleaving changes from run to run.
- **Why only 1.7× faster, not 4×?** A slice is about 0.4 µs of work, but each one sleeps `usleep(10)` (about 14 µs here), and every round pays a fork/join (about 13 µs with libgomp on macOS). Q100 measures this.
- **Why the default (static) schedule?** The slices are all 10 instructions. Static keeps task *i* of the round on thread *i*, so a processor's registers stay in the same core's cache.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `fir.txt`, `fir_data.byte`, `long_loop.txt`, `long_loop_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `sum_array_print.txt`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

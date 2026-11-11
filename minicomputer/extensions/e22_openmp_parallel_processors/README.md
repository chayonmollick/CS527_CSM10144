# e22 — Run the simulated processors in parallel with OpenMP

## Question (as it could appear in the test)
The simulator has NP = 4 processors, but the scheduler runs their time slices one
after another, so they never really execute at the same time. Use OpenMP so that in
every scheduler round the ready tasks' time slices run in parallel, one thread per
processor. Identify the shared state that becomes a data race and protect it. Show
that every program still produces the same results, and compare the wall-clock time
with 1 and 4 threads.

## Solution idea
- **Parallel round:** a round already gives each ready task one time slice, and each
  task sits on its own processor with its own registers (all `[NP]` arrays) and its own
  frames. So the slices of one round are independent:
  `#pragma omp parallel for schedule(dynamic, 1)` over the round's ready tasks. Removing
  finished tasks and re-queueing the others stays serial after the loop.
- **Race 1, the current-instruction globals:** `proc_id`, `opcode`, `dest`, `src1`,
  `src2` and `instr_pc` describe the instruction being executed. They were single
  globals, so two threads would overwrite each other's instruction. Fix:
  `#pragma omp threadprivate(...)`, one copy per thread.
- **Race 2, the free-frame list:** a page fault calls `getFreePage()`, which scans and
  updates the shared `freePages[]`. Two processors faulting at once could both take the
  same frame. Fix: `#pragma omp critical(frames)` around taking the frame and writing
  the page-table entry.
- **Race 3, the log file:** `print` writes and flushes it. `#pragma omp critical(log_file)`
  keeps each line whole.
- **No race:** `memory[]` itself. Tasks' frames are disjoint, and each processor's page
  table is its own 10 bytes of frame 0.
- **What changes:** only the *order* of lines in `log.txt` within a round. Results, the
  `.out` files and the `.freq` files are identical.

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (globals) | `#pragma omp threadprivate(proc_id, opcode, dest, src1, src2)` |
| `processor.cpp` | (globals) | `#pragma omp threadprivate(instr_pc)` |
| `processor.cpp` | `execute()` `OP_PRINT` | `#pragma omp critical(log_file)` around the write |
| `os.cpp` | `getPhysicalAddress()` | `#pragma omp critical(frames)` around `getFreePage()` + page-table update |
| `os.cpp` | `scheduler()` | the round's slices run in `#pragma omp parallel for` |
| `Makefile` | | `-fopenmp` (finds Homebrew g++ or clang + libomp on macOS) |

## Run
```bash
make
OMP_NUM_THREADS=1 ./sim -b heavy.txt heavy_data.byte heavy.txt heavy_data.byte heavy.txt heavy_data.byte heavy.txt heavy_data.byte
OMP_NUM_THREADS=4 ./sim -b heavy.txt heavy_data.byte heavy.txt heavy_data.byte heavy.txt heavy_data.byte heavy.txt heavy_data.byte
./test.sh                                    # 7 checks
OMP_NUM_THREADS=1 SIM=$PWD/sim ../../tests/run_tests.sh    # 33 passed (the original order)
```

## Expected output
```
  time with 1 thread(s): 0.070 s
  ok    1 thread(s): all 4 tasks computed 6000
  time with 4 thread(s): 0.035 s
  ok    4 thread(s): all 4 tasks computed 6000
  ok    20 runs x 3 tasks with simultaneous page faults all correct
  ok    fir in parallel
  ok    array_add in parallel
  ok    sum_array in parallel
  ok    log has every print line (9 + 10)
7 passed, 0 failed
```
- **The base tests with 4 threads:** 32 of 33 pass. The only difference is the
  round-robin log-order check (`3,3,1,1,4,4,2,2` instead of `1,1,2,2,3,3,4,4`), because
  the processors now print at the same time.
- **Why the speedup is 2× and not 4×:** each time slice is only 10 instructions plus
  the `usleep(10)` from the handout. A parallel region costs a few microseconds, which is
  comparable to a slice. The sleeps overlap, which is where most of the gain comes from.
  With `TIME_SLICE 10000` the instruction work dominates.

## Viva questions
- **Why `threadprivate` and not `private`?** These are global variables used by several
  functions (`fetch`, `execute`, ...), not variables of the parallel loop. `private` only
  works for variables visible in the construct. `threadprivate` gives every thread its
  own copy of a global for the whole program.
- **Why does `freePages` need a critical section, but `memory[]` does not?** Two
  processors can page-fault at the same moment. Both would find the same first free
  frame and both map it, so two tasks would share a frame. Each task writes only its own
  frames and its own page-table bytes, so those accesses never overlap.
- **Is a round still fair?** Yes. Every ready task still gets exactly one slice per round,
  and finishing and re-queueing happen serially in the original order. Only the
  interleaving inside a round changes.
- **Why `schedule(dynamic, 1)`?** Slices differ in cost: one task may halt after 2
  instructions while another does vector loads. Dynamic scheduling hands each thread the
  next task as soon as it is free.
- **Why can't the shell or the loader run inside the parallel region?** They change the
  task queues and the frame list (create, load, finish). Keeping them serial, between
  rounds, avoids locking all of that.

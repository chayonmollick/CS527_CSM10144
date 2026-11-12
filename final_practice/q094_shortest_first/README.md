# Q094 — Admit the shortest waiting program first

## Question
Change the OS so that, when several tasks are waiting for a processor, the one with the
shortest program (the fewest instructions) is started first. Tasks with programs of
equal length keep their arrival order. A task that is already running keeps its
processor. Show four waiting tasks of different lengths, entered longest first.

## Answer
- **Where:** `admit_waiting()` runs at the start of every round and from
  `create_task()`. It walks the waiting queue in order and calls `loader()` for each task.
  Lab6 keeps that queue in arrival order (FIFO).
- **The change:** sort the queue before the walk, with `std::stable_sort` by
  `prog.code.size()`. That is 4 bytes per instruction, including the final `halt`, so it
  equals the instruction count printed when the task is created.
  - A stable sort keeps equal lengths in arrival order.
  - The walk itself is unchanged, so a task that does not fit in the free frames is still
    skipped and a later one can start.
- **Non-preemptive:** lab6 has no context switch, so a long task that is already running
  keeps its processor. Only the choice of the next task changes. This is
  shortest-job-first for the long-term (admission) scheduler.
- **The test run.** Spin jobs of 2, 4, 6 and 6 rounds hold the 4 processors. Then long
  (10 instructions), short (3), medium (6) and short (3) arrive, as pids 5–8. Each of
  them finishes in one slice.
  1. Round 3: processor 0 is free; short pid 6 starts.
  2. Round 4: short pid 8 starts.
  3. Round 5: processors 0 and 1 are free; medium pid 7 and long pid 5 start, in that order.

  Lab6 would start 5, 6, 7, 8.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `admit_waiting` | `std::stable_sort` of the waiting queue by program size, shortest first, before the loader loop |

## Test
`./test.sh`:
- **Waiting:** 8 tasks run; pids 5–8 wait for a processor.
- **Start order:** they start as `6 8 7 5`. The two equal shorts keep their order.
- **Results:** each task computes its own result (8, 1, 4, 1).
- **Old programs:** `sum_array` still gives 45.

## Viva
- **Why shortest first?** If the lengths are the running times, SJF gives the smallest average waiting time: every short job that goes first delays only short work. Here each waiting job takes one slice, so the test checks only the order.
- **Is program length a good estimate?** Not always. A 5-instruction loop can run for a million instructions (see `spin.txt`, whose run time depends on its data). Real SJF predicts the next CPU burst, for example as an exponential average of past bursts.
- **What is the risk?** Starvation. A long program can wait forever while short ones keep arriving. Aging (Q089) fixes it: raise a task's rank the longer it waits.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `long.txt`, `medium.txt`, `r2.byte`, `r4.byte`, `r6.byte`, `short.txt`, `sjf_data.byte`, `spin.txt`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

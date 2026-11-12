# Q093 — Processor affinity: `prog data @2`

## Question
Add processor affinity. A task started as `prog data @N`, on the command line or in the
shell, may run only on processor N. While processor N is busy, the task waits, even if
other processors are free. Tasks without `@N` behave as before and take the first free
processor. Reject a processor that does not exist. Show two tasks pinned to processor 2
and one free task.

## Answer
- **The decision is in `loader()`:** it picks the processor for a waiting task. One
  condition is added to its search:
  `if (!proc_owner[i] && (t.affinity < 0 || t.affinity == i))`. A pinned task whose
  processor is busy finds no processor, so `loader()` returns false and the task stays
  in the waiting queue.
  - `admit_waiting()` still tries every waiting task, so a pinned task never blocks the
    unpinned ones behind it.
  - It is retried at the start of every round, so the task starts as soon as its
    processor is free.
- **The affinity is set at creation.** `Task.affinity` (-1 = any) comes from a new
  `create_task()` parameter: the optional third word, `""` or `"@N"`.
  - `create_task()` checks it with the other start-up errors. Anything other than
    `@0`..`@3` gives `cannot start task: '@7' is not a processor @0..@3`. The test
    accepts one digit, which is enough for NP ≤ 10.
  - `main()` takes `prog data [@N]` triples, after first checking the whole list as
    lab6 did. `run_command()` accepts a 3-word line whose last word starts with `@`.
- **The test run** (count3 = 15 instructions = 2 slices). Pid 1 @2 goes to processor 2,
  leaving 0 and 1 free. Pid 2 @2 waits. Pid 3 takes processor 0.
  - Rounds 1–2: pids 1 and 3 run side by side; `log.txt` shows `1 1 3 3 1 3`.
  - Round 3: processor 2 is free again, so pid 2 starts there and prints `2 2 2`.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `struct Task` | `int affinity = -1` |
| `os.cpp` | `loader` | a pinned task only accepts its own processor |
| `os.h/.cpp` | `create_task` | new parameter `where` ("" or "@N"); check it, set `affinity` |
| `os.cpp` | `run_command` | `<program> <data> @N` |
| `main.cpp` | `usage`, `main` | tasks are `prog data [@N]`: check the whole list, then pass the `@N` word on |

## Test
`./test.sh`:
- **Pinned and free tasks:** `count3 @2`, `count3 @2`, `count3`.
  - Pid 1 is loaded on processor 2. Pid 2 waits. Pid 3 is loaded on processor 0.
  - Pid 2 is loaded on processor 2 only after pid 1 finished there.
  - `log.txt` shows `1 1 3 3 1 3 2 2 2`, and all three outputs are 3.
- **Errors:** `@7` is refused with a message (exit code 1).
- **Shell:** the line `count3.txt count3_data.byte @3` runs the task on processor 3.
- **Old programs:** `sum_array` still gives 45.

## Viva
- **Why would anyone pin a task?** Cache affinity: a task that comes back to the same CPU finds its data still in that CPU's cache. Other reasons are dedicating a core to a time-critical task, or a device whose interrupts go to one CPU (Linux `taskset`, `sched_setaffinity`).
- **What does it cost?** A pinned task can wait while other processors idle: lower utilisation and longer turnaround. That is why real schedulers use *soft* affinity (prefer the last CPU) unless told otherwise.
- **Why doesn't pid 2 hold up pid 3?** `admit_waiting()` tries every task in the waiting queue, not only the first one, so a task that cannot start does not stop the others.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `count3.txt`, `count3_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

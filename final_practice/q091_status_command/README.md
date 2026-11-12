# Q091 — Shell command `status`

## Question
Add a shell command `status` that prints one line describing the whole system:
- the scheduler round number;
- how many tasks have finished, are running and are waiting;
- how many physical frames are free;
- which processors are busy, and with which pid.

Count only rounds in which some task ran, so that the number does not depend on how
fast the user types.

## Answer
- **Round counter.** `round_no` is a new global. `scheduler()` adds 1 at the start of
  every round that has ready tasks (`n > 0`). The scheduler also loops while it waits
  for shell input with nothing to run; counting those idle passes would make the
  number depend on timing. A status typed between rounds therefore shows the number
  of the round that has just ended.
- **`status()`** reads the OS tables:
  - **Task states**, from the process table `tasks`: `DONE` = finished, `READY` =
    loaded on a processor, which in lab6 means running (a task keeps its processor
    until it ends), and `WAITING` = in the waiting queue.
  - **Free frames:** `free_page_count()`, out of `NUM_PHYSICAL_PAGES - 1`, because frame
    0 holds the page tables.
  - **Busy processors:** `proc_owner[p]` holds the pid on processor p, or 0 when it is free.
- **`run_command()`** calls it for the one-word line `status`, then prints the prompt as
  the other commands do.
- **The test session.** Pid 1 (`quick`, 4 instructions) ends in round 1; pids 2–4 run
  `counter` (5 rounds); pid 5 waits. The shell runs after round 1:
  - The first status shows `p0=free` and 9 free frames (15 − 3 tasks × 2 frames).
  - Typing one more task calls `create_task()`, which runs `admit_waiting()`. The older
    pid 5 gets p0 and the new pid 6 waits, so the second status shows 4 busy processors
    and 7 free frames.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | globals | `round_no` |
| `os.cpp` | `scheduler` | `if (n > 0) round_no++;` before the round's slices |
| `os.cpp` | `status` | new: count task states, free frames, busy processors; print one line |
| `os.cpp` | `run_command` | `status` → `status(); prompt();` |

## Test
`./test.sh`. The shell input comes from a file, so it is read in the first `shell()` call:
- With no tasks: `round 0: 0 finished, 0 running, 0 waiting; 15 of 15 frames free; 0 of 4 processors busy`.
- With 5 tasks, after round 1:
  - First line: `round 1: 1 finished, 3 running, 1 waiting; 9 of 15 frames free; 3 of 4 processors busy: p0=free p1=pid2 p2=pid3 p3=pid4`.
  - After typing a task: `… 4 running, 1 waiting; 7 of 15 frames free; 4 of 4 processors busy: p0=pid5 …`.
- All 6 tasks finish.
- `sum_array` still gives 45.

## Viva
- **Why can `status` never show a task in the middle of its slice?** The shell runs only between rounds, after `process_instructions()` has returned for every task. The OS tables are therefore consistent when it reads them.
- **Why are there processors free while a task waits?** Here, never for long. `admit_waiting()` runs at the start of every round and from `create_task()`. A task waits only while all processors are busy or too few frames are free.
- **What would `ps` add?** One line per task: its state, processor and instructions so far (`steps[p]`), as in extension e17. `status` is the one-line summary.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_data.byte`, `quick.txt`, `quick_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

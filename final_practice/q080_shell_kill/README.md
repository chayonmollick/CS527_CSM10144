# Q080 — Shell command `kill <pid>`

## Question
Add a shell command `kill <pid>`. Killing a running (loaded) task stops it at once and
frees its processor and its physical frames, so that a waiting task can start. Killing a
waiting task removes it from the waiting queue, so it never runs. Refuse a pid that does
not exist or has already finished. Show it with a long job and more tasks than processors.

## Answer
- **A loaded (READY) task:**
  1. remove it from the ready queue and mark it `killed`;
  2. call the normal end-of-task path, `finish_task()`. Its `finalize()` writes
     `<data>_<pid>.out` and the `.freq` file as they are now, returns every frame in the
     task's page table to the free list, clears the table and sets `PTBR[p] = -1`;
  3. `finish_task()` then sets `proc_owner[p] = 0`, which frees the processor. The `[os]`
     line says `KILLED` instead of `finished`.
- **A WAITING task** has no processor and no frames yet. It is erased from the waiting
  queue and marked DONE and killed. It writes no output.
- **Safe timing.** The shell runs between rounds, never inside the round loop, so erasing
  from `ready_queue` cannot upset the loop.
- **Reuse.** At the start of the next round, `admit_waiting()` sees the free processor and
  frames and loads the next waiting task.
- **Details.**
  - `finish_task()` is defined after `run_command()`, so the patch adds a forward
    declaration.
  - A kill is not an error: `failed` stays 0, so the exit code stays 0.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `struct Task` | `bool killed` |
| `os.cpp` | `finalize` | the message says `KILLED` for a killed task |
| `os.cpp` | before `run_command` | forward declaration of `finish_task` |
| `os.cpp` | `run_command` | `kill <pid>`: unknown or DONE → message; WAITING → erase from `waiting_queue`; READY → erase from `ready_queue`, `finish_task(pid)` |
| `os.cpp` | includes, `run_command` | `<cstdlib>` for `atoi`; the usage message mentions `kill` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` starts 6 tasks: `long_job.txt` (a 65025-turn loop that stores its count at
[0]), then counters. Pids 5 and 6 wait. The shell input comes from a file, so it runs
after round 1: `kill 1`, `kill 5`, `kill 1`, `kill 9`, `exit`.
- **pid 1** is KILLED on processor 0 after 10 instructions. Its `.out` file has [0] = 2:
  3 setup instructions plus 7 of the 4-instruction loop include two stores.
- **pid 5** is killed while waiting. It never runs: no `log.txt` lines and no `.out` file.
- **Refused:** the second `kill 1` and `kill 9`.
- **Round 2** loads pid 6 on processor 0 with 7 frames left. That is the same number as
  before the kill, so pid 1's 2 frames were returned; without them 5 would be left.
- **The rest:** the exit code is 0, the 4 other tasks finish (pid 6 counts to 10), and
  `sum_array` still gives 45.

## Viva
- **Why reuse `finish_task()`?** Every way out of a task frees the frames and the processor in the same code, so a kill cannot leak memory. The partial `.out` is also useful for debugging.
- **When does the kill take effect?** Between two slices, because the shell only runs between rounds. A real OS also acts on a signal only when the process next enters the kernel or is scheduled.
- **What else would a real kill release?** Open files, locks and child processes, then a parent's `wait` would collect the exit status. Here a task holds only frames and a processor.

# Q079 — Shell command `ps`

## Question
Add a shell command `ps` that lists every task the OS has created: its pid, program name,
state (waiting, ready or done), the processor it is on (`-` if none) and the number of
instructions it has executed so far. Show it with more tasks than processors, while one
task has already finished and one is still waiting.

## Answer
- **Where.** One more `if` in `run_command()`, before the `prog data` case. The shell runs
  between scheduler rounds, so no task is in the middle of a slice and every count is exact.
- **The process table** is `tasks` (`tasks[pid - 1]`): every task ever created, in pid
  order. `ps` walks it.
- **State:**
  - `WAITING`: in the waiting queue, with no processor and no frames yet;
  - `READY`: loaded; it owns a processor and gets a slice every round;
  - `DONE`, or `FAILED` if it stopped with an error.
- **Instructions so far.** A loaded task's live count is its processor's `steps[t.proc]`
  (the loader reset it to 0). When the task ends, `finalize()` copies it into `t.steps`.
  A waiting task has run 0.
- **Processor:** `t.proc`, which is -1 when the task has none (printed as `-`).

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `run_command` | `ps`: a header, then per task: pid, `prog.name`, state, `t.proc` or `-`, `steps[t.proc]` (READY) or `t.steps` |
| `os.cpp` | `run_command` | the usage message mentions `ps` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` starts 5 tasks on the command line, with `tiny.txt` (3 instructions) as pid 2.
The shell input `ps`, `exit` comes from a file, so the shell reads it right after round 1:

```
  PID  PROGRAM          STATE    PROC  INSTRUCTIONS
    1  counter          READY       0            10
    2  tiny             DONE        -             3
    3  counter_by100    READY       2            10
    4  counter          READY       3            10
    5  counter_by100    WAITING     -             0
```
- pid 5 is still waiting although pid 2 has freed processor 1: tasks are admitted at the
  start of a round, so pid 5 starts in round 2. All tasks then finish (pid 5 = 1000).
- A shell-only session: `ps` with no tasks prints just the header. A task typed in the
  shell shows `READY 0 0`, because no round has run yet.
- `sum_array` still gives 45.

## Viva
- **Why is there no RUNNING state?** In lab6 a loaded task owns its processor until it ends and runs every round, so between rounds READY means "on a processor". With more tasks than processors (context switching, extension e18) READY and RUNNING would differ.
- **Why read `steps[p]` for a READY task?** The processor arrays hold the live state, like hardware registers. `t.steps` is the copy kept in the process table once the task ends, like a PCB field.
- **Why is pid 5 still WAITING with a free processor?** `admit_waiting()` runs at the start of each round and when a task is created, not when one finishes.

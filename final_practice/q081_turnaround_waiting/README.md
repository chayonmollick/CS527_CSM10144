# Q081 — Turnaround and waiting time in rounds

## Question
When all tasks have finished, print for each task its arrival round, its finish round, its
turnaround time and its waiting time, all measured in scheduler rounds, followed by the
average turnaround and the average waiting time. A task from the command line arrives in
round 0; a task typed in the shell arrives in the round after which the shell read it.
Check the numbers by hand for 5 counters on 4 processors.

## Answer
- **The clock.** A global `rounds` goes up by one at the start of every round in which at
  least one task runs (`n > 0`). Idle passes, while the shell waits for input, are not
  counted, so the numbers do not depend on how fast you type.
- **Per task (`struct Task`):**
  - `arrival = rounds` in `create_task()`;
  - `finish = rounds` in `finish_task()`, because the task halts during this round;
  - `slices++` after each `process_instructions()` call.
- **Formulas:**
  - turnaround = finish − arrival;
  - waiting = turnaround − slices, the rounds the task spent in the system without
    running. In lab6 that is its time in the waiting queue, because a loaded task runs
    every round.
- **Report.** After the scheduler loop: one line per finished task, then the averages (`%.2f`).

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `struct Task` | `int arrival, finish, slices` |
| `os.cpp` | globals | `static int rounds = 0;` |
| `os.cpp` | `create_task` | `t.arrival = rounds;` |
| `os.cpp` | `finish_task` | `t.finish = rounds;` |
| `os.cpp` | `scheduler` | `if (n > 0) rounds++;` at the start of a round; `slices++` after each slice; the table and the averages at the end |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`counter` runs 43 instructions = 5 slices (10 + 10 + 10 + 10 + 3); `counter_by100` runs
54 = 6 slices. In batch mode pids 1–4 load at once and pid 5 waits. Pids 1 and 3 end in
round 5, so round 6 loads pid 5 on processor 0, and it runs in rounds 6–10.

| pid | program | slices | arrival | finish | turnaround | waiting |
|---|---|---|---|---|---|---|
| 1 | counter | 5 | 0 | 5 | 5 | 0 |
| 2 | counter_by100 | 6 | 0 | 6 | 6 | 0 |
| 3 | counter | 5 | 0 | 5 | 5 | 0 |
| 4 | counter_by100 | 6 | 0 | 6 | 6 | 0 |
| 5 | counter | 5 | 0 | 10 | 10 | 5 |

The averages are 32/5 = 6.40 and 5/5 = 1.00 rounds. `./test.sh` checks every line, and
two more cases:
- **The 5th counter typed in the shell.** The input comes from a file, which the shell
  reads after round 1. That task arrives in round 1, so its turnaround is 9 and its
  waiting time 4; the averages become 6.20 and 0.80.
- **`sum_array` alone** (71 instructions = 8 slices) gives `0 8 8 0` and still 45.

## Viva
- **Why rounds and not seconds?** Wall-clock time depends on the machine, on `usleep` and on typing speed. Rounds are deterministic, and one round is at most `TIME_SLICE` instructions per task.
- **Response time?** It is first run − arrival. Here it equals the waiting time, because once loaded a task runs every round.
- **How could the average waiting time be lowered?** Admit the shortest waiting job first when a processor becomes free (SJF), which minimises the average waiting time. Only the waiting queue's order matters in lab6.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_by100.txt`, `counter_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

# Q077 — Option `-s fcfs`: first come, first served

## Question
Add an option `-s fcfs` that replaces round robin by first-come-first-served scheduling:
each loaded task runs to completion before the next task gets the processor, in the order
the tasks arrived. `-s rr` (the default) keeps round robin; any other policy prints the
usage. Decide what the 4 processors do under FCFS and explain your choice. Show, with one
long job followed by two short ones, how the finish order changes.

## Answer
- **What the 4 processors do.** "Runs to completion before the next gets the processor" is
  uniprocessor FCFS, so only **one task is loaded at a time**:
  - the running task always gets processor 0, and processors 1–3 stay idle;
  - the other tasks stay in the waiting queue, in arrival order, with no processor and no
    frames yet.
- **One line does it.** At the top of `loader()`: `if (fcfs && !ready_queue.empty()) return false;`
  - `admit_waiting()` walks the waiting queue from the front, so tasks start in arrival order.
  - When the running task halts, `finish_task()` frees processor 0 and `finalize()` frees its
    frames. At the start of the next round, `admit_waiting()` loads the next task.
- **The scheduler loop is unchanged.** The only ready task gets one slice per round, and the
  shell still runs between rounds, so typing works during a long task. Nothing preempts the
  task, because no other task is ready. Running it in one call (`while (!end) process_instructions(...)`)
  would freeze the shell until it ends.
- **The other reading.** "FCFS per processor" (4 tasks at once, each to completion on its
  own processor) is what lab6 already does: a task never leaves its processor, and round
  robin only sets the order in which the simulator steps the 4 processors.
- **Option.** `-s fcfs` sets a global `bool fcfs` (declared in `os.h`), and `-s rr` leaves it false.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | globals | `bool fcfs = false;` |
| `os.cpp` | `loader` | `if (fcfs && !ready_queue.empty()) return false;`: one loaded task at a time |
| `os.h` | — | `extern bool fcfs;` |
| `main.cpp` | `main` | `-s fcfs` sets `fcfs`, `-s rr` keeps round robin, anything else: usage, exit 1 |
| `main.cpp` | `usage` | a `-s` line |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` runs `counter_by100.txt` (54 instructions = 6 slices), then two copies of
`counter.txt` (43 instructions = 5 slices each), all from the examples.
- **`-s fcfs`:**
  - pids 2 and 3 wait, and each task is loaded on processor 0;
  - `log.txt` holds all of pid 1, then pid 2, then pid 3;
  - the finish order is 1, 2, 3 (rounds 6, 11 and 16);
  - with `-t`, the trace is three unbroken blocks of 54, 43 and 43 instructions;
  - the results are unchanged: 1000, 10, 10.
- **Default and `-s rr`:** the finish order is 2, 3, 1 (rounds 5, 5, 6), and the turns interleave.
- **Errors:** `-s fifo` prints the usage and exits with code 1.
- **Old behaviour:** `-c` still lists, and `sum_array` still gives 45.

## Viva
- **Convoy effect:** under FCFS the two short counters wait behind the long job and finish in rounds 11 and 16 instead of 5 and 5. The average turnaround is (6 + 11 + 16) / 3 = 11 rounds, against (6 + 5 + 5) / 3 = 5.33 with round robin.
- **Is FCFS preemptive?** No. The running task keeps the processor until it halts, so an endless loop blocks every task behind it; here only the `MAX_STEPS` guard would stop it.
- **Why leave processors 1–3 idle?** The question asks for one task at a time. Letting 4 tasks run to completion at once is FCFS per processor, which is just lab6 unchanged.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_by100.txt`, `counter_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

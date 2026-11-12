# Q090 — Shell command `run <prog> <data> <n>`

## Question
Add a shell command `run <program> <data.byte> <n>` that starts n copies of the same
task, as if the line `<program> <data.byte>` had been typed n times. Each copy is a
separate task with its own pid and output file. Copies that find no free processor
wait like any other task. Print a usage message if n is missing or not in 1..64. If the
program cannot be loaded, report it once rather than n times.

## Answer
- **Where:** shell commands are handled in `run_command()`, which already splits the
  line into words. One more `if` before the normal `<program> <data>` case does it.
- **What it does:**
  - `n = atoi(words[3])` when there are 4 words, else 0.
  - n outside 1..`MAX_TASKS` (the process-table size, 64) → usage message.
  - Otherwise it calls `create_task(program, data)` n times. Each call compiles the
    program, gives the copy the next pid, queues it and admits it if a processor and
    frames are free. So the copies are ordinary tasks: copy k writes
    `<data>_<pid>.out` and `<program>_<pid>.freq`, and copies beyond NP wait in the
    waiting queue.
  - The loop stops at the first `create_task()` error, because the next copy would
    fail the same way. The causes are a missing file, a compile error or a full
    process table.
- **The test session** (ticker = 15 instructions = 2 slices, NP = 4):
  - Copies 1–4 start at once and print 1, 2 in round 1, then 3 in round 2.
  - Copies 5 and 6 (and the `sum_array` line typed next, pid 7) wait. They start in
    round 3, when the first four have finished.
- `#include <cstdlib>` is added for `std::atoi`. Without it the code compiles only
  through another header's include.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | includes | `<cstdlib>` for `std::atoi` |
| `os.cpp` | `run_command` | `run <program> <data> <n>`: check n, then `create_task` n times, stopping at the first error; the usage line mentions `run` |

## Test
`./test.sh` pipes a shell session from a file, so every line is read in the first
`shell()` call:
```
run ticker.txt ticker_data.byte 6
sum_array.txt sum_array_data.byte
run ticker.txt ticker_data.byte 0
run nothere.txt ticker_data.byte 3
run ticker.txt
exit
```
- **Copies:** 6 are created; 4 start at once and 3 tasks wait (copies 5 and 6, plus pid 7). Every copy writes its own output with 3.
- **`log.txt`** shows the pids `1 1 2 2 3 3 4 4 1 2 3 4 5 5 6 6 5 6`.
- **Normal start:** the plain line (pid 7) still works and gives 45.
- **Errors:** n = 0 and a missing n print the usage; a missing program gives one error.
- **Old programs:** `sum_array` still gives 45 in batch mode.

## Viva
- **Do the copies share memory?** No. Each copy is a separate task with its own page table and frames, loaded from the same files. Only the code bytes are the same. A real OS could share the read-only code pages between them.
- **Why do copies 5 and 6 start only in round 3?** A task keeps its processor until it finishes, and the 4 copies hold all 4 processors for 2 rounds. `admit_waiting()` at the start of round 3 loads the waiting tasks in queue order.
- **What limits n?** The process table (`MAX_TASKS` = 64 pids, never reused) and, for running at the same time, NP processors and the free frames. Each copy needs 2 frames here.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `sum_array.txt`, `sum_array_data.byte`, `ticker.txt`, `ticker_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

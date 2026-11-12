# Q050 — Option `-r R`: stop tasks after R scheduler rounds

## Question
Add an option `-r R`. Every scheduler round gives each ready task one time slice; a task
that has not finished after R rounds must be stopped (reported as an error, its data
written as usual) while the other tasks continue. Without `-r` nothing changes. Show it
with an endless loop running next to `sum_array`.

## Answer
The round robin is in `scheduler()`: each pass of its loop is one round, and every ready
task gets `process_instructions(p, TIME_SLICE)` once in it. A per-task counter `rounds`
in `struct Task` goes up after each slice; when it reaches `max_rounds` and the task has
not halted, the OS stops it the way a failed task stops, `failed[p] = end_of_simulation[p]
= 1`. The existing `if (end_of_simulation[p]) finish_task(pid);` then writes its `.out`
and `.freq`, frees its frames and the processor, and the exit code becomes 1.
`max_rounds` is a global in `os.cpp`, set by `-r R` in `main()` (0 = no limit).

```
[os] pid 1 has not finished after 10 scheduler rounds: stopped
[os] pid 1 (forever) STOPPED WITH AN ERROR on processor 0 after 100 instructions, ...
```

## Changes
| File | Function | Change |
|---|---|---|
| `os.h` | (declarations) | `extern int max_rounds;` |
| `os.cpp` | `struct Task`, (globals) | `int rounds = 0;` per task; `int max_rounds = 0;` |
| `os.cpp` | `scheduler` | after each slice: `rounds++`; stop the task when `rounds >= max_rounds` |
| `main.cpp` | `main`, `usage` | parse `-r R` (`strtol`, R > 0); usage text; `#include <cstdlib>` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 16 lines by hand.

## Test
`./test.sh`: with `-r 10`, `forever.txt` is stopped after 10 rounds × 10 instructions =
100 instructions (counter 33), while `sum_array` beside it finishes with 45; exit code 1.
sum_array needs 71 instructions = 8 slices (7 × 10 + 1): `-r 8` lets it finish, `-r 7`
stops it after 70. `-r 0` prints the usage, and without `-r` sum_array still stores 45.

## Viva
- **Why count rounds per task, not one global round number?** A task that waited in the waiting queue starts later; counting its own rounds gives every task the same budget of R × TIME_SLICE instructions.
- **How is this different from `-m`?** `-m` limits instructions and the processor checks it after every instruction; `-r` limits time slices and the OS checks it once per round, so a task can only be stopped at the end of a slice.
- **Why set `failed[p]` too?** So `finalize()` prints "STOPPED WITH AN ERROR" and `failed_task_count()` makes the exit code 1, exactly as for a task stopped by the processor.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `forever.txt`, `forever_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

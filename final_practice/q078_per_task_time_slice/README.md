# Q078 — A time slice for each task

## Question
Let every task have its own time slice, given as an optional number after its data file,
both on the command line and in the shell. For example,
`./sim -b p1.txt d1.byte p2.txt d2.byte 20` gives the second task 20 instructions per turn,
while the first keeps the default `TIME_SLICE`. Typing `p.txt d.byte 4` at the `$` prompt
does the same for a new task. Refuse a slice of 0, and print each task's slice when it is
created.

## Answer
- **A field in the task.** `struct Task` gets `int slice = TIME_SLICE`.
  - `create_task()` takes the slice as a third parameter. Its default, `TIME_SLICE`, is in
    the declaration in `os.h`, which now includes `config.h`, so every old two-argument
    call still compiles.
  - It refuses a slice < 1 with the usual "cannot start task" message.
  - The "created" line shows the slice.
- **The scheduler uses it:** `process_instructions(p, tasks[pid - 1].slice)`.
- **Command line.** `main()` no longer demands an even number of arguments. It reads
  `prog data`, then takes the next argument as the slice if it starts with a digit (a file
  name normally does not). A program left without a data file prints the usage.
- **Shell.** `run_command()` also accepts 3 words when the third starts with a digit.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `struct Task` | `int slice = TIME_SLICE;` |
| `os.cpp` | `create_task` | new parameter `slice`; refuse `slice < 1`; print it in the "created" line |
| `os.cpp` | `run_command` | `prog data N` (third word starts with a digit) → `create_task(prog, data, atoi(N))`; usage mentions `[time slice]` |
| `os.cpp` | `scheduler` | `process_instructions(p, tasks[pid - 1].slice)` |
| `os.cpp` | includes | `<cctype>` (`isdigit`), `<cstdlib>` (`atoi`) |
| `os.h` | `create_task` | `int slice = TIME_SLICE` default parameter; `#include "config.h"` |
| `main.cpp` | `main` | loop over `prog data [slice]` groups instead of pairs; leftover argument → usage |
| `main.cpp` | `usage`, includes | `[slice]` in the usage line; `<cctype>`, `<cstdlib>` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` uses `counter.txt` from the examples. Its loop is 4 instructions with one
print, and it runs 43 instructions in all.
- **Command line, `counter.txt counter_data.byte counter.txt counter_data.byte 100`:**
  - pid 1 has slice 10 and pid 2 has slice 100;
  - pid 2 runs all 43 instructions in its first turn, so `log.txt` reads pid 1 twice,
    pid 2 ten times, then pid 1 eight times;
  - pid 2 finishes first.
- **Shell.** The input is a file, so the shell reads every line before round 1:
  `… 4`, `…` (default 10), `… 0` (refused) and `… x` (usage).
  - A slice of 4 is one loop turn with one print. A slice of 10 prints 2, 3, 2, 3 values.
  - So `log.txt` reads `1,2,2,1,2,2,2,1,2,2,1,2,2,2,1,1,1,1,1,1`.
  - pid 2 finishes in round 5 and pid 1 in round 11 (43 = 10 × 4 + 3).
- **`-t` with `counter.txt counter_data.byte 20 counter_by100.txt counter_data.byte`:** the
  trace shows pid 1 in turns of 20, 20 and 3 instructions, with pid 2's turns of 10 in
  between.
- **Errors:** a program without a data file prints the usage and exits with code 1.
- **Old behaviour:** `-c` still prints the listing, and `sum_array` still gives 45.

## Viva
- **Why put the default in `os.h`?** Callers see only the declaration, so the default argument must be there, and only there; repeating it in the definition is a compile error. Old calls `create_task(p, d)` keep working unchanged.
- **Is a bigger slice a priority?** It is a weight: with slices 100 and 10, task 2 gets ten times as many instructions per round. A priority would decide who runs first; here both still run every round.
- **Why do the results not change?** Each task has its own registers and pages; the slice changes only the interleaving, so only the order of the lines in `log.txt`.

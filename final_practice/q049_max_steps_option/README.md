# Q049 — Option `-m N`: instruction limit at run time

## Question
The endless-loop guard `MAX_STEPS` (10 000 000 instructions) is fixed at compile time.
Add a command-line option `-m N` that sets the limit when the simulator starts, for every
task. A task that executes N instructions without halting must be stopped with the same
error as before. `-b`, `-t` and `-c` must keep working, and a bad N must print the usage.

## Answer
`MAX_STEPS` is used in one place: the check after each instruction in
`process_instructions()`. Replace it there by a variable `long max_steps = MAX_STEPS;`
(declared `extern` in `processor.h` so that `main.cpp` can set it), and parse `-m N` in
the option loop of `main()`, next to `-b` and `-t`. `strtol` with an end pointer rejects
`-m abc`, and N <= 0 is rejected too.

```
./sim -b -m 100 forever.txt forever_data.byte
[pid 1, processor 0] error at instruction 3: 100 instructions executed without halting: endless loop?
[os] pid 1 (forever) STOPPED WITH AN ERROR on processor 0 after 100 instructions, ...
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern long max_steps;` |
| `processor.cpp` | (globals) | `long max_steps = MAX_STEPS;` |
| `processor.cpp` | `process_instructions` | compare with `max_steps` instead of `MAX_STEPS` |
| `main.cpp` | `main`, `usage` | parse `-m N` (`strtol`, N > 0); usage text; `#include <cstdlib>` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 12 lines by hand.

## Test
`./test.sh`: `forever.txt` (`x1 = 0`, then a loop of 3 instructions with no exit) is
stopped by `-m 100` after exactly 100 instructions with exit code 1, and its counter is
(100 - 1) / 3 = 33. sum_array needs exactly 71 instructions: `-m 71` is enough, `-m 70`
stops it. `-m 0` prints the usage, `-t -m 5` traces exactly 5 instructions, and without
`-m` sum_array still stores 45.

## Viva
- **Why a variable and not the `#define`?** A `#define` is replaced at compile time, so changing it needs a rebuild; a variable can be set by `main()` at run time. The `#define` stays as the default value.
- **Why is `-m 71` enough for sum_array?** The check is skipped when the task has just halted (`!end_of_simulation[p]`), and its 71st instruction is the halt.
- **Why `i + 1 < argc` in the test for `-m`?** `-m` needs a value; without one, the option falls through to `usage()` instead of reading past the end of `argv`.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `forever.txt`, `forever_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

# Q076 — Option `-q N`: set the time slice at run time

## Question
The time slice is the constant `TIME_SLICE` (10 instructions) in `config.h`, so trying
another value means editing and rebuilding. Add a command-line option `-q N` that sets the
time slice when the simulator starts, for example
`./sim -b -q 4 counter.txt counter_data.byte counter.txt counter_data.byte`.
A missing or non-positive N must print the usage, and `-b`, `-t` and `-c` must keep working.
Show how the order of the lines in `log.txt` changes with N.

## Answer
- **One place uses the slice:** `scheduler()` calls `process_instructions(p, TIME_SLICE)`.
  Replace the macro there with a global variable `int time_slice = TIME_SLICE`, defined in
  `os.cpp` and declared `extern` in `os.h` so that `main.cpp` can set it. The macro stays
  as the default value.
- **Option parsing:** `main()` already loops over the leading `-x` arguments. `-q` also
  reads the next argument and skips it (`i++`).
  - `i + 1 >= argc` catches a missing N.
  - `atoi` returns 0 for a non-number, so the single test `N < 1` rejects `-q 0`, `-q -5`
    and `-q abc`.
- **Nothing else changes.** Each task keeps its own processor; only the number of
  instructions per turn, and so the interleaving, changes. The results stay the same.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | globals | `int time_slice = TIME_SLICE;` |
| `os.cpp` | `scheduler` | `process_instructions(p, time_slice)` instead of `TIME_SLICE` |
| `os.h` | — | `extern int time_slice;` |
| `main.cpp` | `main` | `-q N`: `time_slice = atoi(argv[i + 1])`, usage and exit 1 if N is missing or < 1, then `i++` |
| `main.cpp` | `usage`, includes | a `-q N` line; `<cstdlib>` for `atoi` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` runs two copies of `counter.txt` from the examples. Its loop is 4 instructions
(add, print, sub, BNE); with `x1 = 0` before it and the store and halt after it, a counter
runs 1 + 10 × 4 + 2 = 43 instructions.
- **`-q 4`:** every slice is exactly one loop turn with one print, so `log.txt` alternates
  pids `1,2,1,2,…` (20 lines).
- **`-q 100`:** pid 1 runs all 43 instructions in its first slice, so `log.txt` has ten lines
  of pid 1, then ten of pid 2.
- **No option:** the old order `1,1,2,2,…` (two prints in a first slice of 10).
- **`-t -q 4`:** the trace shows pid 1 in runs of `4,4,4,4,4,4,4,4,4,4,3` instructions.
- **Errors:** `-q 0` and `-q` with no number print the usage and exit with code 1.
- **Old behaviour:** `-c` still prints the listing, and `sum_array` still gives 45.

## Viva
- **Why a variable and not the `#define`?** The preprocessor replaces a macro at compile time, so its value is fixed in the binary. A value chosen at run time needs a variable; the macro remains its default.
- **Small or large slice?** A small slice gives a better response time, because every task gets its turn sooner, but more overhead per instruction: here a `usleep` and a shell check per slice, on a real machine context switches and cache/TLB misses. A very large slice turns round robin into FCFS, as `-q 100` shows.
- **Why do the results not change?** Each task has its own registers and pages and nothing is shared, so only the order of the lines in `log.txt` depends on the slice.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

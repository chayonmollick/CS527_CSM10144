# Q052 — Taken / not-taken counts in the frequency report

## Question
Extend the Lab 6 frequency report `<program>_<pid>.freq`: on every line that is a branch,
add two columns with how many times the branch was taken and how many times it was not
taken. Other lines must stay exactly as before. Check the numbers of `sum_array` by hand.

## Answer
A branch line's count is already `exec_count[p][i]` (all its executions), so only the
taken ones need a new counter: `taken_count[NP][MAX_INSTRUCTIONS]`, increased in
`execute()` inside the `if (condition_holds(...))` that changes the PC. Not taken =
executed − taken. `write_frequency_file()` already maps instructions to lines; it now also
keeps, per line, the taken count of a branch instruction (opcode 0x10..0x1E), with −1
meaning "not a branch", and prints two more tab-separated columns on those lines:

```
10	x15 = x1 - x4   % compare
10	BEQ .exit	taken 1	not taken 9
...
9	BAL .loopback	taken 9	not taken 0
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern long taken_count[NP][MAX_INSTRUCTIONS];` |
| `processor.cpp` | (globals), `reset` | define it; clear row `p` |
| `processor.cpp` | `execute` | branch taken → `taken_count[p][instr_pc / 4]++` |
| `os.cpp` | `write_frequency_file` | per line: taken count of its branch (−1 if none); extra columns on branch lines |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 12 lines by hand.

## Test
`./test.sh`: runs `sum_array` and `counter` together. `BEQ .exit` runs 10 times: taken
once (the exit) and not taken 9 times; `BAL .loopback` is taken all 9 times; counter's
`BNE .loop` is taken 9 times and falls through once. Only those lines have extra columns,
the first column is the same as in the Lab 6 report, and `sum_array` still stores 45.

## Viva
- **Why not count "not taken" as well?** Executions are already counted for every instruction, so not taken = executed − taken; one new counter is enough.
- **Is `BAL` ever not taken?** No, its condition is always true, so its not-taken column is always 0.
- **What about a program started from a `.byte` file?** Its report lists disassembled instructions, not source lines; this answer leaves that report unchanged (the base regression test reads it), but the same two columns could be printed there.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

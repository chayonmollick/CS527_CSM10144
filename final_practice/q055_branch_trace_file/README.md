# Q055 — Branch trace file

## Question
For every task, write a file `<program>_<pid>.branches` with one line per executed branch,
in execution order: the instruction index of the branch, the index of its target, and
whether it was taken or not taken. Several tasks run at the same time, so their traces
must not mix.

## Answer
A branch has everything needed: its own index `instr_pc / 4`, its target
`instr_pc / 4 + offset` (the offset counts instructions) and the outcome
`condition_holds()`. The branch case of `execute()` now computes the outcome once, writes
`from -> to taken` or `not taken` to the task's file, and then jumps if taken. Each
processor has its own `FILE *branch_file[p]`: the loader opens `<program>_<pid>.branches`
when it puts the task on `p`, and `finalize()` closes it, so interleaved tasks always
write to different files.

```
5 -> 11 not taken
10 -> 4 taken
...
5 -> 11 taken
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern FILE *branch_file[NP];` |
| `processor.cpp` | (globals) | `FILE *branch_file[NP];` |
| `processor.cpp` | `execute` | branch: `taken = condition_holds(...)`; write `from -> to taken/not taken`; jump if taken |
| `os.cpp` | `loader` | open `<program>_<pid>.branches` |
| `os.cpp` | `finalize` | close it |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 9 lines by hand.

## Test
`./test.sh`: runs sum_array, counter and complex_mul together. sum_array's file has 19
lines (BEQ 10 + BAL 9): it starts `5 -> 11 not taken`, `10 -> 4 taken`, has 9 of each,
and ends with `5 -> 11 taken` (the loop exit). counter's `BNE` at 4 jumps back to 1 nine
times and falls through at the end. complex_mul has no branch, so its file is empty.
sum_array still stores 45.

## Viva
- **Why is "to" the target even when the branch is not taken?** It describes the branch; the last column says whether control went there or on to `from + 1`.
- **Why a file per processor and not in `struct Task`?** The processor writes the trace while executing and only knows its processor id; a task keeps its processor until it ends, so `[p]` is that task's file.
- **Is the file closed when a task fails?** Yes: `finalize()` runs for every task that ends, with or without an error.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `complex_mul.txt`, `complex_mul_data.byte`, `counter.txt`, `counter_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

# Q054 — Breakpoint option `-k N`

## Question
Add a command-line option `-k N`. In every task, the first time instruction N (its index
in the program, as in the `-c` listing) is about to execute, print the pid, the
instruction and every register that is not zero (x registers and vector registers).
Later executions of instruction N print nothing.

## Answer
`execute()` knows the address of the current instruction (`instr_pc`), so the check is
one line at its top, **before** the switch, so that the registers are shown as they are
when the instruction starts, like a debugger stopping at a breakpoint:
`if (instr_pc / 4 == break_at && !break_done[p]) breakpoint(p);`. `breakpoint()` prints
each non-zero `Register[p][r]` and each vector register with a non-zero lane (all 8
lanes). `break_done[p]` makes it once per task: `reset()` clears it when a new task is
loaded on processor p. `break_at` (−1 = off) is set by `-k N` in `main()`.

```
[break] pid 1, instruction 8 (x2 = x2 + x6), registers before it runs: x3=4 x4=9 x6=1 x15=-9
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern int break_at;` |
| `processor.cpp` | (globals), `breakpoint` (new) | `break_at`, `break_done[NP]`; print the non-zero x and v registers |
| `processor.cpp` | `reset`, `execute` | clear `break_done[p]`; check the breakpoint before executing |
| `main.cpp` | `main`, `usage` | parse `-k N` (0 ≤ N < 256); usage text; `#include <cstdlib>` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 30 lines by hand.

## Test
`./test.sh`: `-k 10` with sum_array and array_add at the same time prints one line per
task: sum_array's first `BAL` (after one pass: x1 = 1, x2 = 1, x3 = 4, x4 = 9, x6 = 1,
x15 = 0 − 9) and array_add's first `v3 = v1 + v2` (v1 = A[0..7], v2 = B[0..7], v3 still
0). `-k 8` on sum_array prints once although instruction 8 runs 9 times, with x2 still 0.
`-k 200` prints nothing, `-k 300` is rejected, and sum_array still stores 45.

## Viva
- **Why check before the switch?** That is the state at the breakpoint; after the switch, the destination register already holds its new value.
- **Why `break_done[NP]` and not one flag?** Each task must report its own first execution, and the tasks run interleaved on different processors.
- **How does this differ from `-t`?** `-t` prints every executed instruction but no registers; `-k` prints the whole register state once, at one chosen instruction.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

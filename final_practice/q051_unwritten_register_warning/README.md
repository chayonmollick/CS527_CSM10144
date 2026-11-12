# Q051 — Warn about registers read before they are written

## Question
Registers start at 0, so a program that reads an x register it never wrote usually has a
bug. Make the simulator print a warning the first time a task reads such a register,
naming the pid, the register and the source line. Warn only once per register, let the
task continue, and keep the registers of different tasks apart.

## Answer
A per-processor table `xknown[NP][256]` records whether the task has written each x
register (or was already warned about it); `reset()` clears it when a task is loaded.
At the top of `execute()`, a switch lists the x registers each opcode **reads**:
`check_read()` warns if the entry is still false and then sets it, so each register
warns once. After that, the destination is marked as written for the opcodes that
**write** an x register. Reads are checked first, so `x7 = x7 + 1` with x7 never written
warns. The tasks' programs live in `os.cpp`, so a new OS function `source_line(p, index)`
turns the instruction index into its source line through the compiler's `line_of` map:

```
[warn] pid 1 reads x5 before writing it, at line 3: x2 = x1 + x5      % x5 never written: warning
```

## Changes
| File | Function | Change |
|---|---|---|
| `os.h`, `os.cpp` | `source_line` (new) | `"line L: text"` of an instruction of the task on processor p |
| `processor.cpp` | (globals), `check_read` (new) | `xknown[NP][NUM_XREGS]`; warn once, on stderr |
| `processor.cpp` | `reset` | clear `xknown[p]` |
| `processor.cpp` | `execute` | switch: registers read by each opcode → `check_read`; mark `dest` written |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 28 lines by hand.

## Test
`./test.sh`: `uninit.txt` reads x5 on line 3 (and again on line 4: no second warning), x7
in a loop that runs 3 times (one warning, line 6) and x9 as the address of a store
(line 10): exactly 3 warnings. The program still finishes ([0] = 30, [4] = 3).
`sum_array`, running at the same time, gets no warning and still stores 45.

## Viva
- **Which registers does each instruction read?** `+ - * /`: both sources (with a constant: the first); `[xA] = xS`: xA and xS; `xD = [xA]`, `xD = xA`, `print xR`, `[c] = xS`, `vD = [xA]`, `vD = vA op xB`: the register in the last byte; `[xA] = vS`: xA.
- **Is the destination of a load read?** No: `xD = [xA]` writes xD and reads only the address register xA.
- **Why per processor?** Each processor runs one task until it ends, and `reset()` clears the table for the next one, so one task's writes never hide another task's bug.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `sum_array.txt`, `sum_array_data.byte`, `uninit.txt`, `uninit_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

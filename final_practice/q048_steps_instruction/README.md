# Q048 — Instruction `x1 = steps`

## Question
Add a statement `xD = steps` that puts into xD the number of instructions the task has
executed so far (its own count, not the other tasks'). Give it a new opcode in the range
C0..CF and show it in the listing. Write a program that reads the counter before and
after a loop and check the values by hand.

## Answer
The processor already counts instructions in `steps[p]` (the "after N instructions" of
the `[os]` line), and a task keeps its processor until it ends, so `steps[p]` is exactly
this task's count. A new instruction copies it into a register:
- `config.h`: `OP_STEPS = 0xC0`, encoded `C0 D 0 0`.
- `compiler.cpp` `encode_statement()`: `x<n> = steps` is recognised before the normal assignment, which would reject `steps` as an operand. A vector destination is a compile error.
- `processor.cpp` `execute()`: `x[dest] = steps[p]`. `steps[p]++` runs at the top of `execute()`, so the value **includes the `steps` instruction itself**: a program's first instruction reads 1.
- `disassemble()`: `x1 = steps`, for the `-c` listing and the `-t` trace.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_STEPS = 0xC0` |
| `compiler.cpp` | `encode_statement` | `x<n> = steps` → `set(ins, OP_STEPS, n, 0, 0)` |
| `compiler.cpp` | `disassemble` | `case OP_STEPS` → `"x%d = steps"` |
| `processor.cpp` | `execute` | `case OP_STEPS: x[dest] = (int)steps[p];` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 11 lines by hand.

## Test
`./test.sh`: the listing shows `C0 01 00 00   x1 = steps`. `steps.txt` runs twice at the
same time: `x1 = steps` is the first instruction (1); after `x2 = 0` and 5 loop
iterations of 3 instructions, `x4 = steps` is instruction 2 + 15 + 1 = 18, and the
difference is 17. Both copies get the same values, so the counts do not mix. `v1 = steps`
is a compile error, and `sum_array` still stores 45.

## Viva
- **Why does the first instruction read 1?** `execute()` increments `steps[p]` before the switch, so the count includes the current instruction. Reading before incrementing would give 0; either is fine if you say which.
- **Why not one global counter?** Tasks run interleaved on 4 processors; a global count would include the other tasks' instructions. `steps[p]` belongs to the processor, which the task keeps until it ends.
- **Why handle it before the assignment code?** `parse_operand()` accepts only registers, constants and `[...]`, so `steps` would be reported as "not a register or a constant".

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `steps.txt`, `steps_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

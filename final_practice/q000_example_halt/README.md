# Q000 — Add a `halt` statement (format example)

## Question
Add a statement `halt` to the language that ends the program immediately, wherever
it appears (for example inside a loop). It must not need a new opcode. Show a
program that stops early and check that the code after `halt` never runs.

## Answer
Opcode `00` already means "end of program" (`execute()` sets `end_of_simulation`),
so `halt` only needs the compiler: one more case in `encode_statement()` that emits
`00 00 00 00`. Nothing changes in the processor or the OS.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `encode_statement` | `halt` → `set(ins, OP_HALT, 0, 0, 0)` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 4 lines by hand.

## Test
`./test.sh`: builds lab6 + the patch, runs `early_stop.txt` (the counter reaches 5,
`[0] = 5` and `[4]` stays 0), checks the listing and that `sum_array` still works.

## Viva
- **Why no new opcode?** The processor already stops on `00`; `halt` is the same instruction placed by the programmer instead of by the compiler.
- **What is the difference from the end of the program?** The compiler always appends `00 00 00 00`; `halt` puts one in the middle, so the listing shows two.

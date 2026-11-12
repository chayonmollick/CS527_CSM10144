# Q024 — `x1++` and `x1--`

## Question
Add the statements `xR++` and `xR--`, which add 1 to or subtract 1 from a register.
They must behave exactly like `xR = xR + 1` and `xR = xR - 1`, including the flags, so
that a loop can end with `x1--` followed directly by `BNE .loop`. Do it without
changing the processor.

## Answer
**Compiler only.** These statements need no new instruction: `x1++` is `ADDI x1, x1, 1`
= `09 R R 01`, and `x1--` is `SUBI` = `0A R R 01`. `tokenize()` splits every `+` and `-`,
so `x1++` arrives as the 3 tokens `x1 + +`. In `encode_statement`, the pattern "a
register, then the same sign twice" emits the ADDI or SUBI.

**Flags:** the instruction *is* an add or a subtract, so it sets Z, N, C and V exactly
like `x1 = x1 - 1`. When `x1--` reaches 0 it sets Z, so `BNE` falls through, and `0--`
sets N. There is no new opcode and no processor or disassembler change: the listing
shows `x1 = x1 - 1` for `x1--`.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `encode_statement` | the 3 tokens `xR + +` / `xR - -` → `set(ins, OP_ADDI or OP_SUBI, R, R, 1)` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 6 lines by hand.

## Test
`./test.sh`: runs `inc_dec.txt` and checks:
- the loop `x2 = x2 + x1; x3++; x1--; BNE .loop` gives the sum 55 in 10 iterations
  and ends with x1 = 0;
- `x4--` from 0 gives -1, and the `BMI` after it is taken;
- the listing shows `x3++` as `09 03 03 01` and `x1--` as `0A 01 01 01`;
- `sum_array` still works.

## Viva
- **Why no new opcode?** `x1++` is syntactic sugar: it is the same machine instruction as `x1 = x1 + 1`, just as `i++` in C compiles to an add. Changing only the compiler keeps the instruction set small and leaves old byte code unchanged.
- **What does `x1--` do to the flags when x1 = 0?** The result is -1, so Z = 0, N = 1, C = 0 (0 > 1 is false as unsigned) and V = 0.
- **How would you add `++x1`?** Check `t[0] == t[1]` (both `+` or both `-`) and parse the register from `t[2]`. In a statement (not inside an expression), pre- and post-increment do the same thing.

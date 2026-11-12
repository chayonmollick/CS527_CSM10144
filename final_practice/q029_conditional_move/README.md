# Q029 — Conditional move: `x1 = x3 if x2`

## Question
Add a conditional move `xD = xA if xB`: it copies xA into xD when xB is not 0, and
otherwise leaves xD unchanged. It must not use a branch. Use it to find the last
non-zero element of an array, and to count the non-zero elements, with no branch in
the loop body.

## Answer
The new opcode is `OP_CMOV = 0x68`, with the format `68 D A B` (A is the value, B the
condition), like `ADD`. The statement already has the shape `dest = a <op> b`, with the
word `if` as the operator, so `encode_statement` parses it unchanged and the new case
goes at the top of `encode_arith`. The processor does:
`if (x[src2] != 0) x[dest] = x[src1];`
- The condition is "not 0", as in C: any non-zero value is true, including negative ones.
- **Flags:** neither read nor written, because the condition is a register.
- A conditional increment takes two instructions: `x6 = x4 + 1`, then `x4 = x6 if x5`.

This works like x86 `CMOVcc`, ARM `CSEL` and the RISC-V Zicond instructions. The
instruction always executes and only the write depends on the condition, so there is
no branch to mispredict.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_CMOV = 0x68` (and a comma after `OP_VMULX`) |
| `compiler.cpp` | `encode_arith` | operator `if` → `set(ins, OP_CMOV, dst, a, b)` (x registers only) |
| `compiler.cpp` | `disassemble` | `x%d = x%d if x%d` |
| `processor.cpp` | `execute` | `case OP_CMOV: if (x[src2] != 0) x[dest] = x[src1];` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 12 lines by hand.

## Test
`./test.sh`: runs `cmov.txt` over the array 4 0 9 -3 0 0 and checks:
- the last non-zero element is -3;
- 3 elements are non-zero;
- `x7 = x8 if x0`, with x0 = 0, leaves x7 = 77;
- the listing shows `68 03 05 05   x3 = x5 if x5`;
- `sum_array` still works.

## Viva
- **Why does xD keep its old value when the condition is 0?** That is the definition: the processor simply skips the write. In hardware, the old value of xD is read as a third input and written back unchanged.
- **Conditional move or branch?** A hard-to-predict branch costs a pipeline flush every time it is mispredicted, while a CMOV always costs exactly one instruction. When the condition is predictable, a branch can be faster, because it skips work.
- **Why the word `if` as an operator?** `x1 = x3 if x2` has the same shape as `x1 = x3 + x2`, so the existing parser hands it to `encode_arith` with no other change.

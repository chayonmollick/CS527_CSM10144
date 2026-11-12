# Q033 — Compare: `CMP x1 x2` and `CMP x1 5`

## Question
Add a compare instruction in two forms, `CMP xA xB` and `CMP xA c` (a constant 0..255).
It sets the flags Z, N, C and V exactly as `xA - xB` would, but writes no register, so
a program no longer needs a spare register for every comparison. Use it with `BLE` and
`BLT` on signed data.

## Answer
Two opcodes, like `SUB`/`SUBI` but without a destination: `OP_CMP = 0x6D` (`6D 0 A B`) and
`OP_CMPI = 0x6E` (`6E 0 A c`). The dest byte is 0 and is ignored.
- **Compiler:** `CMP` is a statement keyword. `branch_condition("cmp")` returns -1 because
  the word does not start with `b`. `parse_operand` is called twice: first for an x
  register, then for an x register or a constant.
- **Processor:** `b = (opcode == OP_CMP ? x[src2] : src2); set_sub_flags(p, a, b, a - b);`,
  where `a = x[src1]` is already loaded by `execute()`. There is no `x[dest] = ...` line.
- **Flags:** set by the same function as for `SUB`, so every condition (`BEQ`, `BLT`,
  `BHI`, ...) means the same after `CMP` as after a subtraction. This is ARM's `CMP`
  (a `SUBS` whose result is thrown away) and x86's `cmp`.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_CMP = 0x6D`, `OP_CMPI = 0x6E` |
| `compiler.cpp` | `encode_statement` | `CMP xA xB` / `CMP xA c` → `set(ins, OP_CMP or OP_CMPI, 0, A, B)` |
| `compiler.cpp` | `disassemble` | `CMP x%d x%d`, `CMP x%d %d` |
| `processor.cpp` | `execute` | `case OP_CMP: case OP_CMPI:` → `set_sub_flags` only |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 19 lines by hand.

## Test
`./test.sh`: runs `cmp_max.txt` and checks:
- the maximum of 3 -8 12 7 -1 12, found with `CMP x4 x3` + `BLE`, is 12 (the compare is
  signed, so -8 does not win);
- a count-up loop with `CMP x5 10` + `BLT` sums 0..9 = 45;
- x0 = 77 is unchanged. CMP's dest byte is 0, so a wrong `x[dest] = ...` would
  overwrite x0;
- the listing shows `6D 00 04 03` and `6E 00 05 0A`;
- `sum_array` still works.

## Viva
- **Why not compile CMP to a `SUB` into a scratch register?** That silently destroys a register the program may be using, and nothing says which one. CMP writes none.
- **What is C after `CMP x1 x2`?** By this machine's rule for subtraction, C = 1 when x1 > x2 as unsigned numbers. So `BHI` means unsigned greater-than, while `BGT`/`BGE`/`BLT` compare signed numbers.
- **Can you write `CMP 5 x1`?** No: a constant can only be the second operand. Write `CMP x1 5` and reverse the condition (`BGT` becomes `BLT`).

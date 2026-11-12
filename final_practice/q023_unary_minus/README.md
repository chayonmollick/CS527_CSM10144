# Q023 — Unary minus: `x1 = -x2`

## Question
Allow a minus sign in front of a register: `xD = -xA` stores the negative of xA (two's
complement). The subtraction `xD = xA - xB` must keep working. Implement it in the
compiler and the processor, say what happens to the flags and to -2147483648, and test
it, including `x3 = -x3` in place.

## Answer
`tokenize()` already splits off every `-`, so `x2 = -x1` arrives as the 4 tokens
`x2 = - x1`, while `x6 = x1 - x2` has 5 tokens. In `encode_statement`, right after the
`=`, a `-` followed by exactly one operand is therefore unary minus.

The new opcode is `OP_NEG = 0x62`, encoded like `MOV`: `62 D 0 A`. The processor computes
`0 - xA` in unsigned arithmetic: `x[dest] = (int)(0u - (uint32_t)x[src2])`.
- Unsigned wrap-around is defined in C++, while `-x` on `INT_MIN` would be a signed
  overflow (undefined behaviour). So -(-2147483648) wraps to -2147483648, the same result
  real hardware gives.
- **Flags:** unchanged, because only add and subtract set them and NEG is a separate
  instruction (as with ARM `NEG` without S, or RISC-V `neg`). To branch on the sign of
  the result, use `x9 = x2 + 0` first.
- **Why a new opcode?** Without one, the compiler would need `xD = 0` and then
  `xD = xD - xA`. That is 2 instructions, it changes the flags, and it fails in place:
  for `x3 = -x3` the first instruction destroys x3 and the result is 0. Multiplying by
  -1 is not possible either, because constants are only 0..255.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_NEG = 0x62` (and a comma after `OP_VMULX`) |
| `compiler.cpp` | `encode_statement` | after `=`: `- xA` (exactly two more tokens) → `set(ins, OP_NEG, dst, 0, a)` |
| `compiler.cpp` | `disassemble` | `x%d = -x%d` |
| `processor.cpp` | `execute` | `case OP_NEG: x[dest] = (int)(0u - (uint32_t)x[src2]);` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 10 lines by hand.

## Test
`./test.sh`: runs `negate.txt` and checks:
- -(25) = -25, and -(-7) = 7 in place;
- -(-2147483648) = -2147483648;
- 25 - (-25) = 50, computed with the old SUB;
- the flags survive `x8 = -x1`.

It also checks the listing lines `62 02 00 01   x2 = -x1` and `02 06 01 02` (the
subtraction is still SUB), and that `sum_array` still works.

## Viva
- **How does the compiler tell `x1 = -x2` from `x1 = x2 - x3`?** By position. In the unary form, the token after `=` is `-` and exactly one operand follows it. In the subtraction, an operand comes first.
- **What about `x1 = -5`?** `5` is not a register, so the compiler rejects it, because constants are only 0..255. Extension e03 shows how to build any 32-bit constant.
- **Should NEG set the flags?** Either choice can be defended: x86 `NEG` sets them, while ARM `NEG` (without S) and RISC-V `neg` do not. One added line, `set_sub_flags(p, 0, b, 0 - b);`, makes NEG behave exactly like `0 - xA`.

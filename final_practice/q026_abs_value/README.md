# Q026 — Absolute value: `x1 = abs x2`

## Question
Add `xD = abs xA`, which stores the absolute value of xA. Implement it with a new opcode,
explain why the compiler alone cannot do it well, and say what `abs` gives for
-2147483648. Test it with a program that sums the absolute values of an array.

## Answer
The new opcode is `OP_ABS = 0x63`, encoded like `MOV`: `63 D 0 A`. The compiler looks for
`abs` right after the `=`, followed by exactly one operand (`xD = abs xA`, 4 tokens). The
processor does:
`x[dest] = x[src2] < 0 ? (int)(0u - (uint32_t)x[src2]) : x[src2];`
- The negation uses unsigned arithmetic. -2147483648 has no positive 32-bit partner
  (+2^31 does not fit), so its abs wraps to -2147483648, as on real hardware. Writing
  `-x` in C++ would be signed overflow, which is undefined behaviour.
- **Flags:** unchanged, because only add and subtract set them.
- **Why not compiler only?** abs needs a decision. Without a new opcode it becomes a
  compare, a branch over a negate, and the negate itself, which needs `0 - x` and so a
  scratch register. That is 3-4 instructions plus a label the compiler would have to
  invent, and it changes the flags. One branch-free instruction is simpler and faster.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_ABS = 0x63` (and a comma after `OP_VMULX`) |
| `compiler.cpp` | `encode_statement` | after `=`: `abs xA` → `set(ins, OP_ABS, dst, 0, a)` |
| `compiler.cpp` | `disassemble` | `x%d = abs x%d` |
| `processor.cpp` | `execute` | `case OP_ABS:` negate xA if it is negative |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 10 lines by hand.

## Test
`./test.sh`: runs `abs_sum.txt` and checks:
- a loop computes |3| + |-4| + |0| + |-10| + |6| = 23;
- `x6 = abs x6` in place gives 4;
- `abs(-2147483648)` stays -2147483648;
- the array itself is unchanged;
- the listing shows `63 05 00 04   x5 = abs x4`;
- `sum_array` still works.

## Viva
- **Can abs(x) ever be negative?** Only for -2147483648 (0x80000000), whose negation does not fit in 32 bits. In C, `abs(INT_MIN)` is undefined; here it is defined as wrap-around.
- **How can abs be done without a branch, given the shifts and XOR of e01?** Compute `m = x >> 31` (arithmetic shift, so m is 0 or -1), then `abs = (x ^ m) - m`.
- **Why is the source in operand 2?** It is the same layout as `MOV`, so every one-source instruction is decoded the same way.

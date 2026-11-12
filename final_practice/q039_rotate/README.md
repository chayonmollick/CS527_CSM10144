# Q039 — Rotate `x1 = x2 rol 3`, `x1 = x2 ror x3`

## Question
Add rotate instructions: `x1 = x2 rol 3` rotates x2 left by 3 bits, and `x1 = x2 ror x3`
rotates it right by the amount in x3. Support both a constant and a register amount in
both directions. The bits that leave one end come back in at the other, and the amount is
taken modulo 32. Give the encodings, and explain how a rotate differs from a shift.

## Answer
- **Four opcodes.** They follow the base rule that the constant form is the register form
  plus 8 (ADD 01 / ADDI 09):

  | Statement | Encoding |
  |---|---|
  | `xD = xA rol xB` | `90 D A B` |
  | `xD = xA ror xB` | `91 D A B` |
  | `xD = xA rol c` | `98 D A c` |
  | `xD = xA ror c` | `99 D A c` |
- **Compiler:** `rol` and `ror` are words, so the tokenizer passes them through.
  `x1 = x2 rol 3` reaches `encode_arith()` as `dest = a <op> b` with op `rol`. A check at
  the top picks the opcode from the direction and the kind of `b`.
- **Processor:** one case handles all four opcodes.
  - `n = amount & 31`.
  - A right rotation by n equals a left rotation by 32 − n.
  - Result = `(a << n) | (a >> (32 - n))` on `uint32_t a`.
  - n = 0 returns `a` directly, because `a >> 32` is undefined behaviour in C++.
- **Unsigned matters:** `>>` on a negative `int` copies the sign bit. `0x80000001 rol 3`
  would then give 0xFFFFFFFC instead of 0x0000000C.
- **Flags:** unchanged. Only ADD and SUB set flags.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_ROL 0x90`, `OP_ROR 0x91`, `OP_ROLI 0x98`, `OP_RORI 0x99` |
| `compiler.cpp` | `encode_arith` | Operators `rol` and `ror`: x registers only; a register or constant amount picks the opcode |
| `compiler.cpp` | `disassemble` | `x2 = x1 rol 3`, `x7 = x5 ror x6` |
| `processor.cpp` | `execute` | One case for the 4 opcodes: amount mod 32, right = left by 32 − n, n = 0 handled separately |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `rotate.txt`. It checks:
- `1 ror 1` = 0x80000000, then `0x80000001 rol 3` = 12 (bit 31 wraps to bit 2);
- `0xAB ror 4` = 0xB000000A;
- 0x12345678, built with `rol 8` and adds, then rotated by a register amount: by 8 →
  0x78123456; by 36 (= 4) → 0x23456781; by 32 → unchanged;
- the little-endian bytes in the `.out` file and all four encodings in the listing;
- that `sum_array` still works.

## Viva
- **Rotate or shift?** A shift drops the bits that leave and brings in zeros (or copies of the sign bit). A rotate only moves bits around, so nothing is lost: `rol n` followed by `ror n` gives back the original value.
- **Why mod 32?** Rotating by 32 is a full turn, so only the amount mod 32 matters. x86 `ROL` also masks the count to 5 bits. In `rotate.txt`, rotating by 36 equals rotating by 4.
- **Where are rotates used?** Hash functions and ciphers (SHA-256 is built from rotations), CRCs and checksums. Here, `rol 8` also builds a 32-bit constant from bytes while the top byte is still 0.
- **Why no `rol` for vectors?** It could be added lane-wise (like `vector_op()`), with opcodes in the same range. The question only asks for scalars.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `rotate.txt`, `rotate_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

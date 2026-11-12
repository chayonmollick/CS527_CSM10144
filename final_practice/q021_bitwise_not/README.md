# Q021 — Bitwise NOT: `x1 = not x2`

## Question
Add a statement `xD = not xA` that stores the bitwise complement of xA in xD (every bit
flipped, the C operator `~`). Choose an opcode, update the compiler, the processor and
the listing, and decide whether `not` changes the flags. Show `not 0`, `not 5`, `not`
used in place (`x5 = not x5`), and that a branch after `not` still sees the flags of the
compare before it.

## Answer
Nothing in the instruction set can flip bits (there is no XOR, and constants are only
0..255), so `not` gets a new opcode: `OP_NOT = 0x61`, encoded like `MOV` with the source
in operand 2: `61 D 0 A`.
- **Compiler:** `tokenize()` passes the word `not` through, so `x2 = not x1` arrives as
  the 4 tokens `x2 = not x1`. Right after the `=`, `encode_statement` checks for `not`
  followed by exactly one operand, before the usual `dest = a [op b]` parsing.
- **Processor:** `x[dest] = ~x[src2]`. In two's complement `not x = -x - 1`, so
  `not 5 = -6` and `not 0 = -1`. It can never overflow.
- **Flags:** unchanged. Only add and subtract set the flags (the handout's rule), the
  same as for `*`, `/` and the moves. The test shows it: a `BEQ` after `not` still uses
  the Z flag set by the earlier subtraction.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_NOT = 0x61` (and a comma after `OP_VMULX`) |
| `compiler.cpp` | `encode_statement` | after `=`: `not xA` → `set(ins, OP_NOT, dst, 0, a)` |
| `compiler.cpp` | `disassemble` | `x%d = not x%d` |
| `processor.cpp` | `execute` | `case OP_NOT: x[dest] = ~x[src2];` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 10 lines by hand.

## Test
`./test.sh`: builds lab6 + the patch and runs `not_bits.txt`. It checks `not 0 = -1`,
`not 5 = -6`, `not 0x0F0F0F0F = 0xF0F0F0F0` (in place), `not (not 5) = 5`, and that the
flags survive (`[28] = 1`). It also checks the listing line `61 02 00 01   x2 = not x1`
and that `sum_array` still works.

## Viva
- **Could the compiler do it without a new opcode?** `not x = -1 - x`, so it could emit `xD = 0`, `xD = xD - 1`, `xD = xD - xA`. That is three instructions and they change the flags. They are also wrong in place: for `x5 = not x5`, the first instruction destroys x5 and the result is 0.
- **Bitwise or logical not?** This is bitwise: `not 5 = -6`. A logical not (C's `!x`, which gives 1 if x is 0 and 0 otherwise) is a different instruction; Q028's `x1 = x2 == x0` gives it when x0 holds 0.
- **Why is the source in operand 2?** It is the same layout as `MOV` (`07 D 0 A`) and the loads, so every one-source instruction is decoded the same way.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `not_bits.txt`, `not_bits_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

# Q044 — Strided vector load `vD = [x1], x2`

## Question
`v1 = [x1]` loads 8 consecutive words, so a column of a row-major matrix cannot be loaded
with one instruction. Add a strided load `vD = [x1], x2`: lane i comes from address
x1 + i·x2 (the stride x2 is in bytes, and may be 0 or negative). Explain how the encoding
fits in 4 bytes, and implement it. Use it to transpose an 8 × 3 matrix, and show what
strides 0, −12 and 4 do.

## Answer
- **Encoding `A5 D A S`:** the instruction has three operands, the vector destination, the
  base register and the stride register. They fill the three operand bytes in source
  order: `v1 = [x1], x2` → `A5 01 01 02`.
  - The opcode is VLOAD (0x25) + 0x80.
  - VLOAD itself is `25 D 0 A`: its stride is fixed at 4, so it needs one register byte
    fewer.
  - A register stride is necessary because a constant byte (0..255) cannot hold −12.
- **Compiler:** the tokenizer turns `,` into a blank, so the statement is the six tokens
  `v1 = [ x1 ] x2`. No old statement has that shape. `encode_statement()` checks for a v
  destination and two x registers.
- **Processor:** for i = 0..7, address = x1 + i·x2, computed in `uint32_t` so a huge stride
  wraps instead of overflowing. Each address goes through `load_word()`, so every lane is
  translated and checked on its own. The 8 values go to a temporary first and are copied
  into vD only when all 8 loaded, so a fault leaves vD unchanged (like VLOAD).
- **What the strides do** (all from `stride.txt`):
  - **12** (one row of 3 words) loads a column, so three loads and three stores
    transpose the 8 × 3 matrix;
  - **0** loads the same word into all 8 lanes (a broadcast from memory);
  - **−12**, starting at the last row, walks up a column (reverse order);
  - **4** is the ordinary `v1 = [x1]`.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_VLOADS = 0xA5`: `vD = [xA], xS` |
| `compiler.cpp` | `encode_statement` | Tokens `vD = [ xA ] xS` → `A5 D A S` |
| `compiler.cpp` | `disassemble` | `v1 = [x1], x2` |
| `processor.cpp` | `execute` | `case OP_VLOADS`: lane i from xA + i·xS through `load_word()`, all 8 or nothing |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `stride.txt` on M[r][c] = 10r + c (8 rows, 3
columns, row by row). It checks:
- the three columns (the transpose, at 96, 128 and 160);
- stride 0 (31 in every lane), stride −12 (72 62 … 2) and stride 4 (equal to a normal
  load);
- the encoding `A5 01 01 02`;
- that `stride_far.txt` (stride 765) stops with an error naming lane 6's address, 4590.
  Lanes 1..5 are still inside the data space: each touched a new page, so the task
  reports 5 page faults first;
- that `sum_array` still works.

## Viva
- **Why is a strided load slower than a normal one?** Its 8 words can be in 8 different pages and cache lines, and each needs its own translation. A contiguous load touches one or two. Here `stride_far.txt` causes 5 page faults in one instruction.
- **Where is it used?** Columns of row-major matrices (matrix multiply, transpose), one field of an array of structs (stride = struct size), and every k-th sample of a signal. RISC-V V has `vlse32.v` and ARM SVE has gather loads for this.
- **What is the next step after a stride?** A *gather*: lane i comes from `base + offset[i]`, with the offsets in another vector register. A strided load is the special case offset[i] = i·stride.
- **What about a strided store?** `[x1], x2 = v1` would be the mirror image, with opcode A6 (VSTORE 26 + 0x80) and the same three fields.

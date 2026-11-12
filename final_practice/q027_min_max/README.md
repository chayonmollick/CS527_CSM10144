# Q027 — `x1 = min x2 x3` and `x1 = max x2 x3`

## Question
Add `xD = min xA xB` and `xD = max xA xB`, using signed comparison. Use them to find the
smallest and the largest element of an array with no branch inside the loop body, and
show that -3 counts as smaller than 1 (a common bug is to compare as unsigned).

## Answer
Two new opcodes, in the same 3-register format as `ADD`: `OP_MIN = 0x64` (`64 D A B`) and
`OP_MAX = 0x65` (`65 D A B`). `tokenize()` passes the words `min` and `max` through
unchanged (commas are allowed too), so right after the `=`, the compiler looks for
`min`/`max` followed by exactly two operands. The processor does:
```c
case OP_MIN: x[dest] = x[src1] < x[src2] ? x[src1] : x[src2]; break;
case OP_MAX: x[dest] = x[src1] > x[src2] ? x[src1] : x[src2]; break;
```
- `x[]` is `int`, so the comparison is **signed**: min(-3, 1) = -3. Comparing the
  `uint32_t a` that `execute()` loads at its start would be unsigned, and would make
  0xFFFFFFFD the larger value.
- **Flags:** unchanged.
- Without these instructions, every element costs a compare (sub), a branch, a move and
  a label. With them it costs one instruction and no branch, like RISC-V's `min`/`max`
  or the SIMD `pmin`/`pmax`.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_MIN = 0x64`, `OP_MAX = 0x65` |
| `compiler.cpp` | `encode_statement` | after `=`: `min xA xB` / `max xA xB` → `set(ins, OP_MIN or OP_MAX, dst, a, b)` |
| `compiler.cpp` | `disassemble` | `x%d = min x%d x%d`, `x%d = max x%d x%d` |
| `processor.cpp` | `execute` | `case OP_MIN`, `case OP_MAX` with an `int` (signed) comparison |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 16 lines by hand.

## Test
`./test.sh`: runs `min_max.txt` over the array 5 -3 12 -20 7 0 and checks:
- min = -20 and max = 12;
- min(-3, 1) = -3;
- the listing shows `64 03 03 05   x3 = min x3 x5` and `65 04 04 05   x4 = max x4 x5`;
- `sum_array` still works.

## Viva
- **Signed or unsigned?** Signed, because `x[]` is `int`. An unsigned version (`minu`) would compare `(uint32_t)` values and would return 1 for min(-3, 1).
- **Why is branch-free better?** With no branch there is nothing to mispredict and no pipeline flush (see e07/e08), and the loop body is straight-line code.
- **How would you add `x1 = max x2 0` (ReLU, clamping)?** Add a constant form `OP_MAXI` with `c` in operand 2, the way `ADDI` goes with `ADD`.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `min_max.txt`, `min_max_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

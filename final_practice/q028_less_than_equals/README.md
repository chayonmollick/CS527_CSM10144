# Q028 — Comparisons into a register: `x1 = x2 < x3`, `x1 = x2 == x3`

## Question
Add comparison expressions that store 1 or 0 in a register: `xD = xA < xB` (signed less
than) and `xD = xA == xB` (equal). The tokenizer does not know `<` or `==` yet. Use them
to count, without branches, how many elements of an array are smaller than a threshold
and how many are equal to it.

## Answer
**Tokenizer.** `tokenize()` puts spaces around single symbols, so `==` would become two
`=` tokens, and `<` would stay stuck to its neighbours (`x2<x3`). The loop now runs by
index so that it can look at the next character: `==` becomes one token, and `<` joins
the list of symbols. This is the same idea as e01's `<<` and `>>`.

**Compiler.** `encode_statement` already parses `xD = xA <op> xB` and calls
`encode_arith`, so the new operators go at the top of `encode_arith`: `<` gives
`OP_SLT = 0x66` and `==` gives `OP_SEQ = 0x67`. Both use the `op D A B` format of `ADD`.

**Processor.** It does `x[dest] = x[src1] < x[src2];` and `x[dest] = x[src1] == x[src2];`.
The values are `int`, so `<` is signed (-2 < 5 gives 1), and a `bool` converts to
exactly 1 or 0.

**Flags:** unchanged. The result is a normal register value, so it can be added to a
counter without any branch, like RISC-V and MIPS `slt`.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_SLT = 0x66`, `OP_SEQ = 0x67` |
| `compiler.cpp` | `tokenize` | loop by index; `==` is one token; `<` is split like `+` |
| `compiler.cpp` | `encode_arith` | `<` → `OP_SLT`, `==` → `OP_SEQ` (x registers only) |
| `compiler.cpp` | `disassemble` | `x%d = x%d < x%d`, `x%d = x%d == x%d` |
| `processor.cpp` | `execute` | `x[dest] = x[src1] < x[src2]` and `x[dest] = x[src1] == x[src2]` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the changes by hand (about 20 lines).

## Test
`./test.sh`: runs `count_cmp.txt` over the array 3 5 -2 8 5 10 with the threshold
t = 5, and checks:
- 2 elements are < 5 (the comparison is signed, so -2 counts);
- 2 elements are equal to 5;
- 5 < 5 gives 0;
- `x9 = x2==x2`, written without spaces, gives 1;
- the listing shows `66 07 06 02` and `67 07 06 02`.

It also checks that `sum_array` and the vector program `fir` still work with the new
tokenizer.

## Viva
- **Why is `==` a problem for the tokenizer?** `=` is already the assignment token. Splitting every `=` turns `x1 = x2 == x3` into 6 tokens. Looking one character ahead makes `==` a single token, and a lone `=` still means assignment.
- **How do you get `>`, `<=` and `!=`?** `a > b` is `b < a`, so the compiler can swap the operands and needs no new opcode. `<=` and `!=` are the opposites of `>` and `==`: either add one opcode each, or compute `1 - result` using a register that holds 1.
- **Signed or unsigned `<`?** Signed, like RISC-V `slt`. An unsigned version (`sltu`) compares `(uint32_t)` values, so -2 < 5 would give 0.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `count_cmp.txt`, `count_cmp_data.byte`, `fir.txt`, `fir_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

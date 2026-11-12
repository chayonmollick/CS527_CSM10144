# Q041 — Compound assignment `x1 += x2`

## Question
Add compound assignment to the language: `x1 += x2`, `x1 -= 5`, `x1 *= x3` and `x1 /= 2`
mean `x1 = x1 + x2`, `x1 = x1 - 5` and so on. It must be pure compiler sugar: no new opcodes,
and exactly the same byte code as the written-out statements. It should also work for
vectors (`v1 *= 3`). Where do you make the change, and how small can it be?

## Answer
- **Tokens:** the tokenizer already splits `+=` into `+` `=`, so `x1 += x2` reaches
  `encode_statement()` as the four tokens `x1 + = x2`.
- **Rewrite:** one `if` recognises that shape (`t[2]` is `=` and `t[1]` is one of `+ - * /`).
  It calls `encode_statement()` again with the tokens `x1 = x1 + x2`. All the real work is
  the existing code:
  - the register-or-constant choice (ADD or ADDI);
  - vectors;
  - range checks and error messages.
- **Same byte code:** `x1 += x2` is `01 01 01 02` and `x1 /= 2` is `0C 01 01 02`, exactly
  what `x1 = x1 + x2` and `x1 = x1 / 2` give. `compound.txt` and the written-out
  `compound_long.txt` compile to identical files.
- **Flags:** the same as the long form. `x5 -= 1` is a SUBI, so it sets Z for the BNE.
- **Vectors:** they work for free. `v1 *= 3` is VMULI and `v1 += v1` is VADD. `v1 /= 2`
  gets the existing "vector division is not supported" error.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `encode_statement` | Tokens `a op = b` → encode the statement `a = a op b` instead (2 lines) |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `compound.txt`. It checks:
- ((10 + 3) − 5) · 4 / 2 = 16;
- a loop that sums 10..1 = 55 with `+=` and `-=`;
- a vector that becomes 6× its input (6 and 48 in lanes 0 and 7);
- the listing bytes of `x1 += x2` and `x1 /= 2`;
- that the byte code equals that of `compound_long.txt`, the same program written out;
- that `sum_array` still works.

## Viva
- **Why does the processor not change?** It only sees bytes, and `x1 += x2` and `x1 = x1 + x2` compile to the same 4 bytes. Syntax that changes nothing below the compiler is called "syntactic sugar".
- **Why call `encode_statement()` again instead of encoding directly?** The rewritten statement goes through all the existing checks, so `x1 += 300`, `x1 += v2` and the rest get the normal error messages, and any later fix to the normal path also covers compound statements.
- **Is `x1 += x2 * x3` the same kind of sugar?** No. `x1 = x1 + x2 * x3` has two operators, so it needs two instructions and a temporary register, or a new multiply-accumulate instruction (Q035).
- **What would `[x1] += 5` need?** Three instructions (load, add, store) and a free register. The compiler would then have to expand one line into several and count them in pass 1, as Q034 does.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `compound.txt`, `compound_data.byte`, `compound_long.txt`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

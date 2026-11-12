# Q034 — Negative constants

## Question
Constants are 0..255, so `x1 = x2 + -5` and `x1 = -7` are compile errors. Without adding
an opcode, make the compiler accept `x1 = x2 + -5`, `x1 = x2 - -3` and `x1 = -7` by turning
them into the opposite operation or into a short sequence of existing instructions.
Labels, branch offsets and the Lab 6 frequency report must stay correct. Show the byte
code produced, and say what happens to `x1 = x2 * -3`.

## Answer
The tokenizer already splits `-5` into `-` `5`, so the compiler sees `x1 = x2 + - 5`.

- **`a + -c` and `a - -c`:** `encode_statement()` rewrites the tokens with the opposite
  operator and drops the sign, then encodes the new statement. `x1 = x2 + -5` becomes
  `x1 = x2 - 5` (`0A 01 02 05`), and `x1 = x2 - -3` becomes `x1 = x2 + 3` (`09 01 02 03`).
  It is still one instruction, with the same constant byte.
- **`xD = -c`:** no instruction loads a negative constant (MOVC's byte is zero-extended),
  so `expand()` turns the line into two statements: `xD = 0` (MOVC), then `xD = xD - c`
  (SUBI). For −7 that gives 0 − 7 = 0xFFFFFFF9.
- **Labels:** a label is an instruction number, so pass 1 must count `x1 = -7` as 2.
  Pass 1 and pass 2 both call `expand()`, so they always agree. Pass 2 gives both
  instructions the same `line_of`, so the listing and the trace show the source line.
- **Frequency report:** the two instructions of a line both run each time the line runs.
  `write_frequency_file()` now adds only the count of the *first* instruction of a line,
  so a line run 3 times shows 3, not 6.
- **`* -c` and `/ -c`** have no one-instruction form, so they are a compile error with a
  clear message. Vectors work for free: `v1 = v2 + -3` becomes VSUBI.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `encode_statement` | `a + -c` → `a - c`, `a - -c` → `a + c` (rewrite the tokens, encode again); `*`/`/` with `-c` → error |
| `compiler.cpp` | `expand` (new) | `xD = -c` → two statements, `xD = 0` and `xD = xD - c`; any other line → itself |
| `compiler.cpp` | `compile_file` | Pass 1: `count += expand(...).size()`. Pass 2: encode every part, all with the same `line_of` |
| `os.cpp` | `write_frequency_file` | Add `exec_count` only for the first instruction of each line |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `negconst.txt`. It checks:
- the results 15, 23, −7, −8, −6 at addresses 0..16;
- the listing: one SUBI and one ADDI, `x1 = -7` as `0F 01 00 00` + `0A 01 01 07`, 19
  instructions, and `BNE -4`. The loop's label comes after a 2-instruction line
  (`x1 = -7`), so a wrong pass-1 count would break the branch;
- that the `.freq` count of `x6 = -2` (inside the loop) is 3;
- that `x2 = x1 * -3` (`negconst_bad.txt`) is rejected;
- that `sum_array` still works.

## Viva
- **Why does `x1 = -7` take two instructions?** MOVC's constant is one byte and is zero-extended, so it can only make 0..255. RISC-V does it in one (`addi x1, x0, -7`) because x0 is always 0 and its immediate is signed. Here no register is guaranteed to be 0, so the compiler makes one first.
- **What breaks if pass 1 still counts one per line?** In `negconst.txt`, `.loop` would be instruction 11 instead of 12, so the offset would be `BNE -5`. The branch would land on `x5 = 0`, which clears the sum on every pass, and address 16 would hold −2 instead of −6.
- **Are the flags the same as adding −5?** The result is the same 32 bits, so Z, N and V match. C follows the subtraction rule (x2 > 5, unsigned). Note that `x1 = -7` changes the flags, because it ends with a SUBI, but `x1 = 7` (MOVC) does not.
- **Why count only the first instruction in `.freq`?** The report counts executions of a *source line*. Each run of the line executes both instructions once, so summing them would double the count.

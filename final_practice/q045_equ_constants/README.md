# Q045 — Named constants `.equ NAME value`

## Question
Add named constants with a directive `.equ NAME value`, where value is 0..255 (decimal or
hex). After `.equ N 5`, the name works wherever a constant is allowed: `x1 = N`,
`x3 = x3 + STEP`, `[RESULT] = x2`, `Write x5 SLOT`. A `.equ` line produces no instruction.
Today a line that starts with `.` is a label, so `.equ N 5` fails with "bad label". In which
pass do you handle the directive, and how are names replaced?

## Answer
- **Pass 1, before the label check:** both `.equ` and labels start with `.`, and the label
  check demands `.name` alone on its line. That is why lab6 says
  `bad label '.equ N 5' (a label is .name, alone on its line)`. Pass 1 now tokenizes the
  line and, if the first token is `.equ`:
  - checks the NAME: it starts with a letter and is not a register such as `x1`;
  - checks the value with `parse_number()` (decimal or hex, 0..255);
  - rejects a name defined twice;
  - stores NAME → value in a map, then `continue`s. There is no instruction and no label,
    and `count` does not change.
- **Pass 2:** lines that start with `.` are already skipped, so `.equ` lines need nothing.
  Before a statement is encoded, every token that is a defined name is replaced by its value.
  From then on it is an ordinary number, so it is allowed exactly where a decimal constant
  is (second operand, `[N]`, `xD = N`, `Write xS c`), with the same range check.
- **Forward use:** all names are collected in pass 1 before pass 2 encodes anything, so a
  name can be used above its `.equ` line, like a forward label. `equ.txt` defines `SLOT`
  on its last line.
- **No run-time cost:** the byte code is the same as for the program written with numbers.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `is_name` (new) | A name starts with a letter and is not `x<n>` or `v<n>` |
| `compiler.cpp` | `compile_file`, pass 1 | `.equ NAME value` is recognised before the label test; it is checked and stored in the map `equ` |
| `compiler.cpp` | `compile_file`, pass 2 | Each token that is a defined name is replaced by its value before `encode_statement()` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `equ.txt`, which sums N = 5 words starting at
FIRST and stores at RESULT (0x40). It checks:
- the sum 35 at 64, and 15 at 68 written with `Write x5 SLOT`, where `SLOT` is defined
  after its use;
- the listing: 13 instructions (no `.equ` line produced one), `BNE -4`, `x3 = FIRST` →
  `0F 03 00 04`, `[RESULT] = x2` → `0E 40 00 02`;
- that the `.freq` report leaves out the `.equ` lines but keeps the label `.loop`;
- that `.equ BIG 300` and `.equ x1 5` are compile errors;
- that `sum_array` still works.

## Viva
- **Why replace tokens and not text?** A text replacement of `N` would also turn `BNE .loop` into `B5E .loop`. Tokens are whole words, so only the name itself is replaced.
- **Why forbid register names?** After `.equ x1 5`, every `x1` in the program would become the number 5, so `x1 = x1 + 1` would stop compiling.
- **What is the difference between `.equ` and a register that holds the value?** `.equ` costs nothing at run time, because it becomes the constant byte of the instruction. It must fit in 0..255, though, and it cannot change. A register can hold any 32-bit value and can change, but it costs an instruction to set and it uses up a register.
- **Where else is this used?** GNU `as` (`.equ`, `.set`), NASM (`EQU`) and C's `#define`. All of them are handled by the assembler or the preprocessor, never by the CPU.

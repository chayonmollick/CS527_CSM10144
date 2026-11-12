# Q030 — Branch on a register: `BZ x1 .label`, `BNZ x1 .label`

## Question
Add two branches that test a register instead of the flags: `BZ xR .label` jumps when xR
is 0, and `BNZ xR .label` jumps when xR is not 0. They must neither read nor change the
flags. Put the register in the destination byte of the instruction, and keep the offset
relative, like the Lab 2 branches.

## Answer
The new opcodes are `OP_BZ = 0x69` and `OP_BNZ = 0x6A`, with the format `69 R 0 offset`.
The register goes in the dest byte, and the signed offset stays in operand 2, exactly
where `B<cond>` keeps it.
- **Compiler:** `branch_condition("bz")` returns -1, since there is no condition named
  `Z`, so the old branch code ignores these words. A new case in `encode_statement`
  parses `xR` and `.label`, then does the same arithmetic as the old branches: computes
  `offset = label - index`, checks that it is in -128..127, and stores `offset & 0xFF`.
- **Processor:** `if (x[dest] == 0) PC[p] = instr_pc + 4 * (signed char)src2;`, with `!= 0`
  for BNZ. `instr_pc` is the address of the branch itself, so the offset is relative to
  the branch, as in Lab 2.
- **Flags:** neither read nor changed. A loop needs no compare before the branch, and the
  branch does not depend on whichever add or sub happened last. MIPS and RISC-V `beqz`
  and ARM `CBZ`/`CBNZ` work the same way.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_BZ = 0x69`, `OP_BNZ = 0x6A` |
| `compiler.cpp` | `encode_statement` | `BZ`/`BNZ xR .label` → `set(ins, OP_BZ or OP_BNZ, R, 0, offset & 0xFF)` |
| `compiler.cpp` | `disassemble` | `BZ x%d %+d`, `BNZ x%d %+d` |
| `processor.cpp` | `execute` | if `x[dest]` is 0 (or not 0): `PC[p] = instr_pc + 4 * (signed char)src2` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 20 lines by hand.

## Test
`./test.sh`: runs `bz_count.txt` and checks:
- it counts the zeros in 7 0 3 0 9 (2 of them) with `BNZ x4` placed right after an
  address `add` that leaves Z = 0;
- `BNZ x1 .next` closes the loop with a backward offset (-5);
- `BZ x5 .skip` jumps although Z = 0, so the store after it is skipped;
- the listing shows `6A 04 00 02`, `6A 01 00 FB` and `69 05 00 02`;
- `sum_array` still works.

## Viva
- **Why is the register in the dest byte?** The offset stays in operand 2, where `B<cond>` keeps it, so the jump arithmetic is unchanged. A branch writes no register, so its dest byte is free; the Lab 2 branches leave it 0.
- **What does BZ save?** A compare instruction and a scratch register for every test. It also cannot be broken by an add or sub placed between the compare and the branch.
- **How do flag-less machines branch?** RISC-V and MIPS have no flags at all: every branch compares registers (`beq x1, x2, label`, `beqz x1, label`). BZ and BNZ are a step towards that design.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `bz_count.txt`, `bz_count_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

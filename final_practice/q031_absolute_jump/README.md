# Q031 — Absolute jump: `JMP .label`

## Question
Add an unconditional jump `JMP .label` whose operand is the absolute instruction index of
the label, not an offset like `B<cond>` uses. Compare the two kinds of jump: which
targets can each one reach, and what happens if the code is moved? Show a jump that
`BAL` cannot make.

## Answer
The new opcode is `OP_JMP = 0x6B`, with the format `6B 0 0 index`. Pass 1 already gives
every label's instruction index, so the compiler stores that index itself. The processor
sets `PC[p] = 4 * src2` (byte address = index × 4). The flags are neither read nor
changed. One byte is enough for the index because a task has at most 256 instructions
(`INSTR_SPACE` 1024 / 4). So `JMP` reaches every instruction, including the `halt` that
the compiler adds at the end (a label on the last line points to it).

| | `B<cond> .label` (Lab 2) | `JMP .label` |
|---|---|---|
| Operand | offset = label − this instruction (signed byte) | the label's index (unsigned byte) |
| New PC | `instr_pc + 4 * offset` | `4 * index` |
| Range | 128 back to 127 forward | all 256 instructions |
| Code moved to another address | still correct (position independent) | wrong until every JMP is fixed (relocation) |
| Conditional | yes, 15 conditions | no |

In this simulator, paging gives every task its own logical instruction space starting
at 0, so absolute indices are always right. With a load address other than 0, or with
shared code, relative branches are the safer choice.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_JMP = 0x6B` (and a comma after `OP_VMULX`) |
| `compiler.cpp` | `encode_statement` | `JMP .label` → `set(ins, OP_JMP, 0, 0, labels[label])` |
| `compiler.cpp` | `disassemble` | `JMP %d` |
| `processor.cpp` | `execute` | `case OP_JMP: PC[p] = 4 * src2;` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 11 lines by hand.

## Test
`./test.sh`: first runs `jmp_loop.txt`:
- a loop closed by `JMP .loop` (`6B 00 00 02`) computes the sum 55;
- `JMP .end` (`6B 00 00 09`) skips a store.

Then it generates `far.txt`, which has `JMP .far` over 200 instructions:
- the jump encodes as `6B 00 00 CA` (index 202), and only 4 instructions run;
- the same program with `BAL .far` fails to compile, because the offset 201 is more
  than 127.

Finally it checks that `sum_array` still works.

## Viva
- **Why must code with JMP be relocated when it is moved?** The operand names a fixed position. A relative offset depends only on the distance, which stays the same when the whole program moves. This is why shared libraries use PC-relative (position-independent) code.
- **Why is the PC in bytes but the operand in instructions?** Instructions are 4 bytes and always aligned, so storing index = address / 4 lets one byte reach all 1024 bytes. Branch offsets also count instructions for the same reason.
- **How would you reach more than 256 instructions?** Use the two free bytes (dest and operand 1) as well, for a 24-bit index, or add a register-indirect jump `JMP xR` (PC = xR), like `jr`/`jalr` in MIPS and RISC-V.
- **How do you make a far conditional jump?** Invert the condition and jump over a JMP: `BNE .skip`, then `JMP .far`, then `.skip`.

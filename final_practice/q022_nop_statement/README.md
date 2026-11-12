# Q022 — A `nop` statement

## Question
Add a statement `nop` (no operation). It takes one instruction slot and changes nothing:
no register, no memory and no flag. Decide whether it needs a new opcode or can reuse
an existing encoding, and justify the choice. Show that a `nop` placed between a
compare and its branch does not disturb the branch.

## Answer
**Decision: reuse an existing encoding.** `nop` compiles to `07 00 00 00`, which is
`MOV x0, x0` (`x0 = x0`).
- It writes x0 with x0's own value, so no register changes, and `MOV` never sets the
  flags. The processor needs no change. Byte code that contains `nop` even runs on
  an unmodified lab6 simulator.
- `00 00 00 00` cannot be used, because opcode 0 is `halt`.
- `x0 = x0 + 0` (`09 00 00 00`) is **not** a nop: add sets Z, N, C and V, and that
  would break a `BNE` placed after it.
- Real ISAs do the same thing. RISC-V's `nop` is `addi x0, x0, 0` (x0 is always 0 and
  RISC-V has no flags), and ARM's `nop` used to be `mov r0, r0`.

The compiler change is one case in `encode_statement` (like `halt` in Q000). One line in
`disassemble` lists `07 00 00 00` as `nop` instead of `x0 = x0`. The alternative, a new
`OP_NOP` with `case OP_NOP: break;` in `execute()`, is also correct, but it costs an
opcode and a processor change.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `encode_statement` | `nop` → `set(ins, OP_MOV, 0, 0, 0)` |
| `compiler.cpp` | `disassemble` | `07 00 00 00` is listed as `nop` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 5 lines by hand.

## Test
`./test.sh`: runs `nop_loop.txt`, which sums 1..10 = 55 with three nops in the loop, two
of them between the compare and `BNE`; x0 keeps the value 7. The test also checks:
- 77 instructions ran, so each nop takes a slot;
- every nop line has the count 10 in the `.freq` report;
- the listing shows `07 00 00 00   nop`;
- `sum_array` still works.

## Viva
- **What is a nop for?** Filling a slot without any effect: a branch delay slot (MIPS), aligning or padding code, timing loops, and patching an instruction out of a binary without moving the others, so every branch offset stays valid.
- **Does a nop cost time here?** Yes. It is fetched and executed, it counts in the instruction total and in the `.freq` report, and it uses one of the task's 10 instructions per time slice.
- **Why x0?** Any `xR = xR` would do. Fixing it to x0 gives one known encoding, `07 00 00 00`, which the disassembler can recognise and print as `nop`.

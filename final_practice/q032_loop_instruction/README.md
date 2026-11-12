# Q032 — A `loop x1 .label` instruction

## Question
Add the instruction `loop xR .label`: it subtracts 1 from xR and jumps to `.label` if the
result is not 0 (like x86 `LOOP`). Use it to sum 1..n and to write two nested loops.
How many instructions does it save compared with `xR = xR - 1` followed by `BNE`?

## Answer
The new opcode is `OP_LOOP = 0x6C`, with the format `6C R 0 offset`. The counter goes in
the dest byte, because the instruction writes it. The offset is relative and is stored
exactly like a `B<cond>` offset: `label - index`, in -128..127. The processor does:
```c
case OP_LOOP:
    x[dest] = (int)((uint32_t)x[dest] - 1);
    if (x[dest] != 0) PC[p] = instr_pc + 4 * (signed char)src2;
    break;
```
- **Flags:** unchanged (x86 `LOOP` leaves them alone too), so flags computed in the loop
  body survive the `loop` instruction.
- **Savings:** one instruction per iteration. The test runs 58 instructions, where `sub`
  + `BNE` would need 83 (the loop instructions run 25 times in total).
- **Compiler:** the same steps as a branch (look up the label, compute the offset, check
  its range), plus parsing the register.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_LOOP = 0x6C` (and a comma after `OP_VMULX`) |
| `compiler.cpp` | `encode_statement` | `loop xR .label` → `set(ins, OP_LOOP, R, 0, offset & 0xFF)` |
| `compiler.cpp` | `disassemble` | `loop x%d %+d` |
| `processor.cpp` | `execute` | `case OP_LOOP:` decrement `x[dest]`, then branch if it is not 0 |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 20 lines by hand.

## Test
`./test.sh`: runs `loop_sum.txt` and checks:
- `loop x1 .again` (`6C 01 00 FF`) sums 10 + 9 + ... + 1 = 55, and the counter ends at 0;
- the nested `loop x4 .inner` / `loop x3 .outer` (`6C 03 00 FD`) run the body 12 times;
- 58 instructions run in total;
- `sum_array` still works.

## Viva
- **What if xR is 0 at the start?** It becomes -1, which is not 0, so the loop would run 2^32 times; here the endless-loop guard (`MAX_STEPS`) stops the task. Test for 0 first with `BEQ` (x86 has `JECXZ` for exactly this).
- **Why decrement before testing?** With `xR = n` the body runs exactly n times (n, n-1, ..., 1), and the counter ends at 0.
- **What do nested loops need?** A different counter for each loop (x4 inner, x3 outer), and the inner counter is set again on every outer iteration (`x4 = 4` after `.outer`).

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `loop_sum.txt`, `loop_sum_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

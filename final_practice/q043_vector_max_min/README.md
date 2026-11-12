# Q043 — Vector reduction `x1 = vmax v2`, `x1 = vmin v2`

## Question
Add two horizontal (reduction) instructions: `x1 = vmax v2` puts the largest of the 8 lanes
of v2 into x1, and `x1 = vmin v2` the smallest. Lanes are signed integers. Give the
encodings and update the compiler, the processor and the disassembler. Use them to find the
maximum and the minimum of a 16-element array with two vector loads. What goes wrong if the
comparison is unsigned, or if the search starts from 0?

## Answer
- **Encodings:** `B0 D 0 A` (vmax) and `B1 D 0 A` (vmin). D is an x register and A a v
  register, in the same layout as MOV (`07 D 0 A`): one source, one destination.
- **Compiler:** `vmax` and `vmin` are words, so `x1 = vmax v2` reaches `encode_statement()`
  as four tokens. One check recognises them, requires an x destination and a v source, and
  picks the opcode.
- **Processor:** start with m = lane 0, then compare lanes 1..7, as `int` (signed).
  - **Signed** is needed because −3 is 0xFFFFFFFD, which is the biggest value as unsigned.
    An unsigned vmax of `5 -3 12 0 -20 7 12 1` would return −3 instead of 12.
  - **Starting from lane 0, not from 0,** is needed when every lane is negative. Starting
    from 0 would return 0 for the max of −91 … −70 instead of −70.
- **Flags:** unchanged. To branch on the result, compare it with a SUB first, as
  `maxmin.txt` does to combine the two halves.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_VMAXH = 0xB0`, `OP_VMINH = 0xB1` |
| `compiler.cpp` | `encode_statement` | `xD = vmax vA` / `xD = vmin vA` → `B0`/`B1 D 0 A` |
| `compiler.cpp` | `disassemble` | `x1 = vmax v1`, `x3 = vmin v1` |
| `processor.cpp` | `execute` | `case OP_VMAXH/OP_VMINH`: signed scan of the 8 lanes, starting from lane 0 |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `maxmin.txt` on 16 numbers. It checks:
- max 30 and min −25 of the whole array: one vmax/vmin per half, combined with
  `SUB` + `BGE`/`BLE`;
- 12 and −20 for the first half, which an unsigned compare would get wrong;
- −70 for an all-negative vector, which starting from 0 would get wrong;
- both encodings;
- that `sum_array` still works.

## Viva
- **Vertical or horizontal?** `v3 = v1 + v2` is vertical: lane i only meets lane i. `vmax` is horizontal: it combines the lanes of one register, so the result is a scalar. Horizontal operations are slower in hardware: a tree of 3 compare levels for 8 lanes (log₂ 8).
- **How do you do a long array fast?** Keep the work vertical inside the loop (a lane-wise max of each new block into an accumulator vector) and reduce horizontally only once at the end. A lane-wise `vmax` would need another opcode; this question adds only the reductions.
- **Why put the result in an x register?** Because it is used as a scalar: stored, compared, or used to branch. Branches only look at the flags, which a scalar SUB sets.
- **How would you get the index of the maximum?** You need another instruction (an "argmax" that returns the lane number, 0..7), or a compare of each lane with the maximum.

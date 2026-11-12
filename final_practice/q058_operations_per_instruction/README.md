# Q058 — Effective operations per instruction

## Question
A vector instruction does the work of 8 scalar ones. Count the "effective operations" of
each task: a scalar arithmetic instruction (+ − × ÷, with a register or a constant) is 1
operation, a vector arithmetic instruction is 8 (one per lane); loads, stores, moves,
branches, print and halt are 0. When the task ends, report the operations, the
instructions and the operations per instruction.

## Answer
One counter per processor, `alu_ops[p]`, increased at the top of `execute()` by opcode
range: scalar ALU 0x01–0x04 and 0x09–0x0C add 1; vector ALU 0x21–0x23, 0x29–0x2B and
0x31–0x33 add `VLEN` (8). The ranges are written out exactly because the vector loads
and stores (0x25, 0x26, 0x2C, 0x2E) lie between them. `finalize()` divides by the
instruction count `t.steps`:

```
[ops] pid 3 (scale): 36 operations in 18 instructions = 2.00 per instruction
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern long alu_ops[NP];` |
| `processor.cpp` | (globals), `reset` | define it; set to 0 |
| `processor.cpp` | `execute` | +1 for a scalar ALU opcode, +VLEN for a vector ALU opcode |
| `os.cpp` | `finalize` | print `[ops] ...` with `alu_ops / steps` (0 if no instruction ran) |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 10 lines by hand.

## Test
`./test.sh`: sum_array is scalar only: 10 compares + 9 × 3 additions = 37 operations in
71 instructions (0.52). array_add: 14 scalar + 2 vector adds × 8 = 30 in 31 (0.97).
`scale.txt` computes 3x + 1 for 16 numbers with `v1 = v1 * x3` and `v1 = v1 + 1`:
2 × (8 + 8 + 1 + 1) = 36 operations in 18 instructions (2.00), and the results are right
(4 and 49). sum_array still stores 45.

## Viva
- **How can operations per instruction exceed 1?** Each vector instruction does 8 additions or multiplications at once (SIMD); with enough vector work the ratio approaches 8.
- **Is a vector copy `vD = vA` an operation?** It is encoded as `vD = vA + 0` (VADDI), so this count gives it 8; leave out VADDI with constant 0 if the question says copies are free.
- **Why are moves and loads not operations?** They only move data; the question counts arithmetic done by the ALU.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `scale.txt`, `scale_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

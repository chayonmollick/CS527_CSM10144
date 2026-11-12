# Q042 — Vector division by a constant `vD = vA / c`

## Question
The vector unit has `+ - *` but no division: `v2 = v1 / 3` is a compile error. Add
`vD = vA / c`, which divides each of the 8 lanes by the constant c. The division is signed
(−7 / 2 = −3, rounding toward zero, as in C). Because the divisor is known when compiling,
`v2 = v1 / 0` must be a compile error, not a run-time one. Give the encoding, and say what
happens if a program divides a vector by a register.

## Answer
- **Encoding `AC D A c`:** the vector-constant group is VADDI 29, VSUBI 2A, VMULI 2B, so
  VDIVI would naturally be 2C. That code is VLOADC, so it is 0x2C + 0x80 = 0xAC, with the
  same `op D A c` layout.
- **Compiler:** in `encode_arith()`, the vector branch used to reject `/` outright. Now it
  checks the divisor:
  - a constant 1..255 → `AC D A c`;
  - a constant 0 → compile error "vector division by 0";
  - a vector or x register → compile error "a vector can only be divided by a constant".
    That needs a run-time check for 0 in every lane, which the question does not ask for.
- **Processor:** one loop over the lanes, `vD[i] = vA[i] / c`, on `int`. C++ integer
  division is signed and rounds toward zero, so −100 / 3 = −33 and −1 / 3 = 0. The
  divisor is 1..255, so the one overflowing case (INT_MIN / −1) cannot happen. Each lane
  reads its own value before writing it, so `v1 = v1 / 255` works in place.
- **Defence in depth:** hand-written byte code (`AC 02 01 00`) could still divide by 0, so
  the processor reports a "division by zero" error instead of crashing.
- **Flags:** unchanged. Vector operations never set flags.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_VDIVI = 0xAC`: `vD = vA / c` |
| `compiler.cpp` | `encode_arith` | Vector `/`: a constant → `AC D A c`; 0 or a register → compile error |
| `compiler.cpp` | `disassemble` | `v2 = v1 / 3` |
| `processor.cpp` | `execute` | `case OP_VDIVI`: check the registers, guard against 0, divide lane by lane |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `vdiv.txt` on the lanes 100 −100 7 −7 0 1000 −1 255.
It checks:
- `/ 3` → 33 −33 2 −2 0 333 0 85, all 8 lanes;
- `(v1 + v1) / 2` → v1 again;
- `v1 = v1 / 255` in place → 0 0 0 0 0 3 0 1;
- the encoding `AC 02 01 03`;
- that `v2 = v1 / 0` (`vdiv_bad.txt`) is a compile error;
- that `sum_array` still works.

## Viva
- **Why is −100 / 3 = −33 and not −34?** C and C++ integer division truncates toward zero. Floor division (Python's `//`) would give −34. An unsigned divide would read −100 as 0xFFFFFF9C and give 1431655732.
- **Why a compile error and not a run-time error?** The constant is in the instruction, so the compiler already knows it is 0. Catching it there means the program never starts. A register divisor is only known at run time, so it needs a run-time check.
- **Why do SIMD units rarely have integer divide?** Division is slow (many cycles, and not pipelined like add and mul). Compilers turn division by a constant into a multiply by a "magic" reciprocal plus a shift, which is why x86 SSE/AVX has no packed integer divide.
- **What is `(v1 + v2) / 2` used for?** The lane-wise average, as in image blending. In `vdiv.txt`, averaging v1 with itself must give v1 back.

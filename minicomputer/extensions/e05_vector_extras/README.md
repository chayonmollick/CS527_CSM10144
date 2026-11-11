# e05 — Vector extras: broadcast, horizontal sum, max, print a vector

## Question (as it could appear in the test)
> Extend the vector unit (Lab 3) with four instructions:
> - `vD = xA` copies a scalar into all 8 lanes (broadcast);
> - `xD = sum vA` is the horizontal sum of the 8 lanes;
> - `vD = vA max vB` is the lane-wise maximum;
> - `print vA` prints all 8 lanes to the log.
>
> Give their encodings and update the compiler, the processor and the disassembler.
> Write a dot product of two 16-element arrays using vectors and `sum`, and rewrite the
> Lab 3 FIR filter with broadcast/sum. Compare its instruction count with your Lab 3 version.

## Solution idea
**Encodings.** In the handout, a vector opcode is the scalar opcode + 0x20 (ADD 01 → VADD
21, LOAD 05 → VLOAD 25, STORE 06 → VSTORE 26). Broadcast and print follow that rule; the
others take free opcodes next to `VADDX`..`VMULX` (31..33):

| Statement | Encoding | Meaning |
|---|---|---|
| `vD = xA` | `27 D 0 A` | broadcast: every lane = xA (like `MOV 07 D 0 A`) |
| `print vA` | `28 0 0 A` | log line with lanes 0..7 in hex (like `PRINT 08 0 0 R`) |
| `xD = sum vA` | `38 D 0 A` | xD = vA[0] + … + vA[7] (wraps around like add) |
| `vD = vA max vB` | `39 D A B` | lane-wise **signed** max |
| `vD = vA max c` | `3A D A c` | max with a constant (c = 0 gives ReLU) |
| `vD = vA max xB` | `3B D A B` | max with a scalar register |

**Syntax for max.** The main form is infix, `v3 = v1 max v2`, like `+ - *`, so it goes through
`encode_arith()`. The prefix form `v3 = max v1 v2` (or `max v1, v2`) is accepted as well and
gives the same byte code.

**Compiler.**
- `encode_move()` now accepts an x register as the source of a vector assignment (broadcast). It used to be an error.
- `encode_statement()` recognises `xD = sum vA` and the prefix `max` before the normal `dest = a op b` parsing.
- `print` accepts a v register.

**Processor.** One case each, and `vector_max()` handles the three max forms like `vector_op()` handles + − *:
- `max` compares lanes as signed `int`, so max(−5, 3) = 3. An unsigned compare would pick 0xFFFFFFFB.
- `sum` adds the lanes as `uint32_t`, so the total wraps around like the scalar add, with no undefined signed overflow.
- None of the new instructions touches the flags; they are vector or data-movement operations, and only scalar add/sub set flags.

**FIR with broadcast/sum** (`fir_sum.txt`). The handout's pseudo-code computes each output as a dot product,
`out[k] = Σ in[k+i]·w[i]`. With `sum`, the filter can be written exactly like that:
- Load all 8 weights with **one** vector load (`v9 = [4]`).
- Then, for every k: `v0 = [in + 4k]`, `v1 = v0 * v9`, `out[k] = sum v1`.

| Same data (`fir_data.byte`, N = 24 → 16 outputs) | `fir_lab3.txt` (Lab 3, `examples/fir.txt`) | `fir_sum.txt` (e05) |
|---|---|---|
| Program size | 53 instructions | **17** instructions |
| Weight loads | 8 scalar loads | **1** vector load |
| Outputs per loop pass | 8 (vertical: `v1 += in[k+j..] * wj`) | 1 (horizontal `sum`) |
| Executed instructions | **93** | 169 |
| In general, M = N − 8 outputs | 38·M/8 + 17 ≈ 4.75 per output | 10·M + 9 = 10 per output |
| N must be a multiple of 8 | yes | no (any N ≥ 8) |

**Verdict.**
- The `sum` version is about 3× shorter and reads like the pseudo-code. It is also more general: any N, and the outputs need not come in blocks of 8.
- The Lab 3 vertical version executes about 1.8× fewer instructions. Each of its vector multiply-adds produces 8 outputs at once, while a horizontal reduction collapses 8 lanes into one value for every output.
- Rule of thumb: keep the work vertical inside the loop and reduce horizontally *once* at the end. That is exactly what `dot.txt` does: 8 running partial sums in `v3`, then one `sum`.
- Broadcast does not shorten the FIR, because the base already has `v * xB` (VMULX). Without VMULX, you would broadcast the 8 weights into v10..v17 once before the loop, and broadcast is how such an ISA gets a scalar into every lane. In the examples, broadcast clears accumulators (`v3 = x5` with x5 = 0) and builds a constant vector (`v7 = x3`).

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_VBCAST` 0x27, `OP_VPRINT` 0x28, `OP_VSUM` 0x38, `OP_VMAX` 0x39, `OP_VMAXI` 0x3A, `OP_VMAXX` 0x3B |
| `compiler.cpp` | `encode_move()` | `vD = xA` → VBCAST |
| `compiler.cpp` | `encode_arith()` | Operator `max` (vector, constant or scalar second operand) |
| `compiler.cpp` | `encode_statement()` | `xD = sum vA`; prefix `vD = max vA vB`; `print vA` → VPRINT (errors such as `v40` are reported as such) |
| `compiler.cpp` | `disassemble()` | `v7 = x3`, `x5 = sum v1`, `v2 = v1 max x3`, `print v4`, … |
| `processor.cpp` | `vector_max()` (new) | Lane-wise signed max for the three forms |
| `processor.cpp` | `execute()` | `OP_VBCAST`, `OP_VSUM`, `OP_VMAX*`, `OP_VPRINT` (`Process id: p   vA : l0 l1 … l7`) |
| `Makefile` | `test` | Runs `./test.sh`, then the 33 base checks |

Find every change with `grep -n "EXT e05" *.cpp *.h Makefile`, or read `CHANGES.diff`.

## Run
```bash
cd minicomputer/extensions/e05_vector_extras
make                                    # no warnings
./test.sh                               # 30 checks of this extension
SIM=$PWD/sim ../../tests/run_tests.sh   # the 33 base checks still pass
./sim -c fir_sum.txt
./sim -b dot.txt dot_data.byte fir_sum.txt fir_data.byte fir_lab3.txt fir_data.byte relu.txt relu_data.byte
cat log.txt                             # print v / print x output
../../tools/words.sh dot_data_1.out 132
make clean
```
| Program | Data | Result |
|---|---|---|
| `dot.txt` | N = 16 at 0, A = 1..16 at 4, B = 16..1 at 68 | A·B at 4 + 8N = 132 (816); logs the 8 partial sums |
| `fir_sum.txt` | `fir_data.byte` (= `examples/fir_data.byte`) | 16 outputs from 0x100, identical to `fir_lab3.txt` |
| `fir_lab3.txt` | same | unchanged copy of `examples/fir.txt`, for the comparison |
| `relu.txt` | N = 16 at 0, 3 −2 7 −8 0 12 −5 6 −1 9 −4 5 10 −10 2 −7 | ReLU in place; at 68/72/76: Σmax(a,0) = 54, Σmax(a,5) = 99 (scalar), 99 (broadcast) |

## Expected output
`./test.sh` ends with `30 passed, 0 failed`, and the base tests with `33 passed, 0 failed`.
```
$ ./sim -c fir_sum.txt
fir_sum.txt -> program.byte (17 instructions)
  0: 0D 01 00 00   x1 = [0]           | x1 = [0]          % N
  1: 0A 02 01 08   x2 = x1 - 8        | x2 = x1 - 8       % outputs left
  2: 2C 09 00 04   v9 = [4]           | v9 = [4]          % w0..w7 with ONE vector load (fir_lab3.txt needs 8 scalar loads)
  ...
  8: 25 00 00 03   v0 = [x3]          | v0 = [x3]         % in[k .. k+7]
  9: 23 01 00 09   v1 = v0 * v9       | v1 = v0 * v9      % in[k+j] * w[j], j = 0..7
 10: 38 05 00 01   x5 = sum v1        | x5 = sum v1       % out[k]
 11: 06 04 00 05   [x4] = x5          | [x4] = x5
 ...
 16: 00 00 00 00   halt               | (end of program)

$ ./sim -b dot.txt dot_data.byte fir_sum.txt fir_data.byte fir_lab3.txt fir_data.byte relu.txt relu_data.byte
...
[os] pid 1 (dot) finished on processor 0 after 34 instructions, 0 page fault(s) -> dot_data_1.out, dot_1.freq
[os] pid 4 (relu) finished on processor 3 after 47 instructions, 0 page fault(s) -> relu_data_4.out, relu_4.freq
[os] pid 3 (fir_lab3) finished on processor 2 after 93 instructions, 0 page fault(s) -> fir_data_3.out, fir_lab3_3.freq
[os] pid 2 (fir_sum) finished on processor 1 after 169 instructions, 0 page fault(s) -> fir_data_2.out, fir_sum_2.freq

$ cat log.txt
Process id: 1   v3 : 00000058 00000064 0000006C 00000070 00000070 0000006C 00000064 00000058
Process id: 1   x8 : 00000330
Process id: 4   v4 : 00000003 00000009 00000007 00000005 0000000A 0000000C 00000002 00000006

$ ../../tools/words.sh dot_data_1.out 132
   132 (0x0084): 30 03 00 00  = 816
$ ../../tools/words.sh fir_data_2.out 256;  ../../tools/words.sh fir_data_2.out 316
   256 (0x0100): CC 00 00 00  = 204         <- out[0], same as the Lab 3 FIR
   316 (0x013C): E8 02 00 00  = 744         <- out[15]
$ ../../tools/words.sh relu_data_4.out 68      (then 72 and 76: 99, 99)
    68 (0x0044): 36 00 00 00  = 54
```
In `fir_sum_2.freq`, the loop body shows 16 (one pass per output), and the test runs 17 times.

## Viva questions
1. **Why is `sum` slower than the Lab 3 FIR, although it is "one instruction"?**
   - A horizontal reduction turns 8 lanes into 1 value, so `fir_sum` needs a full pass (10 instructions) per output.
   - The vertical version's 8 multiply-adds per block produce 8 outputs together, about 4.75 instructions per output.
   - In real hardware a horizontal add is also slower than a lane-wise add, because it is a tree of adds across lanes.
2. **When is `sum` the right tool?**
   - Once, at the end of a reduction. `dot.txt` keeps 8 partial sums in `v3` (lane j adds A[i+j]·B[i+j]) and calls `sum` once.
   - Lane-wise, then reduce once: that is how SIMD dot products and matrix products are written.
3. **What is broadcast good for?**
   - It turns a scalar into a vector so it can meet vector operands: a constant vector (`v7 = x3`), clearing an accumulator (`v3 = x5` with x5 = 0), or weights in an ISA without vector-by-scalar instructions.
   - x86 has `vpbroadcastd` and ARM NEON has `dup`.
4. **Is `max` signed or unsigned, and why does it matter?**
   - Signed, because lanes hold `int`s.
   - max(−5, 3) must be 3, but as unsigned numbers −5 = 0xFFFFFFFB is the biggest. `test.sh` checks this with sum(max(−5, 3)) = 24.
   - `v1 = v1 max 0` is ReLU, the standard neural-network activation.
5. **Why does `print v` use one log line?**
   - It keeps the same `Process id: <pid>   <reg> : <hex>` format as `print x`, with the 8 lanes in order.
   - Lines from different tasks still interleave by time slice, but one task's vector is never split across lines.
6. **Do the new instructions set flags?**
   No. As in the handout, only scalar add/sub set flags. To branch on a sum, compare it first: `x9 = x5 - 0`.

# Q035 — Multiply-accumulate `x1 += x2 * x3`

## Question
Add a multiply-accumulate instruction, written `x1 += x2 * x3` (x1 = x1 + x2 * x3), that
is ONE instruction. The destination is also a source: explain how the three registers fit
in the 4-byte instruction format, and give the encoding. Update the compiler, the processor
and the disassembler, and use it for the dot product of two arrays. How many instructions
does it save, and does it change the flags?

## Answer
- **Encoding `83 D A B`:** 0x83 is MUL (0x03) + 0x80. D is the accumulator, A and B are the
  factors. The accumulator is both a source and the destination, so it needs no extra
  field: the processor reads xD, adds xA·xB and writes xD back. A general
  `xD = xA * xB + xC` would need four register fields, one more than the format has.
- **Compiler:** the tokenizer splits `+=` into `+` `=`, so the statement arrives as the six
  tokens `x1 + = x2 * x3`. One check in `encode_statement()` recognises that shape and
  requires three x registers.
- **Processor:** `x[dest] = x[dest] + x[src1] * x[src2]`, computed in `uint32_t` so it wraps
  like ADD and MUL. `x[dest]` is read before it is written, so `x7 += x7 * x7` with x7 = 3
  gives 3 + 9 = 12.
- **Flags:** unchanged. Only ADD and SUB set flags in this machine. MAC is a multiply,
  like ARM's `MLA`, which also leaves the flags alone.
- **Saving:** the dot-product loop needs 7 instructions per element instead of 8
  (`x7 = x5 * x6` + `x4 = x4 + x7`), and no temporary register.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_MAC = 0x83`: `xD = xD + xA * xB` |
| `compiler.cpp` | `encode_statement` | Tokens `xD + = xA * xB` → `83 D A B`; anything other than three x registers is an error |
| `compiler.cpp` | `disassemble` | `x4 += x5 * x6` |
| `processor.cpp` | `execute` | `case OP_MAC`: multiply-add in `uint32_t`, flags unchanged |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `dot_mac.txt` on A = 1 −2 3 4 and B = 5 6 7 8.
It checks:
- the dot product, 46, at address 36;
- `x7 += x7 * x7` = 12 at 40. A BEQ right after the MAC proves that Z (set by
  `x9 = x9 - 5`) survived the MAC;
- 41 executed instructions;
- the listing bytes `83 04 05 06` and `83 07 07 07`;
- that `x1 += x1 * 3` (`mac_bad.txt`) is rejected;
- that `sum_array` still works.

## Viva
- **Why `uint32_t`?** Signed overflow is undefined behaviour in C++. Unsigned arithmetic wraps modulo 2³², and that gives the same 32 bits as two's-complement hardware.
- **Where is MAC used?** Dot products, FIR filters, matrix multiplication and convolution: every inner loop is `sum += a * b`. DSPs have single-cycle MAC units, and CPUs and GPUs have FMA (fused multiply-add).
- **What if MAC set the flags?** The BEQ in `dot_mac.txt` would see Z = 0 from the result 12, x7 would be cleared, and address 40 would hold 0 instead of 12.
- **Could the constant form `x1 += x2 * 5` exist?** Yes, as a second opcode with `c` in the B byte (`8B D A c`), just as ADDI sits next to ADD.

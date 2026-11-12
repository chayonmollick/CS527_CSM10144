# Q036 — Post-increment loads and stores `x1 = [x2]+`

## Question
Array loops spend two instructions per access: `x6 = [x3]` and then `x3 = x3 + 4`. Add
post-increment addressing: `x1 = [x2]+` loads the word at x2 and then adds 4 to x2, and
`[x2]+ = x1` stores x1 at x2 and then adds 4 to x2. Give the encodings, change the
compiler, processor and disassembler, and use both to copy an array and sum it. What
should `x2 = [x2]+` do?

## Answer
- **Encodings:** they copy LOAD and STORE with 0x80 added, so the fields mean the same:
  - `x1 = [x2]+` → `85 D 0 A` (LOAD is `05 D 0 A`);
  - `[x2]+ = x1` → `86 A 0 S` (STORE is `06 A 0 S`).

  The +4 is implied by the opcode, so no field is needed for it.
- **Compiler:** the tokenizer already splits `[`, `]` and `+`, so `x1 = [x2]+` is the six
  tokens `x1 = [ x2 ] +` and `[x2]+ = x1` is `[ x2 ] + = x1`. `encode_statement()` checks
  for these two shapes before the normal assignment.
- **Processor:** access memory first, and increment only if the access worked. A fault
  stops the task anyway, and the register is then left as it was.
  - Load: `value = [xA]`, then `xA += 4`, then `xD = value`.
  - Store: `[xA] = xS`, then `xA += 4`. So `[x4]+ = x4` stores the *old* x4.
- **`x2 = [x2]+`** would write x2 twice (the loaded word and the address + 4). ARM calls
  that UNPREDICTABLE. Here the compiler rejects it. In hand-written byte code the loaded
  value wins, because it is written last.
- **Flags:** unchanged. The +4 is address arithmetic, not an ADD instruction.
- **Saving:** the copy-and-sum loop is 5 instructions per element instead of 7.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_LOADPI = 0x85` (`xD = [xA]+`), `OP_STOREPI = 0x86` (`[xA]+ = xS`) |
| `compiler.cpp` | `encode_statement` | Tokens `xD = [ xA ] +` → `85 D 0 A`; `[ xA ] + = xS` → `86 A 0 S`; `xD = [xD]+` → error |
| `compiler.cpp` | `disassemble` | `x6 = [x3]+`, `[x4]+ = x6` |
| `processor.cpp` | `execute` | `case OP_LOADPI` and `case OP_STOREPI`: the access, then `+= 4` if it worked |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `copy_sum.txt` on N = 5, A = 10 20 30 40 50.
It checks:
- the copy at 64..80;
- the sum 150 at 84, stored with the pointer that kept moving;
- the final source pointer 24 at 88;
- `[x4]+ = x4`, which stores the old 92 at 92 and then leaves x4 = 96;
- 34 executed instructions;
- the encodings `85 06 00 03` and `86 04 00 06`;
- that `x3 = [x3]+` (`postinc_bad.txt`) is rejected;
- that `sum_array` still works.

## Viva
- **Where does this come from?** ARM has `LDR r1, [r2], #4` (post-indexed), and the PDP-11 and 68000 have `(R2)+`. C's `*p++` maps onto it directly. It suits streaming through arrays, and stack pops.
- **Why add 4 and not 1?** Addresses are in bytes, and every load and store moves a 32-bit word. A vector version (`v1 = [x2]+`) would add 32.
- **Why increment only when the access worked?** The instruction then either completes or has no effect on the register, which is what a precise fault needs: after fixing the page the instruction can simply run again.
- **How would you add pre-decrement, `[-x2] = x1` (a push)?** Subtract 4 first, then store. With `x1 = [x2]+` as the pop, that gives a full-descending stack.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `copy_sum.txt`, `copy_sum_data.byte`, `postinc_bad.txt`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

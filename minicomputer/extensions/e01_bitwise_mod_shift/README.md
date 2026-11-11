# e01 — Bitwise, shift and modulus operators

## Question (as it could appear in the test)
> Add the integer operators `&` (and), `|` (or), `^` (xor), `<<` (shift left), `>>`
> (arithmetic shift right) and `mod` (remainder; `%` already starts a comment) to the
> language and the processor. The second operand can be an x register or a constant 0..255:
> `x1 = x2 & x3`, `x1 = x2 << 3`, `x1 = x2 mod 10`. Choose opcodes, update the compiler, the
> processor and the disassembler, and say which flags the new instructions change.
> Write programs that (a) compute gcd(a, b) with Euclid's algorithm, (b) count the 1 bits
> of a number, and (c) count the even and the odd numbers of an array using `& 1`.

## Solution idea
**Byte code.** There are 12 new opcodes in the usual format `<op> D A B` / `<op> D A c`. Each
operator gets an even opcode for the register form; the constant form is the next opcode.
The compiler therefore computes `OP_MOD + 2k` (+1 for a constant), where k is the operator's
position in `{mod, &, |, ^, <<, >>}`.

| Operator | `xD = xA op xB` | `xD = xA op c` |
|---|---|---|
| `mod` | `40 D A B` | `41 D A c` |
| `&` | `42` | `43` |
| `\|` | `44` | `45` |
| `^` | `46` | `47` |
| `<<` | `48` | `49` |
| `>>` | `4A` | `4B` |

**Compiler.**
- `tokenize()` now splits `&`, `|` and `^` the way it splits `+`.
- For `<<` and `>>` it looks one character ahead and emits a single two-character token, so `x1=x2<<3` works without spaces. A lone `<` stays part of a word and gives `unknown operator '<'`.
- `mod` is a word, so it needs spaces around it: `x1 = x2 mod 10`.
- `encode_arith()` checks for the new operators first. Both operands must be x registers; the second can also be a constant.

**Processor.** `execute()` has one case per register/constant pair. Where the result is not obvious:
- **`mod`** is the remainder of the signed division `/`, exactly like C `%`: the result has the sign of xA. So `-100 mod 7 = -2`, because -100 = -14·7 − 2.
  - `x mod 0` stops the task with `modulus by zero`, like division by zero.
  - `INT_MIN mod -1` gives 0. Computing it with `%` would overflow.
- **`>>` is arithmetic:** bit 31 is copied in, so `-100 >> 2 = -25` (division by 4, rounding down). C++17 leaves `>>` of a negative `int` implementation-defined, so `shift_right_arith()` computes `~(~v >> n)` for negative v, which is portable.
- **Shift amount ≥ 32** (or a negative amount in a register): `<<` gives 0 and `>>` gives 0 or −1. C++ leaves shifts by ≥ 32 undefined, so the simulator has to choose a result.

**Flags: unchanged.** The handout says only add and subtract set Z N C V. The new
instructions follow the same rule as `*` and `/`, so putting a bitwise instruction between a
compare and its branch never changes the branch. To branch on a result, compare it first:
`x9 = x2 - 0`, then `BEQ`. (If the examiner wants them to set flags, add
`FlagZ[p] = (r == 0); FlagN[p] = r >> 31;` in each case and leave C and V alone, as ARM's
`ANDS` does.)

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_MOD` 0x40 … `OP_SHRI` 0x4B (register form even, constant form +1) |
| `compiler.cpp` | `tokenize()` | Splits `& \| ^`; `<<` and `>>` become one token (one-character look-ahead) |
| `compiler.cpp` | `BIT_OPS`, `bitwise_operator()` (new) | Operator name → index k (case-insensitive, so `MOD` works) |
| `compiler.cpp` | `encode_arith()` | New operators first: `OP_MOD + 2k` (register) or `OP_MODI + 2k` (constant); x registers only |
| `compiler.cpp` | `disassemble()` | `x1 = x2 mod x3`, `x1 = x2 << 3`, … in listings, traces and `.freq` of `.byte` programs |
| `processor.cpp` | `shift_right_arith()` (new) | Portable arithmetic shift right; amount ≥ 32 → 0 or −1 |
| `processor.cpp` | `execute()` | Cases for mod (with the divide-by-zero check), and, or, xor, shl, shr. Flags untouched |
| `Makefile` | `test` | Runs `./test.sh`, then the 33 base checks |

Find every change with `grep -n "EXT e01" *.cpp *.h Makefile`, or read `CHANGES.diff`.

## Run
```bash
cd minicomputer/extensions/e01_bitwise_mod_shift
make                                    # no warnings
./test.sh                               # 31 checks of this extension
SIM=$PWD/sim ../../tests/run_tests.sh   # the 33 base checks still pass
./sim -c popcount.txt                   # byte-code listing with the new opcodes
./sim -b gcd.txt gcd_data.byte popcount.txt popcount_data.byte \
         evenodd.txt evenodd_data.byte bitops.txt bitops_data.byte
../../tools/words.sh gcd_data_1.out     # every word of a result file
cat popcount_2.freq
make clean                              # remove sim, *.o and the result files
```
| Program | Data | Result |
|---|---|---|
| `gcd.txt` | a = 1071 at 0, b = 462 at 4 | gcd at 8 |
| `popcount.txt` | n = 0x12345678 at 0 | number of 1 bits at 4 |
| `evenodd.txt` | N = 7 at 0, then 3 8 −5 0 12 7 −4 | evens at 4N+4, odds at 4N+8 |
| `bitops.txt` | a = −100 at 0, b = 7 at 4 | every operator in both forms, at 8 … 60; flag check at 64 |

## Expected output
`./test.sh` ends with `31 passed, 0 failed`, and the base tests with `33 passed, 0 failed`.

```
$ ./sim -c popcount.txt
popcount.txt -> program.byte (10 instructions)
  0: 0D 01 00 00   x1 = [0]           | x1 = [0]          % n
  1: 0F 02 00 00   x2 = 0             | x2 = 0            % count
  2: 0F 03 00 20   x3 = 32            | x3 = 32           % bits left to look at
  3: 43 04 01 01   x4 = x1 & 1        | x4 = x1 & 1       % lowest bit
  4: 01 02 02 04   x2 = x2 + x4       | x2 = x2 + x4
  5: 4B 01 01 01   x1 = x1 >> 1       | x1 = x1 >> 1      % next bit
  6: 0A 03 03 01   x3 = x3 - 1        | x3 = x3 - 1       % sets Z when the 32 bits are done
  7: 11 00 00 FC   BNE -4             | BNE .loop
  8: 0E 04 00 02   [4] = x2           | [4] = x2
  9: 00 00 00 00   halt               | (end of program)

$ ./sim -b gcd.txt gcd_data.byte popcount.txt popcount_data.byte evenodd.txt evenodd_data.byte bitops.txt bitops_data.byte
...
[os] pid 1 (gcd) finished on processor 0 after 24 instructions, 0 page fault(s) -> gcd_data_1.out, gcd_1.freq
[os] pid 4 (bitops) finished on processor 3 after 38 instructions, 0 page fault(s) -> bitops_data_4.out, bitops_4.freq
[os] pid 3 (evenodd) finished on processor 2 after 83 instructions, 0 page fault(s) -> evenodd_data_3.out, evenodd_3.freq
[os] pid 2 (popcount) finished on processor 1 after 165 instructions, 0 page fault(s) -> popcount_data_2.out, popcount_2.freq

$ ../../tools/words.sh gcd_data_1.out
     0 (0x0000): 2F 04 00 00  = 1071
     4 (0x0004): CE 01 00 00  = 462
     8 (0x0008): 15 00 00 00  = 21          <- gcd(1071, 462)
$ ../../tools/words.sh popcount_data_2.out 4
     4 (0x0004): 0D 00 00 00  = 13          <- 1 bits in 0x12345678
$ ../../tools/words.sh evenodd_data_3.out 32;  ../../tools/words.sh evenodd_data_3.out 36
    32 (0x0020): 04 00 00 00  = 4           <- evens: 8 0 12 -4
    36 (0x0024): 03 00 00 00  = 3           <- odds: 3 -5 7

$ cat popcount_2.freq
1	x1 = [0]          % n
1	x2 = 0            % count
1	x3 = 32           % bits left to look at
.loop
32	x4 = x1 & 1       % lowest bit
32	x2 = x2 + x4
32	x1 = x1 >> 1      % next bit
32	x3 = x3 - 1       % sets Z when the 32 bits are done
32	BNE .loop
1	[4] = x2
```
`bitops_data_4.out`, from address 8 on: 4, −97, −101, 896, −1, −2, 156, 15, −157, 1879048192,
−25, 2, 0, −1, then 1 at 64 (the `BEQ` after `&` was not taken, so `&` left the flags alone).

## Viva questions
1. **Why `mod` and not `%`?**
   `clean_line()` cuts every line at `%` (comments) before tokenizing, so `x2 = x1 % 3` silently compiles as `x2 = x1`; `test.sh` checks this.
2. **How does the tokenizer read `<<`?**
   When it sees `<` or `>`, it looks at the next character. If that is the same character, it emits the two-character token and skips one position.
3. **Arithmetic vs logical shift right?**
   - Arithmetic copies the sign bit: `-8 >> 1 = -4`, i.e. division by 2 rounding down.
   - Logical shifts in zeros: `0xFFFFFFF8 >>> 1 = 0x7FFFFFFC`. In C, that is `(uint32_t)x >> n`. Adding it as `>>>` would take one more opcode pair.
4. **What is `-7 mod 3`, and why?**
   −1. The division truncates toward zero (−7 / 3 = −2), and `a = (a / b)·b + a mod b` must hold. Mathematical modulo would give 2.
5. **Why test even/odd with `& 1` rather than `mod 2`?**
   `& 1` is 0 or 1 for every sign (two's complement), but `-5 mod 2 = -1`.
6. **Why do the new instructions not set flags, and how would you make them?**
   - The handout lets only add and subtract set flags, and keeping that rule means existing compare-and-branch code never changes meaning.
   - To make them set flags, set `FlagZ = (r == 0)` and `FlagN = r >> 31` in each case and leave C and V alone (ARM `ANDS`).

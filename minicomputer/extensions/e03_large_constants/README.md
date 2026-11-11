# e03 — Constants of any 32-bit size (one line → several instructions)

## Question (as it could appear in the test)
> Constants are limited to 0..255 because an instruction has one byte for them. Allow any
> 32-bit constant, including negative ones, in `xD = <constant>`: `x1 = 100000`, `x2 = -5`,
> `x3 = 0x12345678`. Add one new instruction, "shift left 8 and or" (`xD = (xD << 8) | c`),
> and make the compiler expand a large constant into a MOVC followed by up to three of
> them. Branch offsets must stay correct. The Lab 6 frequency report must still count how
> many times each *source line* executed. Test: compute 100000 + 200000; store −5 and check
> that memory holds FF FF FF FB.

## Solution idea
**New instruction.** `SHLOR xD, c`, encoded `60 D 0 c`, computes `xD = (xD << 8) | c`. It is a
data move, so like MOVC it does not touch the flags.

**Expansion.** The 32-bit value is written as bytes b3 b2 b1 b0. The compiler emits
`MOVC xD, <highest non-zero byte>`, then one `SHLOR` for every lower byte:

| Source | Value | Instructions |
|---|---|---|
| `x1 = 255` | 0x000000FF | `0F 01 00 FF` (unchanged: one MOVC) |
| `x1 = 256` | 0x00000100 | `0F 01 00 01`, `60 01 00 00` |
| `x1 = 100000` | 0x000186A0 | `0F 01 00 01`, `60 01 00 86`, `60 01 00 A0` |
| `x2 = -5` | 0xFFFFFFFB | `0F 02 00 FF`, `60 02 00 FF`, `60 02 00 FF`, `60 02 00 FB` |
| `x3 = 0x12345678` | | `0F 03 00 12`, `60 03 00 34`, `60 03 00 56`, `60 03 00 78` |

- **Negative numbers:** the tokenizer splits `-5` into `-` `5`, so `wide_constant_move()` accepts `xD = - <number>` and takes the two's complement (`0u - 5` = 0xFFFFFFFB).
- **Range:** −2147483648 .. 4294967295. 4294967295 and −1 are the same 32 bits. Anything larger is a compile error.
- **Small constants are untouched:** 0..255 still compiles to the single MOVC the base produced, so all old byte code is identical (the handout's sum_array listing test still passes).
- **Other operands stay 0..255** (`x1 = x2 + 1000` is an error with a hint). Only `xD = <constant>` is expanded.

**Labels and branch offsets.** A label is an *instruction* number, so pass 1 must know how
many instructions each line becomes.
- Pass 1 adds `instructions_for(line)` (1..4) instead of 1.
- Pass 2 calls the same helpers, so both passes always agree.
- `bigloop.txt` checks a backward branch over a 3-instruction line (`BNE -6`) and a forward one over a 4-instruction line (`BNE +6`).

**Source map.** Every instruction of an expanded line gets the same `line_of` entry, so the
listing, the trace and the `.freq` file all point at the right source line.

**Frequency report: count the first instruction of the line.**
- The base `write_frequency_file()` *sums* the counts of all instructions of a line. That was correct while one line was one instruction. Now `x3 = 100000` inside a 10-iteration loop would show **30**.
- The extension adds `exec_count` only for the **first** instruction of each line (`line_of[i-1] != line_of[i]`). The first instruction runs exactly once each time the line starts, so the report shows **10**.
- The expansion is straight-line code and no label points into its middle, so all its instructions run the same number of times anyway. The only exception is a task stopped by an error halfway through a line.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_SHLOR` = 0x60: `xD = (xD << 8) \| c` |
| `processor.cpp` | `execute()` | `case OP_SHLOR` (flags unchanged) |
| `compiler.cpp` | `parse_wide_number()` (new) | Decimal or hex without sign, any length (the range is checked by the caller) |
| `compiler.cpp` | `wide_constant_move()` (new) | Recognises `xD = [-]<number>` outside 0..255; range check; two's complement |
| `compiler.cpp` | `wide_constant_code()` (new) | MOVC + up to 3 SHLOR, skipping leading zero bytes |
| `compiler.cpp` | `instructions_for()` (new) | Size of a line, for pass 1 |
| `compiler.cpp` | `compile_file()` pass 1 | `count += instructions_for(...)` (labels stay right) |
| `compiler.cpp` | `compile_file()` pass 2 | Emits 1..4 instructions per line, all with `line_of = ln` |
| `compiler.cpp` | `parse_simple()` | Error hint: only `xD = <constant>` takes 32-bit values |
| `compiler.cpp` | `disassemble()` | `x1 = (x1 << 8) \| 0x86` |
| `os.cpp` | `write_frequency_file()` | Counts the first instruction of each line, not the sum |
| `Makefile` | `test` | Runs `./test.sh`, then the 33 base checks |

Find every change with `grep -n "EXT e03" *.cpp *.h Makefile`, or read `CHANGES.diff`.

## Run
```bash
cd minicomputer/extensions/e03_large_constants
make                                    # no warnings
./test.sh                               # 30 checks of this extension
SIM=$PWD/sim ../../tests/run_tests.sh   # the 33 base checks still pass
./sim -c bigsum.txt                     # see each line expand
./sim -b bigsum.txt bigsum_data.byte bigloop.txt bigloop_data.byte
../../tools/words.sh bigsum_data_1.out
cat bigloop_2.freq
make clean
```
| Program | Result |
|---|---|
| `bigsum.txt` | 100000 + 200000 at 0, −5 at 4, 0x12345678 at 8, 255 at 12 |
| `bigloop.txt` | 10 × 100000 at 0, −1 at 4, [8] stays 0 (forward branch over a 4-instruction line) |

## Expected output
`./test.sh` ends with `30 passed, 0 failed`, and the base tests with `33 passed, 0 failed`.
```
$ ./sim -c bigsum.txt
bigsum.txt -> program.byte (21 instructions)
  0: 0F 01 00 01   x1 = 1             | x1 = 100000       % 3 instructions
  1: 60 01 00 86   x1 = (x1 << 8) | 0x86 | x1 = 100000       % 3 instructions
  2: 60 01 00 A0   x1 = (x1 << 8) | 0xA0 | x1 = 100000       % 3 instructions
  3: 0F 02 00 03   x2 = 3             | x2 = 200000       % 0x00030D40: 3
  4: 60 02 00 0D   x2 = (x2 << 8) | 0x0D | x2 = 200000       % 0x00030D40: 3
  5: 60 02 00 40   x2 = (x2 << 8) | 0x40 | x2 = 200000       % 0x00030D40: 3
  6: 01 03 01 02   x3 = x1 + x2       | x3 = x1 + x2
  7: 0E 00 00 03   [0] = x3           | [0] = x3          % 300000 = E0 93 04 00
  8: 0F 04 00 FF   x4 = 255           | x4 = -5           % 0xFFFFFFFB: 4
  9: 60 04 00 FF   x4 = (x4 << 8) | 0xFF | x4 = -5           % 0xFFFFFFFB: 4
 10: 60 04 00 FF   x4 = (x4 << 8) | 0xFF | x4 = -5           % 0xFFFFFFFB: 4
 11: 60 04 00 FB   x4 = (x4 << 8) | 0xFB | x4 = -5           % 0xFFFFFFFB: 4
 12: 0E 04 00 04   [4] = x4           | [4] = x4          % FB FF FF FF
 ...
 18: 0F 06 00 FF   x6 = 255           | x6 = 255          % fits in a byte: 1 (MOVC)
 19: 0E 0C 00 06   [12] = x6          | [12] = x6
 20: 00 00 00 00   halt               | (end of program)

$ ./sim -b bigsum.txt bigsum_data.byte bigloop.txt bigloop_data.byte
...
[os] pid 1 (bigsum) finished on processor 0 after 21 instructions, 0 page fault(s) -> bigsum_data_1.out, bigsum_1.freq
[os] pid 2 (bigloop) finished on processor 1 after 81 instructions, 0 page fault(s) -> bigloop_data_2.out, bigloop_2.freq

$ ../../tools/words.sh bigsum_data_1.out
     0 (0x0000): E0 93 04 00  = 300000
     4 (0x0004): FB FF FF FF  = -5          <- the word 0xFFFFFFFB, lowest byte first
     8 (0x0008): 78 56 34 12  = 305419896
    12 (0x000C): FF 00 00 00  = 255

$ cat bigloop_2.freq                   (10, not 30, for the 3-instruction line)
1	x1 = 0            % i
1	x2 = 0            % sum
.loop
10	x3 = 100000       % 3 instructions, run 10 times
10	x2 = x2 + x3
10	x1 = x1 + 1
10	x4 = x1 - 10
10	BNE .loop         % offset -6: back over 3 + 3
1	[0] = x2          % 1000000 = 0x000F4240
1	x9 = x2 - 0
1	BNE .skip         % taken; offset +6 = 1 + 4 + 1
0	x5 = 0x7FFFFFFF   % 4 instructions, skipped
0	[8] = x5          % runs only if the offset is wrong
.skip
1	x5 = -1           % 4 instructions
1	[4] = x5
```
`bigloop_data_2.out` holds 1000000 at 0, −1 at 4 and 0 at 8. A constant that is too large is
rejected at compile time:
```
[os] cannot start task: big.txt:1: constant 4294967296 does not fit in 32 bits (-2147483648 .. 4294967295)
```

## Viva questions
1. **Why not just put the constant in the instruction?**
   - An instruction is 4 bytes: opcode, dest and two operand bytes, so at most 8 or 16 bits of immediate fit.
   - Real RISC processors build 32-bit constants from pieces the same way: MIPS `lui`+`ori`, RISC-V `lui`+`addi`, ARM `movw`+`movt`.
2. **Why must pass 1 know the expansion size?**
   Labels are stored as instruction indexes and branch offsets are differences of indexes. Counting `x1 = 100000` as one instruction would make every later label 2 too small, so the branches would land inside other lines.
3. **Why count only the first instruction of a line in `.freq`?**
   - The report is about *source lines*: "how many times was this statement executed".
   - The first instruction runs once per execution of the line. Summing all three instructions of `x3 = 100000` would report 3× the true count.
4. **How is −5 encoded?**
   - Two's complement: 0x100000000 − 5 = 0xFFFFFFFB, so MOVC 0xFF and three SHLOR (0xFF, 0xFF, 0xFB).
   - In memory, the little-endian `.out` file shows `FB FF FF FF`.
5. **How many instructions for 256? For 65536? For −1?**
   - 256 = 0x100: 2. 65536 = 0x10000: 3. −1 = 0xFFFFFFFF: 4.
   - Leading zero bytes are skipped because `MOVC` already clears the upper 24 bits.
6. **Do the expanded instructions change the flags?**
   No. MOVC and SHLOR are data moves, so a compare followed by a branch still works if a constant load sits between them.

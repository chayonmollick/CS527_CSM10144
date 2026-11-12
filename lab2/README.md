# Lab 2 — Constants, flags, memory addressing, labels and branches

This folder is a complete, standalone solution to the CS527 Lab 2 handout (released 7 August 2026).
It contains Lab 1 plus everything Lab 2 adds. It is the full simulator in [`../lab6/`](../lab6/), cut
back to Lab 2: one processor, no OS, no `print`, no vectors and no paging.

## What the lab asked
Everything from Lab 1 (compiler → byte code → memory → processor → data file, as one executable),
plus the following. The handout marks these changes in blue.
- **32-bit registers.** All registers are 32 bits. Memory is byte addressable, and data is always 32 bits.
- **Constants in arithmetic.** The second operand may be a constant: `x1 = x2 + 10`. A constant is at most 255.
- **Flags `Z N C V`.** Every addition and subtraction updates four global flags:
  - **Z:** the result is 0.
  - **N:** the most significant bit of the result is 1.
  - **C (add):** the unsigned result is smaller than either unsigned input.
  - **C (subtract):** operand 1 is bigger than operand 2.
  - **V (add):** both operands have the same sign, and the result has a different sign.
  - **V (subtract):** the operands have different signs, and the result has operand 2's sign.
- **Square brackets.** `x1 = [x2]` reads memory and `[x2] = x1` writes it. The address may be a
  register or a constant (`x1 = [0]`), at most 255. `Read`/`Write` become legacy instructions, but
  they must still compile.
- **Labels.**
  - A label starts with `.` at the beginning of a line.
  - It is alphanumeric, with no special symbols and no `:`.
  - It is alone on its line.
- **Branches.** `B{suffix} <label>` with the ARM condition suffixes (`BEQ .loop`, `BGE .label1`).
  - The byte code is `<0x10 + code> 0 0 <relative offset>`.
  - The next instruction is the current instruction plus the offset.
- **Comments and `;`.** Comments start with `%`. The example also ends a line with `;`.
- **Byte code (opcodes).**

  | Operation | Second operand a variable | Second operand a constant |
  |---|---|---|
  | add, subtract, multiply, divide | `01`..`04` | `09`..`0C` |
  | memory read | `05` | `0C` (sic, see Decisions) |
  | memory write | `06` | `0E` |
  | data movement | `07` | `0F` |
  | branch | `10` + the condition code | |

  Operand 1 is 0 for memory read, memory write and data movement.
- **Files.** `program.byte` and `data.byte` hold hex bytes, four per line, without `0x`. For example, -1 is `FF FF FF FF`.
- **Memory.** `char Instruction[256]` and `char Data[4096]`.
- **Processor.** As in Lab 1, plus the global flags. It is suggested that the program file is passed through `argc`/`argv`.
- **Examples.** The handout's sum-of-array program, with its byte code listing. The suggested programs are
  the three from Lab 1, plus sum of N numbers with N given at run time, and an FIR filter whose weights
  are constants in the program.

## What is implemented

| Handout item | Where |
|---|---|
| `xD = xA op c` → `09..0C D A c` (and `xD = xA op xB` → `01..04 D A B`) | `compiler.cpp` `encode_arith` |
| `xD = c` → `0F D 0 c`, `xD = xA` → `07 D 0 A` | `compiler.cpp` `encode_move` |
| `xD = [xA]` → `05 D 0 A`, `xD = [c]` → `0D D 0 c` | `compiler.cpp` `parse_operand`, `encode_move` |
| `[xA] = xS` → `06 A 0 S`, `[c] = xS` → `0E c 0 S` | `compiler.cpp` `encode_move` |
| Legacy `Read xD, c` / `Write xS, c` → `0D` / `0E` | `compiler.cpp` `encode_statement` |
| Labels: pass 1 maps every label to the index of the next instruction | `compiler.cpp` `compile` (pass 1) |
| `B{cond} .label` → `1k 00 00 offset`, `B` alone = `BAL` | `compiler.cpp` `branch_condition`, `encode_statement` |
| `%` comments, trailing `;`, blank lines | `compiler.cpp` `clean_line` |
| Constants 0..255, decimal or `0x` hex; registers `x0`..`x255` | `compiler.cpp` `parse_number`, `parse_simple` |
| Compile errors as `file:line: message` | `compiler.cpp` `compile` |
| Global flags N Z C V, set by add/sub only | `processor.cpp` `set_add_flags`, `set_sub_flags` |
| Condition codes EQ..AL | `processor.cpp` `condition_holds` |
| Branch: `PC = this instruction + 4 × offset` | `processor.cpp` `execute` |
| `reset()` (registers, PC, flags), `fetch()`, `decode()` (empty), `execute()` | `processor.cpp` |
| 32-bit little-endian words in `Data[]`; addresses checked | `processor.cpp` `load_word`, `store_word` |
| Run-time errors: divide by 0, bad address, PC outside the program, unknown opcode, endless loop | `processor.cpp` `processor_error` |
| `Instruction[256]`, `Data[4096]`, `initialize()`, `finalize()` | `memory.cpp` (sizes in `config.h`) |
| Hex `.byte` files, 4 bytes per line | `memory.cpp` `read_byte_file`, `write_byte_file` |
| `main(argc, argv)`: compile → initialize → reset → loop → finalize | `main.cpp` `main` |
| `-c`: compile only, print the byte-code listing | `main.cpp` `compile_only` |
| Tests: examples, the handout's listing, every encoding, flags, all 15 conditions, errors | `test.sh` (59 checks) |

## Byte code
Every instruction is `<opcode> <dest> <operand 1> <operand 2>`. All fields are hex bytes; `c` is a constant from 0 to 255.

| Statement | Opcode | Encoding |
|---|---|---|
| `xD = xA + xB` / `-` / `*` / `/` | 01 02 03 04 | `op D A B` |
| `xD = xA + c` / `-` / `*` / `/` | 09 0A 0B 0C | `op D A c` |
| `xD = [xA]` | 05 | `05 D 00 A` |
| `[xA] = xS` | 06 | `06 A 00 S` |
| `xD = xA` | 07 | `07 D 00 A` |
| `xD = [c]`, `Read xD, c` | 0D | `0D D 00 c` |
| `[c] = xS`, `Write xS, c` | 0E | `0E c 00 S` |
| `xD = c` | 0F | `0F D 00 c` |
| `B{cond} .label` | 10 + cond | `1k 00 00 offset` (signed byte, counted in instructions) |
| end of program (added by the compiler) | 00 | `00 00 00 00` |

Opcode `08` is not used; it becomes `print` in Lab 4. Only add and subtract change the flags.
The branch conditions follow the ARM order (`B` alone means `BAL`; `HS` = `CS`, `LO` = `CC`):

| Code | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Suffix | EQ | NE | CS | CC | MI | PL | VS | VC | HI | LS | GE | LT | GT | LE | AL |
| Taken when | Z | !Z | C | !C | N | !N | V | !V | C && !Z | !C \|\| Z | N == V | N != V | !Z && N == V | Z \|\| N != V | always |

## Build and run
```bash
make                      # g++ -std=c++17 -O2 -Wall -Wextra (no warnings)
make test                 # bash test.sh -> "59 passed, 0 failed"

./sim examples/sum_array.txt examples/sum_array_data.byte            # program.byte + sum_array_data.out
./sim examples/sum_n.txt examples/sum_n_data.byte result.byte        # name the output file
./sim program.byte examples/sum_array_data.byte                      # run byte code that is already compiled
./sim -c examples/sum_array.txt                                      # compile only: program.byte + listing
make clean                                                           # remove sim, *.o, program.byte, *.out
```
Without `make`: `g++ -std=c++17 -O2 -Wall -Wextra -o sim main.cpp compiler.cpp processor.cpp memory.cpp`.

The exit code is 0 when the program finishes. It is 1 for a compile error (`file:line: message`), a
bad or missing file, or a run-time error (`error at instruction N: ...`).

## Examples
Every example runs with `./sim examples/<name>.txt examples/<name>_data.byte`, and the result
is written to `<name>_data.out`.

### `sum_array`: the handout's example, unchanged
The data is N = 9 at address 0, followed by 1..9. The byte code is the handout's listing
(`F 1 0 0` … `6 3 0 2`), written as two hex digits, plus the end instruction:
```
$ ./sim -c examples/sum_array.txt
examples/sum_array.txt -> program.byte (13 instructions)
  0: 0F 01 00 00   | x1 = 0       % index
  1: 0F 02 00 00   | x2 = 0        % sum
  2: 0F 03 00 00   | x3 = 0        % array address
  3: 05 04 00 03   | x4 =  [x3]    %  read the size of array
  4: 02 0F 01 04   | x15 = x1 - x4   % compare
  5: 10 00 00 06   | BEQ .exit
  6: 09 03 03 04   | x3 = x3 + 4    % increase the address
  7: 05 06 00 03   | x6 =  [x3]       % load the array data
  8: 01 02 02 06   | x2 = x2 + x6  % update sum
  9: 09 01 01 01   | x1 = x1 + 1    % increment index
 10: 1E 00 00 FA   | BAL .loopback
 11: 06 03 00 02   | [x3] = x2
 12: 00 00 00 00   | (end of program)
$ ./sim examples/sum_array.txt examples/sum_array_data.byte
compiled examples/sum_array.txt -> program.byte (13 instructions)
finished after 71 instructions; data memory written to sum_array_data.out
$ tail -n 2 sum_array_data.out
08 00 00 00
2D 00 00 00
```
The sum 45 (`2D`) is stored at 36 = 4N, where it overwrites the last element (see Decisions).
`BEQ .exit` jumps 6 instructions forward (5 → 11). `BAL .loopback` jumps 6 back (10 → 4); -6 as a byte is `FA`.

### `sum_n`: sum of N numbers, with N given at run time
The data is N at 0 and the numbers at 4..4N. The sum is written at 4N + 4:
```
$ ./sim examples/sum_n.txt examples/sum_n_data.byte
compiled examples/sum_n.txt -> program.byte (13 instructions)
finished after 43 instructions; data memory written to sum_n_data.out
$ cat sum_n_data.out
05 00 00 00
0A 00 00 00
FF FF FF FF
14 00 00 00
1E 00 00 00
28 00 00 00
63 00 00 00
```
The result is 10 - 1 + 20 + 30 + 40 = 99 = `0x63`. The loop exit is `x5 = x2 - x1` followed by `BGE .done`, a signed `i >= N`.

### `fir`: scalar FIR filter, with the weights as constants in the program
The filter is y[n] = 1·x[n] + 2·x[n-1] + 3·x[n-2] + 4·x[n-3]. Inputs before x[0] count as 0, and
there are N outputs. The previous three inputs are kept in registers (`x10`..`x12`, the delay line),
which shift with `xD = xA`.
- **Input:** N at 0, followed by x[0..N-1].
- **Output:** written from `0x100` (see Decisions).

For x = 1..8, the result is y = 1 4 10 20 30 40 50 60:
```
$ ./sim examples/fir.txt examples/fir_data.byte
compiled examples/fir.txt -> program.byte (27 instructions)
finished after 155 instructions; data memory written to fir_data.out
$ tail -n 8 fir_data.out
01 00 00 00
04 00 00 00
0A 00 00 00
14 00 00 00
1E 00 00 00
28 00 00 00
32 00 00 00
3C 00 00 00
```
The test also feeds the filter an impulse (1 0 0 0 0 0). Its output is the weights: 1 2 3 4 0 0.

### `max_array`: maximum of an array of signed numbers
The data is N at 0 (N >= 1) and the elements at 4..4N. The maximum is written at 4N + 4.
`x7 = x6 - x2` followed by `BLE .next` is a signed compare: it uses N and V, so it works for negative numbers too.
```
$ ./sim examples/max_array.txt examples/max_array_data.byte
compiled examples/max_array.txt -> program.byte (15 instructions)
finished after 49 instructions; data memory written to max_array_data.out
$ cat max_array_data.out
06 00 00 00
07 00 00 00
FD FF FF FF
2A 00 00 00
0F 00 00 00
9C FF FF FF
29 00 00 00
2A 00 00 00
```
The elements are 7, -3, 42, 15, -100 and 41, so the maximum is 42 = `0x2A`. The tests also check the
all-negative array -5 -2 -9 (result -2) and a single element.

### Errors
```
$ ./sim nolabel.txt examples/sum_n_data.byte          # line 2: BNE .nowhere
nolabel.txt:2: unknown label .nowhere
    BNE .nowhere
$ ./sim loop.txt examples/sum_n_data.byte             # .forever / B .forever
compiled loop.txt -> program.byte (2 instructions)
error at instruction 0: 10000000 instructions without reaching the end: endless loop?
stopped by an error after 10000000 instructions; data memory written to sum_n_data.out
```

## Decisions
These are the places where the handout is ambiguous or wrong, and what this code does instead.

| Handout | What this code does |
|---|---|
| The opcode table gives `0x0C` both for divide by a constant and for memory read from a constant address | `xD = [c]` (and `Read`) use the free opcode `0x0D`. |
| "Operand 1 is 0" for memory read, memory write and data movement | `05 D 0 A`, `06 A 0 S`, `07 D 0 A`, `0D D 0 c`, `0E c 0 S` and `0F D 0 c`. This matches the example listing (`F 1 0 0`, `5 4 0 3`, `6 3 0 2`). |
| The language section lists only `xD = <constant>` as data movement, but the table has `0x07` for "a variable" | `xD = xA` compiles to `07 D 0 A`. |
| `Read`/`Write` are legacy but must still compile | They compile to the constant-address forms `0D`/`0E`. `05`/`06` now take the address from a register. |
| Sum-of-array example: "result stored at size * 4 + 4", but the program stores at `[x3]` = size * 4 | The program is kept unchanged, because its byte code must equal the listing. The sum lands on the last element. `sum_n.txt` stores at 4N + 4. |
| The branch-suffix table is an image (taken from ARM) | ARM order, EQ = 0 … AL = 14. This is consistent with `BEQ` = `0x10` and `BAL` = `0x1E` in the example. `B` alone = `BAL`, and `BHS`/`BLO` are accepted. |
| C for subtraction: "operand 1 is bigger than operand 2" | Unsigned `a > b`, exactly as written (ARM uses `a >= b`). So after subtracting equal numbers, `BCS` is not taken. `BHI`/`BLS` still give exactly unsigned `>` / `<=`. |
| Labels must start in column 1 | Leading blanks are tolerated, so a valid program is never rejected. Letters, digits and `_` are allowed, and a comment may follow the label. Anything else on the line is a compile error. |
| FIR: "Output is stored at 0x10", which is inside the input (it starts at 4) | Output from `0x100` = 256, which is clear of the input for N <= 63. It is built as 128 * 2, because constants stop at 255. |
| FIR definition is not given | y[n] = Σ w[k]·x[n-k] with weights 1 2 3 4, x[n-k] = 0 before the start, and N outputs. |
| Hex bytes: "four bytes in hex format" | Written as two digits (`0F`). Reading accepts one or two digits, because the handout's own listing writes `F 1 0 0`. |
| A program with an endless loop never stops | Stopped with an error after 10,000,000 instructions (`MAX_STEPS` in `config.h`). |
| Constants | 0..255, written in decimal or as `0x` hex. A larger constant is a compile error. |
| Branch offset is one byte | The offset is signed (-128..127) and counted in instructions. With 64 instructions it always fits, but the compiler checks anyway. |
| Same as Lab 1 | `unsigned char` memories, `while (!end_of_simulation)`, files from the command line, output to `<data>.out`, signed division rounded toward 0 with division by 0 as an error, 63 statements + end. |

## Viva questions
**How is a branch offset computed?**
The offset is the index of the label's instruction minus the index of the branch, counted in
instructions. `execute()` then sets `PC = branch address + 4 × offset`. For example, `BEQ .exit` at
5 jumps to 11, which gives `10 00 00 06`. `BAL .loopback` at 10 jumps to 4, an offset of -6, which
is the byte `FA`.

**Why does the compiler make two passes?**
Because of forward branches: `BEQ .exit` comes before `.exit` is defined. Pass 1 records the
instruction index of every label, and pass 2 encodes the statements.

**What is the difference between C and V?**
C is about unsigned numbers (a carry out of an add; for a subtract, operand 1 > operand 2). V is
signed overflow: the result's sign is impossible for the operands' signs. For example,
0x7FFFFFFF + 1 gives V = 1 and C = 0, while -1 + 1 gives C = 1 (and Z = 1) and V = 0.

**How does `BLT`/`BGE` compare signed numbers?**
After `a - b`, a < b exactly when N != V. N alone is wrong when the subtraction overflows:
0x80000000 - 1 gives 0x7FFFFFFF, so N = 0. But V = 1, so `BLT` is still taken (`test.sh` checks
this case).

**Which instructions change the flags?**
Only add and subtract, in both the register and the constant form. Multiply, divide, moves, loads
and stores leave the flags as they were, so a branch tests the last add or subtract.

**Why is `xD = [c]` `0x0D` and not `0x0C`?**
`0x0C` is already "divide by a constant" in the same table. The two must differ, so memory read
from a constant address takes the unused `0x0D`.

**What does `Read x1, 0` compile to now?**
The same as `x1 = [0]`: `0D 01 00 00`. `Write x7, 16` is `[16] = x7`: `0E 10 00 07`. Opcodes 05
and 06 now take the address from a register (`[xA]`).

**Where does the handout's sum-of-array program store the sum, and how would you fix it?**
When the loop exits, `x3` still holds the address of the last element read (4N), so `[x3] = x2`
overwrites that element. To store at 4N + 4 as the text says, add `x3 = x3 + 4` before the store.
`sum_n.txt` does this.

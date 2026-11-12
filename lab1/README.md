# Lab 1 — Mini-computer simulator: compiler, processor and memory

This folder is a complete, standalone solution to the CS527 Lab 1 handout (released 27 July 2026).
It is the full simulator in [`../lab6/`](../lab6/), cut back to what Lab 1 asks for. That means no
constants in arithmetic, no `[ ]`, no labels, branches or comments, and decimal `.byte` files.

## What the lab asked
- **The system.** Simulate a mini-computer:
  - A **compiler** turns a program written in a small language into **byte code**.
  - The computer system loads the byte code into **memory**.
  - The **processor** executes the instructions one by one and changes the data memory.
  - At the end, the data memory is **written to a file**.
- **One program.** Compilation and simulation are started from one main file and run as one executable.
- **Language:**
  - Arithmetic: `Dest = Operand1 <op> Operand2` with `+ - * /`, where both operands are variables.
  - Memory: `Read <variable> <address>` and `Write <variable> <address>`, where the address is a constant.
  - Data movement: `Dest = <constant>`.
  - Variables are `x0`..`x255`. There are no control operations yet.
- **Byte code.** One line per instruction: `<operation> <dest register> <operand 1> <operand 2>`.
  - Every field is one byte (0..255).
  - Opcodes: add 1, subtract 2, multiply 3, divide 4, memory read 5, memory write 6, data movement 7.
  - Operand 2 is 0 for memory read, memory write and data movement.
  - The byte code is stored in `program.byte`.
- **Processor.**
  - State: `int Register[256]`; `int PC, opcode, dest, src1, src2`; `int end_of_simulation = 0`.
  - `reset()` sets every register and the PC to 0.
  - `fetch()` reads the four bytes at the PC into opcode, dest, src1 and src2.
  - `decode()` is an empty function.
  - `execute()` performs the operation. On opcode 0 it sets `end_of_simulation = 1`.
- **Memory.** A separate file holds `char Instruction[256]` and `char Data[256]` (byte addressable).
  - `initialize()` loads `program.byte` and `data.byte`.
  - `finalize()` writes the data memory back.
- **`main()`:** `compile(); initialise(); reset(); while (...) { fetch(); decode(); execute(); } finalize();`
- **Files:** `main`, `compiler`, `processor` and `memory`, each with its header, plus a `Makefile`.
- **Test programs:**
  - The handout's "add four variables" example.
  - Sum of N numbers, with N fixed in the program.
  - Multiply two complex numbers.
  - Determinant of a 3x3 matrix.
  - The other two suggestions (sum with N given at run time, FIR filter) need branches, which arrive in Lab 2.
- **Data size:** 8 or 32 bits; 32 bits is preferred.

## What is implemented

| Handout item | Where |
|---|---|
| Compiler: source → byte code in `program.byte` | `compiler.cpp` `compile` |
| `xD = xA op xB` → `1..4 D A B`; `xD = c` → `7 D c 0` | `compiler.cpp` `encode_statement` |
| `Read xD, a` → `5 D a 0`; `Write xS, a` → `6 S a 0` (comma optional) | `compiler.cpp` `encode_statement` |
| Registers `x0`..`x255`, constants and addresses 0..255 | `compiler.cpp` `parse_operand` |
| End of program `0 0 0 0` appended | `compiler.cpp` `compile` |
| Compile errors as `file:line: message` (Lab 2 syntax is named as such) | `compiler.cpp` `compile`, `encode_statement` |
| `Instruction[256]`, `Data[256]` | `memory.cpp` (sizes in `config.h`) |
| `initialize()`: `program.byte` → `Instruction[]`, data file → `Data[]` | `memory.cpp` `initialize` |
| `finalize()`: data memory → output file | `memory.cpp` `finalize` |
| `.byte` files: decimal bytes, 4 per line | `memory.cpp` `read_byte_file`, `write_byte_file` |
| `Register[256]`, `PC`, `opcode`, `dest`, `src1`, `src2`, `end_of_simulation` | `processor.cpp` |
| `reset()`, `fetch()`, `decode()` (empty), `execute()` | `processor.cpp` |
| 32-bit words in byte-addressable `Data[]` (little-endian) | `processor.cpp` `load_word`, `store_word` |
| Run-time errors: divide by 0, address outside `Data[]`, unknown opcode | `processor.cpp` `processor_error` |
| `main`: compile → initialize → reset → fetch/decode/execute loop → finalize | `main.cpp` `main` |
| `-c`: compile only, print the byte-code listing | `main.cpp` `compile_only` |
| Opcodes and sizes in one place | `config.h` |
| Tests: every example, the handout's listing, arithmetic, errors | `test.sh` (40 checks) |

## Byte code

| Statement | Operation | Opcode | Encoding |
|---|---|---|---|
| `xD = xA + xB` | Add | 1 | `1 D A B` |
| `xD = xA - xB` | Subtract | 2 | `2 D A B` |
| `xD = xA * xB` | Multiply | 3 | `3 D A B` |
| `xD = xA / xB` | Divide | 4 | `4 D A B` |
| `Read xD, a` | Memory read: xD = word at address a | 5 | `5 D a 0` |
| `Write xS, a` | Memory write: word at address a = xS | 6 | `6 S a 0` |
| `xD = c` | Data movement | 7 | `7 D c 0` |
| (added by the compiler) | End of program | 0 | `0 0 0 0` |

**File format:** every `.byte` file holds decimal bytes, four per line.
- `program.byte` has one instruction per line, for example `5 4 12 0`.
- A data word is 32 bits: four bytes, least significant first (little-endian).

| Value | Bytes |
|---|---|
| 100 | `100 0 0 0` |
| 1000 | `232 3 0 0` (1000 = 3 × 256 + 232) |
| -1 | `255 255 255 255` |
| -146 | `110 255 255 255` |

## Build and run
```bash
make                      # g++ -std=c++17 -O2 -Wall -Wextra (no warnings)
make test                 # bash test.sh -> "40 passed, 0 failed"

./sim examples/sum4.txt examples/sum4_data.byte            # writes program.byte and sum4_data.out
./sim examples/sum4.txt examples/sum4_data.byte out.byte   # name the output file
./sim program.byte examples/sum4_data.byte                 # run byte code that is already compiled
./sim -c examples/sum4.txt                                 # compile only: program.byte + listing
make clean                                                 # remove sim, *.o, program.byte, *.out
```
Without `make`: `g++ -std=c++17 -O2 -Wall -Wextra -o sim main.cpp compiler.cpp processor.cpp memory.cpp`.

The exit code is 0 when the program finishes. It is 1 for a compile error (`file:line: message`), a
bad or missing file, or a run-time error (`error at instruction N: ...`).

## Examples
Every example runs with `./sim examples/<name>.txt examples/<name>_data.byte`, and the result
is written to `<name>_data.out`.

### `sum4`: the handout's "add four variables"
The words at 0, 4, 8 and 12 are added, and the sum is written to 16. The generated byte code is
the handout's listing, plus the end instruction:
```
$ ./sim -c examples/sum4.txt
examples/sum4.txt -> program.byte (9 instructions)
  0:   5   1   0   0   | Read x1, 0
  1:   5   2   4   0   | Read x2, 4
  2:   5   3   8   0   | Read x3, 8
  3:   5   4  12   0   | Read x4, 12
  4:   1   5   1   2   | x5 = x1 + x2
  5:   1   6   3   4   | x6 = x3 + x4
  6:   1   7   5   6   | x7 = x5 + x6
  7:   6   7  16   0   | Write x7, 16
  8:   0   0   0   0   | (end of program)
$ ./sim examples/sum4.txt examples/sum4_data.byte
compiled examples/sum4.txt -> program.byte (9 instructions)
finished after 9 instructions; data memory written to sum4_data.out
$ cat sum4_data.out
10 0 0 0
20 0 0 0
30 0 0 0
40 0 0 0
100 0 0 0
```

### `sum_n_fixed`: sum of N numbers, with N = 6 fixed in the program
Each number takes one `Read` into `x2` and one `x1 = x1 + x2`. The data is 5, 12, -3, 100, 7 and
1000, and the sum goes to 24:
```
$ ./sim examples/sum_n_fixed.txt examples/sum_n_fixed_data.byte
compiled examples/sum_n_fixed.txt -> program.byte (13 instructions)
finished after 13 instructions; data memory written to sum_n_fixed_data.out
$ cat sum_n_fixed_data.out
5 0 0 0
12 0 0 0
253 255 255 255
100 0 0 0
7 0 0 0
232 3 0 0
97 4 0 0
```
The last line is 97 + 4 × 256 = 1121.

### `complex_mul`: (a + bi)(c + di) = (ac - bd) + (ad + bc)i
The inputs are a, b, c, d at 0, 4, 8, 12. The real part goes to 16 and the imaginary part to 20.
Here the product is (1 + 2i)(3 + 4i) = -5 + 10i:
```
$ ./sim examples/complex_mul.txt examples/complex_mul_data.byte
compiled examples/complex_mul.txt -> program.byte (13 instructions)
finished after 13 instructions; data memory written to complex_mul_data.out
$ cat complex_mul_data.out
1 0 0 0
2 0 0 0
3 0 0 0
4 0 0 0
251 255 255 255
10 0 0 0
```

### `det3x3`: determinant of a 3x3 matrix
The program computes a(ei - fh) - b(di - fg) + c(dh - eg). The matrix is stored row by row at
0..32, and the result goes to 36. For [[6,1,1],[4,2,5],[2,8,7]] the determinant is -146:
```
$ ./sim examples/det3x3.txt examples/det3x3_data.byte
compiled examples/det3x3.txt -> program.byte (25 instructions)
finished after 25 instructions; data memory written to det3x3_data.out
$ tail -n 1 det3x3_data.out
110 255 255 255
```

### Errors
```
$ ./sim bad.txt examples/sum4_data.byte          # bad.txt line 2: x2 = x1 + 10
bad.txt:2: both operands must be registers: xD = xA + xB (constants in arithmetic arrive in Lab 2)
    x2 = x1 + 10
$ ./sim div.txt examples/sum4_data.byte          # x1 = 5, then x3 = x1 / x2 with x2 = 0
compiled div.txt -> program.byte (3 instructions)
error at instruction 1: division by zero (x2 is 0)
stopped by an error after 2 instructions; data memory written to sum4_data.out
```

## Decisions
These are the places where the handout is ambiguous or wrong, and what this code does instead.

| Handout | What this code does |
|---|---|
| The example's source reads `x4` from 16 and writes to 20, but its listing has `5 4 12 0` and `6 7 16 0` | `examples/sum4.txt` uses addresses 0, 4, 8, 12 → 16, so its byte code is exactly the listing. |
| Format of `program.byte` | Decimal, one instruction per line, like the example output (hex only arrives in Lab 2). |
| Format of `data.byte` is not given | Decimal bytes, four per line. A word is 32-bit little-endian. |
| "8 or 32 bits, 32 bit preferable" | Registers and data words are 32 bits. `+ - *` wrap around at 32 bits; they are computed as `uint32_t`, so there is no undefined behaviour. |
| `char Instruction[256], Data[256]` | `unsigned char`: with a signed `char`, bytes 128..255 would read as negative numbers. |
| `main()` loops `while(end_of_simulation)` | `while(!end_of_simulation)`. As written, the loop would never run. |
| `main()` takes no arguments and assumes `program.byte` and `data.byte` exist | The program and data files are command-line arguments. The compiler still writes `program.byte`, and `initialize()` reads it back from the file. |
| `finalize()`: "Write data.byte" | Writes `<data file name>.out` by default, so the input stays unchanged and the run can be repeated. Pass `data.byte` as the third argument to overwrite it, as the handout says. |
| Size of the output file | Every byte of the data file, plus any word the program wrote beyond it, rounded up to whole words. |
| "Operand 2 is 0" for read, write and data movement | The address or constant goes in operand 1. For `Write`, the dest field holds the register that is written out (`6 7 16 0`). |
| `Read x1, 0` versus `Read <variable> <address>` | The comma is optional; commas count as blanks. `Read`/`Write` are not case-sensitive; `x` or `X` both work. |
| A word at an address above 252 would run past `Data[256]` | Run-time error. |
| Division | Signed and rounded toward 0. Division by 0 is a run-time error that stops the run; the data memory is still written and the exit code is 1. `INT_MIN / -1` gives `INT_MIN`. |
| Syntax that only arrives in Lab 2: constants in arithmetic, `[ ]`, labels, branches, `%`, `xD = xA` | Compile error that names the feature, for example "constants in arithmetic arrive in Lab 2". |
| `Instruction[256]` = 64 instructions | At most 63 statements, plus the end instruction. More is a compile error. |
| Files named `main.c`, `compiler.c`, ... | C++17 `.cpp` files with the same split, like the rest of the simulator. |

## Viva questions
**Why does the compiler add `0 0 0 0` at the end?**
Opcode 0 is the handout's stop instruction: `execute()` sets `end_of_simulation = 1` when it sees it,
so every program ends explicitly. `fetch()` reports an error if the PC ever runs off
`Instruction[256]`. That can only happen with a hand-written `.byte` file that has no end.

**Why `unsigned char` and not `char` for the memories?**
`char` is signed on x86, so the byte 200 would read as -56. Register numbers, addresses and data
bytes from 128 to 255 would all break. For example, -1 is `255 255 255 255` and must be built from
four unsigned bytes.

**How is a 32-bit number stored in byte-addressable memory?**
As four bytes, least significant first (little-endian). `load_word` builds
`Data[a] | Data[a+1]<<8 | Data[a+2]<<16 | Data[a+3]<<24`. For example, -146 is `0xFFFFFF6E`,
which is stored as `110 255 255 255`.

**What does `decode()` do?**
Nothing. Every instruction has the same four fixed fields, so `fetch()` has already split them into
`opcode`, `dest`, `src1` and `src2`. The handout asks for an empty function.

**What is wrong with the handout's `while(end_of_simulation)`?**
`end_of_simulation` starts at 0, so that loop would never run. The loop has to continue while it is
0, which is `while (!end_of_simulation)`.

**Why is the address in operand 1 for `Read`/`Write`?**
The handout says operand 2 is 0 for memory read, memory write and data movement. Its listing has
`5 4 12 0` for `Read x4, 12` and `6 7 16 0` for `Write x7, 16`. (Lab 2 later moves the address to
operand 2.)

**What happens on overflow, and on division by zero?**
Add, subtract and multiply wrap around at 32 bits. For example, `65025 * 65025` gives -66716671,
and `test.sh` checks this. Division by zero prints `error at instruction N: division by zero`,
stops the simulation and makes the exit code 1.

**How long can a program be?**
`Instruction[256]` holds 256 / 4 = 64 instructions. The last one must be the end, which leaves 63
statements; the compiler rejects a longer program.

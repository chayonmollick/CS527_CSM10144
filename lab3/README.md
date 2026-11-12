# Lab 3 — Vector instructions (the Lab 3 test)

A mini-computer with one processor: a compiler turns a program into byte code (`program.byte`),
memory is loaded from `program.byte` and `data.byte`, the processor runs fetch/decode/execute,
and the final data memory is written back to a file. Lab 3 adds 32 vector registers and vector
instructions to the Lab 2 machine, plus the two test programs (array add and FIR filter).

## What the lab asked
The Lab 3 test (handout pages 17–27; released 13 Aug 2026 at 2:25 pm, due 5:30 pm, 20 points):

- **Everything from Lab 2 still works:**
  - registers x0..x255 of 32 bits; `+ - * /` with a register or a constant (0..255) as the second operand;
  - flags Z N C V, updated by add and subtract;
  - `xD = c` and `xD = xA`; memory with square brackets: `xD = [xA]`, `xD = [c]`, `[xA] = xS`, `[c] = xS`;
  - legacy `Read`/`Write`, which must still compile;
  - labels (`.name` alone on a line) and `B{cond} .label` with an ARM condition suffix;
  - `%` comments; hex `.byte` files with four bytes per line.
- **New: 32 vector registers v0..v31**, each 256 bits wide, used as 8 integers of 32 bits.
  - Vector operators are `+`, `-` and `*`. The first operand is always a vector; the second can
    be a vector, an integer register or a constant: `v3 = v1 + v4`, `v3 = v1 + 15`, `v3 = v1 * x4`.
  - Vector load and store move 8 consecutive words: `v1 = [x2]`, `v1 = [32]`, `[x2] = v1`.
  - Opcodes: add 0x21/0x29, subtract 0x22/0x2A, multiply 0x23/0x2B, read 0x25/0x2C and
    write 0x26/0x2E (second operand a register / a constant).
- **Processor:** `reset()`, `fetch()`, `decode()` (empty) and `execute()`; `Register[256]`, `PC`,
  `opcode, dest, src1, src2`, the flags, `end_of_simulation`; opcode 0 ends the program.
- **Memory** in its own file: `Instruction[256]`, `Data[4096]`; `initialize()` reads `program.byte`
  and `data.byte`; `finalize()` writes the data back.
- **main:** `compile(); initialise(); reset();` then `fetch(); decode(); execute();` until the end,
  then `finalize()`. Files: `main`, `compiler`, `processor`, `memory` (source + header) and a `Makefile`.
- **Two programs, using vectors:**
  - **Array add:** N at 0x0 (a multiple of 8), then A (4N bytes), then B; the result C goes in the next 4N bytes.
  - **FIR filter:** N at 0x0 (a multiple of 8, at most 64), 8 weights in the next 32 bytes, then N
    inputs; the output goes from 0x100. Pseudo code: `for k < N-8: out[k] = sum over i < 8 of in[k+i] * w[i]`.
- **Rubric:**

  | Part | Points |
  |---|---|
  | Compiler changes for vectors | 3 |
  | Processor changes for vectors | 3 |
  | Array add program | 4 |
  | FIR program | 3 |
  | Code quality, including Labs 1–2 | 3 |
  | An executable taking program.byte and data.byte on the command line, built by a Makefile | 1 |
  | Unseen test cases | 3 |

  The marker also looks at whether the two programs use vector instructions optimally, and the viva counts in every part.

## What is implemented
| Feature | File / function |
|---|---|
| Sizes and every opcode in one place | `config.h` (`NUM_VREGS 32`, `VLEN 8`, `INSTR_SIZE 256`, `DATA_SIZE 4096`, `enum Opcode`) |
| Compiler: two passes (labels, then encoding), errors as `file:line: message` | `compiler.cpp` `compile()` |
| Operands `x0..x255`, `v0..v31`, constants 0..255 (decimal or `0x..`), `[xA]`, `[c]` | `compiler.cpp` `parse_simple()`, `parse_operand()` |
| Lab 3: vector arithmetic `vD = vA op vB / c / xB` → 21–23, 29–2B, 31–33 | `compiler.cpp` `encode_arith()` |
| Lab 3: vector read/write `vD = [xA]`, `vD = [c]`, `[xA] = vS`, `[c] = vS` → 25, 2C, 26, 2E | `compiler.cpp` `encode_move()` |
| Lab 2 statements, legacy `Read`/`Write`, `B{cond} .label` (offset in instructions) | `compiler.cpp` `encode_move()`, `encode_arith()`, `encode_statement()`, `branch_condition()` |
| `program.byte` writer, `-c` listing | `compiler.cpp` `write_program_byte()`, `disassemble()`; `main.cpp` `compile_only()` |
| `Instruction[256]`, `Data[4096]`, `initialize()`, `finalize()`, `.byte` reader/writer | `memory.cpp` |
| `Register[256]`, `PC`, `opcode/dest/src1/src2`, flags, `end_of_simulation` | `processor.cpp` |
| `reset()`, `fetch()`, `decode()`, `execute()` | `processor.cpp` |
| Lab 3: `VRegister[32][8]`; lane-wise `+ - *` (wrapping at 32 bits); flags unchanged | `processor.cpp` `vector_op()` |
| Lab 3: vector read/write of 8 words (lane i ↔ address + 4i) | `processor.cpp` `execute()` cases `OP_VLOAD`/`OP_VLOADC`/`OP_VSTORE`/`OP_VSTOREC` |
| Flags Z N C V on add/subtract; branch conditions | `processor.cpp` `set_add_flags()`, `set_sub_flags()`, `condition_holds()` |
| Run-time errors (division by zero, address outside `Data[]`/`Instruction[]`, bad opcode, v32+, endless loop) | `processor.cpp` `processor_error()`, `load_word()`, `store_word()`, `fetch()` |
| `compile(); initialize(); reset(); fetch/decode/execute; finalize();` and the command line | `main.cpp` `main()` |
| Build | `Makefile` (`make`, `make test`, `make clean`) |
| Array add and FIR filter in the test's layouts; every vector instruction once | `examples/array_add.txt`, `examples/fir.txt`, `examples/vector_ops.txt` |
| Checks | `test.sh` (40 checks) |

## Byte code
Every instruction is 4 bytes: `<opcode> <dest> <operand 1> <operand 2>`; `c` is a constant 0..255.
The compiler ends every program with `00 00 00 00`.

| Statement | Opcode | Encoding |
|---|---|---|
| `xD = xA + xB` / `-` / `*` / `/` | 01 02 03 04 | `op D A B` |
| `xD = xA + c` / `-` / `*` / `/` | 09 0A 0B 0C | `op D A c` |
| `xD = [xA]` | 05 | `05 D 0 A` |
| `[xA] = xS` | 06 | `06 A 0 S` |
| `xD = xA` | 07 | `07 D 0 A` |
| `xD = [c]`, `Read xD c` | 0D | `0D D 0 c` |
| `[c] = xS`, `Write xS c` | 0E | `0E c 0 S` |
| `xD = c` | 0F | `0F D 0 c` |
| `B{cond} .label` | 10 + cond | `1k 0 0 offset` (signed, in instructions) |
| `vD = vA + vB` / `-` / `*` | 21 22 23 | `op D A B` |
| `vD = [xA]` | 25 | `25 D 0 A` |
| `[xA] = vS` | 26 | `26 A 0 S` |
| `vD = vA + c` / `-` / `*` | 29 2A 2B | `op D A c` |
| `vD = [c]` | 2C | `2C D 0 c` |
| `[c] = vS` | 2E | `2E c 0 S` |
| `vD = vA + xB` / `-` / `*` | 31 32 33 | `op D A B` (B is an x register) |
| `vD = vA` (copy) | 29 | `29 D A 00`, i.e. `vD = vA + 0` |
| end of program | 00 | `00 00 00 00` |

Branch conditions (`B` alone = `BAL`; `BHS` = `BCS`, `BLO` = `BCC`):

| Code | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Suffix | EQ | NE | CS | CC | MI | PL | VS | VC | HI | LS | GE | LT | GT | LE | AL |
| Taken when | Z | !Z | C | !C | N | !N | V | !V | C && !Z | !C \|\| Z | N == V | N != V | !Z && N == V | Z \|\| N != V | always |

The flags follow the handout exactly; only scalar add and subtract change them:

- **Z:** the result is 0.
- **N:** bit 31 of the result.
- **C:** for add, the unsigned result is smaller than an input; for subtract, operand 1 > operand 2 (unsigned).
- **V:** for add, both operands have the same sign and the result has the other sign; for subtract,
  the operand signs differ and the result has operand 2's sign.

## Build and run
```bash
make                                    # g++ -std=c++17 -O2 -Wall -Wextra: no warnings
make test                               # 40 checks, prints "40 passed, 0 failed"

./sim examples/array_add.txt examples/array_add_data.byte    # compile to program.byte, run, write array_add_data.out
./sim program.byte examples/array_add_data.byte               # run an already compiled program
./sim prog.txt data.byte data.byte      # third argument = output file; this overwrites data.byte, like the handout's finalize()
./sim -c examples/fir.txt               # compile only: write program.byte and print the listing
make clean                              # remove sim, *.o, program.byte, *.out
```
- **Output:** the final data memory goes to `<data>.out` in the current directory, in the same
  format as `data.byte`, unless a third argument names the file.
- **Exit code:** 0 when the program reached its end; 1 after a compile error, a bad file or a run-time error.

## Examples
| Program | Data | What it shows | Result |
|---|---|---|---|
| `sum_array.txt` | `sum_array_data.byte` (N = 9: 1..9) | The handout's Lab 2 program, unchanged; its byte code equals the handout's listing | 45 at address 36 |
| `array_add.txt` | `array_add_data.byte` (N = 16: A = 1..16, B = 100..115) | Array add in the test's layout | C = 101, 103, …, 131 from address 132 |
| `fir.txt` | `fir_data.byte` (N = 24: w = 1..8, in = 1..24) | FIR in the test's layout | out[k] = 36k + 204: 204, 240, …, 744 from 0x100 |
| `fir.txt` | `fir64_data.byte` (N = 64, signed values) | The largest N: the output area overlaps the end of the input | all 56 outputs match the pseudo code |
| `vector_ops.txt` | `vector_ops_data.byte` (1..8) | Every vector opcode once, and that vectors leave the flags alone | see the comments in the program |

Compiling the array add (real output; vector instructions are at 8–11):
```text
$ ./sim -c examples/array_add.txt
examples/array_add.txt -> program.byte (18 instructions)
  0: 0D 01 00 00   x1 = [0]           | x1 = [0]          % N
  1: 0F 02 00 04   x2 = 4             | x2 = 4            % address of A[i]
  2: 0B 03 01 04   x3 = x1 * 4        | x3 = x1 * 4       % 4N = bytes in one array
  3: 01 04 02 03   x4 = x2 + x3       | x4 = x2 + x3      % address of B[i] (A ends here)
  4: 01 05 04 03   x5 = x4 + x3       | x5 = x4 + x3      % address of C[i]
  5: 07 06 00 04   x6 = x4            | x6 = x4           % end of A: stop when the address of A[i] reaches it
  6: 02 07 02 06   x7 = x2 - x6       | x7 = x2 - x6
  7: 1A 00 00 0A   BGE +10            | BGE .done         % N = 0: nothing to add
  8: 25 01 00 02   v1 = [x2]          | v1 = [x2]         % A[i .. i+7]
  9: 25 02 00 04   v2 = [x4]          | v2 = [x4]         % B[i .. i+7]
 10: 21 03 01 02   v3 = v1 + v2       | v3 = v1 + v2      % 8 additions in one instruction
 11: 26 05 00 03   [x5] = v3          | [x5] = v3         % C[i .. i+7]
 12: 09 02 02 20   x2 = x2 + 32       | x2 = x2 + 32      % next 8 elements (8 words = 32 bytes)
 13: 09 04 04 20   x4 = x4 + 32       | x4 = x4 + 32
 14: 09 05 05 20   x5 = x5 + 32       | x5 = x5 + 32
 15: 02 07 02 06   x7 = x2 - x6       | x7 = x2 - x6
 16: 1B 00 00 F8   BLT -8             | BLT .loop         % more elements left
 17: 00 00 00 00   halt               | (end of program)
```

Running the programs. Line `n` of a `.out` file is the word at byte address `4(n-1)`:
```text
$ ./sim examples/array_add.txt examples/array_add_data.byte
compiled examples/array_add.txt -> program.byte (18 instructions)
finished after 27 instructions; data memory written to array_add_data.out
$ sed -n '34,36p;49p' array_add_data.out          # C[0], C[1], C[2], C[15] (addresses 132, 136, 140, 192)
65 00 00 00
67 00 00 00
69 00 00 00
83 00 00 00
$ ./sim examples/fir.txt examples/fir_data.byte
compiled examples/fir.txt -> program.byte (52 instructions)
finished after 88 instructions; data memory written to fir_data.out
$ sed -n '65,67p;80p' fir_data.out                # out[0], out[1], out[2], out[15] (0x100 = line 65)
CC 00 00 00
F0 00 00 00
14 01 00 00
E8 02 00 00
$ ./sim examples/fir.txt examples/fir64_data.byte
compiled examples/fir.txt -> program.byte (52 instructions)
finished after 268 instructions; data memory written to fir64_data.out
$ ./sim examples/sum_array.txt examples/sum_array_data.byte
compiled examples/sum_array.txt -> program.byte (13 instructions)
finished after 71 instructions; data memory written to sum_array_data.out
$ sed -n 10p sum_array_data.out                    # address 36
2D 00 00 00
```
In decimal, C[0..2] are 101, 103, 105 and C[15] is 131. The FIR outputs are 204, 240, 276 and 744, and the sum is 45.

**How well the vectors are used:**
- **Array add:** 4 vector instructions and 5 scalar ones per 8 elements; there is no base + offset
  addressing, so the three addresses must each be stepped. N = 16 takes 27 instructions; the same
  loop written with scalar loads and adds takes 153.
- **FIR:** each lane computes one output. A block of 8 outputs takes 8 vector reads of the sliding
  window `in[k+i .. k+i+7]`, 8 multiplies by a weight held in an x register (`v2 = v0 * x11`),
  7 vector adds and 1 vector write. That is 36 instructions per 8 outputs; each output needs
  8 multiplies and 7 adds.

`make test` builds with `make -B`, checks that there are no compiler warnings, and runs 40 checks in a
temporary directory:
- the handout's sum program and its byte code;
- `program.byte` given directly;
- the byte code of every vector statement;
- the result of every vector opcode;
- array add for N = 16 and N = 40 (with negative numbers), and its instruction count;
- FIR for N = 8, 16, 24 and 64, every output compared with the pseudo code computed in awk from the data file;
- compile and run-time errors.

## Decisions
| The handout says / leaves open | What this code does |
|---|---|
| The opcode table gives 0x0C to both divide-by-constant and read-from-constant-address | `xD = [c]` uses the unused 0x0D |
| A vector's second operand "can be integer" (`v3 = v1 * x4`) but has no opcode | `vD = vA + xB` / `-` / `*` use 0x31–0x33 (0x21–0x23 plus 0x10), so the byte `B` is read as a register number, not a vector or a constant |
| The branch-condition table is an image | ARM order (EQ = 0 … AL = 14), which gives the handout's `BEQ` = 0x10 and `BAL` = 0x1E |
| Data size "32 bit is preferable" | 32-bit words, little-endian in `.byte` files (`0A 00 00 00` = 10, `FF FF FF FF` = −1); any byte address can be used |
| `char Instruction[256]` | 256 bytes = 64 instructions; the compiler adds the end-of-program instruction, so a program has at most 63 statements (the FIR needs 51) |
| Do vector operations change Z N C V? | No. The flags describe one scalar result; a vector has 8 |
| Vector overflow | Each lane wraps at 32 bits, like scalar arithmetic |
| Vector division | Not in the handout's operator list (`+ - *`); the compiler rejects it |
| Copying a vector | `vD = vA` compiles to `vD = vA + 0` (0x29), so no new opcode |
| The sum example says "result at size*4 + 4", but its program stores at `[x3]` = size*4 | Kept the program as written, so its byte code matches the handout; the result is at 36 for N = 9 |
| The handout's main loops `while(end_of_simulation)` | It loops while the flag is 0, and also stops if `fetch()` finds PC outside `Instruction[]` |
| `finalize()` "writes data.byte" | Writes to `<data>.out` by default so the input survives; giving `data.byte` as the third argument overwrites it |
| How much of `Data[4096]` to write back | The size of the data file, extended to the highest word written (the array-add result and the FIR output lie past the input) |
| FIR loop `k < N-8` | Followed as written: N − 8 outputs (a full FIR of N inputs would give N − 7). With N a multiple of 8 that is exactly N/8 − 1 vector blocks |
| FIR with N = 64 | The outputs from 0x100 overlap in[55..62]. Computing blocks first-to-last overwrites inputs that the last block still needs, so `fir.txt` computes the blocks from last to first, which is correct for every N |
| Labels "start in the first column" | Leading blanks before a label are tolerated; a label is `.` followed by letters, digits or `_` |
| A program that never ends | Stopped with an error after 10,000,000 instructions (`MAX_STEPS` in `config.h`) |

## Viva questions
1. **How are the vector registers stored, and how big are they?**
   `int VRegister[32][8]`: 32 registers of 8 lanes × 32 bits = 256 bits each.
2. **What does `v1 = [x2]` do?**
   It reads 8 consecutive words: lane i gets the word at address `x2 + 4i`. `[x2] = v1` writes the
   lanes back the same way. Both work on whole 32-byte blocks, so a loop steps the address by 32.
3. **Why does `vD = vA * xB` need its own opcode (0x33)?**
   The fourth byte is just a number. The opcode says whether it names a vector register (0x23),
   is a constant (0x2B) or names an x register (0x33); the processor reads operand 2 from a
   different place in each case.
4. **Why is the second operand of a vector operation an x register in the FIR?**
   Each weight multiplies all 8 lanes, so `v2 = v0 * x11` broadcasts the scalar weight. This saves
   building a vector of 8 copies of the weight.
5. **How is the FIR mapped onto vectors?**
   Lane j computes output k + j. For tap i, `in[k+i .. k+i+7]` is one vector read (a window sliding by
   4 bytes), multiplied by w[i] and added to the accumulator. 8 reads, 8 multiplies, 7 adds and 1 write
   give 8 outputs.
6. **Why does `fir.txt` compute the blocks from the last one to the first?**
   With N = 64 the input runs to address 291, but the output starts at 0x100 = 256. Writing out[0..7]
   first would destroy in[55..62], which out[48..55] still need. Going backwards, every input is read
   before anything is written over it.
7. **How is a branch encoded?**
   As `0x10 + condition`, with the offset in instructions relative to the branch itself:
   `PC = branch address + 4 × offset`. In `array_add.txt`, `BLT .loop` at instruction 16 jumps to 8,
   so the offset is −8 = `F8`.
8. **Which instructions change the flags, and how do `BGE`/`BLT` work?**
   Only scalar add and subtract change them. `x7 = x2 - x6` followed by `BLT` is a signed `x2 < x6`
   test: N != V. Vector instructions, loads, stores and multiplies leave the flags alone.

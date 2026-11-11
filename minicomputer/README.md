# CS527 mini-computer simulator (Labs 1–6)

One simulator that implements every mini-computer lab: the compiler for the
course language, the processor, vector instructions, the multiprocessing OS
with shell and loader, paging, and both Lab 6 test tasks (execution-frequency
report, page tables stored in physical memory).

| Folder | Contents |
|---|---|
| [`simulator/`](simulator/) | The source: `main`, `compiler`, `processor`, `memory`, `os` (+ `config.h`), and the Makefile |
| [`examples/`](examples/) | Programs and data files for every lab |
| [`tests/run_tests.sh`](tests/run_tests.sh) | 33 regression checks (`make test`) |
| [`GUIDE.md`](GUIDE.md) | **How to extend the simulator in the test**: recipes, likely questions, viva answers |
| [`extensions/`](extensions/) | Worked solutions to likely test questions, each a tested copy of the simulator plus `CHANGES.diff` |
| [`programs/`](programs/) | Solved programs written in the course language (max, sort, search, matrices, vectors, ...) |

## Build and run
```bash
cd simulator && make                 # g++ -std=c++17 -O2 -Wall -Wextra, no warnings
make test                            # runs ../tests/run_tests.sh: 33 checks

./sim -b ../examples/sum_array.txt ../examples/sum_array_data.byte      # one task, then stop
./sim -b prog1.txt data1.byte prog2.txt data2.byte                      # several tasks at once
./sim prog1.txt data1.byte           # tasks + shell: type "<program> <data>" lines, then "exit"
./sim -c prog.txt                    # compile only: writes program.byte and prints a listing
./sim -b -t prog.txt data.byte       # trace every executed instruction (stderr)
```
A program can be a source file (`.txt`, compiled first; the byte code is also
written to `program.byte`) or an already compiled `.byte` file.

Each finished task writes, in the current directory:

| File | Contents |
|---|---|
| `<data>_<pid>.out` | The task's final data memory, same format as `data.byte` |
| `<program>_<pid>.freq` | Lab 6 task 1: how many times each source line executed |
| `log.txt` | `print` output: `Process id: <pid>   x<r> : <hex>` (append mode) |

The exit code is 0 when every task finished without an error.

## Where each lab lives in the code

| Lab | Feature | Code |
|---|---|---|
| 1 | Compiler, byte code, processor `fetch`/`decode`/`execute`, memory, `main` | `compiler.cpp` `compile_file`; `processor.cpp`; `memory.cpp`; `main.cpp` |
| 1 | Legacy `Read xD c` / `Write xS c` | `compiler.cpp` `encode_statement` |
| 2 | Constant operands, `xD = [xA]`, `[xA] = xS`, comments, `;` | `compiler.cpp` `parse_operand`, `encode_move`, `encode_arith` |
| 2 | Flags Z N C V on add/sub | `processor.cpp` `set_add_flags`, `set_sub_flags` |
| 2 | Labels and `B<cond> .label` (relative offset in instructions) | `compiler.cpp` pass 1 + `branch_condition`; `processor.cpp` `condition_holds` |
| 2 | Hex `.byte` files, `Data[4096]` | `memory.cpp`; `config.h` `DATA_SPACE` |
| 3 | 32 vector registers × 8 lanes; vector `+ - *`, load, store | `processor.cpp` `vector_op`, `OP_VLOAD`/`OP_VSTORE`; `compiler.cpp` |
| 4 | NP processors (all state is `[NP]` arrays), `process_instructions` | `processor.h/.cpp` |
| 4 | OS: ready/waiting queues, PID ↔ processor map, round robin, `print`, log file | `os.cpp` `scheduler`, `loader`, `map_pid_proc_id`; `OP_PRINT` |
| 4 | Non-blocking shell (`select` + `read`), `exit` | `os.cpp` `shell`, `run_command` |
| 5 | Logical vs physical memory, pages/frames, free-frame list, frame 0 reserved | `os.cpp` `getFreePage`, `initialize`, `finalize` |
| 5 | Address translation `getPhysicalAddress(proc, isFetch, address)` | `os.cpp`; called from `processor.cpp` `read_byte`/`store_word` |
| 6 | Task 1: executions per source line → `<program>_<pid>.freq` | `compiler.cpp` `line_of`; `processor.cpp` `exec_count`; `os.cpp` `write_frequency_file` |
| 6 | Task 2: page tables in physical frame 0, `PTBR[NP]` | `os.cpp` `initialize`, `getPhysicalAddress`, `finalize` |

## Instruction set
Every instruction is 4 bytes: `<opcode> <dest> <operand 1> <operand 2>`. `c` is a constant 0..255.

| Statement | Opcode | Encoding |
|---|---|---|
| `xD = xA + xB` / `-` / `*` / `/` | 01 02 03 04 | `op D A B` |
| `xD = xA + c` / `-` / `*` / `/` | 09 0A 0B 0C | `op D A c` |
| `xD = [xA]` | 05 | `05 D 0 A` |
| `[xA] = xS` | 06 | `06 A 0 S` |
| `xD = xA` | 07 | `07 D 0 A` |
| `print xR` | 08 | `08 0 0 R` |
| `xD = [c]`, `Read xD c` | 0D | `0D D 0 c` |
| `[c] = xS`, `Write xS c` | 0E | `0E c 0 S` |
| `xD = c` | 0F | `0F D 0 c` |
| `B<cond> .label` | 10 + cond | `1k 0 0 offset` (signed, in instructions) |
| `vD = vA + vB` / `-` / `*` | 21 22 23 | `op D A B` |
| `vD = [xA]` | 25 | `25 D 0 A` |
| `[xA] = vS` | 26 | `26 A 0 S` |
| `vD = vA + c` / `-` / `*` | 29 2A 2B | `op D A c` |
| `vD = [c]` | 2C | `2C D 0 c` |
| `[c] = vS` | 2E | `2E c 0 S` |
| `vD = vA + xB` / `-` / `*` | 31 32 33 | `op D A B` |
| `vD = vA` (copy) | 29 | `29 D A 0` (adds 0) |
| end of program | 00 | `00 0 0 0` (added by the compiler) |

Branch conditions (`B` alone = `BAL`; `HS` = `CS`, `LO` = `CC`):

| Code | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Suffix | EQ | NE | CS | CC | MI | PL | VS | VC | HI | LS | GE | LT | GT | LE | AL |
| True when | Z | !Z | C | !C | N | !N | V | !V | C && !Z | !C \|\| Z | N == V | N != V | !Z && N == V | Z \|\| N != V | always |

Flags (only add and subtract change them, as in the handout):
- **Z:** the result is 0.
- **N:** bit 31 of the result.
- **C:** for add, the unsigned result is smaller than an input (carry out); for subtract, operand 1 is bigger than operand 2 (unsigned).
- **V:** for add, both operands have the same sign and the result has the other sign; for subtract, the operand signs differ and the result has operand 2's sign.

## Memory and paging (Labs 5–6)
```
logical instruction space: 0 .. 1023   -> pages 0, 1     (page-table index = address / 512)
logical data space:        0 .. 4095   -> pages 2 .. 9   (index = address / 512 + 1024 / 512)
physical memory: 8192 bytes = 16 frames of 512; frame 0 is never given to a task

frame 0 (Lab 6 task 2):  bytes  0..9  page table of processor 0   <- PTBR[0] = 0
                         bytes 10..19 page table of processor 1   <- PTBR[1] = 10
                         bytes 20..29 processor 2, bytes 30..39 processor 3
one byte per entry = frame number, 0 = not mapped

physical address = frame * 512 + address % 512,   frame = memory[PTBR[p] + index]
```
- **Loader:** maps only the pages the program and the data file actually use: a 13-instruction program plus a 40-byte data file needs 2 frames. That leaves room for 4 tasks at once in 15 frames.
- **First touch of an unmapped data page:** it gets a zeroed frame ("page fault", counted per task). An unmapped instruction page or an address outside the logical space stops the task with an error.
- **Task start:** a task starts only when a processor *and* enough frames are free; otherwise it waits.
- **Finalize:** writes the data space back to `<data>_<pid>.out`, frees the task's frames, clears its page table and resets its PTBR.

## Choices where the handout is ambiguous
| Handout | What this code does |
|---|---|
| The opcode table gives `0x0C` for both divide-by-constant and read-from-constant-address | Read from a constant address uses the unused `0x0D` |
| A vector's second operand "can be an integer register" but has no opcode | `0x31`–`0x33` for `vD = vA op xB` |
| The branch-condition table is an image | ARM order, consistent with the examples `BEQ` = `0x10`, `BAL` = `0x1E` |
| Data size | 32-bit words, little-endian in `.byte` files (`0A 00 00 00` = 10), as in the Lab 6 submission |
| Lab 2 example "result at size*4+4" | The example program actually stores at `size*4` (it overwrites the last element). `examples/sum_array.txt` keeps the handout's program unchanged; `sum_n.txt` stores at `4N+4` |
| Lab 6 frequency report: labels "must not be included", but the example output shows them | Labels are printed without a count (matches the example); blank and comment-only lines are left out |
| "If no free page found, print ERROR and exit from simulation" | Prints the ERROR and stops that task; the other tasks keep running |
| Where finalize writes the data | `<data>_<pid>.out`, so several tasks never overwrite each other's data file |

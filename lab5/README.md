# Lab 5 — Logical and physical memory (paging)

The mini-computer simulator of Labs 1–4 (compiler, processor, vectors, multiprocessing OS)
with Lab 5's split between logical and physical memory. This folder is standalone:
`make && make test` builds it and checks it.

## What the lab asked
The Lab 5 handout repeats the whole system and adds paging:

- **The system.** Programs in the course language are compiled to byte code (`program.byte`).
  The computer loads the byte code and a data file (`data.byte`), runs the program and writes
  the data memory back to a file. One executable does all of it.
- **Language (Labs 1–3, unchanged).**
  - Registers: `x0..x255` (32-bit) and `v0..v31` (256-bit, 8 lanes of 32 bits).
  - Arithmetic: `+ - * /`, where the second operand is a register or a constant 0..255.
  - Memory: `[x]` / `[c]` reads and writes, plus the legacy `Read` / `Write`.
  - Control flow: labels and `B<cond> .label`. Add and subtract set the flags Z N C V.
  - Vectors: `+ - *`, and vector load/store of 8 consecutive words.
  - Other: `print x` writes to a log file; `%` starts a comment.
  - Every instruction is 4 bytes, `<opcode> <dest> <op1> <op2>`.
- **Multiprocessing (Lab 4, unchanged).**
  - NP processors; every register and PC is an array indexed by processor.
  - `process_instructions(proc_id, n)` runs one time slice and then sleeps.
  - The OS has three parts:
    - a scheduler: ready and waiting queues, a PID → processor map, round robin with 10
      instructions per turn, then the shell;
    - a non-blocking shell that reads `program data` lines and `exit`;
    - a loader that gives a new task a free processor, or puts it in the waiting queue.
- **New in Lab 5: logical vs physical memory.**
  - Every address the compiler generates and the processor uses is *logical*. Every access
    ends in one *physical* memory, `memory[MEMSIZE]` with `MEMSIZE` = 8192.
  - Logical memory is split into **pages** and physical memory into **frames**, both
    `PAGESIZE` = 512 bytes, set with `#define`.
  - The OS gets a memory-management unit:
    - an array of page tables, `char pageTable[MAX_PROC][NUM_LOGICAL_PAGES]`. A table has
      1024/512 + 4096/512 = 10 entries (instruction space 1024 bytes, data space 4096 bytes);
    - a free-frame list, `char freePages[NUM_PHYSICAL_PAGES]` (16 frames; 0 = free,
      1 = allocated);
    - `getFreePage()`, which returns the first free frame and marks it allocated. With no
      free frame it prints ERROR and exits;
    - **frame 0 is reserved** and is never allocated.
  - `initialize()` loads `program.byte` and `data.byte` frame by frame: it counts the pages
    of each file, takes a frame for every page with `getFreePage()` and writes the frame
    number into the page table.
  - `finalize()` writes the data back, resets the page table and frees the frames.
  - **The processor translates first.** `fetch()` and the loads/stores in `execute()` call
    `getPhysicalAddress(proc_id, isFetch, address)`:
    `index = isFetch ? address/PAGESIZE : address/PAGESIZE + 1024/PAGESIZE`,
    `physical = pageTable[..][index] * PAGESIZE + address % PAGESIZE`.
- **Files and programs.** `main`, `compiler`, `processor`, `memory`, `os` (`.c` + `.h`) and
  a Makefile. The example programs are vector array addition and an FIR filter. Data is
  32-bit.

## What is implemented
| Feature | File | Function / data |
|---|---|---|
| Sizes: `NP`, `MEMSIZE`, `PAGESIZE`, logical spaces, page counts | `config.h` | `INSTR_SPACE`, `DATA_SPACE`, `NUM_INSTR_PAGES` (2), `NUM_DATA_PAGES` (8), `NUM_LOGICAL_PAGES` (10), `NUM_PHYSICAL_PAGES` (16) |
| Physical memory | `memory.h/.cpp` | `memory[MEMSIZE]` |
| Page tables, one per processor | `os.cpp` | `char pageTable[NP][NUM_LOGICAL_PAGES]` (entry = frame, 0 = not mapped) |
| Free-frame list, frame 0 reserved | `os.cpp` | `char freePages[NUM_PHYSICAL_PAGES]`; `os_init()` sets `freePages[0] = 1` |
| First free frame | `os.cpp` | `getFreePage()` |
| Logical → physical translation | `os.cpp` | `getPhysicalAddress(proc_id, isFetch, address)` |
| Data pages that `data.byte` did not reach: mapped on first touch, counted as page faults | `os.cpp` | `getPhysicalAddress()` |
| Load program and data page by page, write the page table | `os.cpp` | `initialize()`, `load_pages()` |
| Write data back, free the frames, reset the page table | `os.cpp` | `finalize()` |
| Start a task only when a processor **and** enough frames are free; otherwise it waits | `os.cpp` | `loader()`, `admit_waiting()`, `create_task()` |
| Processor uses logical addresses (`isFetch` = 1 for fetch, 0 for loads/stores) | `processor.cpp` | `fetch()`, `read_byte()`, `load_word()`, `store_word()` |
| reset / fetch / decode / execute, time slice + sleep | `processor.cpp` | `reset()`, `fetch()`, `decode()`, `execute()`, `process_instructions()` |
| Scheduler: ready/waiting queues, PID → processor, round robin | `os.cpp` | `scheduler()`, `map_pid_proc_id()`, `finish_task()` |
| Non-blocking shell (`select` + `read`), `exit` | `os.cpp` | `shell()`, `run_command()` |
| Compiler (Labs 1–3 language + `print`), two passes | `compiler.cpp` | `compile_file()`, `encode_statement()`, `disassemble()` |
| `.byte` files (hex, little-endian words) | `memory.cpp` | `read_byte_file()`, `write_byte_file()` |
| Tasks from the command line; `-b` batch, `-t` trace, `-c` compile only | `main.cpp` | `main()` |
| Tests | `test.sh` | 59 checks (`make test`) |

## Memory layout
```
logical instruction space (each task): bytes 0 .. 1023 = instruction pages I0, I1 -> page-table entries 0, 1
logical data space (each task):        bytes 0 .. 4095 = data pages D0 .. D7      -> page-table entries 2 .. 9
physical memory: memory[8192] = 16 frames of 512 bytes; frame 0 is reserved, frames 1..15 are for tasks

index    = isFetch ? address / PAGESIZE : address / PAGESIZE + 1024 / PAGESIZE
frame    = pageTable[proc_id][index]                   (0 = page not mapped)
physical = frame * PAGESIZE + address % PAGESIZE
```
The OS prints a page table as `[I0 I1 | D0 D1 D2 D3 D4 D5 D6 D7]`, with `-` for a page that
is not mapped. Take `pages.txt`, run alone. It has 15 instructions (60 bytes, 1 page) and a
4-byte data file (1 page). The loader maps 2 pages; the program then touches data pages 1,
3 and 7. When it finishes, its page table is:
```
entry:  0   1  |  2   3   4   5   6   7   8   9
page:   I0  I1 |  D0  D1  D2  D3  D4  D5  D6  D7
frame:  1   -  |  2   3   -   4   -   -   -   5          printed as [1 - | 2 3 - 4 - - - 5]

physical memory: frame 0 reserved | 1 = I0 | 2 = D0 | 3 = D1 | 4 = D3 | 5 = D7 | 6..15 free
```
**Worked translation.** Instruction 7 of `pages.txt` is `[x5] = x1`, with x5 = 3825.
- **Fetch:** PC = 28, `isFetch` = 1. index = 28 / 512 = 0, and entry 0 = frame 1.
  physical = 1 × 512 + 28 = **540**. Bytes 540..543 hold `06 05 00 01`.
- **Store:** address 3825, `isFetch` = 0. index = 3825 / 512 + 1024 / 512 = 7 + 2 = 9, and
  entry 9 = frame 5. physical = 5 × 512 + 3825 % 512 = 2560 + 241 = **2801**. The word goes
  to bytes 2801..2804, all inside frame 5 (2560..3071).
- **Page fault:** before this store, entry 9 was 0. The first touch took frame 5, the first
  free frame, and zeroed it.

Translation is done **byte by byte**, so a word may cross a page boundary. In
`cross_page.txt` the word at 510..513 has bytes 510–511 in D0's frame and 512–513 in D1's
frame. Started together with `pages.txt`, which finishes first and frees frame 1, its D0 is
frame 4 and its D1 is frame 1, so the word's bytes are at physical 2558, 2559, 512 and 513.

Sizes: a page table is 10 one-byte entries, so the 4 tables take 40 bytes. `freePages` is
16 bytes.

## Byte code
Lab 5 did not change the instruction set; it is the same as Lab 4. `c` is a constant 0..255.

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

Two places where the encoding departs from the handout, both carried over from earlier labs:
- Read from a constant address is `0x0D`, because the handout gives `0x0C`, which is already
  divide-by-constant.
- `vD = vA op xB` uses `0x31`–`0x33`: the handout allows a scalar second operand but gives
  it no opcode.

## Build and run
```bash
make                     # g++ -std=c++17 -O2 -Wall -Wextra, no warnings -> ./sim
make test                # ./test.sh: 59 checks, ends with "59 passed, 0 failed"
make clean               # removes sim, *.o, program.byte, *.out, log.txt

./sim -b examples/pages.txt examples/pages_data.byte          # one task, then stop
./sim -b prog1.txt data1.byte prog2.txt data2.byte ...        # several tasks at once
./sim prog1.txt data1.byte      # tasks + shell: type "<program> <data>" lines, then "exit"
./sim -c prog.txt               # compile only: program.byte + listing with logical addresses
./sim -b -t prog.txt data.byte  # trace every executed instruction (stderr)
SIM=/path/to/other/sim ./test.sh  # run these checks against another build (e.g. a modified copy)
```
- **Programs:** a program can be a source file (`.txt`; its byte code is also written to
  `program.byte`) or an already compiled `.byte` file.
- **Output files** go to the current directory:
  - `<data>_<pid>.out`: the task's final data memory, in the same format as `data.byte`.
    Line k is the word at address 4k, least significant byte first.
  - `log.txt`: the `print` output, appended.
- **Exit code:** 0 when every task finished without an error.

## Examples
| Files | Shows | Result |
|---|---|---|
| `pages.txt` + `pages_data.byte` | Data pages beyond the data file are mapped on first touch | 3 page faults; [0] = 21, [3825] = 7 |
| `touch_all.txt` + `touch_all_data.byte` | All 8 data pages. One copy fits; two copies run out of frames | Alone: 7 page faults, [0] = 28. Two copies: pid 2 gets the ERROR, pid 1 still gives 28 |
| `cross_page.txt` + `cross_page_data.byte` | A word that crosses a page boundary | [0] = [510] = 0x01020304 = 16909060 |
| `same_addr.txt` + `same_addr_a.byte` / `same_addr_b.byte` | Two tasks use logical address 2000 at the same time, in different frames | [4] = 11 and 22 |
| `big_sum.txt` + `big_sum_short.byte` / `big_sum_data.byte` | 5 frames per task: 3 tasks fill memory and a 4th waits for frames | Sums 80200 (N = 400) and 125250 (N = 500) |
| `sum_array.txt` + `sum_array_data.byte` | The handout's Lab 2 program, unchanged; byte code identical to the handout listing | [36] = 45 (the program stores at 4N, over the last element, although the handout's text says 4N + 4) |
| `array_add.txt` + `array_add_data.byte` | Lab 3: vector add | C[0] at 132 = 101, C[15] at 192 = 131 |
| `fir.txt` + `fir_data.byte` | Lab 3: FIR filter with vectors | out[0] at 256 = 204, out[15] at 316 = 744 |
| `counter.txt` + `counter_data.byte` | Lab 4: `print`, round robin (run 5 copies: the 5th waits for a processor) | [0] = 10; log.txt |

**Pages mapped on first touch.** The loader maps 2 pages. The stores to 1020, 2040 and 3825
touch D1, D3 and D7, which get frames 3, 4 and 5.
```
$ ./sim -b examples/pages.txt examples/pages_data.byte
[os] pid 1 created: examples/pages.txt examples/pages_data.byte (15 instructions)
[os] pid 1 (pages) loaded on processor 0: page table [1 - | 2 - - - - - - -], 13 frame(s) free
[os] pid 1 (pages) finished on processor 0 after 15 instructions, 3 page fault(s) -> pages_data_1.out
[os]     page table [1 - | 2 3 - 4 - - - 5] reset: 5 frame(s) freed, 15 free
[os] all tasks finished; 15 of 15 frames free; print output is in log.txt
```

**Same logical address, different frames.** Both tasks write their value at 2000 (D3) and
read it back after the other task has run. D3 is frame 5 for pid 1 and frame 6 for pid 2,
so each gets its own value back (11 and 22).
```
$ ./sim -b examples/same_addr.txt examples/same_addr_a.byte examples/same_addr.txt examples/same_addr_b.byte
[os] pid 1 created: examples/same_addr.txt examples/same_addr_a.byte (11 instructions)
[os] pid 1 (same_addr) loaded on processor 0: page table [1 - | 2 - - - - - - -], 13 frame(s) free
[os] pid 2 created: examples/same_addr.txt examples/same_addr_b.byte (11 instructions)
[os] pid 2 (same_addr) loaded on processor 1: page table [3 - | 4 - - - - - - -], 11 frame(s) free
[os] pid 1 (same_addr) finished on processor 0 after 68 instructions, 1 page fault(s) -> same_addr_a_1.out
[os]     page table [1 - | 2 - - 5 - - - -] reset: 3 frame(s) freed, 12 free
[os] pid 2 (same_addr) finished on processor 1 after 68 instructions, 1 page fault(s) -> same_addr_b_2.out
[os]     page table [3 - | 4 - - 6 - - - -] reset: 3 frame(s) freed, 15 free
[os] all tasks finished; 15 of 15 frames free; print output is in log.txt
```

**Frames run out at load, so a task waits.** Each `big_sum` task needs 5 frames. Pids 1–3
take all 15 frames, so pid 4 waits even though processor 3 is free. Pid 1, the shortest,
finishes first. Pid 4 then starts in its frames 1–5 while pids 2 and 3 are still running.
```
$ ./sim -b examples/big_sum.txt examples/big_sum_short.byte examples/big_sum.txt examples/big_sum_data.byte \
           examples/big_sum.txt examples/big_sum_data.byte examples/big_sum.txt examples/big_sum_data.byte
[os] pid 1 created: examples/big_sum.txt examples/big_sum_short.byte (13 instructions)
[os] pid 1 (big_sum) loaded on processor 0: page table [1 - | 2 3 4 5 - - - -], 10 frame(s) free
[os] pid 2 created: examples/big_sum.txt examples/big_sum_data.byte (13 instructions)
[os] pid 2 (big_sum) loaded on processor 1: page table [6 - | 7 8 9 10 - - - -], 5 frame(s) free
[os] pid 3 created: examples/big_sum.txt examples/big_sum_data.byte (13 instructions)
[os] pid 3 (big_sum) loaded on processor 2: page table [11 - | 12 13 14 15 - - - -], 0 frame(s) free
[os] pid 4 created: examples/big_sum.txt examples/big_sum_data.byte (13 instructions)
[os] pid 4 is waiting: needs 5 frame(s), 0 free
[os] pid 1 (big_sum) finished on processor 0 after 2808 instructions, 0 page fault(s) -> big_sum_short_1.out
[os]     page table [1 - | 2 3 4 5 - - - -] reset: 5 frame(s) freed, 5 free
[os] pid 4 (big_sum) loaded on processor 0: page table [1 - | 2 3 4 5 - - - -], 0 frame(s) free
[os] pid 2 (big_sum) finished on processor 1 after 3508 instructions, 0 page fault(s) -> big_sum_data_2.out
[os]     page table [6 - | 7 8 9 10 - - - -] reset: 5 frame(s) freed, 5 free
[os] pid 3 (big_sum) finished on processor 2 after 3508 instructions, 0 page fault(s) -> big_sum_data_3.out
[os]     page table [11 - | 12 13 14 15 - - - -] reset: 5 frame(s) freed, 10 free
[os] pid 4 (big_sum) finished on processor 0 after 3508 instructions, 0 page fault(s) -> big_sum_data_4.out
[os]     page table [1 - | 2 3 4 5 - - - -] reset: 5 frame(s) freed, 15 free
[os] all tasks finished; 15 of 15 frames free; print output is in log.txt
```

**No frame at a first touch: ERROR.** Two copies of `touch_all` each want 9 frames (18 > 15).
They take frames in turn. When pid 2 touches its data page 6, no frame is left: it gets the
ERROR, is stopped, and its 7 frames are freed. Pid 1 goes on, and its last page reuses
frame 3, which used to be pid 2's code page. The exit code is 1 because a task failed.
```
$ ./sim -b examples/touch_all.txt examples/touch_all_data.byte examples/touch_all.txt examples/touch_all_data.byte
[os] pid 1 created: examples/touch_all.txt examples/touch_all_data.byte (14 instructions)
[os] pid 1 (touch_all) loaded on processor 0: page table [1 - | 2 - - - - - - -], 13 frame(s) free
[os] pid 2 created: examples/touch_all.txt examples/touch_all_data.byte (14 instructions)
[os] pid 2 (touch_all) loaded on processor 1: page table [3 - | 4 - - - - - - -], 11 frame(s) free
[pid 2, processor 1] error at instruction 7: ERROR: no free physical frame for data page 6
[os] pid 2 (touch_all) STOPPED WITH AN ERROR on processor 1 after 53 instructions, 5 page fault(s) -> touch_all_data_2.out
[os]     page table [3 - | 4 6 8 10 12 14 - -] reset: 7 frame(s) freed, 7 free
[os] pid 1 (touch_all) finished on processor 0 after 68 instructions, 7 page fault(s) -> touch_all_data_1.out
[os]     page table [1 - | 2 5 7 9 11 13 15 3] reset: 9 frame(s) freed, 15 free
[os] all tasks finished; 15 of 15 frames free; print output is in log.txt
```

**The earlier labs' programs, sharing memory.** Each needs 2 frames; all four run at once.
```
$ ./sim -b examples/sum_array.txt examples/sum_array_data.byte examples/array_add.txt examples/array_add_data.byte \
           examples/fir.txt examples/fir_data.byte examples/counter.txt examples/counter_data.byte
[os] pid 1 created: examples/sum_array.txt examples/sum_array_data.byte (13 instructions)
[os] pid 1 (sum_array) loaded on processor 0: page table [1 - | 2 - - - - - - -], 13 frame(s) free
[os] pid 2 created: examples/array_add.txt examples/array_add_data.byte (18 instructions)
[os] pid 2 (array_add) loaded on processor 1: page table [3 - | 4 - - - - - - -], 11 frame(s) free
[os] pid 3 created: examples/fir.txt examples/fir_data.byte (53 instructions)
[os] pid 3 (fir) loaded on processor 2: page table [5 - | 6 - - - - - - -], 9 frame(s) free
[os] pid 4 created: examples/counter.txt examples/counter_data.byte (7 instructions)
[os] pid 4 (counter) loaded on processor 3: page table [7 - | 8 - - - - - - -], 7 frame(s) free
[os] pid 2 (array_add) finished on processor 1 after 31 instructions, 0 page fault(s) -> array_add_data_2.out
[os]     page table [3 - | 4 - - - - - - -] reset: 2 frame(s) freed, 9 free
[os] pid 4 (counter) finished on processor 3 after 43 instructions, 0 page fault(s) -> counter_data_4.out
[os]     page table [7 - | 8 - - - - - - -] reset: 2 frame(s) freed, 11 free
[os] pid 1 (sum_array) finished on processor 0 after 71 instructions, 0 page fault(s) -> sum_array_data_1.out
[os]     page table [1 - | 2 - - - - - - -] reset: 2 frame(s) freed, 13 free
[os] pid 3 (fir) finished on processor 2 after 93 instructions, 0 page fault(s) -> fir_data_3.out
[os]     page table [5 - | 6 - - - - - - -] reset: 2 frame(s) freed, 15 free
[os] all tasks finished; 15 of 15 frames free; print output is in log.txt
```

**Shell.** Here the commands come from a pipe; at a terminal you type them after the `$`.
```
$ printf 'examples/counter.txt examples/counter_data.byte\nexamples/pages.txt examples/pages_data.byte\nexit\n' | ./sim
[shell] type '<program> <data.byte>' to start a task, 'exit' to stop accepting tasks
$ [os] pid 1 created: examples/counter.txt examples/counter_data.byte (7 instructions)
[os] pid 1 (counter) loaded on processor 0: page table [1 - | 2 - - - - - - -], 13 frame(s) free
$ [os] pid 2 created: examples/pages.txt examples/pages_data.byte (15 instructions)
[os] pid 2 (pages) loaded on processor 1: page table [3 - | 4 - - - - - - -], 11 frame(s) free
$ [shell] exit: no new tasks; waiting for the running ones to finish
[os] pid 2 (pages) finished on processor 1 after 15 instructions, 3 page fault(s) -> pages_data_2.out
[os]     page table [3 - | 4 5 - 6 - - - 7] reset: 5 frame(s) freed, 13 free
[os] pid 1 (counter) finished on processor 0 after 43 instructions, 0 page fault(s) -> counter_data_1.out
[os]     page table [1 - | 2 - - - - - - -] reset: 2 frame(s) freed, 15 free
[os] all tasks finished; 15 of 15 frames free; print output is in log.txt
```
`log.txt` then starts with `Process id: 1   x1 : 00000001`.

## Decisions
| The handout says | What this code does, and why |
|---|---|
| `getPhysicallAddress(...)` | Spelled `getPhysicalAddress`. Same arguments: `(proc_id, isFetch, address)`. |
| `execute()` calls `getPhysicallAddress(proc_id, 1, address)` for memory read/write | Loads and stores pass `isFetch = 0`. The handout itself defines isFetch as "1 if access is done by fetch and 0 otherwise", and a data address must be looked up among the data pages (index + 1024/PAGESIZE). |
| `char Instruction[NP][256]`, `Data[NP][4096]` are still listed in the processor and memory sections (left over from Lab 4) | Both are replaced by `memory[MEMSIZE]` plus the page tables; the processor has no memory of its own any more. The instruction space is 256 instructions = **1024 bytes**, as the `1024/PAGESIZE` in the index formula requires. |
| `char pageTable[MAX_PROC][NUM_LOGICAL_PAGES]` | `pageTable[NP][NUM_LOGICAL_PAGES]`: one table per processor slot. A task gets a processor in `loader()` and keeps it until `finalize()`; it is never moved or swapped out. Only tasks that are on a processor own frames, because a waiting task has not been loaded. So at any moment the processor id names exactly one task's page table, and `getPhysicalAddress` is given the processor id. MAX_PROC = NP. |
| Frame 0 is reserved | `freePages[0] = 1` from the start, and `getFreePage()` scans from frame 1. Because no page is ever in frame 0, a page-table entry of 0 means "not mapped" and no valid bit is needed. |
| `initialize()`: "finds how many PAGES are there in program.byte / data.byte" | Only the pages the files fill are mapped: `ceil(size / 512)` each. A 13-instruction program with a 40-byte data file takes 2 frames. The rest of a partly filled page is zeroed. |
| Nothing about data addresses beyond the data file | A data page that `data.byte` did not reach is mapped on its **first touch** to a zeroed frame from `getFreePage()`, counted as a page fault. Programs often write results past their input (`pages.txt`, `touch_all.txt`). An unmapped *instruction* page is an error: the program jumped past its end. |
| `getFreePage()`: "If no free page found, print ERROR and exit from simulation" | **At load** (`initialize`): the loader first checks that the task's pages fit in the free frames. If they don't, the task stays in the waiting queue, as it would with no free processor, and starts when finished tasks have freed enough frames. A task is never half loaded, and a task that arrives at a busy moment does not end the whole simulation. **At a first touch:** `getFreePage()` returns -1. `getPhysicalAddress` prints `ERROR: no free physical frame ...` and stops that task only (its frames are freed); the other tasks go on. To exit the whole simulation as the handout says, replace that `processor_error(...)` with `printf("ERROR\n"); exit(1);`. |
| `finalize()`: "Write data.byte" | Writes `<data>_<pid>.out` rather than overwriting `data.byte`, because several tasks may share a data file. The data is read back through the page table. The file is as long as `data.byte`, or reaches the highest address written if that is further. |
| `char memory[MEMSIZE]` | `unsigned char`, so bytes 0x80..0xFF do not sign-extend when a word is put together. `pageTable` and `freePages` keep the handout's `char`; frame numbers are at most 15, and a `static_assert` keeps them below 128. |
| Address translation | Done for every byte, so a word that crosses a page boundary works (`cross_page.txt`). |
| `scheduler()` sketch: `Proc_id = map_pid_proc_id(pid); process_instructions(pid, 10);` | Passes the processor id: `process_instructions(p, TIME_SLICE)` runs the processor that holds the task. |
| Opcodes, branch-condition table (an image), data size | As in Labs 1–4: see [Byte code](#byte-code). Words are 32-bit little-endian in `.byte` files (`0A 00 00 00` = 10). |

## How Lab 6 changes this
The Lab 6 test (folder `../lab6`) adds two things to this code. **Task 1, instrumentation:**
the compiler keeps `line_of[instruction]` (the source line of each instruction) and the
source text, `execute()` counts `exec_count[p][instruction]`, and `finalize()` writes
`<program>_<pid>.freq` with the execution count of every source line. **Task 2, page tables in
physical memory:** the `pageTable[NP][NUM_LOGICAL_PAGES]` array is removed and the tables move
into frame 0, which Lab 5 already reserves. Processor p's table starts at physical address
p × NUM_LOGICAL_PAGES (bytes 0..39 for 4 processors), and a new register `PTBR[NP]` holds that
address. `initialize()` sets `PTBR[p]` and writes each frame number to
`memory[PTBR[p] + index]`; `getPhysicalAddress()` reads `frame = memory[PTBR[p] + index]`
instead of `pageTable[p][index]`; `finalize()` frees the frames listed there, clears the
region and sets `PTBR[p] = -1`. The free-frame list, `getFreePage()`, the index formula,
first-touch mapping and the loader stay as they are here, and every example in this folder
gives the same `.out` files and `log.txt` in both labs.

## Viva questions
1. **How big is a page table, and how many frames are there?** 10 entries. The instruction
   space is 1024 B = 2 pages of 512 B and the data space is 4096 B = 8 pages. Physical
   memory is 8192 / 512 = 16 frames, and 15 of them can be given to tasks.
2. **Translate data address 3825 for `pages.txt`.** index = 3825 / 512 + 1024 / 512 = 7 + 2 = 9.
   Entry 9 is frame 5, so physical = 5 × 512 + 3825 % 512 = 2560 + 241 = 2801.
3. **Why is frame 0 reserved?** No page ever lives there, so an entry of 0 can mean "not
   mapped". It also leaves the OS a frame of its own: Lab 6 stores the page tables in it.
4. **The handout says `pageTable[MAX_PROC]`. Why NP tables?** A task keeps its processor
   from `loader()` to `finalize()`, and only loaded tasks own frames. So the processor id
   names the task's page table, and `getPhysicalAddress` receives the processor id.
5. **Two tasks write logical address 2000 at the same time. Do they clash?** No. Each page
   table maps data page 3 to a different frame: frames 5 and 6 in `same_addr.txt`. A task
   can only reach frames that are in its own page table.
6. **What is a page fault here?** The first access to a data page that `data.byte` did not
   cover. The OS gives it the first free frame, zeroes it, writes the entry and counts the
   fault. `pages.txt` has 3.
7. **What if memory is full?** At load time the task waits for frames: in `big_sum`, pid 4
   waits although processor 3 is free. At a first touch the task gets the ERROR and stops
   alone, and its frames go back to the free list (`touch_all` × 2).
8. **Why translate one byte at a time?** A 4-byte word can cross a page boundary, and the
   next page can be in any frame. In `cross_page.txt` the word sits in frames 4 and 1.

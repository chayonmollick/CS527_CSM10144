# Lab 6 (Test) — Instrumentation and page tables in physical memory

Lab 6 was a lab test: add two features to the Lab 5 simulator. This folder is the
complete simulator after Lab 6, so it also contains everything from Labs 1–5. It
is the final version, and the base for the worked extensions in
[`../minicomputer/extensions/`](../minicomputer/extensions/). The full design
(instruction set, flags, memory layout, choices where the handouts are
ambiguous) is in [`../minicomputer/README.md`](../minicomputer/README.md).

## What the lab asked
**Task 1 — count executed lines.**
- For an input program, count how many times each executable source statement runs.
- When the task ends, write a separate file listing, line by line, the count and the source statement.
- Leave out empty lines, comment-only lines and labels; they generate no byte code.
- Hints:
  - The compiler keeps the mapping byte-code instruction → source line.
  - The processor counts executions per instruction address.
  - The processor must not parse the source again.
  - Name the file `<program_name>_<PID>.freq`, since several tasks run at once.

**Task 2 — store page tables in physical memory.**
- Remove the `pageTable` array. The page tables live in the reserved physical frame 0, one byte (a frame number) per entry.
- Each processor gets a page-table base register: `PTBR[p]` = the physical address of its table. The suggested layout is `p * NUM_LOGICAL_PAGES`.
- `getPhysicalAddress()` reads the entry through `PTBR`.
- The loader writes the entries and sets `PTBR` before the task starts.
- `finalize()` reads the entries to free the task's frames, clears its table and resets `PTBR`.

Everything else is as in Lab 5.

## What is implemented
| Feature | Where |
|---|---|
| Compiler records instruction → source line | [`compiler.cpp`](compiler.cpp#L269) `compile_file` (`prog.line_of`, `prog.is_label`) |
| Processor counts executions per instruction | [`processor.cpp`](processor.cpp#L157) `execute`: `exec_count[p][instr_pc / 4]++` |
| `<program>_<pid>.freq` written when the task ends | [`os.cpp`](os.cpp#L116) `write_frequency_file`, called from `finalize` |
| Page tables in frame 0, `PTBR[p] = p * NUM_LOGICAL_PAGES` | [`os.cpp`](os.cpp#L107) `initialize` |
| Translation reads the entry at `PTBR[p] + index` | [`os.cpp`](os.cpp#L67) `getPhysicalAddress` |
| Frames freed by reading the table; table cleared; `PTBR = -1` | [`os.cpp`](os.cpp#L145) `finalize` |
| Labs 1–5 (compiler, flags, branches, vectors, OS, shell, paging) | see the table in [`../minicomputer/README.md`](../minicomputer/README.md) |

Frame 0 layout (4 processors × 10 entries = 40 bytes):
```
bytes  0..9   page table of processor 0   <- PTBR[0] = 0
bytes 10..19  processor 1                 <- PTBR[1] = 10
bytes 20..29  processor 2,  bytes 30..39 processor 3
entry = frame number (1..15); 0 = page not mapped (frame 0 is never given to a task)
physical address = memory[PTBR[p] + index] * 512 + address % 512
index = address / 512 for instructions, address / 512 + 2 for data
```

## Build and run
```bash
make                 # g++ -std=c++17 -O2 -Wall -Wextra, no warnings
make test            # 33 checks covering every lab (../minicomputer/tests/run_tests.sh)
./sim -b ../minicomputer/examples/sum_array_print.txt ../minicomputer/examples/sum_array_data.byte
cat sum_array_print_1.freq
```
Options:
- `-b`: batch mode, with no shell.
- `-t`: trace every instruction.
- `-c prog.txt`: compile only and show the listing.

Without `-b`, the shell accepts `<program> <data>` lines until `exit`.

## Example
Two tasks at once (`sum_array_print.txt` with 9 elements, and `array_add.txt`):
```
[os] pid 1 (sum_array_print) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 (array_add) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 2 (array_add) finished on processor 1 after 31 instructions, 0 page fault(s) -> array_add_data_2.out, array_add_2.freq
[os] pid 1 (sum_array_print) finished on processor 0 after 80 instructions, 0 page fault(s) -> sum_array_data_1.out, sum_array_print_1.freq
```
`sum_array_print_1.freq`, identical to the handout's example output:
```
1	x1 = 0       % index
1	x2 = 0        % sum
1	x3 = 0        % array address
1	x4 =  [x3]    %  read the size of array
.loopback
10	x15 = x1 - x4   % compare
10	BEQ .exit
9	x3 = x3 + 4    % increase the address
9	x6 =  [x3]       % load the array data
9	x2 = x2 + x6  % update sum
9	x1 = x1 + 1    % increment index
9	print x1
9	BAL .loopback
.exit
1	[x3] = x2
```

## Decisions
- **Labels:** the handout says to leave them out, but its example output shows them. They are printed without a count, matching the example; empty and comment-only lines are left out.
- **Per-line counts:** each source line produces exactly one instruction here, so a line's count is its instruction's count. If a statement produced several instructions, it should count only the line's first instruction (see `../minicomputer/extensions/e03_large_constants`).
- **Page tables per processor, not per process:** a task keeps its processor until it finishes, so one table per processor is enough. With context switching it would be one per task (`../minicomputer/extensions/e18_context_switch_pcb`).
- **Programs given as compiled `.byte` files:** there is no source, so the `.freq` file lists the disassembled instructions.

## Viva questions
- **Why keep the line map in the compiler?** The compiler already reads the source line by line. Recording `line_of[instruction] = line` costs nothing, and the processor then only counts instruction addresses, as the handout requires.
- **How is a line's count computed?** Add up `exec_count` over the instructions whose `line_of` is that line.
- **What does the PTBR change for a context switch?** The MMU finds the current table through one register. Switching tasks means loading a different `PTBR` value instead of copying a table. Real hardware does the same: x86 CR3, ARM TTBR, RISC-V satp.
- **Why can a page-table entry be 0?** Frame 0 holds the page tables and is never given to a task, so 0 can mean "not mapped".
- **How much of frame 0 do the page tables use?** 4 × 10 = 40 of its 512 bytes. A `static_assert` in `os.cpp` checks that `NP * NUM_LOGICAL_PAGES <= PAGESIZE`.
- **What is the cost of tables in memory?** Every translation is an extra memory read (the entry). Real systems hide this with a TLB (`../minicomputer/extensions/e11_tlb`).
- **How are the frames freed at the end?** `finalize()` reads the task's 10 entries from frame 0, marks those frames free, zeroes the entries and sets `PTBR[p] = -1`.

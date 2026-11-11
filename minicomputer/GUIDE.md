# Extending the simulator in the lab test

The CS527 lab tests (Lab 3: vector instructions; Lab 6: instrumentation and
page tables in memory) asked you to **add a feature to your own simulator** and
to write programs for it. This guide covers how to do that quickly with this
codebase. For anything bigger, start from the matching worked solution in
`extensions/`.

---

## 0. Exam-day setup (5 minutes)
```bash
git clone https://github.com/chayonmollick/CS527_CSM10144.git && cd CS527_CSM10144/minicomputer
cd simulator && make && make test          # must end with "33 passed, 0 failed"
mkdir -p ~/exam && cp -r ../simulator ~/exam/sim && cd ~/exam/sim   # work on a copy
make clean && make                         # always rebuild a copied folder (stale .o files)
```
- Find every change a worked solution made: `grep -n "EXT e13" ../extensions/e13_*/*.cpp`, or read its `CHANGES.diff`.
- Check your edited copy against all the old behaviour: `SIM=$PWD/sim <repo>/minicomputer/tests/run_tests.sh`.
- Submit the way the last tests did: `tar -czvf <entry_no>_final.tgz <folder>`, then check it with `tar -tzvf`.

## 1. How the code fits together
```
prog.txt --compile_file()--> Program{code, source, line_of}   (compiler.cpp)
                                  |
create_task() --> waiting queue --loader()--> initialize(): code + data pages into free frames,
                                  |            page table in frame 0 at PTBR[p]        (os.cpp)
scheduler(): each round, every ready task: process_instructions(p, TIME_SLICE)
                                  |                  fetch -> decode -> execute         (processor.cpp)
                                  |                  every memory byte: getPhysicalAddress()
               task halted ---> finalize(): <data>_<pid>.out, <prog>_<pid>.freq, free frames
               then shell(): reads typed commands without blocking
```

| You want to change… | Edit |
|---|---|
| Sizes: NP, page size, memory size, time slice | `config.h` only |
| The language (new statement or syntax) | `compiler.cpp`: `encode_statement` / `encode_move` / `encode_arith`, `tokenize` for new symbols |
| What an instruction does | `config.h` (opcode) + `processor.cpp` `execute()` + `compiler.cpp` `disassemble()` |
| Per-task counters or statistics | `processor.h/.cpp` (array `[NP]`, reset in `reset()`, count in `execute()`), report in `os.cpp` `finalize()` |
| Address translation, TLB, cache, protection, swapping | `os.cpp` `getPhysicalAddress()` (every access passes through it) |
| Loading or ending a task, its output files | `os.cpp` `loader()`, `initialize()`, `finalize()` |
| Scheduling | `os.cpp` `scheduler()`, `admit_waiting()`, `struct Task` |
| Shell commands | `os.cpp` `run_command()` |
| Command-line options | `main.cpp` |

## 2. Recipes

### Recipe A — add an instruction (example: `x1 = x2 mod x3`, `x1 = x2 mod 7`)
`%` is the comment character, so modulus needs a word. There are four places to change:

1. `config.h`: add opcodes to `enum Opcode`:
   ```c
   OP_MOD = 0x40,    /* xD = xA mod xB     40 D A B */
   OP_MODI = 0x41,   /* xD = xA mod c      41 D A c */
   ```
2. `compiler.cpp` `encode_arith()`: put this first. The tokenizer already passes `mod` through as a word.
   ```c
   if (lower(op) == "mod") {
       if (dst.kind != Operand::XREG || a.kind != Operand::XREG) { msg = "mod works on x registers"; return false; }
       if (b.kind == Operand::XREG)  { set(ins, OP_MOD, dst.value, a.value, b.value); return true; }
       if (b.kind == Operand::CONST) { set(ins, OP_MODI, dst.value, a.value, b.value); return true; }
       msg = "mod needs an x register or a constant"; return false;
   }
   ```
   A new **symbol** (e.g. `&`, `<<`) must also be split out in `tokenize()`: add it to the `"=+-*/[]"` list, and for two-character symbols check the next character.
3. `processor.cpp` `execute()`:
   ```c
   case OP_MOD: case OP_MODI: {
       int divisor = opcode == OP_MOD ? x[src2] : src2;
       if (divisor == 0) { processor_error("mod by zero"); break; }
       x[dest] = (divisor == -1) ? 0 : x[src1] % divisor;
       break;
   }
   ```
4. `compiler.cpp` `disassemble()`: `if (op == OP_MOD) { snprintf(buf, sizeof buf, "x%d = x%d mod x%d", d, a, b); return buf; }` (same for `OP_MODI`).

Test it: `./sim -c t.txt` shows the encoding; `./sim -b -t t.txt d.byte` traces it; check the `.out` file.
Full versions: `extensions/e01_bitwise_mod_shift` (and/or/xor/shifts/mod), `e04_base_offset_addressing`, `e05_vector_extras`.

### Recipe B — a statement with a new keyword (example: `halt`, `nop`, `swap`)
In `encode_statement()`, next to `print`:
```c
if (head == "halt" && t.size() == 1) { set(ins, OP_HALT, 0, 0, 0); return true; }
```
If one statement must become **several** instructions (e.g. `swap x1 x2` = `x1 = x1 + x2`, `x2 = x1 - x2`, `x1 = x1 - x2`):
- **Pass 1** must count how many instructions each line produces, because labels count instructions.
- **Pass 2** pushes every instruction, with the same source line in `line_of`.
- **Frequency report:** count only the *first* instruction of a line, or the `.freq` report triples the count.

Full version: `extensions/e03_large_constants` (one line → up to 4 instructions).

### Recipe C — count something per task (instrumentation, like Lab 6 task 1)
```c
/* processor.h */   extern long mem_accesses[NP];
/* processor.cpp */ long mem_accesses[NP];
/*   reset(p)   */  mem_accesses[p] = 0;
/*   load_word / store_word */  mem_accesses[proc_id]++;
/* os.cpp finalize(p, t) */
std::printf("[os] pid %d made %ld memory accesses\n", t.pid, mem_accesses[p]);
```
- **Per instruction:** `exec_count[p][instr_pc / 4]` already exists.
- **Per source line:** go through `t.prog.line_of`, as `write_frequency_file()` does.
- **Writing a file:** copy `write_frequency_file()` and change the name and contents.

Full versions: `e06_instruction_mix_cpi`, `e07_branch_predictor`, `e08_pipeline_hazards`, `e09_line_profile_hotspots`, `e10_register_dump_trace`.

### Recipe D — change the memory system
`getPhysicalAddress(p, isFetch, address)` is the only place where logical addresses become physical ones. In order, it:
1. checks the address range;
2. computes `index = address/PAGESIZE (+ NUM_INSTR_PAGES for data)`;
3. reads `frame = memory[PTBR[p] + index]`;
4. on frame 0 (unmapped), demand-allocates a data page;
5. returns `frame*PAGESIZE + offset`.

| Feature | Change |
|---|---|
| TLB | Look up `(p, index)` before step 3; fill it after. Flush it in `loader()` |
| Protection / dirty / referenced bits | Use the free high bits of the 1-byte entry (16 frames need only 4 bits); mask them off when computing the frame |
| Replacement / swapping | When `getFreePage()` returns -1, pick a victim, copy its frame to a backing store, clear its entry, reuse the frame |
| Cache | Wrap the `memory[pa]` accesses in `processor.cpp` |

Full versions: `e11_tlb`, `e12_data_cache`, `e13_page_replacement`, `e14_pte_flags_protection`, `e15_memory_map_dump`.

### Recipe E — change the OS
- **Shell command:** add it in `run_command()` (e.g. `if (words[0] == "ps") { ...; prompt(); return; }`).
- **New per-task field:** add it to `struct Task`; set it in `create_task()`; report it in `finalize()`.
- **Scheduling:** in `scheduler()`, the order in which `ready_queue` is served is the policy. `admit_waiting()` decides who starts.

Full versions: `e16_priority_scheduling`, `e17_shell_commands`, `e18_context_switch_pcb` (more tasks than processors), `e19_gantt_timeline`, `e20_fork_getpid`, `e21_wait_exit_code`.

### Recipe F — write a program in the language
See `programs/README.md`; there are 35 solved programs there. Remember:
- **Constants** are 0..255. Build bigger numbers: `x2 = 255`, `x3 = x2 * 4`.
- **Compare** with `x9 = xA - xB`, then `BEQ/BNE/BGE/BLT/BGT/BLE`. Only add and subtract set the flags.
- **Labels** go alone on their line, start with `.`, and need no `:`. `%` starts a comment.
- **Vectors** take 8 consecutive words: `v1 = [x2]`, then step the address by 32.

## 3. Quick changes (one or two lines)
| Asked for | Do |
|---|---|
| 2 processors / time slice 5 / page size 256 | `config.h`: `NP 2` / `TIME_SLICE 5` / `PAGESIZE 256`, then `make clean && make`. The page-table size and frame count follow automatically; the `static_assert` in `os.cpp` checks that all page tables still fit in frame 0 |
| `print` in decimal, or also on the screen | `processor.cpp` `OP_PRINT`: change the `fprintf` format, add `printf` |
| x0 always zero (RISC-V style) | End of `execute()`: `Register[p][0] = 0;` |
| Write the output back into `data.byte` itself | `os.cpp` `finalize()`: `out_name = t.data_path;` |
| Stop the whole simulation when memory is full (handout wording) | `os.cpp` `getPhysicalAddress()`: replace `processor_error(...)` by `printf("ERROR\n"); exit(1);` |
| Show every instruction | Run with `-t` |
| Count executed instructions | Already printed at the end of each task (`after N instructions`) |
| Run the programs from the handout | `examples/`: sum_array, sum_n, array_add, fir, complex_mul, det3x3, counter, pages |

## 4. Debugging
- `./sim -c prog.txt`: compile errors show `file:line: message`; the listing shows each instruction's bytes, disassembly and source line.
- `./sim -b -t prog.txt data.byte 2> trace.txt`: every executed instruction.
- `../tools/words.sh x_1.out`: every 32-bit word of a data or output file, with its address, in hex and decimal.
- **Wrong branch:** check the flags. Only add/sub set them, so a `mul` between the compare and the branch does not change them, but another `sub` does.
- **Task stops "outside the logical data space":** an address went past 4095. Remember that addresses are in bytes (word i is at 4i).

## 5. Likely questions → where the answer is
| Question | Worked solution |
|---|---|
| Add AND/OR/XOR/shift/mod instructions | `extensions/e01_bitwise_mod_shift` |
| Add subroutines: call/return, push/pop, a stack | `extensions/e02_call_ret_stack` |
| Allow constants > 255 or negative constants | `extensions/e03_large_constants` |
| Base + offset addressing `[x1 + 8]` | `extensions/e04_base_offset_addressing` |
| Vector broadcast / horizontal sum / max / print a vector | `extensions/e05_vector_extras` |
| Count instructions by type, cycles, CPI | `extensions/e06_instruction_mix_cpi` |
| Branch prediction accuracy (1-bit, 2-bit) | `extensions/e07_branch_predictor` |
| Pipeline: data hazards, stalls, forwarding | `extensions/e08_pipeline_hazards` |
| Profile: cycles per line, hot spots, dead code | `extensions/e09_line_profile_hotspots` |
| Dump registers / trace file | `extensions/e10_register_dump_trace` |
| Add a TLB | `extensions/e11_tlb` |
| Add a data cache | `extensions/e12_data_cache` |
| Page replacement (FIFO/LRU) and swapping | `extensions/e13_page_replacement` |
| Valid/dirty/referenced bits, segmentation fault | `extensions/e14_pte_flags_protection` |
| Print the memory map / page tables | `extensions/e15_memory_map_dump` |
| Priority / shortest-job-first scheduling | `extensions/e16_priority_scheduling` |
| Shell commands ps / kill / mem | `extensions/e17_shell_commands` |
| More tasks than processors (context switch, PCB) | `extensions/e18_context_switch_pcb` |
| Gantt chart, waiting and turnaround time | `extensions/e19_gantt_timeline` |
| `fork()` and `getpid` | `extensions/e20_fork_getpid` |
| `exit` with an exit code, `wait` | `extensions/e21_wait_exit_code` |
| Make the NP processors really run in parallel with OpenMP (combines both halves of the course) | `extensions/e22_openmp_parallel_processors` |
| Write a program: max, sort, search, matrix, vector dot product… | `programs/` |

## 6. Viva answers on the base design
- **Why is a page table 10 entries?** The instruction space is 1024 B = 2 pages of 512 and the data space is 4096 B = 8 pages, so 10 logical pages per task.
- **Why is frame 0 reserved?**
  - It holds the page tables (Lab 6 task 2): processor p's table is at bytes `p*10 .. p*10+9`, and `PTBR[p]` holds that address.
  - Because no page is ever placed in frame 0, an entry of 0 can mean "not mapped".
- **What does the PTBR give you?** The MMU finds the current task's table with one register, so a context switch only has to change `PTBR`, not copy a table. Real hardware does the same (x86 CR3, ARM TTBR, RISC-V satp).
- **How is a logical address translated?** `frame = memory[PTBR[p] + address/512 (+2 for data)]`, then `physical = frame*512 + address%512`.
- **Why is translation done one byte at a time?** A 4-byte word may cross a page boundary, and the next page can be in a non-adjacent frame.
- **What does the loader do when memory is full?** The task waits in the waiting queue until a finishing task frees frames. A data page outside the data file gets a frame on first touch (demand paging).
- **What is the branch offset relative to?** It counts *instructions* relative to the branch itself: `PC = branch_address + 4*offset`. `BAL .loopback` at instruction 10 to instruction 4 is offset −6 = `FA`.
- **Which instructions set the flags?** Only add/sub, as the handout says. C for subtraction is "operand 1 > operand 2 (unsigned)", so `BHI` (C && !Z) means unsigned >. `BGE` (N == V) is signed ≥.
- **How does vector load/store work?** It moves 8 consecutive 32-bit words: lane i ↔ address + 4i. The array-add and FIR programs process 8 elements per instruction.
- **Round robin:** each ready task gets `TIME_SLICE` (10) instructions per round, then the shell runs, so typing never stops the tasks. The waiting queue starts a task when a processor and enough frames are free.
- **Why is the shell non-blocking?** `select()` with a zero timeout says whether input is waiting, so `read()` never blocks the scheduler. With a terminal, characters arrive a line at a time (canonical mode).
- **Lab 6 task 1 — how does the frequency report work?**
  - The compiler records `line_of[instruction] = source line`.
  - The processor counts `exec_count[p][instruction]`, and finalize sums them per line.
  - The processor never re-reads the source, as the handout requires.
- **Why does each task get its own output file?** Several tasks run at once; writing all of them to `data.byte` would make them overwrite each other.

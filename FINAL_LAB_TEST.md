# CS527 final lab test — start here

The CS527 labs had two halves. Every lab has its own complete folder (`lab1/` …
`lab9/`) with code, examples, tests and a README, and each half has a guide and
worked practice questions.

| Labs | Topic | Guide | Worked questions |
|---|---|---|---|
| 1–6 | Mini-computer simulator: compiler, processor, vectors, OS with shell and scheduler, paging, page tables in memory | [`minicomputer/GUIDE.md`](minicomputer/GUIDE.md) | [`minicomputer/extensions/`](minicomputer/extensions/), [`minicomputer/programs/`](minicomputer/programs/) |
| 7–9 | OpenMP: loops, reductions, matrices, search/sort/histogram, tasks, scheduling | [`GUIDE.md`](GUIDE.md), [`CHEATSHEET.md`](CHEATSHEET.md) | [`PROBLEMS.md`](PROBLEMS.md), [`practice/`](practice/) |

## Every lab and where it is

| Classroom lab | What it asked | Folder | Checked by |
|---|---|---|---|
| Lab 1: Mini computer system | Compiler → byte code, memory, fetch/decode/execute | [`lab1/`](lab1/) | `make test`: byte code identical to the handout's listing |
| Lab 2 (Practice) | Constants, flags ZNCV, `[x]` memory, labels, branches, comments, hex files | [`lab2/`](lab2/) | `make test`: byte code identical to the handout's listing |
| Lab 3 Test / Takehome | Vector registers and instructions; array-add and FIR programs | [`lab3/`](lab3/) | `make test` |
| Lab 4 take home | NP processors, OS, round-robin scheduler, non-blocking shell, loader, `print` | [`lab4/`](lab4/) | `make test` (5 tasks on 4 processors) |
| Lab 5 take home | Logical/physical memory, paging, free frames, frame 0 reserved | [`lab5/`](lab5/) | `make test` |
| Lab 6 Test | Task 1: executions per source line (`.freq`); task 2: page tables in frame 0 + PTBR. Final simulator with Labs 1–5 | [`lab6/`](lab6/) | `make test`: 33 checks for every lab; `.freq` identical to the handout's example |
| Lab 7: Getting started with OpenMP | Vector add, sum, max/min with index, matrix add, matmul | [`lab7/`](lab7/) | each program checks itself against serial; `REPORT.md` |
| Lab 8 (OpenMP session 2) | Search, sort, histogram, transpose, convolution | [`lab8/`](lab8/) | same |
| Lab 9 (OpenMP session 3) | Merge sort with tasks, static/dynamic/guided, even/odd count | [`lab9/`](lab9/) | same |

## How past lab tests ran
- **Lab 3 Test (13 Aug):**
  - The problem was released at 2:25 and submissions closed at 5:30.
  - The rubric gave marks for: updating the compiler, updating the processor, two programs, code quality, a Makefile with an executable taking program.byte/data.byte, and **unseen test cases** by the instructor. The viva was part of every category.
- **Lab 6 Test (12 Sep):**
  - A system was assigned to you, with about 20 minutes to set up and test your code.
  - The problem statement came as a PDF at about 2:25, then the network went off.
  - Only the IDE, the problem PDF and cppreference were allowed.
  - You submitted a `tar -czvf` archive on Classroom.

In both tests you **extended your own lab code** with a new feature, so expect the same.

## Setup (first 20 minutes)
```bash
git clone https://github.com/chayonmollick/CS527_CSM10144.git && cd CS527_CSM10144
(cd lab6 && make && make test)                      # final simulator: 33 passed, 0 failed
make -s -C lab7 && make -s -C lab8 && make -s -C lab9 && make -s -C problems   # OpenMP side builds
nproc; g++ --version | head -1; tar --version | head -1
mkdir -p ~/exam && cp -r lab6 ~/exam/sim && (cd ~/exam/sim && make clean && make)
```
Then download the problem PDF, and open this file and the two guides in the IDE.

## Which guide for which question
| The problem says… | Go to |
|---|---|
| "add an instruction / operator / addressing mode / statement" | `minicomputer/GUIDE.md` recipes A–B, `extensions/e01`–`e05` |
| "count / measure / report (instructions, cycles, branches, lines)" | recipe C, `extensions/e06`–`e10` |
| "TLB / cache / page replacement / protection / memory map" | recipe D, `extensions/e11`–`e15` |
| "scheduler / shell command / fork / exit / context switch" | recipe E, `extensions/e16`–`e21` |
| "write a program that…" (array, matrix, sort, vector) | `minicomputer/programs/` |
| "use OpenMP to make the simulator's processors run in parallel" | `minicomputer/extensions/e22_openmp_parallel_processors` |
| "parallelise this C code with OpenMP, time it, explain the speedup" | `GUIDE.md`, then `PROBLEMS.md` (104 solved problems) |
| matrix rules, which matrix loops can run in parallel | `MATRIX_RULES.md` |

## Example questions you may get, and their worked answers

**100 more practice questions** in the same style, each with a tested answer (a small
patch against `lab6/` or a program) and a guide on how to use them:
[`final_practice/`](final_practice/) ([`GUIDE.md`](final_practice/GUIDE.md)).

**Simulator (Labs 1–6):**
1. Add `mod`, `&`, `|`, `^`, `<<`, `>>` to the language and processor → `e01_bitwise_mod_shift`
2. Add subroutines: `call .label`, `ret`, `push`, `pop` with a stack in data memory → `e02_call_ret_stack`
3. Allow constants larger than 255 and negative constants → `e03_large_constants`
4. Add base + offset addressing `x1 = [x2 + 8]` → `e04_base_offset_addressing`
5. Add vector broadcast, horizontal sum and `print v1`; redo the FIR filter with them → `e05_vector_extras`
6. Report the instruction mix, total cycles and CPI per task → `e06_instruction_mix_cpi`
7. Add a branch predictor and report its accuracy → `e07_branch_predictor`
8. Count pipeline stalls with and without forwarding → `e08_pipeline_hazards`
9. Extend the Lab 6 report: cycles per line, hot spots, dead code → `e09_line_profile_hotspots`
10. Dump the registers at the end / trace every instruction to a file → `e10_register_dump_trace`
11. Add a TLB and report its hit rate → `e11_tlb`
12. Add a data cache (direct-mapped, write-back/write-through) → `e12_data_cache`
13. Page replacement (FIFO/LRU) with a backing store, when memory is full → `e13_page_replacement`
14. Valid/dirty/referenced bits in the page-table entry; segmentation fault → `e14_pte_flags_protection`
15. Print the physical memory map and the page tables from frame 0 → `e15_memory_map_dump`
16. Priority or shortest-job-first scheduling with waiting/turnaround times → `e16_priority_scheduling`
17. Shell commands `ps`, `kill <pid>`, `mem` → `e17_shell_commands`
18. More tasks than processors: context switch with a PCB → `e18_context_switch_pcb`
19. Gantt chart of the schedule → `e19_gantt_timeline`
20. `fork()` / `getpid` → `e20_fork_getpid`; `exit` codes and `wait` → `e21_wait_exit_code`
21. Run the NP processors in parallel with OpenMP → `e22_openmp_parallel_processors`
22. Write programs: max/min with index, sorting, binary search, GCD, factorial, matrix and vector programs → `minicomputer/programs/`

**OpenMP (Labs 7–9):** the 104 problems in `PROBLEMS.md` plus `practice/q01`–`q34`. The most likely ones:
- **Variants of the lab tasks:** dot product, π, matrix-vector, max with index, histogram, transpose, convolution kernels, merge sort and quicksort with tasks, schedules on uneven work.
- **Textbook exercises from the lab handouts' reference book:** LU factorisation (`p070`), wavefront `x[i][j] = x[i-1][j] + x[i][j-1]` (`p078`), Mandelbrot area (`p086`), smallest factor with tasks (`p087`), Pascal's triangle (`p088`), N-body (`p096`), lock deadlock (`p015`).
- **Lecture slides:** red-black Gauss–Seidel (`p077`) and Amdahl's law (`p008`, `p009`).

## Time plan (about 3 hours)
| Time | Do |
|---|---|
| 0:00–0:15 | Read the problem twice. List: new syntax, new opcodes, new outputs, programs to write, questions |
| 0:15–0:30 | Open the closest worked solution; read its README and `CHANGES.diff` |
| 0:30–1:45 | Implement it in `~/exam/sim`, one recipe step at a time; `make` after each step |
| 1:45–2:15 | Test programs plus edge cases (negative values, zero, N = 0, the largest sizes). Run the old tests: `SIM=$PWD/sim <repo>/minicomputer/tests/run_tests.sh` |
| 2:15–2:45 | README: what changed, how to run, sample output, answers to the questions |
| 2:45–end | `make clean && make` once more, then `tar -czvf`, check with `tar -tzvf`, upload |

## Submission checklist
- [ ] `make` builds with no warnings; the executable takes the program and data files on the command line.
- [ ] Test programs and data files are included, each with its expected result in the README.
- [ ] The old features still work (the 33 base tests).
- [ ] The README explains what you changed, file by file. Mark changes in the code with comments, so the viva is easy.
- [ ] `tar -czvf <entry_no>_final.tgz <folder>`, checked with `tar -tzvf`, uploaded before the network deadline.

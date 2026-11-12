# CS527 Computer Systems — Labs 1 to 9

Every CS527 lab has its own complete folder, with its code, examples, tests and a
README explaining what the lab asked and how it is solved. Guides and banks of
solved practice questions for the lab test sit alongside the labs.

**Final lab test: start with [`FINAL_LAB_TEST.md`](FINAL_LAB_TEST.md).**

**100 practice questions with tested answers and a guide: [`final_practice/`](final_practice/).**

**Parallel program examples** (one per OpenMP construct, and jobs split across the simulator's processors): [`parallel_examples/`](parallel_examples/).

## The labs
| Lab (problems) | Folder |
|---|---|
| **Lab 1** (Mini computer system: compiler → byte code, memory, processor fetch/decode/execute) | [`lab1/`](lab1/) |
| **Lab 2** (Constants in arithmetic, flags Z N C V, `[x]` memory access, labels and branches, comments, hex files) | [`lab2/`](lab2/) |
| **Lab 3** (Vector registers and instructions; array addition and FIR filter with vectors) | [`lab3/`](lab3/) |
| **Lab 4** (Multiprocessing: 4 processors, OS with round-robin scheduler, shell, loader, `print`) | [`lab4/`](lab4/) |
| **Lab 5** (Paging: logical and physical memory, page tables, free frames) | [`lab5/`](lab5/) |
| **Lab 6** (Count executed lines per source line; store page tables in physical memory with PTBR) — the final simulator | [`lab6/`](lab6/) |
| **Lab 7** (OpenMP: vector addition, sum of array elements, maximum/minimum with index, matrix addition, matrix multiplication) | [`lab7/`](lab7/) |
| **Lab 8** (OpenMP: array search, array sorting, histogram, matrix transpose, 2D convolution) | [`lab8/`](lab8/) |
| **Lab 9** (OpenMP: merge sort with tasks, static/dynamic/guided scheduling, even/odd count) | [`lab9/`](lab9/) |

Build any lab with `make` inside its folder. `make test` runs the checks in Labs 1–6; the
OpenMP programs of Labs 7–9 check themselves against their serial versions.

## Guides and practice questions
| | Mini-computer (Labs 1–6) | OpenMP (Labs 7–9) |
|---|---|---|
| Guide | [`minicomputer/GUIDE.md`](minicomputer/GUIDE.md): how to extend the simulator, viva answers | [`GUIDE.md`](GUIDE.md): how to solve a new question; [`CHEATSHEET.md`](CHEATSHEET.md): syntax and report answers |
| Worked test questions | [`minicomputer/extensions/`](minicomputer/extensions/): 22 likely extensions, solved and tested | [`PROBLEMS.md`](PROBLEMS.md): 104 solved problems ([`problems/`](problems/)); [`practice/`](practice/): 31 programs |
| Programs | [`minicomputer/programs/`](minicomputer/programs/): 35 programs in the course language | [`practice/q00_template.c`](practice/q00_template.c): template for a new question |
| Also | [`minicomputer/examples/`](minicomputer/examples/), [`minicomputer/tests/`](minicomputer/tests/) | [`MATRIX_RULES.md`](MATRIX_RULES.md): matrix rules |

## Lab day
```bash
git clone https://github.com/chayonmollick/CS527_CSM10144.git && cd CS527_CSM10144
(cd lab6 && make && make test)        # simulator: 33 passed, 0 failed
make -C lab7 && make -C lab8 && make -C lab9 && make -C problems
```

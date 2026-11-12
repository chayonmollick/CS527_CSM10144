# CS527 Computer Systems — Labs 1 to 9

Every CS527 lab has its own complete folder, with its code, examples, tests and a
README explaining what the lab asked and how it is solved. Guides and banks of
solved practice questions for the lab test sit alongside the labs.

**Final lab test: start with [`FINAL_LAB_TEST.md`](FINAL_LAB_TEST.md).**

**100 practice questions with tested answers and a guide: [`final_practice/`](final_practice/).**

## The labs
| Lab | Folder | Topic |
|---|---|---|
| 1 | [`lab1/`](lab1/) | Mini-computer: compiler → byte code, memory, processor (fetch/decode/execute) |
| 2 | [`lab2/`](lab2/) | Constants, flags Z N C V, `[x]` memory, labels and branches, comments, hex files |
| 3 | [`lab3/`](lab3/) | Vector registers and vector instructions; array add and FIR filter (the Lab 3 test) |
| 4 | [`lab4/`](lab4/) | Multiprocessing: NP processors, OS with round-robin scheduler, shell, loader, `print` |
| 5 | [`lab5/`](lab5/) | Logical and physical memory: paging, page tables, free frames |
| 6 | [`lab6/`](lab6/) | The Lab 6 test: execution frequency per source line, page tables in physical memory (PTBR). This is the final simulator, with everything from Labs 1–5 |
| 7 | [`lab7/`](lab7/) | OpenMP: vector add, sum, max/min with index, matrix add, matrix multiply |
| 8 | [`lab8/`](lab8/) | OpenMP: array search, parallel sort, histogram, transpose, 2D convolution |
| 9 | [`lab9/`](lab9/) | OpenMP: merge sort with tasks, static/dynamic/guided scheduling, even/odd count |

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

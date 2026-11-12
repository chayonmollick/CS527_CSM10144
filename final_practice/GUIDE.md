# Final lab test practice — guide

100 practice questions in the style of the CS527 lab tests, each with a tested
answer. Past lab tests (Lab 3, Lab 6) gave you your own simulator and asked you to
**add a feature** and **write programs for it**, so most questions here are exactly
that. The answers are small patches against [`../lab6/`](../lab6/) (the final
simulator), so each one shows precisely which lines to write and where.

## The questions
| Range | Kind of question | Answer is |
|---|---|---|
| Q001–Q020 | Write a program in the course language | `solution.txt` |
| Q021–Q045 | Add an instruction or new syntax (compiler + processor) | `solution.patch` |
| Q046–Q060 | Instrumentation: counters, traces, reports, options | `solution.patch` |
| Q061–Q075 | Memory: paging, frames, page tables, protection, TLB | `solution.patch` |
| Q076–Q095 | OS: scheduling, shell commands, processes, synchronisation | `solution.patch` |
| Q096–Q100 | OpenMP inside the simulator (both halves of the course) | `solution.patch` |

[`README.md`](README.md) lists all 100 with their titles. Every question folder has:
- `README.md`: the question as it could appear in the test, the answer explained, a table of changes (file, function, change), what the test checks, and viva points;
- `solution.patch` or `solution.txt`: the answer;
- test programs and data;
- `test.sh`: applies the answer to a fresh copy of `lab6`, builds it, runs the programs and checks the results.

## How to practise
1. Read the question in its `README.md`, then **close it** and try to write the change
   yourself in a copy of lab6:
   `cp -r ../lab6 ~/try && cd ~/try && make clean && make`.
2. Compare with `solution.patch`. Lines starting with `+` were added and lines with `-`
   were removed; the `@@` lines and the context around them show where.
3. Check your version with the question's programs:
   `./sim -b <question>/prog.txt <question>/prog_data.byte`.
4. Run the official test to see the expected numbers: `./qNNN_name/test.sh`.
5. Make a list of the questions you got wrong and redo them.

To check everything at once: `./run_all.sh` (all 100), or `./run_all.sh q02` (Q020–Q029).

## Using an answer in the exam
- **Apply a patch to your copy of the simulator:**
  - `cd ~/exam/sim && patch -p1 < <repo>/final_practice/qNNN_name/solution.patch`
  - Without `patch`, use `git apply -p1 <file>`.
- **Combine several patches:** apply them one after another. Two patches that change the same lines may conflict. Then apply one and type the other by hand, using the README's change table.
- **Create your own patch, to show exactly what you changed:** `<repo>/final_practice/make_patch.sh ~/exam/sim > my_changes.patch`.
- **Opcode ranges:** each group of questions uses its own range of opcodes, so answers from different groups rarely clash. Check `config.h` if two answers add instructions:

  | Questions | Opcodes |
  |---|---|
  | Q021–Q033 | 0x40–0x7F |
  | Q034–Q045 | 0x80–0xBF |
  | Q046–Q060 | 0xC0–0xCF |
  | Q061–Q075 | 0xD0–0xDF |
  | Q076–Q095 | 0xE0–0xF7 |

## Attacking an unseen question
1. **Classify it** with the table above: is it a program, an instruction, instrumentation, memory, OS, or OpenMP?
2. **Find the closest practice question or worked extension**: here, in [`../minicomputer/extensions/`](../minicomputer/extensions/) (22 bigger ones), and in [`../minicomputer/programs/`](../minicomputer/programs/) (35 programs). Search them: `grep -il "keyword" */README.md ../minicomputer/extensions/*/README.md`.
3. **Follow the recipe** in [`../minicomputer/GUIDE.md`](../minicomputer/GUIDE.md):
   - New instruction: `config.h` opcode → compiler `encode_*` → processor `execute()` → `disassemble()`.
   - Counter: an `[NP]` array, reset in `reset()`, counted in `execute()`, reported in `finalize()`.
   - Memory: `getPhysicalAddress()`.
   - OS: `scheduler()`, `loader()`, `run_command()`.
4. **Write a test program first** with a result you can compute by hand, then implement. Run `make` after every step; fix warnings at once.
5. **Check the old behaviour still works:** `SIM=$PWD/sim <repo>/minicomputer/tests/run_tests.sh` (33 checks).
6. **Write the README for the submission:** what you changed (a table like the ones here), how to run it, the test programs with expected and actual output.

## Things that catch people out
- **`%` starts a comment anywhere.** `x1 = x2 % x3` silently becomes `x1 = x2`. Use a word (`mod`) for new operators.
- **Constants** are 0..255 and only allowed as the second operand. Build bigger values with arithmetic.
- **Flags:** only add and subtract set them. A new compare-like instruction must set them itself.
- **Labels count instructions.** A statement that expands to several instructions must be counted in **pass 1** too. Otherwise every branch after it jumps to the wrong place.
- **The frequency report** sums executions per line. A line that becomes several instructions must count only its first instruction.
- **A task keeps its processor until it finishes.** "Blocking" a task (wait, sleep, recv) in this design leaves the processor idle. With more blocked tasks than processors you can deadlock.
- **After copying a folder**, run `make clean && make`. Stale `.o` files from the copy can hide your changes.

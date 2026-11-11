# e07 — Branch predictors (static and dynamic)

## Question (as it could appear in the test)

> Add branch prediction to the processor. For every executed branch (BAL counts as a branch
> that is always taken), evaluate four predictors at the same time:
> (a) static always-taken, (b) static backward-taken / forward-not-taken (BTFN),
> (c) 1-bit and (d) 2-bit saturating-counter predictors kept in a table indexed by the
> instruction index. Each processor has its own table, and it is reset when a task is loaded.
> At the end of a task, report predictions, mispredictions and accuracy per predictor in
> `<program>_<pid>.stats` and print them. Show the results for sum_array and for a
> loop-inside-a-loop program, and explain why the 2-bit predictor does better than the 1-bit
> one.

## Solution idea

- All predictors watch the real outcome in `execute()`. The branch is resolved exactly as
  before, and `predict_branch(p, index, offset, taken)` first scores every predictor's guess
  against `taken`, then trains the dynamic ones.
  - always-taken: guess = taken.
  - BTFN: guess taken if `offset <= 0`. A backward branch (including one to itself) closes a
    loop.
  - 1-bit: `bp_last[p][index % BP_TABLE_SIZE]` holds the last outcome of this branch.
  - 2-bit: `bp_counter[p][...]` is a saturating counter 0..3 that guesses taken when it is
    `>= 2`. Taken: `++` up to 3. Not taken: `--` down to 0.
- Tables and statistics are per processor (`[NP]`), like every other piece of processor
  state. `reset(p)` puts them back to `BP_1BIT_INIT = 0` (not taken) and
  `BP_2BIT_INIT = 1` (weakly not taken) whenever the loader starts a task on `p`.
- `finalize()` writes the totals and a per-branch table (executions, taken, mispredictions of
  each predictor) to `<program>_<pid>.stats` and prints `[bpred] ...`.
- `BP_TABLE_SIZE = 256` equals the number of instruction slots, so there is no aliasing. Set
  it to e.g. 4 to see two branches share one entry.

## Changes

| file | function | change |
|---|---|---|
| config.h | (global) | `BP_TABLE_SIZE`, `BP_1BIT_INIT`, `BP_2BIT_INIT`, `enum Predictor` |
| processor.h | (declarations) | `extern` statistics: `bp_branches`, `bp_taken`, `bp_miss`, per-branch `bp_taken_at`, `bp_miss_at`, `PREDICTOR_NAME` |
| processor.cpp | (globals) | statistics, plus `bp_last[NP][BP_TABLE_SIZE]` (1-bit) and `bp_counter[NP][BP_TABLE_SIZE]` (2-bit) |
| processor.cpp | `reset()` | clear the statistics, set the tables to their initial states |
| processor.cpp | `predict_branch()` (new) | guess with all 4 predictors, count mispredictions, train 1-bit and 2-bit |
| processor.cpp | `execute()` | branch case: compute `taken`, call `predict_branch()`, then jump as before |
| os.cpp | `branch_text()`, `write_branch_file()` (new) | write `<program>_<pid>.stats` (totals + per-branch table) |
| os.cpp | `finalize()` | call it; print `[bpred] pid N (prog): B branches, mispredicted (accuracy): ...` |
| main.cpp, os.cpp | header comments | mention the `.stats` output |
| Makefile | `test`, `clean` | run the base tests on this build and `./test.sh`; delete `*.stats` |

`grep -n "EXT e07" *.cpp *.h Makefile` lists every change. The full diff is in `CHANGES.diff`.

## Run

```sh
cd minicomputer/extensions/e07_branch_predictor
make                                          # zero warnings
./test.sh                                     # e07 checks (24)
SIM=$PWD/sim ../../tests/run_tests.sh         # base regression tests (33)
mkdir -p /tmp/e07 && cp ../../examples/* nested.txt nested_data.byte /tmp/e07 && cd /tmp/e07
$OLDPWD/sim -b sum_array.txt sum_array_data.byte nested.txt nested_data.byte
cat sum_array_1.stats nested_2.stats
```

`nested.txt` is a loop inside a loop: outer 4 times, inner 5 times. Both loops are tested at
the bottom with a backward `BNE`, and the program stores 20 at [0].

## Expected output

```
[bpred] pid 1 (sum_array): 19 branches, mispredicted (accuracy): always-taken 9 (52.63%), btfn 1 (94.74%), 1-bit 2 (89.47%), 2-bit 2 (89.47%) -> sum_array_1.stats
[bpred] pid 2 (nested): 24 branches, mispredicted (accuracy): always-taken 5 (79.17%), btfn 5 (79.17%), 1-bit 10 (58.33%), 2-bit 7 (70.83%) -> nested_2.stats
```
`nested_2.stats`:
```
Branch prediction for nested (pid 2)
branches 24   taken 19   not taken 5

predictor      predictions mispredictions  accuracy
always-taken            24              5    79.17%
btfn                    24              5    79.17%
1-bit                   24             10    58.33%
2-bit                   24              7    70.83%

per branch: executions, times taken, mispredictions of each predictor
instr  branch             executed  taken  always  btfn  1-bit  2-bit
    6  BNE .inner               20     16       4     4      8      5
    9  BNE .outer                4      3       1     1      2      2

(1-bit entries start at 0, 2-bit counters at 1; 256 entries indexed by instruction index)
```

**Derivation for sum_array (N = 9).** The branches are `BEQ .exit` (instr 5, offset +6,
forward), whose outcomes are NT x9 and then T, and `BAL .loopback` (instr 10, offset -6,
backward), which is T x9. That is 19 branches, 10 taken and 9 not taken.

| predictor | BEQ (NNNNNNNNNT) | BAL (TTTTTTTTT) | total | accuracy |
|---|---|---|---|---|
| always-taken | 9 (every N) | 0 | 9 | 10/19 = 52.63% |
| BTFN | 1 (forward, so it guesses N and misses the exit) | 0 (backward, so T) | 1 | 18/19 = 94.74% |
| 1-bit (starts N) | 1 (the exit) | 1 (the first BAL) | 2 | 17/19 = 89.47% |
| 2-bit (starts 1 = weak N) | 1 (exit: counter 0 says N) | 1 (first BAL: 1 says N, counter goes to 2) | 2 | 89.47% |

**Derivation for nested.txt.** The inner `BNE` (instr 6) is T T T T N in each of the 4 outer
iterations: 20 executions, 16 taken. The outer `BNE` (instr 9) is T T T N: 4 executions, 3
taken. Together that is 24 branches, 19 taken and 5 not taken.

- Always-taken and BTFN (both branches are backward) miss only the 5 loop exits, so 19/24 =
  79.17%.
- **1-bit**: every time the inner loop is entered again, the entry still remembers the
  previous exit (N), so it misses the first T and also the exit. That is **2 per run of the
  inner loop**, 4 x 2 = 8. The outer branch misses its first T and its exit, 2. Total 10, so
  14/24 = 58.33%.
- **2-bit**: in the first inner run the counter goes 1 to 2 on a miss, then 3, 3, 3, and the
  exit misses and drops it to 2. In every later run the counter is 2, which still guesses T,
  so the first T is correct and only the exit misses. That is 2 + 1 + 1 + 1 = 5, plus 2 for
  the outer branch (first T and exit). Total 7, so 17/24 = 70.83%.
- In steady state the 2-bit predictor misses **once per loop execution** (the exit) and the
  1-bit predictor misses **twice** (the exit and the re-entry). That is the classic reason to
  use 2 bits: a single unusual outcome does not flip the prediction.

**counter.txt** has a bottom-tested loop, so its `BNE` is T x9 and then N. The mispredictions
are always 1, btfn 1, 1-bit 2 and 2-bit 2 out of 10. `./test.sh` also runs 5 copies of
nested.txt at once (NP = 4). All 5 get 1-bit 10 and 2-bit 7, including pid 5 on a reused
processor. Without the reset in `reset(p)`, pid 5 would see a warm 2-bit table (counters at
2) and report only 5 mispredictions. `./test.sh` ends with `24 passed, 0 failed`.

## Viva questions

1. **Why does BTFN do so well on sum_array but always-taken does not?** sum_array tests its
   loop at the top with a forward `BEQ .exit` that is almost never taken. BTFN guesses "not
   taken" for forward branches, while always-taken misses it 9 times. Compilers lay out loops
   so that backward branches are the loop branches.
2. **What does the 2-bit counter buy over 1 bit?** Hysteresis. It needs two misses in a row
   to change its prediction, so the single N at a loop exit does not make the next run of the
   loop mispredict its first iteration. In steady state that is 1 miss per inner loop run
   instead of 2.
3. **Why reset the table when a task is loaded, and why per processor?** The table is
   indexed by instruction index, and a different program has different branches at those
   indexes. A stale table would make the results depend on what ran before (pid 5 would get 5
   instead of 7). Each processor has its own predictor, like its own registers.
4. **What is aliasing?** It happens when the table has fewer entries than there are
   branches, so it is indexed by `index % BP_TABLE_SIZE` and two branches can share an
   entry and disturb each other. With 256 entries (one per instruction slot) it cannot
   happen here.
5. **Why count BAL at all?** The question says BAL is a branch that is always taken. Real
   CPUs know the direction of an unconditional branch at decode, but a dynamic predictor
   indexed by the PC still has to learn it once. That is why 1-bit and 2-bit each miss the
   first `BAL` in sum_array.
6. **Can a 2-bit predictor be worse than 1-bit?** Yes. Take the pattern T T N N T T N N.
   1-bit misses each change, 2 of every 4. The 2-bit counter goes 1, 2, 3, 2, 1 and misses
   3 of every 4 (the first T and both N). The pure alternation T N T N fools both
   completely. Patterns like these need a history-based (correlating, two-level) predictor.

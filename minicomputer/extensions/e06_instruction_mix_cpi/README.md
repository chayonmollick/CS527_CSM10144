# e06 — Instruction mix and CPI estimate

## Question (as it could appear in the test)

> Instrument the simulator to report the **instruction mix** of every task. Classify each
> executed instruction as integer ALU (add/sub/mov), multiply, divide, load, store, branch
> taken, branch not taken, vector arithmetic, vector load/store, print or halt. Using the
> latency table below (define it with `#define` in `config.h`), estimate the total number of
> cycles and the CPI of the task: ALU 1, MUL 3, DIV 10, LOAD 3, STORE 3, BRANCH 1 (+2 when
> taken), VECTOR 4, VLOAD/VSTORE 8, PRINT 1. When a task finishes, write
> `<program>_<pid>.stats` and print a one-line summary. Several tasks run at the same time.

## Solution idea

- The processor already counts every executed instruction (`exec_count`, `steps`). Add one
  more counter array, `class_count[NP][NUM_CLASSES]`. It is indexed by the processor id,
  like `Register[p]`, because up to NP tasks interleave (round robin) and each processor
  runs one task.
- `execute()` classifies the instruction (`instr_class()`) and increments its class. Whether
  a branch is taken depends on the flags only, and a branch never changes the flags, so the
  condition can be evaluated once at the top of `execute()`. The switch below then makes the
  same decision.
- `reset(p)` clears the counters, and the loader calls it whenever a new task is put on
  processor `p`.
- `finalize()` computes `cycles = sum(count x latency)` (a taken branch costs 1 + 2) and
  `CPI = cycles / instructions`, writes the `.stats` file and prints `[stats] ...`.
- The latencies are `#define`s in `config.h`. To change one, edit it and run `make`, which
  rebuilds every object because the Makefile makes them depend on all headers.

## Changes

| file | function | change |
|---|---|---|
| config.h | (global) | `LAT_*` latency `#define`s, `enum InstrClass` (one row per class) |
| processor.h | (declarations) | `extern class_count`, `CLASS_NAME`, `CLASS_LATENCY`; `instr_class()` |
| processor.cpp | (globals) | `class_count[NP][NUM_CLASSES]`, name table, latency table (taken = 1 + 2) |
| processor.cpp | `instr_class()` (new) | opcode (+ taken?) -> class |
| processor.cpp | `reset()` | clear `class_count[p]` |
| processor.cpp | `execute()` | classify and count every executed instruction |
| os.cpp | `write_stats_file()` (new) | write `<program>_<pid>.stats` (count, %, latency, cycles, CPI) |
| os.cpp | `finalize()` | call it; print `[stats] pid N (prog): I instructions, C cycles, CPI x.xx` |
| main.cpp, os.cpp | header comments | mention the `.stats` output |
| Makefile | `test`, `clean` | run the base tests on this build and `./test.sh`; delete `*.stats` |

`grep -n "EXT e06" *.cpp *.h Makefile` lists every change. The full diff is in `CHANGES.diff`.

## Run

```sh
cd minicomputer/extensions/e06_instruction_mix_cpi
make                                          # zero warnings
./test.sh                                     # e06 checks (33)
SIM=$PWD/sim ../../tests/run_tests.sh         # base regression tests (33)
mkdir -p /tmp/e06 && cp ../../examples/* mix.txt mix_data.byte /tmp/e06 && cd /tmp/e06
$OLDPWD/sim -b sum_array.txt sum_array_data.byte && cat sum_array_1.stats
```

`mix.txt` is a straight-line program with at least one instruction of every class. Its data
file `mix_data.byte` holds 0 at address 0 and the numbers 1..8 at 4..32.

## Expected output

```
[os] pid 1 (sum_array) finished on processor 0 after 71 instructions, 0 page fault(s) -> sum_array_data_1.out, sum_array_1.freq
[stats] pid 1 (sum_array): 71 instructions, 113 cycles, CPI 1.59 -> sum_array_1.stats
```
`sum_array_1.stats`:
```
Instruction mix of sum_array (pid 1)
class                 count   %instr  latency   cycles
alu                      40   56.34%        1       40
mul                       0    0.00%        3        0
div                       0    0.00%       10        0
load                     10   14.08%        3       30
store                     1    1.41%        3        3
branch_taken             10   14.08%        3       30
branch_not_taken          9   12.68%        1        9
vector                    0    0.00%        4        0
vector_mem                0    0.00%        8        0
print                     0    0.00%        1        0
halt                      1    1.41%        1        1
total                    71  100.00%               113
CPI = 113 cycles / 71 instructions = 1.59
(branch_taken latency = 1 + 2 taken penalty)
```

**Derivation for sum_array with N = 9.** The loop body runs 9 times. The compare and the
`BEQ` run 10 times: 9 times they fall through and the 10th time `BEQ` exits.

| idx | instruction | class | executions | cycles |
|---|---|---|---|---|
| 0-2 | `x1 = 0`, `x2 = 0`, `x3 = 0` | alu | 1 each | 3 |
| 3 | `x4 = [x3]` | load | 1 | 3 |
| 4 | `x15 = x1 - x4` | alu | 10 | 10 |
| 5 | `BEQ .exit` | not taken 9, taken 1 | 10 | 9x1 + 1x3 = 12 |
| 6 | `x3 = x3 + 4` | alu | 9 | 9 |
| 7 | `x6 = [x3]` | load | 9 | 27 |
| 8, 9 | `x2 = x2 + x6`, `x1 = x1 + 1` | alu | 9 each | 18 |
| 10 | `BAL .loopback` | taken | 9 | 27 |
| 11 | `[x3] = x2` | store | 1 | 3 |
| 12 | halt | halt | 1 | 1 |

That gives alu = 3 + 10 + 27 = 40, load = 10, store = 1, taken = 9 + 1 = 10, not taken = 9
and halt = 1, so there are **71** instructions, the same count the OS reports. The cycles
are 40 + 30 + 3 + 30 + 9 + 1 = **113**, and CPI = 113 / 71 = **1.59**.

**mix.txt.** `x8 = 99` is skipped by the taken `BEQ`, so it is not counted. The classes are
alu 5 (`x1 x2 x5 x7 x9`), mul 1, div 1, load 1, store 1, taken 1, not taken 1, vector 2
(`v2 v3`), vector_mem 2 (`v1 = [x2]`, `[64] = v3`), print 1 and halt 1. That is 17
instructions, and the cycles are 5 + 3 + 10 + 3 + 3 + 3 + 1 + 8 + 16 + 1 + 1 = **54**, so
CPI = 54 / 17 = **3.18**. The program also checks itself: [0] = 12x4 + 12/4 = 51, and
[64..92] = v1 + 3 v1 = 4 8 .. 32.

**fir.txt (N = 24, test.sh).** `x2 = N - 8 = 16`, and k = 0, 8 run the block, so 2
iterations. Before the loop there are 9 loads, 4 alu and 1 mul. Each iteration has 12 alu,
8 vector loads + 1 vector store, 15 vector ops (1 + 7x2), one `BGE` that is not taken and one
`BAL`. The exit adds 1 compare, the taken `BGE` and the halt. That gives alu 29, load 9,
mul 1, vector_mem 18, vector 30, taken 3, not taken 2 and halt 1: **93** instructions and
**335** cycles (CPI 3.60). In **counter.txt** the loop runs 10 times and the `BNE` is taken 9
times: alu 21, print 10, taken 9, not taken 1, store 1, halt 1, so 43 instructions and 63
cycles.

`./test.sh` ends with `33 passed, 0 failed`. It also runs 5 tasks at once (NP = 4), and
pid 5, which reuses a processor, gets exactly the single-run numbers.

## Viva questions

1. **Why is `class_count` indexed by processor, not global?** Up to NP tasks interleave on
   the scheduler, one per processor. A global counter would mix their instructions. The
   counters are cleared in `reset(p)` when the loader puts a new task on processor `p`.
2. **Why does a taken branch cost more than a not-taken one?** The target is known only
   after the branch is executed. The instructions already fetched after it (the fall-through
   path) are thrown away, which is the penalty. A not-taken branch just continues. This is
   the "predict not taken" model.
3. **Why is the CPI above 1 when most instructions take 1 cycle?** Loads and taken branches
   cost 3 cycles. In sum_array they are 20 of the 71 instructions but 60 of the 113 cycles.
4. **How do you get execution time from this?** Time = instructions x CPI x clock period.
   For example, 113 cycles at 1 GHz is 113 ns.
5. **Does the vector version of array_add pay off despite VECTOR 4 / VMEM 8?** One vector
   instruction replaces 8 scalar ones. Adding 8 elements with vectors costs 8 + 8 + 4 + 8 =
   28 cycles for 2 loads, 1 add and 1 store. The scalar version costs 8 x (3 + 3 + 1 + 3) =
   80 cycles, without counting the extra address updates and branches.
6. **Where is the halt counted, and what if a task fails?** The halt is an executed
   instruction, so it counts (class `halt`, 1 cycle), just as `exec_count` and `steps` count
   it. An instruction that fails is counted too, because counting happens before it runs.

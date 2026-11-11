# e08 — 5-stage pipeline: data hazards, stalls and cycles

## Question (as it could appear in the test)

> Model a classic 5-stage in-order pipeline (IF ID EX MEM WB) on the stream of instructions
> each task executes. Count the RAW data hazards (an instruction reads a register, x or v,
> including address registers of loads and stores, written by one of the previous two
> instructions), the load-use hazards and the taken branches (flush penalty 2 cycles). Report,
> per task, the number of cycles **without forwarding** (a reader waits until the writer's
> WB) and **with forwarding** (only a load-use stalls, 1 cycle), using
> `cycles = 4 + instructions + stalls + 2 x taken branches`. Give a small straight-line
> program and derive its stalls by hand.

## Solution idea

- `execute()` calls `pipeline_step(p, taken)` for every executed instruction. That function
  replays the instruction on two pipelines at once, one without and one with forwarding.
  The real simulation does not change.
- `pipe_operands()` lists the registers an instruction reads and writes. x registers are
  numbered 0..255 and v registers 256..287. `[xA] = xS` reads both xA and xS, `xD = [xA]`
  reads xA, and branches and halt read no register. The flags never stall, because they are
  forwarded to EX where the branch is resolved.
- **Timing model.** Each instruction has an ID cycle in which it reads its registers.
  Without stalls, instruction k is in ID in cycle k + 2. After a taken branch, 2 bubbles
  are inserted, because the 2 instructions fetched behind it are flushed.
  - Without forwarding, a register written by an instruction with ID cycle `D` can be read
    in ID at cycle `D + 3`, which is the writer's WB cycle (write in the first half of the
    cycle, read in the second). So a reader at distance 1 stalls 2 cycles and a reader at
    distance 2 stalls 1 cycle.
  - With forwarding, an ALU result reaches the next instruction's EX in time, so it never
    stalls. A load's value is only ready after MEM, so the reader must be in ID at `D + 2` or
    later: a load followed immediately by its user stalls 1 cycle (load-use).
  - An instruction is in ID at max(previous ID + 1 + bubbles, ready cycle of every register it
    reads). The difference is its stall.
- **Hazard count.** A read is a hazard when its last writer is 1 or 2 pipeline slots earlier.
  Bubbles count as slots, so a dependence across a taken branch is never a hazard. A
  distance-1 hazard on a loaded register is also a load-use hazard.
- All state is per processor and is reset in `reset(p)` at task load.
  `finalize()` writes `<program>_<pid>.pipe`, which has the totals, both cycle counts and a
  per-instruction log of the first 40 instructions (ID cycles and stalls in both pipelines),
  and prints `[pipe] ...`.

## Changes

| file | function | change |
|---|---|---|
| config.h | (global) | `PIPE_FILL 4`, `PIPE_BRANCH_PENALTY 2`, `PIPE_NOFWD_GAP 3`, `PIPE_LOAD_GAP 2`, `PIPE_LOG_ROWS 40` |
| processor.h | (declarations) | `struct PipeStats` (instructions, taken branches, d1/d2 hazards, load-use, stalls), `pipe_stats[NP]`, `pipe_log[NP]` |
| processor.cpp | (globals) | per-processor slot, next ID cycles, last writer slot / load flag / ready cycles of every register |
| processor.cpp | `reset()` | empty pipeline: first ID in cycle 2, no register written |
| processor.cpp | `pipe_operands()`, `reg_name()` (new) | registers read / written by an instruction |
| processor.cpp | `pipeline_step()` (new) | hazards, stalls in both pipelines, bubbles after a taken branch, log row |
| processor.cpp | `execute()` | call `pipeline_step(p, taken)` for every executed instruction |
| os.cpp | `write_pipeline_file()` (new) | write `<program>_<pid>.pipe` |
| os.cpp | `finalize()` | call it; print `[pipe] pid N (prog): ... cycles X without forwarding, Y with forwarding` |
| main.cpp, os.cpp | header comments | mention the `.pipe` output |
| Makefile | `test`, `clean` | run the base tests on this build and `./test.sh`; delete `*.pipe` |

`grep -n "EXT e08" *.cpp *.h Makefile` lists every change. The full diff is in `CHANGES.diff`.

## Run

```sh
cd minicomputer/extensions/e08_pipeline_hazards
make                                          # zero warnings
./test.sh                                     # e08 checks (22)
SIM=$PWD/sim ../../tests/run_tests.sh         # base regression tests (33)
mkdir -p /tmp/e08 && cp ../../examples/* hazards.txt hazards_data.byte /tmp/e08 && cd /tmp/e08
$OLDPWD/sim -b hazards.txt hazards_data.byte && cat hazards_1.pipe
```

`hazards.txt` is the straight-line test program, and `hazards_data.byte` holds 10 at
address 0.

## Expected output

```
[pipe] pid 1 (hazards): 11 instructions, RAW hazards 6 (load-use 2), 0 taken branches, cycles 24 without forwarding, 17 with forwarding -> hazards_1.pipe
```
`hazards_1.pipe`:
```
5-stage pipeline (IF ID EX MEM WB) for hazards (pid 1)
instructions         11
taken_branches        0   (2 flushed cycles each)
raw_hazards_d1        4   (reader right after the writer)
raw_hazards_d2        2   (one instruction in between)
load_use              2   (d1 hazards on a loaded register)

                     stalls  penalty  cycles    CPI
without_forwarding        9        0      24   2.18
with_forwarding           2        0      17   1.55
cycles = 4 + instructions + stalls + 2 x taken branches

first 40 executed instructions (ID = cycle in which the instruction reads its registers)
 step instr  instruction        RAW hazards                ID stall  ID(fwd) stall
    1     0  x1 = [0]                                       2     0        2     0
    2     1  x2 = x1 + 5        x1:d1(load)                 5     2        4     1
    3     2  x3 = 7                                         6     0        5     0
    4     3  x4 = x2 + x3       x2:d2 x3:d1                 9     2        6     0
    5     4  x5 = x1 * 2                                   10     0        7     0
    6     5  x6 = 9                                        11     0        8     0
    7     6  x7 = x4 - x5       x5:d2                      13     1        9     0
    8     7  [4] = x7           x7:d1                      16     2       10     0
    9     8  x8 = [4]                                      17     0       11     0
   10     9  [8] = x8           x8:d1(load)                20     2       13     1
   11    10  halt                                          21     0       14     0
```

**Hand derivation for hazards.txt** (10 instructions + halt = 11, no branches):

| I | instruction | reads (writer, distance) | no fwd: ID / stall | fwd: ID / stall |
|---|---|---|---|---|
| 0 | `x1 = [0]` | none | 2 / 0, x1 ready at 5 | 2 / 0, x1 (load) ready at 4 |
| 1 | `x2 = x1 + 5` | x1 (I0, d1, load-use) | wants 3, x1 ready 5: **5 / 2** | wants 3, ready 4: **4 / 1** |
| 2 | `x3 = 7` | none | 6 / 0 | 5 / 0 |
| 3 | `x4 = x2 + x3` | x2 (I1, d2), x3 (I2, d1) | wants 7, x3 ready 6+3 = 9: **9 / 2** | 6 / 0 |
| 4 | `x5 = x1 * 2` | x1 (I0, d4: no hazard) | 10 / 0 | 7 / 0 |
| 5 | `x6 = 9` | none | 11 / 0 | 8 / 0 |
| 6 | `x7 = x4 - x5` | x4 (d3: none), x5 (I4, d2) | wants 12, x5 ready 10+3 = 13: **13 / 1** | 9 / 0 |
| 7 | `[4] = x7` | x7 (I6, d1) | wants 14, ready 16: **16 / 2** | 10 / 0 |
| 8 | `x8 = [4]` | none | 17 / 0 | 11 / 0 |
| 9 | `[8] = x8` | x8 (I8, d1, load-use) | wants 18, ready 20: **20 / 2** | wants 12, ready 13: **13 / 1** |
| 10 | halt | none | 21 / 0 | 14 / 0 |

The hazards are d1 = 4 (I0 to I1, I2 to I3, I6 to I7, I8 to I9), d2 = 2 (I1 to I3, I4 to I6)
and load-use = 2. Without forwarding the stalls are 2 + 2 + 1 + 2 + 2 = 9, so the cycles are
4 + 11 + 9 = **24** (the last WB is at ID 21 + 3 = 24). With forwarding the stalls are 1 + 1
= 2, so the cycles are 4 + 11 + 2 = **17**. In I3 the d2 hazard on x2 is hidden by the
2-cycle stall for x3: the stalls overlap, they do not add up.

**sum_array (N = 9).** Each iteration has two hazards: I6 to I7 (`x3 = x3 + 4`, then
`x6 = [x3]`, d1) and I7 to I8 (`x6 = [x3]`, then `x2 = x2 + x6`, d1, load-use). Before the
loop, I2 to I3 (x3, d1) and I3 to the first I4 (x4 loaded, d1, load-use) add two more. The
`x1` written by I9 is read by I4 only after the taken `BAL` and its 2 bubbles, so that is
distance 4 and no hazard. That gives d1 = 2 + 9 x 2 = 20, load-use = 1 + 9 = 10, and d2 = 0.
Without forwarding every d1 costs 2 stalls, so 40 stalls. With forwarding each load-use
costs 1, so 10 stalls. There are 10 taken branches (9 `BAL` + the final `BEQ`), so a penalty
of 20. Cycles are 4 + 71 + 40 + 20 = **135** without forwarding and 4 + 71 + 10 + 20 =
**105** with forwarding.

**array_add (N = 16, 2 iterations, test.sh).** 31 instructions, 3 taken branches. Before the
loop there are x1 (d2 from a load), x2 (d2), x3 (d1 + d2), x4 (d1) and x6 (d1). In each
iteration there is v1 (d2), v2 (d1, load-use) and v3 to the vector store (d1). That gives
d1 = 7, d2 = 5 and load-use = 2. The stalls are 15 without forwarding (1 + 2 + 2 + 2 before
the loop, then 2 + 2 per iteration) and 2 with forwarding. Cycles are 4 + 31 + 15 + 6 =
**56** and 4 + 31 + 2 + 6 = **43**.

`./test.sh` ends with `22 passed, 0 failed`. It also checks single rows of the log, the
formula, and that 3 different tasks, or 5 copies of the same one, run at once give the
single-run numbers.

## Viva questions

1. **Why does a distance-1 dependence cost 2 stalls without forwarding, but distance 2 only
   1?** The writer's WB is 3 cycles after its ID. The register file writes in the first half
   of the cycle and reads in the second, so a reader can be in ID in that same cycle. The
   next instruction is naturally 1 cycle behind, so it waits 2 cycles. The one after it is 2
   cycles behind, so it waits 1.
2. **Why does forwarding not remove the load-use stall?** A load has its value only at the
   end of MEM. The next instruction needs it at the start of its EX, which is the same cycle
   as the load's MEM, one cycle too early. A compiler can hide it by scheduling an
   independent instruction between them.
3. **Why is a dependence across a taken branch never a hazard here?** The taken branch
   flushes 2 instructions, which inserts 2 bubbles. The reader is then at least 4 slots after
   the writer, so the value has already been written back.
4. **Where does the `4` in the formula come from?** The pipeline fill. The first
   instruction needs 5 cycles (IF ID EX MEM WB), and every later one finishes 1 cycle after
   the previous one, so n instructions take n + 4 cycles plus stalls and bubbles.
5. **Why do the flags not cause stalls after `x15 = x1 - x4; BEQ`?** The flags are produced
   at the end of the SUB's EX, and the branch uses them in its own EX one cycle later. That
   is the same path as ALU forwarding. Without any forwarding of the flags, a compare + branch
   pair would stall like any d1 dependence.
6. **How would you reduce the branch penalty?** Resolve branches in ID (penalty 1), predict
   them (see e07, where only mispredictions pay), or use delayed branches.

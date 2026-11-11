# e09 — Line profile: cycles per line, hot spots and dead code

## Question (as it could appear in the test)

> Extend the Lab 6 task 1 instrumentation. Besides the number of executions of each source
> line, estimate the **cycles spent on each line**, using a latency table defined in
> `config.h` (ALU 1, MUL 3, DIV 10, LOAD 3, STORE 3, BRANCH 1 + 2 when taken, VECTOR 4,
> VLOAD/VSTORE 8, PRINT 1). For every task write `<program>_<pid>.prof` with, per line:
> executions, cycles and the percentage of all cycles. Then list the **top 3 hottest lines**
> and the lines that **never executed** (dead code, for example code after an unconditional
> branch). The original `<program>_<pid>.freq` file must not change.

## Solution idea

- The processor already keeps `exec_count[p][i]`, the executions of instruction i. Add
  `exec_cycles[p][i]` next to it. On every execution, `execute()` adds that instruction's
  latency through `latency(opcode, taken)`. A taken branch adds 1 + 2, a branch that is not
  taken adds 1. Its outcome can be evaluated before the switch because a branch does not
  change the flags.
- The compiler already maps instruction to source line (`line_of`), so `write_profile_file()`
  adds counts and cycles per source line, the same way `write_frequency_file()` does. Labels
  are shown without numbers, and comment-only or blank lines are left out, as in `.freq`.
  The halt the compiler appends is shown as a `-` row so that the percentages add up to 100.
- **Top 3**: copy the rows, `std::stable_sort` them by cycles (descending), and print the
  first 3 with cycles > 0. Because the sort is stable, equal cycles keep line order.
- **Never executed**: lines with code whose count is 0.
- A task started from a `.byte` file has no source, so the profile is made per instruction
  using the disassembly.
- `write_frequency_file()` is untouched. `reset(p)` clears `exec_cycles[p]` when a task is
  loaded.

## Changes

| file | function | change |
|---|---|---|
| config.h | (global) | latency `#define`s `LAT_ALU` .. `LAT_HALT`, `LAT_TAKEN_PENALTY`; `PROF_TOP 3` |
| processor.h | (declarations) | `extern long exec_cycles[NP][MAX_INSTRUCTIONS]` |
| processor.cpp | (globals) | `exec_cycles` |
| processor.cpp | `latency()` (new) | opcode (+ taken?) -> cycles of one execution |
| processor.cpp | `reset()` | clear `exec_cycles[p]` |
| processor.cpp | `execute()` | add the latency of each execution to `exec_cycles[p][index]` |
| os.cpp | `struct ProfRow`, `strip()`, `write_profile_file()` (new) | write `<program>_<pid>.prof`: table, top 3, never executed |
| os.cpp | `finalize()` | call it; print `[prof] pid N (prog): C cycles, hottest line L (x%), D never executed` |
| main.cpp, os.cpp | header comments | mention the `.prof` output |
| Makefile | `test`, `clean` | run the base tests on this build and `./test.sh`; delete `*.prof` |

`grep -n "EXT e09" *.cpp *.h Makefile` lists every change. The full diff is in `CHANGES.diff`.

## Run

```sh
cd minicomputer/extensions/e09_line_profile_hotspots
make                                          # zero warnings
./test.sh                                     # e09 checks (32)
SIM=$PWD/sim ../../tests/run_tests.sh         # base regression tests (33)
mkdir -p /tmp/e09 && cp ../../examples/* profile.txt profile_data.byte /tmp/e09 && cd /tmp/e09
$OLDPWD/sim -b profile.txt profile_data.byte && cat profile_1.prof
```

`profile.txt` is a loop of 6 iterations with a DIV in it and two lines of dead code after a
`BAL`. `profile_data.byte` holds the divisor 2 at address 0, and the result is stored at 4.

## Expected output

```
[prof] pid 1 (profile): 121 cycles, hottest line 7 (49.59%), 2 never executed -> profile_1.prof
```
`profile_1.prof`:
```
Line profile of profile (pid 1): 41 instructions executed, 121 estimated cycles
 line    count   cycles  %cycles  source
    3        1        1     0.83  x1 = 0            % i
    4        1        1     0.83  x2 = 0            % sum
                                  .loop
    6        6       18    14.88  x5 = [0]          % d             load:  6 x 3 = 18 cycles
    7        6       60    49.59  x3 = x1 / x5      % i / d         div:   6 x 10 = 60 cycles (hottest)
    8        6        6     4.96  x2 = x2 + x3
    9        6        6     4.96  x1 = x1 + 1
   10        6        6     4.96  x4 = x1 - 6
   11        6       16    13.22  BNE .loop         % taken 5 times (3 cycles), not taken once (1 cycle) = 16 cycles
   12        1        3     2.48  BAL .done
   13        0        0     0.00  x9 = 99           % dead code: after an unconditional branch
   14        0        0     0.00  x9 = x9 * 2       % dead code
                                  .done
   16        1        3     2.48  [4] = x2
    -        1        1     0.83  (halt at the end of the program)

top 3 hottest lines (most cycles first):
rank  line   cycles  %cycles    count  source
   1     7       60    49.59        6  x3 = x1 / x5      % i / d         div:   6 x 10 = 60 cycles (hottest)
   2     6       18    14.88        6  x5 = [0]          % d             load:  6 x 3 = 18 cycles
   3    11       16    13.22        6  BNE .loop         % taken 5 times (3 cycles), not taken once (1 cycle) = 16 cycles

never executed (dead code): 2 line(s)
   13  x9 = 99           % dead code: after an unconditional branch
   14  x9 = x9 * 2       % dead code
```
`profile_1.freq` is in the Lab 6 format, unchanged:
```
1	x1 = 0            % i
1	x2 = 0            % sum
.loop
6	x5 = [0]          % d             load:  6 x 3 = 18 cycles
6	x3 = x1 / x5      % i / d         div:   6 x 10 = 60 cycles (hottest)
6	x2 = x2 + x3
6	x1 = x1 + 1
6	x4 = x1 - 6
6	BNE .loop         % taken 5 times (3 cycles), not taken once (1 cycle) = 16 cycles
1	BAL .done
0	x9 = 99           % dead code: after an unconditional branch
0	x9 = x9 * 2       % dead code
.done
1	[4] = x2
```

**Derivation for profile.txt.** i runs from 0 to 5, so the body executes 6 times. The `BNE`
is taken 5 times and falls through once.

| line | statement | count | cycles |
|---|---|---|---|
| 3, 4 | `x1 = 0`, `x2 = 0` | 1 each | 1 each |
| 6 | `x5 = [0]` (load) | 6 | 6 x 3 = 18 |
| 7 | `x3 = x1 / x5` (div) | 6 | 6 x 10 = **60** |
| 8, 9, 10 | add, add, sub | 6 each | 6 each |
| 11 | `BNE .loop` | 6 | 5 x 3 + 1 x 1 = **16** |
| 12 | `BAL .done` | 1 | 3 |
| 13, 14 | dead code | 0 | 0 |
| 16 | `[4] = x2` (store) | 1 | 3 |
| - | halt | 1 | 1 |

That adds up to 41 instructions and 1+1+18+60+6+6+6+16+3+3+1 = **121** cycles. The top 3 are
line 7 (60/121 = 49.59%), line 6 (18/121 = 14.88%) and line 11 (16/121 = 13.22%). Lines 13
and 14 are never executed. The result is [4] = 0+0+1+1+2+2 = 6.

**sum_array (N = 9).** These are the same counts as Lab 6. Line 9 (load) gets 9 x 3 = 27,
line 12 (`BAL`) 9 x 3 = 27, line 7 (`BEQ`) 9 x 1 + 1 x 3 = 12, and line 6 10 x 1 = 10, for a
total of 113, the same as e06. Lines 9 and 12 tie, and the stable sort keeps line order:
```
top 3 hottest lines (most cycles first):
rank  line   cycles  %cycles    count  source
   1     9       27    23.89        9  x6 =  [x3]       % load the array data
   2    12       27    23.89        9  BAL .loopback
   3     7       12    10.62       10  BEQ .exit

never executed (dead code): 0 line(s)
```

`./test.sh` ends with `32 passed, 0 failed`. It also checks that the `.freq` of
sum_array_print still matches the handout (`1,1,1,1,.loopback,10,10,9,...`), the
per-instruction profile of a precompiled `.byte`, and that 5 tasks at once (NP = 4) give
the single-run numbers.

## Viva questions

1. **Why count cycles per instruction in the processor instead of multiplying counts by
   latencies at the end?** A branch's cost depends on its outcome, and the same branch is
   taken some times and not others (line 11 is 5 x 3 + 1 x 1). Only the processor knows each
   outcome. For every other instruction, count x latency would give the same result.
2. **Is the most executed line also the hottest?** No. The loop lines all run 6 times, but
   the DIV line uses 49.59% of the time. The profile shows where to optimise. Here you could
   replace `/ 2` with a shift, or move the loop-invariant load `x5 = [0]` out of the loop,
   which saves 15 cycles.
3. **"Never executed" and "dead code": same thing?** Not exactly. The profile is dynamic: it
   lists lines that did not run on this input. Lines 13 and 14 can never run because they
   follow a `BAL` and no label points at them, so they are truly unreachable. A line skipped
   by a conditional branch may run with other data (that is a coverage gap, not dead code).
4. **How does the profile find the source text?** The compiler records `line_of[i]` (Lab 6
   hint: keep the mapping between each byte-code instruction and its source line). The
   processor never re-parses the source.
5. **Why do the percentages add up to 100 only with the halt row?** The halt the compiler
   appends is an executed instruction (1 cycle) but has no source line, so it gets its own
   `-` row.
6. **What changes for a program run from a `.byte` file?** There is no source, so the rows
   are per instruction index with the disassembly, exactly as the base `.freq` falls back to
   disassembly.

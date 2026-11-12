# Q060 — Most executed instruction of each task

## Question
When a task ends, report its most executed instruction: the instruction index, how many
times it ran, its disassembly and its source line. If several instructions tie, report
the first one. Test it with a program whose hottest instruction is unique, and with
`sum_array`.

## Answer
The Lab 6 data already holds everything: `exec_count[p][i]` is the number of executions
of instruction i, `prog.line_of[i]` its source line, and `disassemble()` gives its text.
`print_hottest()` in `os.cpp` scans the program's instructions for the largest count
(strictly greater, so the first one wins a tie) and prints:

```
[hot] pid 1 (countdown): instruction 3 (BEQ +4) executed 6 times, line 6: BEQ .done
```

A program started from a `.byte` file has no source, so it prints `no source line`.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `print_hottest` (new) | find the largest `exec_count[p][i]`; print index, count, disassembly, source line |
| `os.cpp` | `finalize` | call `print_hottest(p, t)` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 16 lines by hand.

## Test
`./test.sh`: `countdown.txt` tests at the top of its loop (`BEQ .done`), so the test runs
6 times and the body only 5: instruction 3, line 6. In sum_array the compare and the
`BEQ` both run 10 times; the first, instruction 4 `x15 = x1 - x4`, is reported. In
array_add the loop test (3 times) beats the body (2 times). The precompiled
`program.byte` of countdown prints `no source line`, and sum_array still stores 45.

## Viva
- **Why does a loop test run once more than the body?** It also runs the last time, when it sends control out of the loop.
- **Why are there often ties?** Every instruction of a straight loop body runs the same number of times; only a loop test or a label reached from several places can run more.
- **Why not read the source file again?** The compiler kept every line in `prog.source` and the map `line_of`; the Lab 6 handout says the simulator must not parse the source again.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `countdown.txt`, `countdown_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

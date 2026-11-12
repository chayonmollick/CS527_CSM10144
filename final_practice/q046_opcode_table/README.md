# Q046 — Table of executed instructions per opcode

## Question
Instrument the simulator so that, when a task ends, it prints a table of how many
instructions of each opcode the task executed (opcode in hex, a short name, the count)
and the total. Several tasks run at the same time, so the counts must be kept per task.
Check the table of `sum_array` by hand.

## Answer
This is recipe C of the guide (a per-task counter). The processor gets one more counter
array, `op_count[NP][256]`: one row per processor, like `exec_count`, because each
processor runs one task until it ends. `reset(p)` clears the row when the loader puts a
new task on `p`, and `execute()` adds 1 to `op_count[p][opcode]` for every instruction.
When the task ends, `finalize()` can still read the row, so it prints the non-zero
entries, with names from a small table `OP_NAME[]` (`?` for an opcode without a name),
and their sum, which must equal the "after N instructions" of the `[os]` line:

```
[opcodes] pid 1 (sum_array): executed instructions per opcode
    opcode  name      count
    00      halt          1
    01      add           9
    02      sub          10
    05      load         10
    06      store         1
    09      addi         18
    0F      movc          3
    10      beq          10
    1E      bal           9
    total                71
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern long op_count[NP][256];` |
| `processor.cpp` | (globals), `reset` | define `op_count`; clear row `p` |
| `processor.cpp` | `execute` | `op_count[p][opcode]++` |
| `os.cpp` | `OP_NAME[]`, `print_opcode_table` (new) | opcode names; print the non-zero rows and the total |
| `os.cpp` | `finalize` | call `print_opcode_table(p, t)` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 26 lines by hand.

## Test
`./test.sh`: runs `sum_array` and `array_add` at the same time and reads both tables.
sum_array: 3 `movc`, 10 loads, 10 `sub` and 10 `BEQ` (the loop test runs once more than
the body), 18 `addi`, 9 `add`, 9 `BAL`, 1 store, 1 halt: 9 opcodes, total 71. array_add:
4 vector loads, 2 vector adds, 2 vector stores, 3 `BGE`, total 31. Then checks that
`sum_array` still stores 45.

## Viva
- **Why `[NP][256]` and not one array of 256?** Up to 4 tasks run interleaved (round robin), one per processor; one shared array would mix their counts. `reset()` clears the processor's row for the next task.
- **Why 256 columns?** The opcode is one byte, so every value 0..255 can be counted, even an unknown opcode, without a range check.
- **How do you check the table?** Every executed instruction adds 1 to exactly one opcode, so the total must equal `steps[p]`, the instruction count in the `[os]` line.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

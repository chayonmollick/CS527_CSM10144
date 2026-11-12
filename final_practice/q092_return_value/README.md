# Q092 — Return value: x1 to `<program>_<pid>.ret`

## Question
Treat register x1 as a task's return value. When a task halts, the OS must write the
value of its x1, in decimal, to a file `<program>_<pid>.ret`, and print
`pid N returned V`. Negative values must come out signed. A task stopped by an error
returns nothing. Show two different programs, two copies of one program and a task
that fails.

## Answer
- **Where:** every task ends in `finalize(p, t)` (via `finish_task`), which already writes
  `<data>_<pid>.out` and `<program>_<pid>.freq`. At that point the task's registers are
  still in `Register[p]`, so x1 is `Register[p][1]`. `reset()` clears them only when the
  next task is loaded on p.
- **What:** after the usual `[os] … finished` line, if `!t.failed`:
  1. Build the name as `.freq` does: `t.prog.name + "_" + pid + ".ret"`. The pid in
     the name keeps two copies of the same program apart.
  2. `fprintf(f, "%d\n", x1)`. `%d` of the `int` register prints -7 as `-7`. A word in a
     `.out` file would be `F9 FF FF FF` instead.
  3. Print `pid N returned V`.
- **Failed tasks:** `failed[p]` is set by `processor_error()` (division by zero, bad
  address, …). Such a task did not halt, so its x1 means nothing and no `.ret` is
  written. The existing message already says `STOPPED WITH AN ERROR`.
- **Order in the test:** `negative` (3 instructions) ends in round 1. `fact` (18
  instructions) ends in round 2, pid 1 before pid 4. So the messages come as pid 2, 1, 4.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `finalize` | if the task did not fail: write `Register[p][1]` to `<program>_<pid>.ret` and print `pid N returned V` |

## Test
`./test.sh` runs `fact` (5! in x1), `negative` (x1 = 3 - 10), `fails` (divides by
zero) and `fact` again:
- **Exit code** 1, because pid 3 failed.
- **Files:** `fact_1.ret` = 120, `negative_2.ret` = -7, `fact_4.ret` = 120; there is no `fails_3.ret`.
- **Messages:** `pid 2 returned -7`, `pid 1 returned 120`, `pid 4 returned 120`, in that order.
- **Old programs:** `sum_array` still gives 45 and now also returns 9, its final index in x1.

## Viva
- **How do real systems return a value?** By convention, through a register: x86-64 `rax`, ARM `r0`/`x0`, RISC-V `a0`. On `exit(code)` the kernel keeps the code in the process table until the parent collects it with `wait()`.
- **Why read x1 in `finalize` and not later?** The processor's registers belong to the next task once `loader()` calls `reset(p)`. `finalize` is the last moment the task's state is still there. With a PCB, the code would be saved in the PCB instead.
- **Why one file per pid?** Several copies of the same program run at once. With `<program>.ret` they would overwrite each other, the same reason `.out` and `.freq` carry the pid.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `fact.txt`, `fails.txt`, `negative.txt`, `ret_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

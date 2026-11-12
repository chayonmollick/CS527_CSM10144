# Q072 — Read-only data range (`.readonly <start> <end>`)

## Question
Programs often keep constants in their data file. Add a directive `.readonly <start> <end>`
to the language: data bytes `start..end` of the program are read-only. Loads from the range
work as before, but a store that touches any byte of it stops the task with "protection
fault". The directive generates no instruction. Write a program that reads three
constants, stores the sum after them, and then tries to overwrite a constant.

## Answer
Three parts, following the path of the information: compiler → loader → processor.
1. **Compiler** (`compile_file`, pass 1): a line whose first word is `.readonly` is parsed
   into `prog.ro_start`, `prog.ro_end` (new fields of `Program`; the default 0, -1 means
   "none"). A bad range is a compile error. The line is skipped before the label check,
   which would reject it as a bad label. Pass 2 already skips every line that starts with
   `.`, so the directive produces no instruction (and no `.freq` line).
2. **Loader** (`initialize`): copy the range into per-processor registers
   `ro_start[p]`, `ro_end[p]`, like PTBR, because a task keeps its processor.
3. **Processor** (`store_word`): before writing, refuse a store whose 4 bytes
   `addr..addr+3` overlap the range:
   ```c
   if (ro_start[proc_id] <= ro_end[proc_id] && addr <= ro_end[proc_id] && addr + 3 >= ro_start[proc_id])
       processor_error("protection fault: store to %d..%d overlaps the read-only range %d..%d", ...);
   ```
   The check is done before any byte is written, so a refused store changes nothing.
   Every store goes through `store_word`: `[x] =`, `[c] =` and each lane of a vector store.

**consts.txt** (`.readonly 0 11`; data 10, 20, 30, 0, 0):
- It reads all three constants and stores 60 at 12 (bytes 12..15, outside the range: allowed).
- `[8] = x4` (instruction 6) faults, so the task stops after 7 instructions.
- The constant at 8 stays 30, and `[16]` is never written.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.h` | `struct Program` | `int ro_start = 0, ro_end = -1` |
| `compiler.cpp` | `compile_file` (pass 1) | parse `.readonly <start> <end>`, check `0 <= start <= end < 4096`, `continue` |
| `processor.h/.cpp` | (globals) | `int ro_start[NP], ro_end[NP]` |
| `os.cpp` | `initialize` | `ro_start[p] = t.prog.ro_start; ro_end[p] = t.prog.ro_end;` |
| `processor.cpp` | `store_word` | overlap check → "protection fault", return false |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` checks:
- **Listing:** `-c consts.txt` has 9 instructions, so the directive is not one.
- **The fault:** the message at instruction 6, the stop after 7 instructions, and the
  output 60 / 30 / 0 at 12 / 8 / 16. `sum_array` in the same run still gives 45.
- **Partial overlap:** a generated program storing at 10 (bytes 10..13 vs 0..11) also faults.
- **Bad range:** `.readonly 20 10` is a compile error.

`sum_array` alone gives 45.

## Viva
- **Why check in `store_word` and not in `getPhysicalAddress`?** The MMU does not know whether an access is a read or a write (it has no `isWrite` parameter). `store_word` does, and the range is in logical addresses, which `store_word` has before translation.
- **How does real hardware do it?** With a read-only bit in the page-table entry, per page, not per byte range: the linker puts constants in a `.rodata` section, which is mapped read-only. A write raises a protection fault (SIGSEGV). See `extensions/e14_pte_flags_protection` for the bit version.
- **Why per-processor registers?** The check runs on every store, so it must be cheap and must not search the process table. It is like a base/limit register pair, loaded at task start like PTBR.
- **What does a `.byte` program get?** No source means no directive, so `ro_start = 0, ro_end = -1`: nothing is read-only, as before.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `consts.txt`, `consts_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

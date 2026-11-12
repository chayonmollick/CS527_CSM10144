# Q037 — `printd` in decimal, and `print` on the screen

## Question
The Lab 4 statement `print xR` writes `Process id: <pid>   x<r> : <hex>` to `log.txt` only.
Add a statement `printd xR` that prints the register in signed decimal (`-7` rather than
`FFFFFFF9`). Make both `print` and `printd` also show their line on the screen, so you can
follow a program while it runs, without changing the format `print` writes to `log.txt`.
Give the encoding of `printd`.

## Answer
- **Encoding `88 0 0 R`:** this is PRINT (`08 0 0 R`) + 0x80, with the same field layout.
  The format is fixed at compile time, so the processor only has to look at the opcode.
- **Compiler:** `printd` goes through the same branch of `encode_statement()` as `print`.
  Only the opcode differs, and so does the usage message.
- **Processor:** one case handles both opcodes. It builds the line once, with `%08X` for
  `print` (unchanged) or `%d` for `printd`. It writes that line to `log.txt`, as before,
  and also to `stdout`. `%d` of the `int` register shows the sign, so 0xFFFFFFF9 prints as
  −7.
- **Screen order:** the `[os] ...` messages also go to `stdout`, so the print lines show
  up between "loaded" and "finished" in the order they ran. `log.txt` gets exactly the
  same lines.
- **Compatibility:** the hex lines in `log.txt` are byte-for-byte the same, so the Lab 4
  regression checks, which count log lines and read the pid column, still pass.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_PRINTD = 0x88`: `printd xR` |
| `compiler.cpp` | `encode_statement` | `printd xR` → `88 0 0 R`, in the same branch as `print` |
| `compiler.cpp` | `disassemble` | `printd x2` |
| `processor.cpp` | `execute` | `case OP_PRINT: case OP_PRINTD:` format once (`%08X` or `%d`), then write to `log.txt` and to `stdout` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `printd.txt`. It checks:
- in `log.txt`: `print x1` = `000000C8` (unchanged), `printd` = `200`, `-7` and `65025`,
  and `print x2` = `FFFFFFF9`;
- that the screen (`run.txt`) shows the same 5 lines in the same order;
- the encoding `88 00 00 02`;
- that `sum_array` still works.

## Viva
- **Why does the same register print as −7 and as FFFFFFF9?** It is the same 32 bits. `%X` prints them as an unsigned hex number, and `%d` reads them as a two's-complement signed number.
- **Could `printd` reuse opcode 08?** Yes: PRINT leaves two bytes unused (`08 0 0 R`), so `08 0 1 R` could mean "decimal". Real ISAs use spare fields like this (function bits). A separate opcode keeps the decoder and the disassembler simpler.
- **Why `fflush` the log after every line?** A file is fully buffered, and the flush makes each line visible at once (`tail -f log.txt`) even if the simulator is killed. `stdout` on a terminal is line-buffered already.
- **With 4 tasks, what does the screen show?** Their lines interleave by time slice (two prints per slice in `counter.txt`), exactly as in `log.txt`. The pid on each line tells them apart.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `printd.txt`, `printd_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

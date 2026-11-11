# e10 — Register dump and per-task execution trace

## Question (as it could appear in the test)

> (a) When a task finishes, write `<program>_<pid>.regs` with the final processor state of
> that task: PC, the flags N Z C V, every **non-zero** x register (decimal and hex) and every
> non-zero vector register (all 8 lanes).
> (b) Add a command-line option `-T`. With it, every task writes `<program>_<pid>.trace`
> with one line per executed instruction: step number, instruction index, the 4 instruction
> bytes, the disassembly, and the new value of the destination register. Several tasks run at
> the same time, so their traces must not mix. Show the register files of complex_mul and
> det3x3 and check them by hand.

## Solution idea

- **(a)** Registers are already per processor (`Register[p]`, `VRegister[p]`, `PC[p]`,
  `FlagN[p]` ...). `finalize()` runs before the processor is given to another task, so it can
  still read them. `write_register_file()` prints PC, the flags, each x register `!= 0` (as
  `%d` and `0x%08X`), and each vector register that has a non-zero lane (all 8 lanes).
- **(b)** `main()` sets `trace_files = 1` for `-T`. The loader opens one file per task,
  `trace_fd[p]`, because the task runs on processor p. `process_instructions()` calls
  `write_trace_line(p)` after every `execute()`, and `finalize()` closes the file. Since each
  processor writes to its own file, round-robin interleaving cannot mix the traces.
- What the trace line shows depends on the instruction. If it writes an x register:
  `xD = value (hex)`, plus `NZCV=` for + and -. If it writes a v register: the 8 lanes. A
  store shows `[address] = value`. A branch shows `taken -> instruction k` or `not taken`
  (a branch never changes the flags, so the condition can be evaluated again afterwards).
  `print` shows the logged value, `halt` shows `end of task`, and an instruction that failed
  shows `error: the task stopped here`.
- The existing `-t` (all tasks interleaved on stderr) is unchanged.

## Changes

| file | function | change |
|---|---|---|
| main.cpp | `main()`, `usage()` | new option `-T` sets `trace_files = 1`; usage text and header comment |
| processor.h | (declarations) | `extern int trace_files; extern FILE *trace_fd[NP];` |
| processor.cpp | (globals) | `trace_files`, `trace_fd[NP]`; `#include <string>` |
| processor.cpp | `write_trace_line()` (new) | one line: step, index, 4 bytes, disassembly, result |
| processor.cpp | `process_instructions()` | after `execute()`: `if (trace_fd[p]) write_trace_line(p);` |
| os.cpp | `loader()` | with `-T`: open `<program>_<pid>.trace` for the task, write a 2-line `#` header |
| os.cpp | `write_register_file()` (new) | write `<program>_<pid>.regs` |
| os.cpp | `finalize()` | write `.regs`, close the trace, print `[regs] pid N (prog): X x and V vector register(s) not zero -> ...` |
| os.cpp | header comment | mention `.regs` / `.trace` |
| Makefile | `test`, `clean` | run the base tests on this build and `./test.sh`; delete `*.regs *.trace` |

`grep -n "EXT e10" *.cpp *.h Makefile` lists every change. The full diff is in `CHANGES.diff`.

## Run

```sh
cd minicomputer/extensions/e10_register_dump_trace
make                                          # zero warnings
./test.sh                                     # e10 checks (40)
SIM=$PWD/sim ../../tests/run_tests.sh         # base regression tests (33)
mkdir -p /tmp/e10 && cp ../../examples/* /tmp/e10 && cd /tmp/e10
$OLDPWD/sim -b -T complex_mul.txt complex_mul_data.byte det3x3.txt det3x3_data.byte
cat complex_mul_1.regs complex_mul_1.trace det3x3_2.regs
```

## Expected output

```
[regs] pid 1 (complex_mul): 10 x and 0 vector register(s) not zero -> complex_mul_1.regs, complex_mul_1.trace
[regs] pid 2 (det3x3): 15 x and 0 vector register(s) not zero -> det3x3_2.regs, det3x3_2.trace
```
`complex_mul_1.regs`:
```
Registers of complex_mul (pid 1) on processor 0 after 13 instructions (finished)
PC 52 (0x00000034) = instruction 13
flags N=0 Z=0 C=0 V=0
x registers that are not zero: 10
  x1   =           3  0x00000003
  x2   =           4  0x00000004
  x3   =           5  0x00000005
  x4   =           2  0x00000002
  x5   =          15  0x0000000F
  x6   =           8  0x00000008
  x7   =           7  0x00000007
  x8   =           6  0x00000006
  x9   =          20  0x00000014
  x10  =          26  0x0000001A
vector registers that are not zero: 0
```
`complex_mul_1.trace`:
```
# trace of complex_mul (pid 1) on processor 0
#  step instr  bytes        instruction        result
     1     0  0D 01 00 00  x1 = [0]           x1 = 3 (0x00000003)
     2     1  0D 02 00 04  x2 = [4]           x2 = 4 (0x00000004)
     3     2  0D 03 00 08  x3 = [8]           x3 = 5 (0x00000005)
     4     3  0D 04 00 0C  x4 = [12]          x4 = 2 (0x00000002)
     5     4  03 05 01 03  x5 = x1 * x3       x5 = 15 (0x0000000F)
     6     5  03 06 02 04  x6 = x2 * x4       x6 = 8 (0x00000008)
     7     6  02 07 05 06  x7 = x5 - x6       x7 = 7 (0x00000007)  NZCV=0010
     8     7  03 08 01 04  x8 = x1 * x4       x8 = 6 (0x00000006)
     9     8  03 09 02 03  x9 = x2 * x3       x9 = 20 (0x00000014)
    10     9  01 0A 08 09  x10 = x8 + x9      x10 = 26 (0x0000001A)  NZCV=0000
    11    10  0E 10 00 07  [16] = x7          [16] = 7
    12    11  0E 14 00 0A  [20] = x10         [20] = 26
    13    12  00 00 00 00  halt               end of task
```
`det3x3_2.regs`:
```
Registers of det3x3 (pid 2) on processor 1 after 25 instructions (finished)
PC 100 (0x00000064) = instruction 25
flags N=1 Z=0 C=0 V=0
x registers that are not zero: 15
  x1   =           6  0x00000006
  x2   =           1  0x00000001
  x3   =           1  0x00000001
  x4   =           4  0x00000004
  x5   =           2  0x00000002
  x6   =           5  0x00000005
  x7   =           2  0x00000002
  x8   =           8  0x00000008
  x9   =           7  0x00000007
  x10  =          32  0x00000020
  x11  =           4  0x00000004
  x12  =        -156  0xFFFFFF64
  x13  =          18  0x00000012
  x14  =          28  0x0000001C
  x15  =        -146  0xFFFFFF6E
vector registers that are not zero: 0
```

**Hand check for complex_mul.** The data is a = 3, b = 4, c = 5, d = 2, so x1..x4 = 3 4 5 2,
x5 = ac = 15, x6 = bd = 8, x7 = 15 - 8 = 7, x8 = ad = 6, x9 = bc = 20 and x10 = 6 + 20 = 26.
The last instruction that sets flags is `x10 = x8 + x9`, which is 26: not zero, not
negative, no carry, no overflow, so all flags are 0. In step 7, `x5 - x6` sets C = 1
because, in the handout's definition, C = operand 1 > operand 2 for a subtraction. There
are 12 statements and the compiler adds a halt, so 13 instructions. Fetch has already moved
PC past the halt (instruction 12), so PC = 13 x 4 = **52**.

**Hand check for det3x3** (a..i = 6 1 1 4 2 5 2 8 7):
- x12 = a(ei - fh) = 6 x (14 - 40) = **-156** = 0xFFFFFF64
- x13 = b(di - fg) = 1 x (28 - 10) = **18**
- x14 = c(dh - eg) = 1 x (32 - 4) = **28**
- x15 = x12 - x13 + x14 = -174 + 28 = **-146** = 0xFFFFFF6E
- x10 and x11 hold the last products: dh = 32 and eg = 4.

The last flag-setting operation is the addition -174 + 28 = -146. The result is negative, so
N = 1. Neither unsigned input is larger than the result, so C = 0. The signs of the inputs
differ, so V = 0. That gives N=1 Z=0 C=0 V=0. There are 24 statements plus the halt, so
25 instructions and PC = 100.

**array_add (N = 16, test.sh).** After the second iteration, v1 = A[8..15] = 9..16, v2 =
B[8..15] = 108..115 and v3 = 117 119 .. 131. The exit test `x7 = x6 - x1 = 16 - 16 = 0`
sets Z = 1. x7 = 0, so it is not listed.

**sum_array trace.** It has 71 lines. Step 6 is `BEQ +6 not taken`, step 11 is
`BAL -6 taken -> instruction 4`, step 69 is `BEQ +6 taken -> instruction 11` and step 70 is
`[x3] = x2  [36] = 45`. When 4 tasks run at once with `-T`, each trace file still has steps
1..n in order (13, 71, 25 and 31 lines). `./test.sh` ends with `40 passed, 0 failed`, and it
also covers a division by zero: the trace ends with `error: the task stopped here` and the
`.regs` says `stopped with an error`.

## Viva questions

1. **Why can `finalize()` still read the registers?** It runs as soon as the task ends,
   before `proc_owner[p]` is freed and before the loader calls `reset(p)` for the next task
   on that processor.
2. **Why is PC 52 and not 48 for complex_mul?** `fetch()` advances PC by 4 before
   `execute()` runs, and the halt is at byte 48 (instruction 12), so PC ends at 52.
3. **Why one trace file per task instead of writing everything to stderr (`-t`)?** With round
   robin (TIME_SLICE = 10), the stderr lines of different tasks interleave. A file per
   processor slot (`trace_fd[p]`, opened at load) keeps each task's trace complete and in
   order.
4. **Why print the trace line after `execute()`?** The new value of the destination is known
   only after the instruction has run. For a branch, the outcome is evaluated again from the
   flags, which a branch does not change.
5. **How do -156 and 0xFFFFFF64 relate?** Two's complement: 156 = 0x9C, and
   -156 = 2^32 - 156 = 0xFFFFFF64. The `.regs` file shows `%d` and `0x%08X` of the same 32
   bits.
6. **How would you add the data memory to the dump?** The `.out` file already holds the final
   data memory. For a register-style dump you would loop over the data addresses up to
   `data_high[p]`, translating each one through `getPhysicalAddress()` (or the page table at
   `PTBR[p]`), exactly as `finalize()` does for `.out`.

# Q071 — Shell command `translate <pid> i|d <address>`

## Question
Add a shell command `translate <pid> i|d <address>` that prints the physical address a
running task's logical instruction (`i`) or data (`d`) address maps to, or "not mapped".
The command must not change anything: it must not map a page or stop the task. Show it
on a task that has touched data pages 1 and 3, including an address that is not mapped
and a pid that is not running.

## Answer
New case in `run_command()` (os.cpp). It does the MMU's page-table walk by hand:
1. `p = map_pid_proc_id(pid)`: the processor running the task (-1 = not running).
2. `page = address / 512`; `pte = PTBR[p] + page`, plus `NUM_INSTR_PAGES` (2) for data.
3. `frame = memory[pte]`; 0 means not mapped.
4. `physical = frame * 512 + address % 512`.

It prints each step:
```
$ translate 1 d 1020
[shell] pid 1 data address 1020: page 1, entry memory[3] = frame 3 -> physical address 2044
$ translate 1 d 3000
[shell] pid 1 data address 3000: page 5, entry memory[7] = 0 -> not mapped
```
It deliberately does **not** call `getPhysicalAddress()`, which has side effects:
- for an unmapped data page it would allocate and map a frame and count a page fault;
- for an unmapped instruction page it would call `processor_error()` and stop the task;
- it translates for `proc_id`, the processor that ran last, not the pid that was asked for.

**touch_wait.txt** (pid 1, processor 0, PTBR 0) loads into frames 1 (code) and 2
(data page 0). In its first time slice it stores to 1020 and 2040, so data pages 1 and 3
get frames 3 and 4. Then it loops for 60000 instructions. The shell first reads input
after round 1, so the commands see that mapping:

| Command | Entry | Result |
|---|---|---|
| `translate 1 i 8` | memory[0] = 1 | 1 × 512 + 8 = 520 |
| `translate 1 d 0` | memory[2] = 2 | 1024 |
| `translate 1 d 1020` | memory[3] = 3 | 1536 + 508 = 2044 |
| `translate 1 d 2040` | memory[5] = 4 | 2048 + 504 = 2552 |
| `translate 1 d 3000` | memory[7] = 0 | not mapped (page 5 never touched) |
| `translate 1 i 600` | memory[1] = 0 | not mapped (the code fits in page 0) |
| `translate 2 d 0` | — | pid 2 is not running |
| `translate 1 d 4096` | — | usage (data addresses are 0..4095) |

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | (includes) | `#include <cstdlib>` for `std::atoi` |
| `os.cpp` | `run_command` | `translate`: check the arguments, `map_pid_proc_id`, read `memory[PTBR[p] + index]`, print the frame and physical address or "not mapped" |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` pipes the 8 commands and `exit` into the shell while `touch_wait` runs. It
checks every line of the table. It checks that the task still ends normally with
**2** page faults: `translate` did not map page 5, and did not kill the task over
instruction page 1. It also checks the result 20000, and that `sum_array` alone gives 45.

## Viva
- **Why does the answer depend on when you ask?** The task keeps running between commands. A data page becomes mapped on its first touch, and after the task ends the pid is not running and its frames can belong to another task.
- **Why does the test program loop so long?** The shell runs only between scheduler rounds. The task must still be running when the piped commands are read, and its pages 1 and 3 are mapped in the very first round.
- **What is the real-world version?** Linux `/proc/<pid>/pagemap` gives the frame number of each virtual page. Debuggers and kernel tools read page tables the same side-effect-free way.
- **Why is entry 7 the one for address 3000?** The data page is 3000 / 512 = 5, and data pages start at index 2 of the table, so the index is 7. PTBR[0] = 0, so the entry is at byte 7 of frame 0.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `sum_array.txt`, `sum_array_data.byte`, `touch_wait.txt`, `touch_wait_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

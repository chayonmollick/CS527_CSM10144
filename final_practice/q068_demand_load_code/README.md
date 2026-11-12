# Q068 — Demand-load instruction pages

## Question
The loader copies every instruction page of a program into memory before the task
starts. Change it so that only instruction page 0 is loaded at the start. Instruction
page 1 is loaded from the program image when an instruction in it is fetched for the
first time (a page fault on an instruction page). A jump into a page that the program
does not have must still be an error. Test it with a program longer than 128 instructions.

## Answer
1. **`initialize()`:** copy only the first `PAGESIZE` bytes of the code:
   `copy_in(p, 0, vector(code.begin(), code.begin() + min(code.size(), PAGESIZE)))`.
2. **`loader()`:** a task now needs `1 + data pages` free frames instead of
   `code pages + data pages`, and the loader message says 1 instruction page.
3. **`getPhysicalAddress()`**, in the "frame == 0" branch: a fetch from an unmapped page
   is an error only if the program image has no such page (`index * PAGESIZE >= code.size()`).
   Otherwise the page is handled like a data page fault: a free frame is cleared, then
   the page is copied in from the image, mapped, and counted as a page fault:
   ```c
   if (isFetch) {
       std::memcpy(&memory[frame * PAGESIZE], &code[start], std::min((size_t)PAGESIZE, code.size() - start));
       std::printf("[os] pid %d: instruction page %d loaded on its first fetch (frame %d)\n", ...);
   }
   ```
   The image is the task's `prog.code`. The MMU finds it as `tasks[current_pid[p] - 1]`.

**long.txt** has 130 statements + the end = 131 instructions = 524 bytes. Instructions
0..127 are page 0; 128..130 are page 1.
- It loads with 1 instruction page and 13 frames left. Unmodified lab6 says
  "2 instruction + 1 data page(s), 12 frame(s) left".
- After 128 instructions, the fetch of instruction 128 (address 512) faults. Page 1 goes
  into the next free frame (3: code page 0 has 1, data page 0 has 2).
- The result is 128 only if page 1 really contains the program's last 3 instructions. A
  zero-filled page would read as `00` = end of program: the task would end before the
  store, and word 0 would stay 0.

**jump.byte** is `x1 = 1`, `BAL +127`. The branch goes to instruction 128, but the program
image is 12 bytes, so it stops with the old "fetch from unmapped instruction page 1" error.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `initialize` | copy only instruction page 0 |
| `os.cpp` | `loader` | `need = 1 + data pages`; message prints 1 instruction page |
| `os.cpp` | `getPhysicalAddress` | unmapped instruction page inside the image: free frame + `memcpy` from `prog.code` + map + count; error only past the image; error text names the page type |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` runs `long.txt` and checks:
- the loader line (1 instruction page, 13 frames left);
- the message `instruction page 1 loaded on its first fetch (frame 3)`;
- 131 instructions with 1 page fault, and the result 128.

Then it runs `jump.byte` (exit code 1, error at instruction 128, 2 instructions, 0 page
faults), and `sum_array` alone (45).

## Viva
- **What is gained?** A task starts with fewer frames, and code that never runs (error handling, unused functions) never takes a frame. Real systems load all program pages this way, from the executable file.
- **What is the risk?** The loader no longer reserves a frame for page 1. If memory is full when page 1 is first fetched, the task stops with "no free physical frame for instruction page 1". Without page replacement, demand loading trades safety for memory.
- **Why can an instruction never be split across two pages?** Instructions are 4 bytes at addresses that are multiples of 4, and `PAGESIZE` is a multiple of 4. So the first byte of a fetch takes the fault and the other 3 bytes hit the new mapping.
- **Why copy from `prog.code` and not re-read the file?** The compiled image is already in the task's process-table entry. Re-compiling or re-reading would be slower, and wrong if the file had changed since the task started.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `jump.byte`, `long.txt`, `long_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

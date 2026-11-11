# e18 — Context switching with a PCB per task (more tasks than processors)

## Question (as it could appear in the test)
In Lab 4 a task keeps its processor until it finishes, so at most NP tasks run and the rest wait.

Remove this limit:
- Keep a **Process Control Block** for every task: x and v registers, PC, flags, step/exec counters and its page-table base.
- On every time slice, dispatch the next ready task onto a processor. Save the outgoing task's state into its PCB, restore the incoming task's PCB, and set PTBR to the incoming task's page table.
- Store each task's page table in frame 0 in a slot of its own (PTBR = slot * NUM_LOGICAL_PAGES). How many page tables fit?

Set NP = 1, run 4 counter programs concurrently, show the interleaved output in log.txt, and report the context switches per task.

## Solution idea
- **Base model.** The processor arrays (`Register[p]`, `PC[p]`, ...) hold a task's state for its whole life, so the task is bound to processor p. Its page table is the processor's: `PTBR[p] = p * 10`.
- **PCB** (`struct PCB` in processor.h). It is a copy of every per-processor register: `x[256]`, `v[32][8]`, PC, N Z C V, PTBR, end_of_simulation/failed, steps, `exec_count[256]` and data_high.
  - `save_context(p, pcb)` and `restore_context(p, pcb)` copy between the processor arrays and a PCB.
  - fetch/decode/execute are **unchanged**: they still work on `Register[p]` and the other arrays.
- **Page tables belong to tasks, not processors.**
  - The loader gives a task the first free slot s in frame 0; its table is at `s * NUM_LOGICAL_PAGES`, and the slot is freed when the task finishes.
  - PTBR is part of the PCB, so restoring a task also switches the address space.
  - `getPhysicalAddress()` is unchanged. A demand-paged frame goes into the running task's table, and page faults are counted per task.
- **How many fit.** 512 / 10 = **51** page tables fit in frame 0 (`NUM_PT_SLOTS`, checked by a `static_assert`). Memory is the real limit: there are 15 free frames and each task needs at least 2 (1 instruction page and 1 data page), so at most **7** tasks can be resident. The 8th counter waits for frames.
- **Loader.** It needs a free slot and enough frames, not a free processor. It clears the PCB (what `reset()` used to do), builds the page table in the slot and puts the task on the ready queue.
- **Scheduler.** In each round every ready task gets one slice on processor `p = i % NP` (always 0 when NP = 1).
  - `dispatch(p, pid)` performs the **context switch**: save the task currently on p into its PCB, then restore pid's PCB and count the switch.
  - If p already holds pid (it is the only ready task), nothing is copied (lazy save).
- **Switch counts** for the 4 counters:
  - `counter` runs 1 + 4×10 + 2 = 43 instructions, which is 5 slices and 5 switches.
  - `counter_by100` runs 2 + 5×10 + 2 = 54 instructions, which is 6 slices and 6 switches.
  - Total: 5 + 6 + 5 + 6 = **22**.
- **log.txt is the same as the base on NP = 4.** The base also simulates its 4 processors one after another, in the same round-robin order. The difference is in the machine: here one processor is time-shared and its registers are swapped 22 times.
- **Base regression tests: 32 passed, 1 failed, by design.**
  - The failing check is `5 tasks, 4 processors: pid 5 waited`. Five tasks need 10 of the 15 frames, so all five are resident and nobody waits any more. That waiting is exactly the limit this extension removes.
  - Every result check (values, `.freq`, page faults, errors) passes, and so does the log-order check.
- **Limitation.**
  - The loader reserves only the pages in the files. With up to 7 resident tasks, a task that touches new data pages can find no free frame. It then stops with "no free physical frame".
  - A real OS would page out a frame (page replacement or swapping).

## Changes
| file | function | change |
|---|---|---|
| config.h | `NP` | 1 (base: 4) |
| config.h | `NUM_PT_SLOTS` | new: `PAGESIZE / NUM_LOGICAL_PAGES` = 51 page-table slots in frame 0 |
| processor.h | `struct PCB`, `save_context`, `restore_context` | new: the PCB and the switch helpers |
| processor.cpp | `save_context()`, `restore_context()` | new: copy all per-processor state of p to/from a PCB (restore also sets PTBR) |
| os.cpp | `struct Task` | new `pcb`, `slot`, `slices`, `switches`; `page_faults` counted per task |
| os.cpp | globals, `static_assert` | `proc_owner[p]` = pid whose registers are on p; new `slot_owner[51]` and `context_switches`; `page_faults_on[]` removed; all slots must fit in frame 0 |
| os.cpp | `getPhysicalAddress()` | page fault counted for the running task (`current_pid[p]`) |
| os.cpp | `copy_in()`, `initialize()` | build the table at the task's slot address; PCB cleared |
| os.cpp | `finalize()` | message adds slices and context switches |
| os.cpp | `loader()` | needs a free slot and frames, not a processor; opens the log |
| os.cpp | `dispatch()` | new: the context switch (save old PCB, restore new PCB, count) |
| os.cpp | `create_task()` | waiting message: frames/slot |
| os.cpp | `os_init()`, `finish_task()` | clear / free page-table slots |
| os.cpp | `scheduler()` | `p = i % NP`, `dispatch()` before every slice, total switches at the end |
| Makefile | header, `test` | `make test` = base tests (1 failure expected) + `./test.sh` |
| test.sh | — | 24 checks |

`grep -n "EXT e18" *.cpp *.h Makefile` lists every change; `CHANGES.diff` has the full diff against `simulator/`.

## Run
```
cd minicomputer/extensions/e18_context_switch_pcb
make
X=../../examples
./sim -b $X/counter.txt $X/counter_data.byte $X/counter_by100.txt $X/counter_data.byte \
         $X/counter.txt $X/counter_data.byte $X/counter_by100.txt $X/counter_data.byte
cat log.txt
./sim -t -b $X/counter.txt $X/counter_data.byte $X/counter_by100.txt $X/counter_data.byte 2>&1 >/dev/null | head -24
./test.sh                                # 24 passed, 0 failed
SIM=$PWD/sim ../../tests/run_tests.sh    # 32 passed, 1 failed (pid 5 no longer waits - by design)
make clean
```

## Expected output
```
[os] pid 1 created: ../../examples/counter.txt ../../examples/counter_data.byte (7 instructions)
[os] pid 1 (counter) loaded: page table in slot 0 (PTBR = 0), 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created: ../../examples/counter_by100.txt ../../examples/counter_data.byte (9 instructions)
[os] pid 2 (counter_by100) loaded: page table in slot 1 (PTBR = 10), 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 3 created: ../../examples/counter.txt ../../examples/counter_data.byte (7 instructions)
[os] pid 3 (counter) loaded: page table in slot 2 (PTBR = 20), 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 4 created: ../../examples/counter_by100.txt ../../examples/counter_data.byte (9 instructions)
[os] pid 4 (counter_by100) loaded: page table in slot 3 (PTBR = 30), 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 1 (counter) finished on processor 0 after 43 instructions, 0 page fault(s), 5 slice(s), 5 context switch(es) -> counter_data_1.out, counter_1.freq
[os] pid 3 (counter) finished on processor 0 after 43 instructions, 0 page fault(s), 5 slice(s), 5 context switch(es) -> counter_data_3.out, counter_3.freq
[os] pid 2 (counter_by100) finished on processor 0 after 54 instructions, 0 page fault(s), 6 slice(s), 6 context switch(es) -> counter_data_2.out, counter_by100_2.freq
[os] pid 4 (counter_by100) finished on processor 0 after 54 instructions, 0 page fault(s), 6 slice(s), 6 context switch(es) -> counter_data_4.out, counter_by100_4.freq
[os] 22 context switch(es) on 1 processor(s)
[os] all tasks finished; print output is in log.txt
```
log.txt: one block per time slice, all on the single processor. In slices 2 and 4 a counter prints 3 values, because its print falls at instructions 3, 7, 11, ... (4 per loop).
```
Process id: 1   x1 : 00000001      <- round 1: pid 1 slice
Process id: 1   x1 : 00000002
Process id: 2   x1 : 00000064      <- pid 2 slice (100, 200)
Process id: 2   x1 : 000000C8
Process id: 3   x1 : 00000001
Process id: 3   x1 : 00000002
Process id: 4   x1 : 00000064
Process id: 4   x1 : 000000C8
Process id: 1   x1 : 00000003      <- round 2: pid 1 continues from its PCB
Process id: 1   x1 : 00000004
Process id: 1   x1 : 00000005
Process id: 2   x1 : 0000012C
Process id: 2   x1 : 00000190
...                                   (40 lines; pid order:
                                       1 1 2 2 3 3 4 4 1 1 1 2 2 3 3 3 4 4 1 1 2 2 3 3 4 4
                                       1 1 1 2 2 3 3 3 4 4 2 2 4 4)
Process id: 4   x1 : 00000384
Process id: 4   x1 : 000003E8
```
Trace (`-t`, stderr): the switch happens after the 10th instruction. pid 1 later resumes at instruction 2 (`print x1`), so its PC came back from the PCB.
```
[os] context switch on processor 0: idle -> pid 1 (PTBR = 0)
[pid 1 p0]   0: 0F 01 00 00   x1 = 0
[pid 1 p0]   1: 09 01 01 01   x1 = x1 + 1
...
[pid 1 p0]   1: 09 01 01 01   x1 = x1 + 1
[os] context switch on processor 0: pid 1 -> pid 2 (PTBR = 10)
[pid 2 p0]   0: 0F 01 00 00   x1 = 0
...
[pid 2 p0]   4: 08 00 00 01   print x1
[os] context switch on processor 0: pid 2 -> pid 1 (PTBR = 0)
[pid 1 p0]   2: 08 00 00 01   print x1
```
`./test.sh`:
```
4 counter tasks at once on NP = 1 processor              (10 checks: results, log order, 5/5 6/6 5/5 6/6, 22, .freq)
A task alone keeps the processor: no switch between its slices   (counter alone: 5 slices, 1 switch)
Memory, not processors, limits the resident tasks: 8 counters    (7 resident, pid 8 waits, reuses slot 0, 36 switches)
Vector registers and flags survive switches (5 programs share one processor)
Demand paging uses the running task's own page table
Trace (-t) shows each switch and the new PTBR
24 passed, 0 failed
```
**Mutation check (done while writing this).**
- If `restore_context()` skips the flags, the counters overshoot: x1 ends at 15, not 10. A counter's slice can end right after the `x2 = x1 - 10` that should stop its loop, and its `BNE` then sees another task's flags.
- If it skips the vector registers, `array_add` C[15] = 123, not 131.
- So the tests really check the PCB.

## Viva questions
1. **What is saved on a context switch here, and why is memory not copied?**
   Every per-processor array is saved: x and v registers, PC, N Z C V, PTBR and the counters. Memory stays where it is. Each task has its own frames and its own page table, so loading its PTBR switches the address space. A real CPU would also flush the TLB, or tag entries with an ASID.
2. **Why must PTBR be in the PCB?**
   The same logical address (say data address 0) is in a different frame for every task. PTBR selects which page table translates it.
3. **How many page tables fit in frame 0, and how many tasks can be resident?**
   51 page tables (512 / 10 bytes). But 15 free frames at 2 or more per task allow only 7 resident tasks, so frames, not slots or processors, limit multiprogramming.
4. **Why does a task that runs alone get 5 slices but only 1 context switch?**
   `dispatch()` does nothing when the processor already holds that task. A switch copies about 3 KB twice (save and restore), so it is skipped when it isn't needed.
5. **The log looks the same as the base on 4 processors. What is different?**
   The base simulates its 4 processors one after another in the same order, so the print order matches. Here one processor is time-shared: 22 context switches instead of 0, and any number of tasks can be active, limited only by memory.
6. **What does a context switch cost, and how does the time slice affect it?**
   The switch itself is pure overhead: copying 256 + 256 words of registers and 256 counters. On real hardware it also flushes the pipeline and pollutes the caches and TLB. A larger time slice means fewer switches but slower response; a smaller one means better interactivity but more overhead.

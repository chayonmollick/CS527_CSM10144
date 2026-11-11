# e20 — fork() and getpid(): process creation from inside a program

## Question (as it could appear in the test)

Add Unix-style process creation to the mini-computer. A new statement `xD = fork` creates a
child process that is a copy of the caller: new physical frames holding copies of all the
parent's mapped pages (instruction and data), copies of its x/v registers, flags and PC, so the
child continues after the fork. fork returns the child's pid in the parent and 0 in the child.
Also add `xD = getpid`. The child goes to the ready queue if a processor and enough frames are
free, otherwise it waits. Every process writes its own output file. Demonstrate with a program
in which parent and child write different values to the same address and print their pids.
Viva: why does real Unix use copy-on-write, and what would it take here?

## Solution idea

- `fork` is a system call: `execute()` for opcode `0x40` calls the OS function `os_fork(p, dest)`.
- `os_fork` builds a new `Task` (next pid, `ppid` = caller) with:
  - the same program (byte code + source map, so the child gets its own `.freq`) and data file name;
  - a snapshot of the caller's CPU state (`save_cpu`): x/v registers, flags, `data_high` and the PC,
    which already points past the fork because `fetch()` advanced it. In the snapshot `xD = 0`;
  - a copy of every page mapped in the caller's page table (read through `PTBR[p]` from frame 0),
    including data pages that were mapped on demand.
- The child joins the waiting queue and `admit_waiting()` runs at once. `loader()` needs a free
  processor and as many free frames as the parent had pages. If both are free: `reset(c)`,
  `initialize()` copies each saved page into a new frame and writes the page-table entry,
  `load_cpu()` restores the registers, and the child goes on the ready queue. Otherwise it waits:
  the OS keeps the copy (like a swapped-out process) and loads it in a later round.
- The parent gets `xD = child pid`, or `-1` when the process table (`MAX_TASKS` = 64) is full.
- `getpid` just reads `current_pid[p]`, which the loader sets for the processor.
- A child loaded during the parent's time slice runs from the next round. The round loop runs
  exactly `n = ready_queue.size()` slices, fixed when the round starts, so a task added during the
  round can't run in that round.
- Output: the child keeps the parent's data file name, so it writes `<data>_<childpid>.out`. Its
  step and execution counters start at 0, so its `.freq` counts only what the child executed.

Byte code: `40 D 00 00` = `xD = fork`, `41 D 00 00` = `xD = getpid`.

`fork_tree.txt` (`fork; fork; fork`) gives 2^3 = 8 processes. Arrows mean "forked". The processors
are 4, so pids 6 and 7 wait:

```
1 ─┬─ 2 ─┬─ 5 ── 8
   │     └─ 6
   ├─ 3 ── 7
   └─ 4
```

## Changes

| file | function | change |
|---|---|---|
| config.h | `enum Opcode` | `OP_FORK = 0x40`, `OP_GETPID = 0x41` |
| compiler.cpp | `encode_statement` | parse `xD = fork` and `xD = getpid` (dest must be an x register) |
| compiler.cpp | `disassemble` | text `x1 = fork`, `x5 = getpid` for listings, traces and `.freq` |
| processor.h/.cpp | `struct CpuState`, `save_cpu`, `load_cpu` (new) | copy registers, PC, flags and `data_high` out of / into processor p |
| processor.cpp | `execute` | `OP_GETPID`: `xD = current_pid[p]`; `OP_FORK`: `xD = os_fork(p, dest)` |
| os.h / os.cpp | `os_fork` (new) | build the child: snapshot the registers with xD = 0, copy every mapped page, then waiting queue + `admit_waiting()` |
| os.cpp | `struct Task` | `ppid`, `forked`, `cpu`, `saved_page[NUM_LOGICAL_PAGES]` |
| os.cpp | `page_counts` (new) | pages a task needs (for a child: exactly its parent's mapped pages) |
| os.cpp | `initialize` | child: copy each saved page into a new frame, set its PTE |
| os.cpp | `loader` | frames needed from `page_counts`; `load_cpu` for a child |
| Makefile | `test` | base regression tests on this build, then `./test.sh` |

## Run

```
cd minicomputer/extensions/e20_fork_getpid
make
./sim -c fork_demo.txt                           # listing: 40 01 00 00 = x1 = fork
./sim -b fork_demo.txt fork_demo_data.byte       # parent pid 1, child pid 2
./sim -b fork_tree.txt fork_tree_data.byte       # 8 processes, pids 6 and 7 wait
./sim -b fork_bigmem.txt fork_bigmem_data.byte   # child waits for frames, not a processor
./sim -b fork_bomb.txt fork_tree_data.byte       # 7 forks: table full after 64, fork returns -1
./test.sh                                        # 35 passed, 0 failed
SIM=$PWD/sim ../../tests/run_tests.sh            # 33 passed, 0 failed
make clean
```

## Expected output

```
$ ./sim -c fork_demo.txt
fork_demo.txt -> program.byte (17 instructions)
  0: 41 05 00 00   x5 = getpid        | x5 = getpid        % 1: only the parent exists
  1: 08 00 00 05   print x5           | print x5
  2: 0F 06 00 07   x6 = 7             | x6 = 7
  3: 0E 0C 00 06   [12] = x6          | [12] = x6          % written before the fork
  4: 40 01 00 00   x1 = fork          | x1 = fork          % parent: x1 = child pid, child: x1 = 0
  5: 41 05 00 00   x5 = getpid        | x5 = getpid        % each process has its own pid now
  6: 08 00 00 05   print x5           | print x5
  7: 09 02 01 00   x2 = x1 + 0        | x2 = x1 + 0        % Z = 1 only in the child (x1 == 0)
  8: 10 00 00 05   BEQ +5             | BEQ .child
  ...
$ ./sim -b fork_demo.txt fork_demo_data.byte
[os] pid 1 created: fork_demo.txt fork_demo_data.byte (17 instructions)
[os] pid 1 (fork_demo) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created by fork() in pid 1 (2 page(s) copied)
[os] pid 2 (fork_demo) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 2 (fork_demo) finished on processor 1 after 8 instructions, 0 page fault(s) -> fork_demo_data_2.out, fork_demo_2.freq
[os] pid 1 (fork_demo) finished on processor 0 after 15 instructions, 0 page fault(s) -> fork_demo_data_1.out, fork_demo_1.freq
[os] all tasks finished; print output is in log.txt
$ cat log.txt
Process id: 1   x5 : 00000001
Process id: 1   x5 : 00000001
Process id: 2   x5 : 00000002
$ paste fork_demo_data_1.out fork_demo_data_2.out      # parent | child
64 00 00 00	C8 00 00 00          [0]  = 100 | 200   written after the fork: separate copies
02 00 00 00	00 00 00 00          [4]  = child pid 2 | 0 (the parent's write is not seen)
01 00 00 00	02 00 00 00          [8]  = getpid
07 00 00 00	07 00 00 00          [12] = 7 in both: written before the fork, copied
```

(The notes to the right of the `.out` lines were added by hand; the files hold only the hex bytes.)

```
$ ./sim -b fork_tree.txt fork_tree_data.byte
[os] pid 1 created: fork_tree.txt fork_tree_data.byte (7 instructions)
[os] pid 1 (fork_tree) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created by fork() in pid 1 (2 page(s) copied)
[os] pid 2 (fork_tree) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 3 created by fork() in pid 1 (2 page(s) copied)
[os] pid 3 (fork_tree) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 4 created by fork() in pid 1 (2 page(s) copied)
[os] pid 4 (fork_tree) loaded on processor 3: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 1 (fork_tree) finished on processor 0 after 7 instructions, 0 page fault(s) -> fork_tree_data_1.out, fork_tree_1.freq
[os] pid 5 created by fork() in pid 2 (2 page(s) copied)
[os] pid 5 (fork_tree) loaded on processor 0: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 6 created by fork() in pid 2 (2 page(s) copied)
[os] pid 6 is waiting for a free processor/frames
[os] pid 2 (fork_tree) finished on processor 1 after 6 instructions, 0 page fault(s) -> fork_tree_data_2.out, fork_tree_2.freq
[os] pid 7 created by fork() in pid 3 (2 page(s) copied)
[os] pid 6 (fork_tree) loaded on processor 1: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 7 is waiting for a free processor/frames
[os] pid 3 (fork_tree) finished on processor 2 after 5 instructions, 0 page fault(s) -> fork_tree_data_3.out, fork_tree_3.freq
[os] pid 4 (fork_tree) finished on processor 3 after 4 instructions, 0 page fault(s) -> fork_tree_data_4.out, fork_tree_4.freq
[os] pid 7 (fork_tree) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 8 created by fork() in pid 5 (2 page(s) copied)
[os] pid 8 (fork_tree) loaded on processor 3: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 5 (fork_tree) finished on processor 0 after 5 instructions, 0 page fault(s) -> fork_tree_data_5.out, fork_tree_5.freq
[os] pid 6 (fork_tree) finished on processor 1 after 4 instructions, 0 page fault(s) -> fork_tree_data_6.out, fork_tree_6.freq
[os] pid 7 (fork_tree) finished on processor 2 after 4 instructions, 0 page fault(s) -> fork_tree_data_7.out, fork_tree_7.freq
[os] pid 8 (fork_tree) finished on processor 3 after 4 instructions, 0 page fault(s) -> fork_tree_data_8.out, fork_tree_8.freq
[os] all tasks finished; print output is in log.txt
```

Each `fork_tree_data_<k>.out` holds `k` at `[0]`, and `log.txt` has 8 lines, pids 1 to 8.
Child 6 found all 4 processors busy and waited. Pid 2 then finished, and the next
`admit_waiting()`, inside pid 3's fork, loaded 6 on processor 1; the new child 7 had to wait.
Child 7 was loaded at the start of the next round.

`./test.sh` ends with `35 passed, 0 failed`. `SIM=$PWD/sim ../../tests/run_tests.sh` ends with
`33 passed, 0 failed`.

## Why real Unix uses copy-on-write, and what it would take here

This version copies every page at fork time: the fork costs time and frames, even when the child
never touches most of them. Most forks are followed straight away by `exec`, which throws the
copy away. Copy-on-write (COW) lets parent and child share the parent's frames, marked read-only.
The first write to a shared page causes a fault, and only then does the OS copy that one page.
Pages that are never written, such as all the code, are never copied.

To add COW here:
1. Turn `freePages[]` (0/1) into a reference count per frame. `os_fork` would copy the parent's
   page-table entries instead of the pages, and add 1 to each frame's count.
2. Add a COW/read-only bit to the page-table entry. An entry is one byte holding a frame number
   below 16, so bit 7 is free.
3. Writes must be visible to the MMU: `store_word` would call `getPhysicalAddress` with an
   `isWrite` flag.
4. On a write to a COW page with count > 1: take a free frame, copy the 512 bytes, point the
   entry at the new frame (writable), and subtract 1 from the old frame's count. With count 1,
   just clear the COW bit.
5. `finalize` subtracts 1 from each frame's count and frees the frame only at 0.
6. Instruction pages are never written, so they can stay shared read-only for ever.

A waiting child would then hold only references, not copies, so it needs no frames until it
writes.

## Viva questions

**Q: After `x1 = fork`, how does each process know whether it is the parent or the child?**
A: By the return value: the child pid (> 0) in the parent, 0 in the child, -1 if fork failed.
Both processes continue at the same instruction, so the program branches on x1 (`BEQ .child`).

**Q: Why doesn't the child re-execute the fork?**
A: `fetch()` has already moved PC past the fork when `execute()` runs it. `save_cpu()` copies
that PC, so the child starts at the next instruction.

**Q: The parent writes [4] after the fork. Why does the child's output still have 0 there?**
A: Each process has its own address space. The child got copies of the pages in new frames, so
any later write goes only to the writer's own frame.

**Q: How many processes do `fork; fork; fork` create?**
A: 2^3 = 8 in total, of which 7 are new. Every existing process executes each later fork, so the
count doubles at each fork.

**Q: What if no processor or frames are free, or the process table is full?**
A: The child waits in the waiting queue; the OS keeps its pages and registers, like a swapped-out
process, until `admit_waiting()` can load it. With 64 tasks already in the table, fork returns -1
and no child is made.

**Q: Why doesn't getpid need a call into the OS here?**
A: The loader writes the pid into `current_pid[p]`, which works like a processor register. In a
real OS, getpid is a system call that reads the pid from the PCB.

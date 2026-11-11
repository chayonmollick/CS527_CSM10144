# e21 — exit codes, wait() and the process life cycle

## Question (as it could appear in the test)

Extend the OS with process termination and synchronisation:
(a) `exit xR` ends the running task at once with exit code xR (a program that just reaches
its end exits with 0); the OS reports every task's exit code.
(b) `xD = wait` blocks the calling task (it leaves the ready queue) until one of the tasks it
created with `xD = fork` has finished, and returns that child's pid (-1 if it has no children).
Show what happens to a child that ends before its parent calls wait (zombie) and to a child
whose parent ends first (orphan). When the simulator stops, print a ps-like table of all tasks
with their exit codes.

## Solution idea

Full `wait` is implemented (no fallback). This directory starts from the base simulator, so it
first ports `xD = fork` and `xD = getpid` from e20 unchanged: the child gets copies of all the
parent's mapped pages and registers and continues after the fork with xD = 0.

- **exit xR** (`44 00 00 R`) is handled by the processor: `exit_code[p] = xR; end_of_simulation[p] = 1`.
  The scheduler then ends the task exactly as for HALT. `finish_task` stores the code in the Task:
  `exit_code[p]` (0 if the program just ended), or -1 if the task failed with an error.
- **xD = wait [xS]** (`43 D F S`, F = 1 means "also put the exit code in xS") traps to `os_wait()`:
  1. the caller has a child that already ended (a ZOMBIE): collect the one that ended first.
     xD = its pid and xS = its code; no blocking;
  2. it has children still WAITING / READY / BLOCKED: **block**. `blocked[p] = 1` stops
     `process_instructions()` at once, the scheduler does not push the task back on the ready
     queue, and its state is BLOCKED;
  3. no children at all: xD = -1.
- **Wake-up:** when a child ends and its parent is BLOCKED, `collect()` writes the child's pid
  (and code) straight into the parent's registers on the parent's processor. The PC already
  points after the wait, so the parent carries on as if wait() had just returned. It goes to
  the back of the ready queue and runs from the next round.
- **Where an exit code goes** (in `finish_task`):
  - parent BLOCKED in wait: collected now;
  - parent still running: the child becomes a **ZOMBIE**. Frames, processor and output files
    are released at once; only the process-table entry with the pid and code stays until the
    parent calls wait;
  - no parent (started by the shell/command line), or the parent has ended (**orphan**): the
    OS reaps it. When a parent ends, the OS also reaps its uncollected zombies.
- **A blocked task keeps its processor and frames.** The base model has no context switch, so
  that processor idles. When 4 blocked parents hold all 4 processors while the children they
  wait for sit in the waiting queue, that is a **deadlock**. Once the shell is closed, if nothing
  is ready and nothing can be loaded, `stop_deadlock_victim()` stops the newest BLOCKED task
  (exit code -1). This frees its processor and frames, and the rest unwinds (`deadlock.txt`).
  The real cure is e18-style context switching: save the blocked task in its PCB and give the
  processor to someone else.
- At the end, `print_exit_table()` prints pid, ppid, program, state, exit code, instructions and
  who reaped it (a parent's `wait`, or `os`).

```
WAITING --loader--> READY --exit / HALT / error--+--> parent BLOCKED in wait(): collected now -> DONE
                     |  ^                        +--> parent still running: ZOMBIE --wait()-->  DONE
             wait()  |  | a child ended          |                          (parent ends: os -> DONE)
                     v  |                        +--> no parent / parent ended (orphan): os  -> DONE
                    BLOCKED
```

## Changes

| file | function | change |
|---|---|---|
| config.h | `enum Opcode` | `OP_FORK 0x40`, `OP_GETPID 0x41`, `OP_WAIT 0x43`, `OP_EXIT 0x44` |
| compiler.cpp | `encode_statement` | parse `exit xR`, `xD = fork`, `xD = getpid`, `xD = wait`, `xD = wait xS` |
| compiler.cpp | `disassemble` | text for the four instructions |
| processor.h/.cpp | globals, `reset` | `exit_code[NP]`, `blocked[NP]`, cleared by `reset` |
| processor.h/.cpp | `CpuState`, `save_cpu`, `load_cpu` (new) | register snapshot for fork (as in e20) |
| processor.cpp | `execute` | GETPID; FORK -> `os_fork`; WAIT -> `os_wait`; EXIT sets `exit_code[p]` and ends the task |
| processor.cpp | `process_instructions` | loop also stops when `blocked[p]` |
| os.h / os.cpp | `os_fork` (new) | as in e20: copy registers (xD = 0) and pages, waiting queue + `admit_waiting` |
| os.h / os.cpp | `os_wait`, `collect` (new) | zombie -> collect at once; running children -> BLOCKED; none -> -1 |
| os.cpp | `enum TaskState`, `struct Task` | states BLOCKED, ZOMBIE; `ppid`, fork snapshot, `exit_code`, `end_seq`, `reaped_by`, `wait_dest/status` |
| os.cpp | `page_counts`, `initialize`, `loader` | load a fork child from its copied pages and registers (as in e20) |
| os.cpp | `finalize` | the "[os] pid ... finished" line also shows the exit code |
| os.cpp | `finish_task` | record the code; reap own zombies; wake a blocked parent / become a zombie / orphan |
| os.cpp | `stop_deadlock_victim` (new) | stop the newest blocked task when nothing can run |
| os.cpp | `print_exit_table` (new), `scheduler` | blocked tasks leave the ready queue; deadlock check; exit-code table at the end |
| Makefile | `test` | base regression tests on this build, then `./test.sh` |

## Run

```
cd minicomputer/extensions/e21_wait_exit_code
make
./sim -c wait_demo.txt                                               # 43 04 01 05 = x4 = wait x5
./sim -b exit_codes.txt exit_data.byte exit_fail.txt exit_data.byte  # exit codes 7 and -1
./sim -b wait_demo.txt wait_demo_data.byte                           # zombie + blocking wait, sum 60
./sim -b zombie_orphan.txt exit_data.byte                            # zombie reaped by the os, orphan
./sim -b deadlock.txt exit_data.byte                                 # deadlock detected and broken
./test.sh                                     # 35 passed, 0 failed
SIM=$PWD/sim ../../tests/run_tests.sh         # 33 passed, 0 failed
make clean
```

## Expected output

```
$ ./sim -c wait_demo.txt          (part)
  3: 40 01 00 00   x1 = fork          | x1 = fork          % parent: child pid, child: 0
 10: 43 04 01 05   x4 = wait x5       | x4 = wait x5       % x4 = pid of an ended child, x5 = its exit code
 16: 43 06 00 00   x6 = wait          | x6 = wait          % no children left: -1
 18: 44 00 00 0A   exit x10           | exit x10

$ ./sim -b exit_codes.txt exit_data.byte exit_fail.txt exit_data.byte
[pid 2, processor 1] error at instruction 2: division by zero
[os] pid 1 created: exit_codes.txt exit_data.byte (6 instructions)
[os] pid 1 (exit_codes) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created: exit_fail.txt exit_data.byte (4 instructions)
[os] pid 2 (exit_fail) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 1 (exit_codes) finished on processor 0 after 3 instructions, 0 page fault(s), exit code 7 -> exit_data_1.out, exit_codes_1.freq
[os] pid 2 (exit_fail) STOPPED WITH AN ERROR on processor 1 after 3 instructions, 0 page fault(s), exit code -1 -> exit_data_2.out, exit_fail_2.freq
[os] exit codes:
  pid  ppid  program          state      exit  instructions  reaped by
    1     0  exit_codes       exited        7             3  os
    2     0  exit_fail        error        -1             3  os
[os] all tasks finished; print output is in log.txt
```

`wait_demo`: child 1 (pid 2) ends in round 2, before the parent's first wait, so it is a zombie
and is collected without blocking. Then the parent blocks. Pid 4 (10 spins) ends before pid 3
(30 spins), so wait returns 2, 4, 3, which is the order the children ended, not the order they
were created.

```
$ ./sim -b wait_demo.txt wait_demo_data.byte
[os] pid 1 created: wait_demo.txt wait_demo_data.byte (30 instructions)
[os] pid 1 (wait_demo) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created by fork() in pid 1 (2 page(s) copied)
[os] pid 2 (wait_demo) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 3 created by fork() in pid 1 (2 page(s) copied)
[os] pid 3 (wait_demo) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 2 (wait_demo) finished on processor 1 after 10 instructions, 0 page fault(s), exit code 10 -> wait_demo_data_2.out, wait_demo_2.freq
[os] pid 2 is a zombie until pid 1 calls wait()
[os] pid 4 created by fork() in pid 1 (2 page(s) copied)
[os] pid 4 (wait_demo) loaded on processor 1: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 1: wait() returns pid 2, exit code 10
[os] pid 1 blocks in wait()
[os] pid 4 (wait_demo) finished on processor 1 after 50 instructions, 0 page fault(s), exit code 30 -> wait_demo_data_4.out, wait_demo_4.freq
[os] pid 1: wait() returns pid 4, exit code 30
[os] pid 1 blocks in wait()
[os] pid 3 (wait_demo) finished on processor 2 after 130 instructions, 0 page fault(s), exit code 20 -> wait_demo_data_3.out, wait_demo_3.freq
[os] pid 1: wait() returns pid 3, exit code 20
[os] pid 1: wait() returns -1 (no children left)
[os] pid 1 (wait_demo) finished on processor 0 after 41 instructions, 0 page fault(s), exit code 60 -> wait_demo_data_1.out, wait_demo_1.freq
[os] exit codes:
  pid  ppid  program          state      exit  instructions  reaped by
    1     0  wait_demo        exited       60            41  os
    2     1  wait_demo        exited       10            10  pid 1 (wait)
    3     1  wait_demo        exited       20           130  pid 1 (wait)
    4     1  wait_demo        exited       30            50  pid 1 (wait)
[os] all tasks finished; print output is in log.txt
$ cat log.txt
Process id: 1   x4 : 00000002
Process id: 1   x4 : 00000004
Process id: 1   x4 : 00000003
$ head -2 wait_demo_data_1.out
3C 00 00 00
FF FF FF FF
```

`[0]` = 0x3C = 60 = 10 + 20 + 30, and `[4]` = -1 from the fourth wait.

```
$ ./sim -b zombie_orphan.txt exit_data.byte
[os] pid 1 created: zombie_orphan.txt exit_data.byte (21 instructions)
[os] pid 1 (zombie_orphan) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created by fork() in pid 1 (2 page(s) copied)
[os] pid 2 (zombie_orphan) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 3 created by fork() in pid 1 (2 page(s) copied)
[os] pid 3 (zombie_orphan) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 2 (zombie_orphan) finished on processor 1 after 7 instructions, 0 page fault(s), exit code 1 -> exit_data_2.out, zombie_orphan_2.freq
[os] pid 2 is a zombie until pid 1 calls wait()
[os] pid 1 (zombie_orphan) finished on processor 0 after 31 instructions, 0 page fault(s), exit code 5 -> exit_data_1.out, zombie_orphan_1.freq
[os] pid 2 (zombie child of pid 1) reaped by the os
[os] pid 3 (zombie_orphan) finished on processor 2 after 127 instructions, 0 page fault(s), exit code 2 -> exit_data_3.out, zombie_orphan_3.freq
[os] pid 3 is an orphan (pid 1 has ended): reaped by the os
[os] exit codes:
  pid  ppid  program          state      exit  instructions  reaped by
    1     0  zombie_orphan    exited        5            31  os
    2     1  zombie_orphan    exited        1             7  os
    3     1  zombie_orphan    exited        2           127  os
[os] all tasks finished; print output is in log.txt
```

`deadlock.txt` is a chain in which every process forks once and waits. Pids 1 to 4 block and
hold all 4 processors, so pid 5 can never be loaded:

```
$ ./sim -b deadlock.txt exit_data.byte
[os] pid 1 created: deadlock.txt exit_data.byte (15 instructions)
[os] pid 1 (deadlock) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created by fork() in pid 1 (2 page(s) copied)
[os] pid 2 (deadlock) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 1 blocks in wait()
[os] pid 3 created by fork() in pid 2 (2 page(s) copied)
[os] pid 3 (deadlock) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 2 blocks in wait()
[os] pid 4 created by fork() in pid 3 (2 page(s) copied)
[os] pid 4 (deadlock) loaded on processor 3: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 3 blocks in wait()
[os] pid 5 created by fork() in pid 4 (2 page(s) copied)
[os] pid 5 is waiting for a free processor/frames
[os] pid 4 blocks in wait()
[os] deadlock: blocked tasks hold the processors/frames their children need; stopping pid 4
[os] pid 4 (deadlock) STOPPED WITH AN ERROR on processor 3 after 9 instructions, 0 page fault(s), exit code -1 -> exit_data_4.out, deadlock_4.freq
[os] pid 3: wait() returns pid 4, exit code -1
[os] pid 5 (deadlock) loaded on processor 3: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 3 (deadlock) finished on processor 2 after 12 instructions, 0 page fault(s), exit code 3 -> exit_data_3.out, deadlock_3.freq
[os] pid 2: wait() returns pid 3, exit code 3
[os] pid 5 (deadlock) finished on processor 3 after 7 instructions, 0 page fault(s), exit code 4 -> exit_data_5.out, deadlock_5.freq
[os] pid 5 is an orphan (pid 4 has ended): reaped by the os
[os] pid 2 (deadlock) finished on processor 1 after 12 instructions, 0 page fault(s), exit code 2 -> exit_data_2.out, deadlock_2.freq
[os] pid 1: wait() returns pid 2, exit code 2
[os] pid 1 (deadlock) finished on processor 0 after 9 instructions, 0 page fault(s), exit code 1 -> exit_data_1.out, deadlock_1.freq
[os] exit codes:
  pid  ppid  program          state      exit  instructions  reaped by
    1     0  deadlock         exited        1             9  os
    2     1  deadlock         exited        2            12  pid 1 (wait)
    3     2  deadlock         exited        3            12  pid 2 (wait)
    4     3  deadlock         error        -1             9  pid 3 (wait)
    5     4  deadlock         exited        4             7  os
[os] all tasks finished; print output is in log.txt
```

`./test.sh` ends with `35 passed, 0 failed`. `SIM=$PWD/sim ../../tests/run_tests.sh` ends with
`33 passed, 0 failed`.

## Viva questions

**Q: What is a zombie, and why does the OS keep it?**
A: A process that has ended but whose exit code its parent has not collected with wait() yet.
Its frames, processor and files are freed at once; only the table entry with the pid and exit
code remains, so a later wait() can still return them (wait_demo: pid 2).

**Q: What happens to an orphan?**
A: Its parent ended first, so nobody will wait for it. Unix re-parents it to init (pid 1),
which reaps it. Here the OS reaps it directly when it ends, and also reaps the uncollected
zombies of a parent that ends (zombie_orphan: pids 2 and 3).

**Q: How does a blocked parent get the result of wait()?**
A: When the child ends, `finish_task` sees the parent BLOCKED and writes the child's pid (and
exit code) into the parent's xD (and xS) on its processor. The parent's PC already points
past the wait, so it goes back to the ready queue and continues in the next round.

**Q: Which child does wait() return when several exist?**
A: A child that has already ended (the earliest one) is returned immediately. Otherwise the
caller blocks until the next child ends, whichever that is. In wait_demo the order is 2, 4, 3,
not the creation order 2, 3, 4.

**Q: Why can wait() deadlock in this simulator, and how does a real OS avoid it?**
A: A blocked task keeps its processor, because the base model has one task per processor and
no context switch. Four blocked parents can therefore hold all four processors while the child
they wait for is in the waiting queue. A real OS saves the blocked process in its PCB and runs
another process on the CPU (e18). Here the OS detects the state and stops the newest blocked
task, which is deadlock recovery by terminating a victim.

**Q: What exit codes do a normal end and an error give?**
A: Reaching the end of the program (HALT) gives 0, like returning 0 from main. A runtime error
such as division by zero gives -1 (Unix would report the killing signal instead). Real exit
statuses are only 8 bits (0..255); here the full 32-bit xR is kept.

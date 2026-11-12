# Q088 — Message passing: `send` and `recv`

## Question
Add message passing between tasks. `send xR xP` sends the value of xR to the task whose
pid is in xP; it never waits. `xD = recv` takes the oldest message from the running task's
mailbox into xD. If the mailbox is empty, the task blocks: it executes nothing until a
message arrives. Sending to a pid that does not exist is an error. Show a sender that
sends 5 numbers to a receiver which replies with their sum.

## Answer
- **Two instructions:** `send xR xP` = `E8 0 R P` and `xD = recv` = `E9 D 0 0`.
  `encode_statement()` recognises `send` by its keyword. It recognises `recv` as the
  right-hand side of an assignment, before the normal `x = …` parsing, which would
  reject it.
- **Mailbox:** every `Task` gets a `std::deque<int> mailbox` (oldest message first).
  It lives in the process table, so a task can receive messages before it is loaded.
- **`os_send(pid, value)`, in the OS:**
  - If pid does not exist, it returns false and the processor stops the sender with an error.
  - If the receiver is blocked in `recv` (`recv_reg[p] >= 0`), it writes the value straight
    into the waiting register `Register[p][recv_reg[p]]` and unblocks it (`recv_reg[p] = -1`).
  - Otherwise it appends the value to the mailbox.
- **`os_recv(pid, value)`:** takes the oldest message, or returns false if the mailbox is
  empty. The processor then records the destination register in `recv_reg[p]`. That
  value means "blocked".
- **Skipping slices:** `process_instructions()` runs nothing while `recv_reg[p] >= 0`. The PC
  already points past the `recv`, so when a message arrives the task simply continues.
  `recv` is never executed twice, which keeps the counts and `.freq` exact. **In lab6 a
  blocked task keeps its processor** (there is no context switch), so it sits idle.
  Two tasks that both `recv` first would deadlock, so the test uses only 2 tasks.
- **Timeline** (receiver = pid 1 runs first in each round; sender = pid 2):
  1. Round 1: the receiver blocks in its first `recv` after 5 instructions. The sender's
     first `send` writes 10 straight into the receiver's x1, and 20 goes to the mailbox.
  2. Rounds 2–3: the receiver takes 20 and stores it. The sender queues 30, 40 and 50,
     then blocks in `x5 = recv`.
  3. Round 4: the sender is blocked and executes nothing.
  4. Round 5: the receiver sends 150, which goes straight into the sender's x5, and halts.
     The sender stores 150 and halts.

  Totals: 42 and 25 instructions, exactly one pass through each program.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_SEND 0xE8` (`E8 0 R P`), `OP_RECV 0xE9` (`E9 D 0 0`) |
| `compiler.cpp` | `encode_statement` | parse `send xR xP` and `xD = recv` |
| `compiler.cpp` | `disassemble` | listing text for both |
| `processor.h/.cpp` | `recv_reg[NP]`, `reset` | destination register of a blocked `recv`, -1 = not blocked |
| `processor.cpp` | `execute` | `send` calls `os_send` (error if no such pid); `recv` calls `os_recv`, and on an empty mailbox blocks with `recv_reg[p] = D` |
| `processor.cpp` | `process_instructions` | the slice loop also stops while `recv_reg[p] >= 0` |
| `os.h/.cpp` | `struct Task`, `os_send`, `os_recv` | `mailbox` per task; deliver or queue; take the oldest |

## Test
`./test.sh`:
- **Listing:** `send x1 x2` = `E8 00 01 02` and `x5 = recv` = `E9 05 00 00`.
- **`receiver.txt` (pid 1) + `sender.txt` (pid 2):**
  - The receiver stored 10 20 30 40 50 in the order sent, and the sum 150.
  - The sender received the reply 150.
  - The counts are exactly 42 and 25 instructions, and `recv` ran 5 times: no busy waiting.
- **Errors:** sending to pid 9, which does not exist, stops that task with an error (exit code 1).
- **Old programs:** `sum_array` still gives 45.

## Viva
- **What kind of message passing is this?** Direct naming (by pid), an asynchronous `send` with an unbounded mailbox, and a blocking `recv`. With a bounded mailbox, `send` would also block when the box is full: the producer–consumer problem.
- **How can `os_send` write another task's register?** In lab6 a blocked task stays on its processor, so its registers are still in `Register[p]`. With context switching you would write into its saved PCB instead.
- **Shared memory (Q087) or messages?** Messages copy data and synchronise by themselves: `recv` waits for the data. Shared memory avoids the copy but needs a separate flag or lock.
- **What happens to a message for a finished task?** It stays in that task's mailbox and is never read. A real OS would return an error to the sender.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `msg_data.byte`, `receiver.txt`, `sender.txt`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

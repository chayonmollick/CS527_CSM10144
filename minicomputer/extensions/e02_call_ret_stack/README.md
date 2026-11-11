# e02 — Subroutines: call, ret, push, pop and a stack

## Question (as it could appear in the test)
> Add subroutines to the mini-computer. Introduce `call .label`, `ret`, `push xR` and
> `pop xR`. Each processor gets a stack pointer `SP`, initialised to the top of the task's
> data space, that grows down by 4 bytes. The stack lives in the task's logical data
> memory, so it must go through the MMU like any other data access. `call` saves the return
> address on the stack and jumps; `ret` returns to it. Report stack overflow and underflow
> as errors. Write (a) a subroutine that squares a number, called three times, and (b) a
> recursive factorial of 5.

## Solution idea
**State.** `SP[NP]` is one stack pointer per processor, like `PC[NP]`.
- `reset()` sets it to `DATA_SPACE` (4096), which means an empty stack. The first push writes bytes 4092..4095.
- The stack area is the top page of the data space: `STACK_SIZE` = `PAGESIZE` = 512 bytes, logical addresses 3584..4095, so 128 words.

**Push/pop through the MMU.** `push_word()` does `SP -= 4; [SP] = value`, and `pop_word()` does `value = [SP]; SP += 4`. Both use the existing `store_word()`/`load_word()`, so every byte goes through `getPhysicalAddress()`:
- The stack page is not in the data file, so the first push is a page fault and gets a fresh frame (Lab 5 demand mapping). That is why each example reports `1 page fault(s)`.
- **Overflow:** a push below 3584 is an error. Without the limit, the stack would grow down over the program's data.
- **Underflow:** a pop or `ret` with SP = 4096 is an error.
- `ret` also checks that the popped value is a valid instruction address (0..1023, a multiple of 4).

**call / ret.**

| Statement | Encoding | Action |
|---|---|---|
| `call .label` | `50 0 0 offset` | `push(PC)`, then `PC = this + 4*offset` |
| `ret` | `51 0 0 0` | `PC = pop()` |
| `push xR` | `52 0 0 R` | `SP -= 4; [SP] = xR` |
| `pop xR` | `53 R 0 0` | `xR = [SP]; SP += 4` |

- `fetch()` has already moved `PC` to the next instruction, so `PC` is exactly the return address to push.
- The jump uses a relative offset in instructions, exactly like a branch. The compiler resolves the label in pass 2 with the same `label - index` arithmetic, and the disassembler shows `call -6`.
- `push` keeps its register in operand 2, like `print`. `pop` writes, so its register goes in the dest byte, like a load.

**Calling convention used by the examples.**
- The argument is passed in `x1` and the result comes back in `x2`.
- A recursive routine `push`es what it still needs after the call (n) and `pop`s it afterwards.
- Because the return address is on the stack rather than in a register, calls nest to any depth (recursion).

For fact(5), at the deepest point (inside fact(0)) the stack holds 11 words, 44 bytes:
```
4092: 8   return address after "call .fact" in main (instruction 2)
4088: 5   saved n                4084: 44  return into .recurse (instruction 11: pop x1)
4080: 4   4076: 44   4072: 3   4068: 44   4064: 2   4060: 44   4056: 1   4052: 44   <- SP
```

**Output files.**
- Stack words are not program data. `push_word()` keeps `data_high` unchanged, so the `.out` file keeps the data file's size instead of growing to 4 KB.
- `finalize()` prints how deep the stack went: `used 44 bytes of stack (lowest SP = 4052)`.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | — | `STACK_SIZE` (= `PAGESIZE`); opcodes `OP_CALL` 0x50, `OP_RET` 0x51, `OP_PUSH` 0x52, `OP_POP` 0x53 |
| `processor.h/.cpp` | globals | `int SP[NP]` (stack pointer), `int stack_low[NP]` (deepest SP, for the report) |
| `processor.cpp` | `reset()` | `SP[p] = stack_low[p] = DATA_SPACE` (empty stack) |
| `processor.cpp` | `push_word()`, `pop_word()` (new) | Overflow/underflow checks; go through `store_word`/`load_word` (MMU); push keeps `data_high` |
| `processor.cpp` | `execute()` | `OP_CALL` (push PC, relative jump), `OP_RET` (pop, check, jump), `OP_PUSH`, `OP_POP` |
| `compiler.cpp` | `encode_statement()` | Parses `call .label` (label → relative offset), `ret`, `push xR`, `pop xR` |
| `compiler.cpp` | `disassemble()` | `call +3`, `ret`, `push x1`, `pop x1` |
| `os.cpp` | `finalize()` | Prints the stack depth if the task used the stack |
| `Makefile` | `test` | Runs `./test.sh`, then the 33 base checks |

Find every change with `grep -n "EXT e02" *.cpp *.h Makefile`, or read `CHANGES.diff`.

## Run
```bash
cd minicomputer/extensions/e02_call_ret_stack
make                                    # no warnings
./test.sh                               # 34 checks of this extension
SIM=$PWD/sim ../../tests/run_tests.sh   # the 33 base checks still pass
./sim -c fact.txt                       # listing: call +3 / call -6, push, pop, ret
./sim -b square.txt square_data.byte fact.txt fact_data.byte fib.txt fib_data.byte
../../tools/words.sh square_data_1.out
./sim -b -t fact.txt fact_data.byte 2>&1 | head -30     # watch the calls and returns
make clean
```
| Program | Data | Result |
|---|---|---|
| `square.txt` | 3, 12, 25 at 0, 4, 8 | squares at 12, 16, 20 (one subroutine, 3 calls) |
| `fact.txt` | n = 5 at 0 | n! at 4 (recursion: push/pop n around the call) |
| `fib.txt` | n = 10 at 0 | fib(n) at 4 (two recursive calls per level) |

## Expected output
`./test.sh` ends with `34 passed, 0 failed`, and the base tests with `33 passed, 0 failed`.

```
$ ./sim -b square.txt square_data.byte fact.txt fact_data.byte fib.txt fib_data.byte
...
[os] pid 1 (square) finished on processor 0 after 17 instructions, 1 page fault(s) -> square_data_1.out, square_1.freq
[os] pid 1 used 4 bytes of stack (lowest SP = 4092)
[os] pid 2 (fact) finished on processor 1 after 49 instructions, 1 page fault(s) -> fact_data_2.out, fact_2.freq
[os] pid 2 used 44 bytes of stack (lowest SP = 4052)
[os] pid 3 (fib) finished on processor 2 after 1417 instructions, 1 page fault(s) -> fib_data_3.out, fib_3.freq
[os] pid 3 used 76 bytes of stack (lowest SP = 4020)
[os] all tasks finished; print output is in log.txt

$ ../../tools/words.sh square_data_1.out
     0 (0x0000): 03 00 00 00  = 3
     4 (0x0004): 0C 00 00 00  = 12
     8 (0x0008): 19 00 00 00  = 25
    12 (0x000C): 09 00 00 00  = 9
    16 (0x0010): 90 00 00 00  = 144
    20 (0x0014): 71 02 00 00  = 625
$ ../../tools/words.sh fact_data_2.out 4
     4 (0x0004): 78 00 00 00  = 120
$ ../../tools/words.sh fib_data_3.out 4
     4 (0x0004): 37 00 00 00  = 55

$ cat fact_2.freq                      (6 calls of .fact: 5 recursive + 1 base case)
1	x1 = [0]
1	call .fact
1	[4] = x2
1	BAL .end
.fact
6	x9 = x1 - 0
6	BNE .recurse      % n != 0
1	x2 = 1            % fact(0) = 1
1	ret
.recurse
5	push x1           % save n: the recursive call changes x1
5	x1 = x1 - 1
5	call .fact        % x2 = fact(n - 1)
5	pop x1            % n again
5	x2 = x2 * x1      % n * fact(n - 1)
5	ret
.end
```
Errors (both stop only the faulty task; exit code 1):
```
.f / call .f   ->  error at instruction 0: stack overflow: more than 512 bytes pushed (endless recursion?)
                   [os] pid 1 used 512 bytes of stack (lowest SP = 3584)
x1 = 1 / ret   ->  error at instruction 1: stack underflow: pop or ret with an empty stack
```

## Viva questions
1. **Which return address does `call` push, and why is it `PC`?**
   - `fetch()` has already set `PC = instr_pc + 4`, the instruction after the call, so pushing `PC` is enough.
   - It is a logical byte address in the instruction space: 8 for the call at instruction 1.
2. **Why must the return address go on a stack and not in one link register?**
   - A second call would overwrite a single register. With recursion every level needs its own return address, and a stack gives that (last in, first out).
   - With a link register (ARM `LR`), the callee has to push it itself before calling anything.
3. **Why does `fact` push x1 around the recursive call?**
   - The call changes x1 (it passes n − 1), but n is needed afterwards for `n * fact(n-1)`. This is a caller-saved register.
   - `fib` also pushes its first result (x2) before the second call.
4. **How does the stack interact with paging?**
   - Push and pop use `store_word`/`load_word`, so they are ordinary data accesses through `getPhysicalAddress()`.
   - Logical page 7 of the data space (3584..4095) is not in the data file, so the first push is a page fault and gets a zeroed frame. The other tasks' stacks are in their own frames, because every task has its own page table.
5. **What happens on overflow, and why is a limit needed?**
   - Below `DATA_SPACE - STACK_SIZE` (3584) the push stops the task with an error. Otherwise the stack would silently overwrite the program's data area.
   - Endless recursion hits the limit after 128 words.
6. **Why `SP[NP]` and not one `SP`?**
   - The NP processors run different tasks, interleaved by the round-robin scheduler. Like `PC[NP]` and `Register[NP][256]`, every processor needs its own SP, or the tasks' stacks would mix.
   - The test runs 3 recursive programs at once to check this.

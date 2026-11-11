# e04 — Base + offset addressing `[xA + c]`

## Question (as it could appear in the test)
> Add base + offset addressing for scalar and vector memory instructions:
> `xD = [xA + c]`, `[xA + c] = xS`, `vD = [xA + c]` and `[xA + c] = vS`, with c = 0..255.
> The instruction format has only three bytes after the opcode: explain how you encode the
> base register, the offset and the data register. Use it on an array of (key, value)
> records: walk the array with one pointer and read both fields with offsets 0 and 4,
> computing the sum of all values.

## Solution idea
**Encoding: 3 bytes are enough.** A plain memory instruction already uses two register
bytes, and the handout fixes its operand-1 byte at 0 (`05 D 0 A`, `06 A 0 S`). The offset
goes into that unused byte, and everything else stays where it was:

| Statement | Encoding | Base instruction it extends |
|---|---|---|
| `xD = [xA + c]` | `70 D c A` | `05 D 0 A` (`xD = [xA]`) |
| `[xA + c] = xS` | `71 A c S` | `06 A 0 S` (`[xA] = xS`) |
| `vD = [xA + c]` | `72 D c A` | `25 D 0 A` (`vD = [xA]`) |
| `[xA + c] = vS` | `73 A c S` | `26 A 0 S` (`[xA] = vS`) |

The offset is one unsigned byte, so it is 0..255. Negative offsets (`[x2 - 4]`) would need
a signed byte (−128..127); here they are a compile error.

**Compiler.**
- `x1 = [x2 + 4]` tokenizes to `[` `x2` `+` `4` `]`.
- `parse_operand()` recognises that 5-token shape before the 3-token `[x]`/`[c]` forms and returns a new operand kind `MEM_XC`, which carries the base register in `value` and the offset in `offset`.
- `encode_move()` turns `MEM_XC` into `LOADO`/`VLOADO` as a source, or `STOREO`/`VSTOREO` as a destination.
- These are errors: the base must be an x register, the offset a constant 0..255, and only `+` is allowed.

**Processor.**
- The effective address `xA + c` is computed in `execute()` and then used exactly like a plain address. `load_word`/`store_word` still go through the MMU byte by byte, so pages, demand mapping and page-crossing words all work as before.
- The addition is unsigned, so a huge xA wraps to a negative address that the MMU rejects; no signed overflow happens.
- The vector cases reuse the base `VLOAD`/`VSTORE` code; only the address line is new.

**Why it helps.** Each field of a record needs its own address. With only `[xA]`, a second
register and an extra `add` are needed per field. With `[xA + c]`, one pointer reaches every
field:

| Program | Executed instructions (5 records) |
|---|---|
| `records.txt` (`[x2 + 0]`, `[x2 + 4]`) | **61** |
| `records_plain.txt` (`[x2]`, then `x8 = x2 + 4` and `[x8]`) | 67 (one extra add per record + 1) |

In the vector example `vrecords.txt`, plain addressing would need two extra adds per record
(for b at +32 and c at +64).

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_LOADO` 0x70, `OP_STOREO` 0x71, `OP_VLOADO` 0x72, `OP_VSTOREO` 0x73 |
| `compiler.cpp` | `struct Operand` | New kind `MEM_XC` = `[xA + c]` and field `offset` |
| `compiler.cpp` | `parse_operand()` | Parses `[ xA + c ]`: base must be x, offset must be a constant 0..255 |
| `compiler.cpp` | `encode_move()` | `MEM_XC` as source → LOADO/VLOADO; as destination → STOREO/VSTOREO |
| `compiler.cpp` | `disassemble()` | `x5 = [x2 + 4]`, `[x2 + 64] = v3`, … |
| `processor.cpp` | `offset_address()` (new) | `xA + c` computed without signed overflow |
| `processor.cpp` | `execute()` | `OP_LOADO`, `OP_STOREO`; `OP_VLOADO`/`OP_VSTOREO` share the vector load/store code |
| `Makefile` | `test` | Runs `./test.sh`, then the 33 base checks |

Find every change with `grep -n "EXT e04" *.cpp *.h Makefile`, or read `CHANGES.diff`.

## Run
```bash
cd minicomputer/extensions/e04_base_offset_addressing
make                                    # no warnings
./test.sh                               # 28 checks of this extension
SIM=$PWD/sim ../../tests/run_tests.sh   # the 33 base checks still pass
./sim -c records.txt                    # 70 05 00 02 = x5 = [x2 + 0], 70 06 04 02 = x6 = [x2 + 4]
./sim -b records.txt records_data.byte records_plain.txt records_data.byte vrecords.txt vrecords_data.byte
../../tools/words.sh records_data_1.out
cat records_1.freq
make clean
```
| Program | Data | Result |
|---|---|---|
| `records.txt` | N = 5 at 0, key K = 30 at 4, records (key, value) from 8: (10,100) (20,250) (30,−7) (40,1000) (50,3) | sum of values at 8+8N, value of key K at 12+8N |
| `records_plain.txt` | same | same results with `[xA]` only (for the comparison) |
| `vrecords.txt` | N = 2 at 0, records of a[8], b[8], c[8] (96 bytes) from 4 | c = a + b in each record (offsets 0, 32, 64) |

## Expected output
`./test.sh` ends with `28 passed, 0 failed`, and the base tests with `33 passed, 0 failed`.
```
$ ./sim -b records.txt records_data.byte records_plain.txt records_data.byte vrecords.txt vrecords_data.byte
...
[os] pid 3 (vrecords) finished on processor 2 after 23 instructions, 0 page fault(s) -> vrecords_data_3.out, vrecords_3.freq
[os] pid 1 (records) finished on processor 0 after 61 instructions, 0 page fault(s) -> records_data_1.out, records_1.freq
[os] pid 2 (records_plain) finished on processor 1 after 67 instructions, 0 page fault(s) -> records_data_2.out, records_plain_2.freq

$ ../../tools/words.sh records_data_1.out
     0 (0x0000): 05 00 00 00  = 5
     4 (0x0004): 1E 00 00 00  = 30
     8 (0x0008): 0A 00 00 00  = 10
    12 (0x000C): 64 00 00 00  = 100
   ...
    40 (0x0028): 32 00 00 00  = 50
    44 (0x002C): 03 00 00 00  = 3
    48 (0x0030): 42 05 00 00  = 1346        <- 100 + 250 - 7 + 1000 + 3
    52 (0x0034): F9 FF FF FF  = -7          <- value stored with key 30
$ ../../tools/words.sh vrecords_data_3.out 68      (c[0] of record 0 = 1 + 10)
    68 (0x0044): 0B 00 00 00  = 11
$ ../../tools/words.sh vrecords_data_3.out 164     (c[0] of record 1 = 100 - 1)
   164 (0x00A4): 63 00 00 00  = 99

$ cat records_1.freq
1	x1 = [0]          % records left
1	x7 = [4]          % key to look up
1	x2 = 8            % pointer to the current record
1	x3 = 0            % sum of the values
1	x4 = 0            % value found for key K
.loop
6	x9 = x1 - 0
6	BEQ .done
5	x5 = [x2 + 0]     % key
5	x6 = [x2 + 4]     % value
5	x3 = x3 + x6
5	x9 = x5 - x7
5	BNE .next
1	x4 = x6           % this record has key K
.next
5	x2 = x2 + 8       % next record: one pointer update for both fields
5	x1 = x1 - 1
5	BAL .loop
.done
1	[x2 + 0] = x3     % sum, right after the last record
1	[x2 + 4] = x4     % value of key K
```
Compile errors: `x1 = [x2 + x3]` → `base + offset must be [x<n> + <constant 0..255>]`;
`x1 = [x2 + 256]` → `constant 256 is out of range 0..255`; `x1 = [x2 - 4]` →
`expected [x<n>], [<constant>] or [x<n> + <constant>]`.

## Viva questions
1. **Where did the offset byte come from?**
   - The plain memory instructions leave operand 1 at 0, as the handout requires (`05 D 0 A`).
   - Base + offset puts c there and keeps D/A/S in their old places, so the decoder hardly changes.
2. **Why only 0..255, and how would you allow negative offsets?**
   - It is one byte, read as unsigned.
   - For −128..127, read it as `(signed char)src1`, as branch offsets already do, and accept `[xA - c]` in `parse_operand`. For a bigger range, put the offset in a register: `[xA + xB]`, a new opcode.
3. **What does `[x2 + 4]` save compared to `[x2]`?**
   - An address register and an add per field access. In `records.txt` that is 61 vs 67 executed instructions, and one pointer increment per record instead of one per field.
   - The same idea is behind `ldr r1, [r2, #4]` (ARM) and `lw x1, 4(x2)` (RISC-V).
4. **Does the MMU see anything different?**
   No. The processor adds base + offset first, and `getPhysicalAddress()` only ever sees the final logical address. Range checks, demand paging and page-crossing words behave exactly as before; the test reads a word across the data page 0/1 boundary.
5. **How is `[x2 + 4] = x1` (a store) told apart from `x1 = [x2 + 4]` (a load)?**
   By the side of `=` the `[ ]` is on. `encode_move()` switches on the destination's kind: a `MEM_XC` destination is a store (`71 A c S`); a `MEM_XC` source with a register destination is a load (`70 D c A`).

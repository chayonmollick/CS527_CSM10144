# e14 — Page-table entry flags (VALID, DIRTY, REFERENCED) and memory protection

## Question (as it could appear in the test)
A page-table entry is one byte, but a frame number needs only a few bits. Use the spare
bits: frame number in bits 0-4, bit 5 REFERENCED, bit 6 DIRTY, bit 7 VALID. The MMU must
set REFERENCED on every access and DIRTY on every write. Add a `STRICT_MEMORY` switch: when
it is 1, touching a data page that is not part of the data file is a segmentation fault
(the task is stopped) instead of being demand-mapped. At the end, report how many data
pages were referenced and dirtied, and copy back only the DIRTY pages over the original
data image. Show that the output is still correct.

## Solution idea
- **Entry layout:** `V D R f f f f f` (`config.h`: `PTE_VALID 0x80`, `PTE_DIRTY 0x40`,
  `PTE_REFERENCED 0x20`, `PTE_FRAME_MASK 0x1F`). Mapped now means VALID = 1, so the frame
  value 0 no longer has to mean "unmapped". Frame 0 still holds the page tables and is
  never handed out (`freePages[0] = 1`). A `static_assert` checks that 16 frames fit in 5 bits.
- **Read or write?** The MMU can only set DIRTY if it knows the access is a write. Only
  the processor knows that, so `getPhysicalAddress()` gets a 4th parameter,
  `int isWrite = 0`. The default value keeps the fetch and load calls unchanged, and
  `store_word()` passes 1. Real hardware gets this from the bus's read/write signal.
  A separate `getPhysicalAddressForWrite()` would work too, but would duplicate the
  translation code.
- **MMU** (`getPhysicalAddress`): read the entry.
  - If VALID = 0: a fetch is an error (as before). With `STRICT_MEMORY 1` a data access is
    a **segmentation fault** (`processor_error`, task stopped). Otherwise demand-map a zero
    frame with entry `VALID | frame`.
  - Then `entry |= REFERENCED`, and on a write also `|= DIRTY`, and store the byte back
    in frame 0.
  - The physical address uses `entry & 0x1F`.
- **Loader** (`copy_in`): entries are `VALID | frame`: clean and not yet referenced.
  STRICT_MEMORY needs no extra list of allowed pages. The pages of the data file are
  exactly the VALID ones at load time.
- **finalize():**
  - The output starts as a copy of the original data image `t.data`, zero-extended to
    the output size.
  - A page with DIRTY = 0 was never written, so its frame still equals that image (or
    zeros beyond the file). Only DIRTY pages are copied from memory.
  - It counts mapped / referenced / dirty data pages and prints them.
  - It frees frames with `entry & 0x1F`. The raw byte is now e.g. `0xE3`, so the old
    `freePages[entry]` would index far outside the array.
- **Expected numbers:**
  - **sum_array:** 1 page, read and written (`[x3] = x2`), so `1 1 1`.
  - **pages.txt:** pages 0, 1, 3, 7 all written, so `4 4 4`.
  - **partial.txt** (3-page data file, A in page 1, B in page 2, result in page 0):
    3 referenced, only page 0 dirty. One page is copied back and two are skipped, and
    A and B are still correct in the output.
  - **Read-only program** (`x1 = [0]; print x1`): 1 referenced, 0 dirty. The `.out` file
    is byte-for-byte the data file.
- **STRICT_MEMORY 1:** pages.txt dies at instruction 3 (`[x3] = x1` with x3 = 1020, data
  page 1). A *load* from a missing page faults too: the address is 2043, because
  `load_word` reads the bytes from high to low. Protection works per page: `[100]` is
  past the end of sum_array's 40-byte file but inside page 0, so it is allowed (as with a
  real segmentation fault).

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | | `PTE_FRAME_MASK 0x1F`, `PTE_REFERENCED 0x20`, `PTE_DIRTY 0x40`, `PTE_VALID 0x80`, `STRICT_MEMORY 0` (overridable with `-D`) |
| `os.h` | `getPhysicalAddress()` | new parameter `int isWrite = 0` |
| `os.cpp` | (top) | `static_assert`: frame numbers fit in bits 0-4 |
| `os.cpp` | `getPhysicalAddress()` | VALID decides "mapped"; STRICT → segmentation fault; new entry `VALID \| frame`; sets REFERENCED always and DIRTY on writes; frame = `entry & 0x1F` |
| `os.cpp` | `copy_in()` | loader writes `VALID \| frame` |
| `os.cpp` | `finalize()` | output = original image + DIRTY pages only; counts mapped/referenced/dirty; frees `entry & 0x1F` |
| `processor.cpp` | `store_word()` | calls `getPhysicalAddress(..., 1)` (write) |
| `Makefile` | `test` | runs `./test.sh`, then the 33 base checks |

## Run
```bash
make
./sim -b partial.txt partial_data.byte
./sim -b ../../examples/pages.txt ../../examples/pages_data.byte
make clean && make CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DSTRICT_MEMORY=1"   # strict build
./sim -b ../../examples/pages.txt ../../examples/pages_data.byte              # segmentation fault
make clean && make
./test.sh                                    # 16 checks (builds the strict version itself)
SIM=$PWD/sim ../../tests/run_tests.sh        # 33 passed (the strict build: 30, the 3 pages.txt checks fail by design)
```

## Expected output
```
$ ./sim -b partial.txt partial_data.byte
[os] pid 1 (partial) loaded on processor 0: 1 instruction + 3 data page(s), 11 frame(s) left
[os] pid 1 (partial) finished on processor 0 after 90 instructions, 0 page fault(s) -> partial_data_1.out, partial_1.freq
[os] pid 1 data pages: 3 mapped, 3 referenced, 1 dirty -> copied back 1 dirty page(s), skipped 2 clean
$ ./sim -b ../../examples/pages.txt ../../examples/pages_data.byte
[os] pid 1 (pages) finished on processor 0 after 15 instructions, 3 page fault(s) -> pages_data_1.out, pages_1.freq
[os] pid 1 data pages: 4 mapped, 4 referenced, 4 dirty -> copied back 4 dirty page(s), skipped 0 clean
$ (STRICT_MEMORY 1) ./sim -b ../../examples/pages.txt ../../examples/pages_data.byte
[pid 1, processor 0] error at instruction 3: segmentation fault: data address 1020 is in data page 1, which is not part of the task's data
[os] pid 1 (pages) STOPPED WITH AN ERROR on processor 0 after 4 instructions, 0 page fault(s) -> pages_data_1.out, pages_1.freq
[os] pid 1 data pages: 1 mapped, 0 referenced, 0 dirty -> copied back 0 dirty page(s), skipped 1 clean

$ ./test.sh
e14 (STRICT_MEMORY 0): flags in the page-table entry, only DIRTY pages copied back
  ok    sum_array result 45
  ok    sum_array: 1 data page, referenced and dirty (the final store)
  ok    pages: 7 + 7 + 7 = 21, 7 at 3825 (demand-mapped pages are still allowed)
  ok    pages: data pages 0, 1, 3, 7 mapped, all referenced and dirty
  ok    partial: dot product 1*10 + 2*20 + ... + 8*80 = 2040
  ok    partial: 3 pages mapped and referenced, only page 0 dirty -> 1 copied, 2 skipped
  ok    partial: the skipped clean pages are still correct (A[7] = 8, B[7] = 80)
  ok    read-only program: 3 mapped, 1 referenced, 0 dirty -> nothing copied back
  ok    read-only program: the .out file is identical to the data file
  ok    4 tasks at once: each processor's page table has its own flags
e14 (STRICT_MEMORY 1): a data page that is not in the data file is a segmentation fault
  ok    pages.txt stops (exit code 1)
  ok    store to address 1020 (data page 1) -> segmentation fault, at instruction 3
  ok    the task is reported as stopped
  ok    a load from data page 3 (not in the 3-page file) faults too (bytes are read high first: 2043)
  ok    partial.txt only uses pages of its data file: still runs, 2040
  ok    protection is per page: [100] is past the 40-byte file but in page 0 -> allowed

16 passed, 0 failed
```
In the strict run, page 0 shows "0 referenced": pages.txt touches page 1 first and dies there.

## Viva questions
- **Why does the MMU need to know read vs write?** DIRTY must be set at the moment of the
  write. The translation function only sees an address, so the access type has to be
  passed in (`isWrite`). In hardware that is the R/W line of the memory request.
- **What does the VALID bit buy us?** "Unmapped" no longer has to be encoded as frame 0, so
  0 becomes a legal frame value. Frame 0 is still reserved, because it holds the page
  tables. Without VALID, a mapping to frame 0 would look exactly like "unmapped".
- **Who clears REFERENCED, and what is it for?** Here nobody, so it means "touched at
  least once". A clock / second-chance replacer clears R periodically and evicts pages
  whose R stayed 0. That gives approximate LRU with one bit instead of a timestamp (e13
  could use it).
- **Why is it safe to skip clean pages in finalize()?** DIRTY is set on every store, so
  DIRTY = 0 means no byte of the page changed since it was loaded from the data file (or
  zero-filled). The same idea saves page-outs: a clean victim's backing-store copy is
  still valid, so it can just be dropped.
- **Why is a write at [100] not a segmentation fault, even though the file is 40 bytes?**
  Protection works on whole pages: page 0 is valid, so every byte of it is accessible.
  Real systems behave the same way (overflows inside a page go unnoticed; guard pages or
  tools like AddressSanitizer catch them).
- **How many frames can 5 bits address?** 32. That means MEMSIZE up to 16 KB with
  512-byte pages. The `static_assert` stops the build if `NUM_PHYSICAL_PAGES` exceeds that.

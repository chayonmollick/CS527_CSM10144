# e12 — Data cache (direct-mapped, write-back or write-through)

## Question (as it could appear in the test)
Add a data cache to every processor, indexed by the PHYSICAL address: direct-mapped,
`CACHE_LINES 16` lines of `LINE_SIZE 16` bytes. Instruction fetches bypass the cache.
Support two write policies selected by a switch: write-through with no-write-allocate,
and write-back with write-allocate. Count hits, misses and write-backs per task. At the
end of a task, write back the dirty lines so that the output file is correct. Report the
hit rate for `sum_array` and for a program that walks memory with a stride.

## Solution idea
- **Line, tag, offset:** for a physical address `pa`:
  - offset = `pa % 16`
  - line index = `(pa / 16) % 16`
  - tag = `pa / 256`

  A line is `{valid, dirty, tag, data[16]}` and keeps its own copy of the bytes. So with
  write-back the newest value can be in the cache and **not yet in `memory[]`**.
- **Where:** `dcache[NP][CACHE_LINES]` in `processor.cpp`. `load_word` / `store_word` still
  translate with `getPhysicalAddress()`, then read/write each byte through `cache_read()` /
  `cache_write()`. `fetch()` keeps using `read_byte()` → `memory[]` directly (bypass).
- **Why physical:** two tasks use the same logical addresses, so a logical cache would
  need a flush per task switch or address-space tags. Physical addresses are unique.
  Frames are never shared, so the per-processor caches need no coherence.
- **Miss:** `fill_line()` first writes back the old line if it is dirty, then copies the
  16 bytes of the new line from memory.
- **Write policies** (`WRITE_BACK` in `config.h`):

  | | hit | miss |
  |---|---|---|
  | write-back + write-allocate (1) | write the line, mark it dirty | fetch the line, then as a hit |
  | write-through + no-write-allocate (0) | write the line **and** memory | write memory only, do not fetch the line |

  Write-through lines are never dirty, so it has 0 write-backs.
- **Counting:** one access = one word loaded or stored (a vector load = 8 accesses). A
  word is a hit only if all 4 of its bytes were already cached. Write traffic is reported
  as "bytes written to memory": 16 per write-back, 4 per word written through.
- **Flush at task end:** `finalize()` (os.cpp) calls `cache_flush(p)` **before** it reads
  the task's data out of `memory[]`. That writes back every dirty line, then invalidates
  all lines, because the frames are freed and another task may get them.
  `reset()` also empties the cache when a task is loaded.
  Tried without the flush: sum_array's output keeps the old value 9 at address 36
  instead of the sum 45, because the store is still sitting in a dirty line.
- **Expected numbers** (all hand-derived, all checked by `test.sh`):
  - **sum_array:** words at 0, 4, ..., 36 and a store at 36 = 11 accesses in lines 0, 1, 2.
    One miss per line, so 3 misses and 8 hits = **72.7% (~3/4)**. WB: the line with the
    sum is dirty, so 1 write-back at the end.
  - **stride.txt, S = 4, N = 32:** 4 words per line, so 1 miss + 3 hits per line: 8 misses
    for the array, plus `[0]`. 35 accesses, 9 misses = 74.3%.
  - **stride.txt, S = 16, N = 15:** each load is in a new line, so every load misses:
    18 accesses, 16 misses = **11.1%** (only `[4]` and `[8]` hit, in line 0).
  - **dot.txt, D = 256:** b is exactly one cache size after a, so `a[i]` and `b[i]` have
    the same index and different tags. Each load evicts the other one and all 32 loop
    loads miss: 5.7%. With **D = 272** (one line of padding) it is 74.3% again. These are
    *conflict misses*: the cache has room, but the direct mapping forces the two lines
    into one slot.
  - **memsum.txt (N = 64):** the running sum `[8]` is loaded and stored 64 times, and it
    stays in line 0. WT writes 64 x 4 = 256 bytes. WB writes the line only when it is
    evicted. The last 4 array words (256..271) also map to line 0, so the dirty line is
    written back 4 times + once at the end = 80 bytes. The hit rate is the same for both
    policies (87.6%), because all stores hit.
  - **array_add, WT:** no-write-allocate means the 16 stores to C never bring C's lines
    into the cache, so all 16 miss: 25 misses vs 13 for WB.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | | `CACHE_LINES 16`, `LINE_SIZE 16`, `WRITE_BACK 1` (overridable with `-DWRITE_BACK=0`) |
| `processor.cpp` | (globals) | `struct CacheLine`, `dcache[NP][CACHE_LINES]`, counters `cache_hits/misses/writebacks`, `memory_bytes_written` |
| `processor.cpp` | `line_index()`, `line_tag()`, `in_cache()`, `write_back()`, `fill_line()` | new: address split, lookup, write back a dirty line, fill on a miss |
| `processor.cpp` | `cache_read()`, `cache_write()` | new: byte access through the cache; `cache_write` implements both policies |
| `processor.cpp` | `cache_flush()` | new: write back dirty lines, invalidate all; returns the number written |
| `processor.cpp` | `load_word()`, `store_word()` | bytes go through the cache; one hit/miss counted per word |
| `processor.cpp` | `reset()` | empty cache, zero counters for the new task |
| `processor.h` | | `extern` counters, `cache_flush()` prototype |
| `os.cpp` | `finalize()` | `cache_flush(p)` first; prints `D-cache (...): accesses, hits, misses, hit rate, write-backs, bytes` |
| `Makefile` | `test` | runs `./test.sh`, then the 33 base checks |

## Run
```bash
make
./sim -b ../../examples/sum_array.txt ../../examples/sum_array_data.byte
./sim -b stride.txt stride4_data.byte          # stride 4: 3/4 hits
./sim -b stride.txt stride16_data.byte         # stride 16: no hits in the loop
./sim -b dot.txt dot256_data.byte              # conflict misses
./sim -b dot.txt dot272_data.byte              # padded: conflicts gone
./sim -b memsum.txt memsum_data.byte           # write-back vs write-through traffic
make clean && make CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DWRITE_BACK=0"   # write-through build
./test.sh                                      # 22 checks (builds both policies itself)
SIM=$PWD/sim ../../tests/run_tests.sh          # 33 passed (also with the write-through build)
```

## Expected output
```
$ ./sim -b ../../examples/sum_array.txt ../../examples/sum_array_data.byte
[os] pid 1 D-cache (write-back): 11 accesses, 8 hits, 3 misses, hit rate 72.7%, 1 write-back(s) (1 at the end), 16 bytes written to memory
$ ./sim -b stride.txt stride4_data.byte
[os] pid 1 D-cache (write-back): 35 accesses, 26 hits, 9 misses, hit rate 74.3%, 1 write-back(s) (1 at the end), 16 bytes written to memory
$ ./sim -b stride.txt stride16_data.byte
[os] pid 1 D-cache (write-back): 18 accesses, 2 hits, 16 misses, hit rate 11.1%, 1 write-back(s) (1 at the end), 16 bytes written to memory
$ ./sim -b dot.txt dot256_data.byte
[os] pid 1 D-cache (write-back): 35 accesses, 2 hits, 33 misses, hit rate 5.7%, 1 write-back(s) (1 at the end), 16 bytes written to memory
$ ./sim -b dot.txt dot272_data.byte
[os] pid 1 D-cache (write-back): 35 accesses, 26 hits, 9 misses, hit rate 74.3%, 1 write-back(s) (1 at the end), 16 bytes written to memory
$ ./sim -b memsum.txt memsum_data.byte
[os] pid 1 D-cache (write-back): 193 accesses, 169 hits, 24 misses, hit rate 87.6%, 5 write-back(s) (1 at the end), 80 bytes written to memory
$ (write-through build) ./sim -b memsum.txt memsum_data.byte ; ./sim -b array_add.txt array_add_data.byte
[os] pid 1 D-cache (write-through): 193 accesses, 169 hits, 24 misses, hit rate 87.6%, 0 write-back(s) (0 at the end), 256 bytes written to memory
[os] pid 1 D-cache (write-through): 49 accesses, 24 hits, 25 misses, hit rate 49.0%, 0 write-back(s) (0 at the end), 64 bytes written to memory

$ ./test.sh
e12 write-back: sum_array (sequential words, 4 per line)
  ok    sum_array result 45 (the dirty line was flushed before the output was written)
  ok    sum_array: 11 accesses, 8 hits, 3 misses, 1 write-back (at the end), 16 bytes
e12 write-back: stride.txt (same program, stride 4 vs 16 bytes)
  ok    stride 4, N = 32: sum 1..32 = 528
  ok    stride 4: 35 accesses, 26 hits, 9 misses
  ok    stride 16, N = 15: sum 1+5+..+57 = 435
  ok    stride 16: 18 accesses, 2 hits, 16 misses
e12 write-back: dot.txt (conflict misses of a direct-mapped cache)
  ok    b = a + 256: dot product 1^2+..+16^2 = 1496
  ok    b = a + 256: 35 accesses, 2 hits, 33 misses
  ok    b = a + 272: same result 1496
  ok    b = a + 272 (one line of padding): 35 accesses, 26 hits, 9 misses
e12 write-back: memsum.txt (running sum kept in memory at [8], N = 64)
  ok    memsum result 1+..+64 = 2080
  ok    memsum: 193 accesses, 169 hits, 24 misses, 5 write-backs (1 at the end), 80 bytes
e12 write-back: array_add (vector loads/stores, base example)
  ok    array_add C[0] = 101, C[15] = 131
  ok    array_add: 49 accesses, 36 hits, 13 misses, 5 write-backs, 80 bytes
e12 write-back: 4 tasks at once (each processor has its own cache)
  ok    4 tasks: same statistics as alone
  ok    4 tasks: all results correct
e12 write-through + no-write-allocate (built with -DWRITE_BACK=0)
  ok    WT sum_array result 45
  ok    WT sum_array: same 8 hits, 0 write-backs, 4 bytes (one store)
  ok    WT memsum result 2080
  ok    WT memsum: same hits, 0 write-backs, 64 stores x 4 = 256 bytes
  ok    WT array_add C[0] = 101, C[15] = 131
  ok    WT array_add: 49 accesses, 24 hits, 25 misses, 16 stores x 4 = 64 bytes

22 passed, 0 failed
```

## Viva questions
- **Why must the cache be flushed before `finalize()` writes the output?** `finalize()`
  reads `memory[]`. With write-back, the last stores may only be in dirty lines. Without
  the flush, sum_array's `.out` shows 9 at address 36 instead of 45.
- **Why also invalidate at task end, not only write back?** The task's frames are freed
  and loaded with another task's data (the loader writes `memory[]` directly). If this
  processor kept those lines valid, a later task that gets the same frame could hit on
  stale data.
- **How do you rebuild the physical address of a line for a write-back?**
  `(tag * CACHE_LINES + index) * LINE_SIZE`. Index and tag together are the line address.
- **Why is the stride-16 hit rate 0 in the loop, and what would a bigger line do?** Each
  load uses 4 of the 16 bytes brought in. A 32-byte line would help stride 16 (1 hit per 2
  loads) but waste more bandwidth. Spatial locality decides.
- **Direct-mapped vs set-associative?** dot.txt with D = 256 thrashes one slot while 14
  lines stay unused. A 2-way set-associative cache could keep `a[i]` and `b[i]` together.
  The cost is two tag compares per access and a replacement choice (LRU).
- **When is write-through better?** It is simpler and memory is always current, so the
  flush only invalidates and nothing is lost on a crash. It also wins for data that is
  written once and not re-read: array_add WT wrote 64 bytes, WB 80 (whole lines).
  Write-back wins when a word is written repeatedly (memsum: 80 vs 256 bytes).

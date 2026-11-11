# e11 — Per-processor TLB

## Question (as it could appear in the test)
Every memory access in the simulator reads the page table in frame 0 first, so each
access really costs two memory accesses. Add a TLB to every processor: `TLB_ENTRIES 4`
entries, fully associative, LRU replacement, each holding *logical page index -> frame*.
`getPhysicalAddress()` must look in the TLB first and read the page-table entry only on a
miss (then fill the TLB). Flush the TLB when a task is loaded on the processor. Count TLB
hits and misses per task and print the hit rate when the task finishes. Show a program
whose working set fits in the TLB and one that does not.

## Solution idea
- **Where the TLB lives:** `tlb[NP][TLB_ENTRIES]` in `os.cpp`, next to the MMU
  (`getPhysicalAddress`). It is per processor, like `PTBR[NP]`. An entry is
  `{valid, page, frame, last_used}`. `page` is the page-table index (0-1 code, 2-9 data),
  so one unified TLB serves both fetches and data.
- **Lookup:** fully associative, so compare `page` with all 4 entries. On a hit, stamp
  `last_used = ++tlb_clock[p]` and return the frame. The page table is not touched.
- **Miss:** read `memory[PTBR[p] + index]` as before. If the page is unmapped, the base
  demand-maps a zeroed frame. Then `tlb_insert()` puts the translation in a free entry,
  or else in the entry with the smallest `last_used` (LRU). A just-demand-mapped page is
  inserted after its page-table entry is written, so it is "updated" automatically.
  Only valid translations are ever cached, so the TLB can never hold a stale "unmapped" entry.
- **Flush:** `initialize()` (called by the loader) invalidates all entries of that
  processor, because they belong to the previous task (same logical page numbers, other
  frames). Tasks never move between processors here, so no flush is needed per time slice.
- **Per word, not per byte:** the base translated every byte (4 calls per load). Counted
  per byte, each word would give 1 miss + 3 guaranteed hits, so the hit rate would be at
  least 75% whatever the program does. Hardware translates once per access. So
  `translate_word()` in `processor.cpp` translates once when the 4 bytes are in one page.
  Because `PAGESIZE` is a multiple of 4, that covers every fetch and every aligned
  load/store. Only an unaligned word that crosses a page boundary falls back to byte by
  byte. Result: **one lookup per instruction fetch and one per data word** (a vector
  load/store = 8 words = 8 lookups).
- **Expected numbers, sum_array:** 71 instructions = 71 fetch lookups, all on code page 0.
  Data words: 1 + 9 loads + 1 store = 11, all on data page 0. Misses: the first fetch and
  the first data access only. So 82 lookups, 2 misses, 97.6%.
- **Expected numbers, tlb_walk.txt** (K data pages, R passes reading one word per page;
  instructions = 7K + 14 + R(7K + 8)):
  - The code page is used by every fetch, so it is always the most recently used entry
    and LRU never evicts it. That leaves **3 entries for data**.
  - **K = 3:** 4 misses (code page + 3 data pages), then every lookup hits. Data lookups
    are 2 + K + RK + 1 = 18, of which 15 hit.
  - **K = 5 (or 4):** the pages are read in a cycle of K > 3. LRU always evicts exactly
    the page needed next, so every read misses. Misses = code 1 + `[0]` 1 + fill pages
    1..K-1 + RK reads + final `[8]` 1 = 3 + K + RK. That is 27 for K = 5, R = 4 (22 for K = 4).
    Only `[4]` and the fill store to page 0 hit.
- **TLB reach** = entries x page size = 4 x 512 = 2 KB. A loop touching more distinct
  pages than the TLB holds thrashes it, even though all pages are in memory (only 4 page faults).

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | | `#define TLB_ENTRIES 4` |
| `os.cpp` | (globals) | `struct TlbEntry`, `tlb[NP][TLB_ENTRIES]`, `tlb_clock[NP]`, `tlb_hits/tlb_misses[NP][2]` (data, fetch) |
| `os.cpp` | `tlb_flush()`, `tlb_lookup()`, `tlb_insert()` | new: invalidate all / associative search + LRU stamp / fill a free or the LRU entry |
| `os.cpp` | `getPhysicalAddress()` | TLB lookup first; on a miss: page-table walk (+ demand map), count, `tlb_insert()` |
| `os.cpp` | `initialize()` | `tlb_flush(p)` and reset the counters when a task is loaded |
| `os.cpp` | `finalize()` | prints `TLB: lookups, hits, misses, hit rate (fetch ..., data ...)` |
| `os.cpp` | `struct Task` | `tlb_hits`, `tlb_misses` kept per task |
| `processor.cpp` | `translate_word()` (new), `load_word()`, `store_word()`, `fetch()` | one translation per word; byte by byte only across a page boundary (`read_byte` removed) |
| `Makefile` | `test` | runs `./test.sh`, then the 33 base checks |

## Run
```bash
make
./sim -b ../../examples/sum_array.txt ../../examples/sum_array_data.byte
./sim -b tlb_walk.txt tlb3_data.byte          # K = 3 pages: fits in the TLB
./sim -b tlb_walk.txt tlb5_data.byte          # K = 5 pages: LRU thrashing
./test.sh                                     # 15 checks
SIM=$PWD/sim ../../tests/run_tests.sh         # 33 passed
```
`tlb2/3/5_data.byte` hold K = 2/3/5 and R = 4. For K = 4, use `04 00 00 00` / `04 00 00 00` / `00 00 00 00` / `00 00 00 00`.

## Expected output
```
$ ./sim -b sum_array.txt sum_array_data.byte
[os] pid 1 (sum_array) finished on processor 0 after 71 instructions, 0 page fault(s) -> sum_array_data_1.out, sum_array_1.freq
[os] pid 1 TLB: 82 lookups, 80 hits, 2 misses, hit rate 97.6% (fetch: 70 of 71 hit; data: 10 of 11 hit = 90.9%)

$ ./sim -b tlb_walk.txt tlb3_data.byte
[os] pid 1 (tlb_walk) finished on processor 0 after 151 instructions, 2 page fault(s) -> tlb3_data_1.out, tlb_walk_1.freq
[os] pid 1 TLB: 169 lookups, 165 hits, 4 misses, hit rate 97.6% (fetch: 150 of 151 hit; data: 15 of 18 hit = 83.3%)

$ ./sim -b tlb_walk.txt tlb5_data.byte
[os] pid 1 (tlb_walk) finished on processor 0 after 221 instructions, 4 page fault(s) -> tlb5_data_1.out, tlb_walk_1.freq
[os] pid 1 TLB: 249 lookups, 222 hits, 27 misses, hit rate 89.2% (fetch: 220 of 221 hit; data: 2 of 28 hit = 7.1%)

K = 4:  [os] pid 1 TLB: 209 lookups, 187 hits, 22 misses, hit rate 89.5% (fetch: 185 of 186 hit; data: 2 of 23 hit = 8.7%)

$ ./test.sh
e11: sum_array (one code page, one data page)
  ok    sum_array exit code
  ok    sum_array result 45
  ok    sum_array TLB: 82 lookups, 80 hits, 2 misses, 71 fetch, 11 data (10 hit)
e11: tlb_walk, K = 3 data pages, R = 4 passes (fits: code page + 3 data pages = 4 entries)
  ok    K=3 sum = 4 * (1+2+3) = 24
  ok    K=3 TLB: 169 lookups, 165 hits, 4 misses, 151 fetch, 18 data (15 hit)
e11: tlb_walk, K = 5 data pages, R = 4 passes (5 data pages > 3 free entries: LRU thrashes)
  ok    K=5 sum = 4 * (1+..+5) = 60
  ok    K=5 page p holds p + 1 (page 4, word 3)
  ok    K=5 TLB: 249 lookups, 222 hits, 27 misses, 221 fetch, 28 data (2 hit)
e11: 5 tasks on 4 processors (pid 5 reuses processor 0: its TLB must be flushed)
  ok    pid 5 waited, then ran on processor 0
  ok    pids 1-4 (K=2) each: 129 lookups, 3 misses (per-processor TLBs do not disturb each other)
  ok    pid 5: same 27 misses as when it runs alone (TLB flushed on load)
  ok    pid 1 sum 4 * (1+2) = 12, pid 5 sum 60
e11: an unaligned word that crosses a page boundary (translated byte by byte)
  ok    word at 510..513 written and read back: 40000
  ok    cross TLB: 17 lookups, 14 hits, 3 misses, 8 fetch, 9 data (7 hit)
  ok    cross: page 1 demand-mapped (1 page fault)

15 passed, 0 failed
```
The overall hit rate stays high (89%) even when the data accesses thrash, because
fetches dominate. That is why the report shows the data hit rate separately (83% vs 7%).

## Viva questions
- **What happens if you forget the flush on load?** Tried it: in the 5-task test, pid 5
  inherited pid 1's translations. Its writes to data page 1 went through a stale entry
  to pid 1's old frame, which the OS had already freed. No page fault happened, so pid 5's
  page table never mapped page 1. When LRU later evicted the stale entry, the page-table
  walk found page 1 unmapped and demand-mapped a fresh zero page. Result: 52 instead of 60.
- **Why is the code page never evicted?** Every instruction fetch uses it, so at the
  moment of any data miss the code page was just used (it is the MRU entry). With FIFO it
  would be evicted regularly, because it was inserted first.
- **Why count per word and not per byte?** One access = one translation in real hardware.
  Per-byte counting adds 3 free hits to every word. The hit rate would then measure the
  word size, not the locality of the program.
- **What changes for a real context switch?** Here a task stays on its processor until it
  finishes. With time-sharing on one CPU, the TLB must be flushed on every switch, or the
  entries are tagged with an address-space id (ASID/PCID) so they need no flush.
- **What if the OS changes a page-table entry (e.g. page replacement)?** The cached copy
  in the TLB must be invalidated too: on this processor, and on every other processor
  that may hold it (TLB shootdown). This simulator never unmaps a page while the task runs.
- **Effective access time?** With hit ratio h, TLB time t and memory time m:
  EAT = h(t + m) + (1 - h)(t + 2m). The page-table walk is one extra memory access here,
  because the table is a single level in frame 0.

# Q075 — A 2-entry direct-mapped TLB per processor

## Question
Every translation reads a page-table entry from memory. Give each processor a TLB with
2 entries, direct-mapped: the translation for page-table index `i` can only be in slot
`i & 1`. `getPhysicalAddress()` uses the TLB on a hit and reads the page table only on a
miss, then fills the slot. Count hits and misses per task and print them when the task
ends. Explain the numbers for `sum_array`, and why this TLB is bad for it.

## Answer
In `os.cpp`, next to the MMU:
```c
struct TlbEntry { bool valid; int page, frame; };
static TlbEntry tlb[NP][2];
static long tlb_hits[NP], tlb_misses[NP];
```
- **Lookup** (`getPhysicalAddress`, after the index is computed):
  `TlbEntry &e = tlb[p][index & 1];`. If `e.valid && e.page == index`, it is a hit: count
  it and return `e.frame * 512 + offset` without reading the page table.
- **Miss:** count it, read the entry (and demand-map the page as before), then fill the
  slot with `e = TlbEntry{true, index, frame}`. Error paths return before the fill, so
  only valid translations are cached.
- **Flush** (`initialize`, i.e. whenever a task is loaded on processor `p`): invalidate
  both slots and reset the counters. The counts are then per task.
- **Report** (`finalize`): `pid N TLB: H hits, M misses (R% hits)`.

The MMU is called once per **byte** (a word may cross a page boundary), so a word is 4
lookups, and a vector load or store is 8 words = 32 lookups in one page.

| Task | Lookups (fetch + data) | Misses | Why |
|---|---|---|---|
| tlb_ok | 24×4 + 5×4 = 116 | 2 | code: index 0 → slot 0; data page 1: index 3 → slot 1. They never meet |
| counter | 43×4 + 1×4 = 176 | 3 | first fetch; its one store (data page 0 = index 2 → slot 0) evicts the code; the next fetch |
| sum_array | 71×4 + 11×4 = 328 | 23 | code (index 0) and data page 0 (index 2) share slot 0: 1 + 11 data words + 11 fetches right after them |
| array_add | 31×4 + 49×4 = 320 | 15 | same conflict; 3 at the start + 6 per loop (two vector loads, one vector store, each followed by a fetch) |
| pages | 15×4 + 7×4 = 88 | 9 | data pages 1, 3, 7 are indices 3, 5, 9, all odd: they all fight over slot 1 |

**Why it is bad for `sum_array`.** Instruction page 0 has index 0 and data page 0 has
index 2. Both are even, so both go to slot 0, although slot 1 is free. Every load
evicts the code page and the next fetch evicts the data page. These are **conflict
misses**: a 2-way set-associative TLB (either slot) of the same size would have only 2.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | (globals) | `struct TlbEntry`, `tlb[NP][2]`, `tlb_hits[NP]`, `tlb_misses[NP]` |
| `os.cpp` | `getPhysicalAddress` | look up slot `index & 1` first; on a miss walk the table, then fill the slot |
| `os.cpp` | `initialize` | flush both slots, reset the counters |
| `os.cpp` | `finalize` | print hits, misses, hit rate |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` runs `tlb_ok`, `counter`, `array_add`, `sum_array` and `pages`. `pages` waits
and runs on processor 0 after `tlb_ok`. It checks:
- the five TLB lines of the table;
- that `pages` still gives 21, with 7 at 1020.

The second check proves the flush. Without it, `pages` hits `tlb_ok`'s stale entry
"index 3 → frame 9" (a freed frame) and ends with 14 and 0 at 1020; this was tried.
It also checks 5 for `tlb_ok`, and 45 for `sum_array` (also alone).

## Viva
- **Why is the hit rate high anyway?** The MMU translates per byte, so each word is 4 lookups of one page and at least 3 of them hit. Hardware translates once per access; `extensions/e11_tlb` changes the processor to translate per word.
- **How would you remove the conflict?** Make it 2-way set-associative (a page may use either slot, with LRU), or split it into an instruction TLB and a data TLB, as real CPUs do.
- **Why flush when a task is loaded?** The entries say "index i → frame f" for the previous task. The new task uses the same indices but other frames. Real CPUs flush or tag entries with an address-space id (ASID/PCID) on a context switch.
- **Why no flush when a page is demand-mapped?** Only valid translations are ever cached, and a page is mapped only when it was not mapped, so the TLB cannot hold an entry for it.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `counter.txt`, `counter_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `tlb_ok.txt`, `tlb_ok_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

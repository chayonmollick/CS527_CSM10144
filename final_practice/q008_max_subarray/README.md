# Q008 — Maximum subarray sum (Kadane)

## Question
N (≥ 1) is at address 0 and an array A[0..N-1] of integers, which may be negative, at
4..4N. Write a program that finds the largest sum of a non-empty run of consecutive
elements and stores it at 4N + 4, with the start index of that run at 4N + 8 and the end
index at 4N + 12.

## Answer
Kadane's algorithm, one pass. `cur` (x13) is the best sum of a run that ends at i, and
`best` (x10) the largest cur seen:
```
if cur < 0:  cur = A[i], the run starts at i     (a negative prefix only lowers the sum)
else:        cur = cur + A[i]
if cur > best:  best = cur, best run = start .. i
```
In the program, `x9 = x13 - 0` with `BPL .extend` chooses between the two cases, and
`x9 = x13 - x10` with `BLE .next` keeps the best. best and cur start at A[0], not 0, so an
all-negative array gives its largest element (the run may not be empty).

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `max_subarray_data.byte`: N = 9, `-2 1 -3 4 -1 2 1 -5 4` → `[40] = 6` (`4 -1 2 1`),
  `[44] = 3`, `[48] = 6`.
- `max_subarray_data2.byte`: N = 5, `-8 -3 -6 -2 -5` (all negative) → `[24] = -2`,
  `[28] = 3`, `[32] = 3`.

## Viva
- **Why is restarting when cur < 0 right?** Every run that ends at i and starts earlier is (a run ending at i-1) + A[i]; if the best of those runs is negative, A[i] alone is bigger.
- **What goes wrong if best starts at 0?** An all-negative array gives 0, the sum of the empty run; data2 would give 0 instead of -2.
- **Cost?** O(N). Trying every (start, end) pair is O(N²) with running sums, O(N³) without.

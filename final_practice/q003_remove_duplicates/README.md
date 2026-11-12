# Q003 — Remove duplicates from a sorted array

## Question
An array of N integers sorted in ascending order is at 4..4N, with N at address 0. Write a
program that removes the repeated values in place, so that the L distinct values come
first in their order, and stores the new length L at 4N + 4. The words after the first L
may keep any values.

## Answer
Two pointers over the same array: a write pointer `w` (x3) at the last value kept (A[0]
is always kept) and a read pointer `r` (x2) from &A[1] to the end. The array is sorted, so
equal values are next to each other and A[r] is new exactly when it differs from the last
kept value:
```
x4 = [x2]         % A[r]
x9 = x4 - x5      % A[r] - last kept
BEQ .skip         % duplicate
x3 = x3 + 4       % w = w + 4
[x3] = x4         % keep it
x5 = x4
```
At the end w = &A[L-1] = 4 + 4(L-1) = 4L, so `L = w / 4`. N = 0 is checked first (L = 0).

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `remove_duplicates_data.byte`: N = 12, `-5 -5 -2 0 0 0 3 7 7 9 9 9` → `-5 -2 0 3 7 9`
  at 4..24 and `[52] = 6`.
- `remove_duplicates_data2.byte`: N = 5, all 4 → `[4] = 4`, `[24] = 1`.

## Viva
- **Why must the array be sorted?** The program only compares with the last kept value. In `1 2 1` the second 1 is not next to the first; finding it needs a search (O(N²)) or a table of values already seen.
- **Is it safe to write into the array being read?** Yes: w ≤ r at every store and A[r] is already in a register, so no word is overwritten before it has been read.
- **Cost?** One pass, N - 1 compares, at most N - 1 stores, no extra memory.

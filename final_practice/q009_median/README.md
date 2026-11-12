# Q009 — Median of an odd-length array

## Question
N (odd) is at address 0 and an array A[0..N-1] of integers at 4..4N. Write a program that
stores the median (the middle value of the array once it is sorted) at 4N + 4, without
changing the array.

## Answer
No sorting: the median is the element of rank k = N / 2 in sorted order. For each
candidate c = A[i], one compare per element counts less = #(A[j] < c) and
leq = #(A[j] ≤ c):
```
x9 = x7 - x4      % A[j] - c
BGT .count        % greater: neither count
BEQ .equal        % equal: leq only (same flags)
x10 = x10 + 1     % less
.equal
x11 = x11 + 1     % leq
```
c is the median when `less <= k < leq`; it is stored and the program ends. O(N²)
compares and no stores into A.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `median_data.byte`: N = 9, `42 -7 15 3 15 99 0 8 23` (sorted: -7 0 3 8 15 15 23 42 99)
  → `[40] = 15`, and A at 4..36 is unchanged.
- `median_data2.byte`: N = 5, `-3 -8 -3 10 -3` (sorted: -8 -3 -3 -3 10) → `[24] = -3`.

## Viva
- **Why `less <= k < leq` and not `less == k`?** With repeated values no element may have exactly k smaller ones: in data2 every -3 has 1 smaller element, but k = 2.
- **Other way?** Sort a copy (insertion sort, Q002) and read the word at 4 + 4k: also O(N²), but it needs N extra words, or it changes A.
- **Even N?** The median is the average of the elements of rank N/2 - 1 and N/2; find both with the same test.

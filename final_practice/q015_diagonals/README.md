# Q015 — Trace and anti-diagonal of an 8×8 matrix

## Question
An 8×8 integer matrix A is stored row by row at 0..252 (A[i][j] at 32i + 4j). Write a
program that stores the trace A[0][0] + A[1][1] + … + A[7][7] at 256 and the
anti-diagonal sum A[0][7] + A[1][6] + … + A[7][0] at 260.

## Answer
One loop over the 8 rows with two pointers. One row down is +32 bytes, so:
- &A[i][i] starts at 0 and moves by 32 + 4 = **36** (down and right);
- &A[i][7-i] starts at 28 and moves by 32 - 4 = **28** (down and left).

The loop counts down from 8 (`x2 = x2 - 1`, BNE). A constant address `[c]` only reaches
0..255, so the result address is built in a register: `x7 = 128`, `x7 = x7 * 2`,
`[x7] = x10`, then `x7 = x7 + 4`, `[x7] = x11`.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `diagonals_data.byte`: A[i][j] = i·j → trace `[256] = 140` (0 + 1 + 4 + … + 49),
  anti-diagonal `[260] = 56` (0 + 6 + 10 + 12 + 12 + 10 + 6 + 0).
- `diagonals_data2.byte`: 64 values between -300 and 300 → `[256] = -275`,
  `[260] = 185`.

## Viva
- **Why not vectors?** The 8 wanted elements are in 8 different rows and lanes. Each vector load would bring 7 unwanted values, and there is no add across lanes, so it would cost more than 8 scalar loads.
- **For an n×n matrix?** Steps 4(n+1) and 4(n-1), start addresses 0 and 4(n-1), n iterations. For odd n the centre element is on both diagonals.
- **Why is A[i][j] = 8i + j + 1 a bad test?** Its trace and its anti-diagonal sum are both 260, so swapped results would not be noticed.

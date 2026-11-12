# Q018 — Is a 4×4 matrix symmetric?

## Question
A 4×4 integer matrix A is stored row by row at 0..60 (A[i][j] at 16i + 4j). Write a
program that stores 1 at 64 if A is symmetric (A[i][j] = A[j][i] for all i and j) and 0
otherwise.

## Answer
Only the 6 pairs above the diagonal need a compare (i < j). For row i:
- p (x3) walks right along row i from &A[i][i+1], +4 per step;
- q (x4) walks down column i from &A[i+1][i], +16 per step;
- both start next to &A[i][i] (x7), which moves by 20 (one row + one column) per row;
- the inner loop ends when p reaches the end of row i, 16(i+1) (x6).

The first mismatch (`x9 = x11 - x12`, `BNE .no`) leaves both loops with 0; the outer loop
stops before row 3, which has nothing right of the diagonal.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `symmetric_data.byte`: rows `5 -2 7 0`, `-2 9 3 11`, `7 3 -4 8`, `0 11 8 1` → `[64] = 1`.
- `symmetric_data2.byte`: the same with A[3][2] = -8 ≠ A[2][3] = 8, the last pair checked
  → `[64] = 0`.

## Viva
- **Why only i < j?** The diagonal always matches itself, and checking (j, i) after (i, j) repeats the same compare: 6 compares instead of 16.
- **Why put data2's difference in the last pair?** It shows the loops reach every pair: a loop that stops one row early would answer 1.
- **Skew-symmetric (A = -Aᵀ)?** Check A[i][j] + A[j][i] = 0 for i ≤ j, which includes the diagonal (it must be 0).

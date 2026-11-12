# Q014 — Row N of Pascal's triangle

## Question
N (0 ≤ N ≤ 33) is at address 0. Write a program that writes row N of Pascal's triangle,
C(N,0), C(N,1), …, C(N,N), into memory with C(N,k) at 8 + 4k. Address 4 may be used as
scratch. Use vector instructions for the additions.

## Answer
Build the row in place from row 0 = `1 0 0 …` by applying Pascal's rule
new[j] = old[j] + old[j-1] N times. For 8 values at once:
```
x5 = x4 - 4
v1 = [x4]         % old[j .. j+7]
v2 = [x5]         % old[j-1 .. j+6]: the same words, one to the left
v1 = v1 + v2
[x4] = v1
```
- `[4] = 0` gives C(i,-1) = 0 for the first block.
- The blocks go **right to left**: a block reads old[j-1] from the block on its left,
  which has not been updated yet.
- Zeros right of the row stay 0 (0 + 0), so every row can use all ⌊N/8⌋ + 1 blocks of row
  N: no per-row block count is needed.
- The row area is cleared first with stores of a zero vector (`v0 = v0 - v0`).

Row N costs N·(⌊N/8⌋ + 1) vector adds: 20 for N = 10, 165 for N = 33.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `pascal_row_data.byte`: N = 10 → `1 10 45 120 210 252 210 120 45 10 1` at 8..48,
  `[52] = 0`.
- `pascal_row_data2.byte`: N = 33 (5 blocks; the largest row that fits in 32 bits) → all
  34 values at 8..140, e.g. `[72] = [76] = 1166803110`, and `[144] = 0`.

## Viva
- **Why right to left?** In place, the block at j needs old[j-1], the last word of the block on its left. Doing that block first would replace it with its new value.
- **Why N ≤ 33?** C(34,17) = 2333606220 > 2³¹ - 1, and adds wrap around silently.
- **Scalar alternatives?** `for j = i down to 1: row[j] = row[j] + row[j-1]` for each row (right to left for the same reason), or C(N,k+1) = C(N,k)·(N-k)/(k+1) in one loop; that division is exact, but the product overflows from N = 30.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q014_pascal_row/solution.txt ../final_practice/q014_pascal_row/pascal_row_data.byte`
  (the final data memory is then in `lab6/pascal_row_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference pascal_row_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `pascal_row_data.byte`, `pascal_row_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

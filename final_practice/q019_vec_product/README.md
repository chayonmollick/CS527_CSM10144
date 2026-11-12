# Q019 — Element-wise product with vectors

## Question
N is at address 0, an array A[0..N-1] at 4..4N and an array B[0..N-1] at 4N + 4..8N.
Using vector instructions, write a program that computes C[i] = A[i] · B[i] into an array
C at 8N + 4..12N. N can be any number ≥ 0, not only a multiple of 8.

## Answer
Vector blocks while at least 8 elements are left, then a scalar tail:
```
x7 = x1 - 8       % a full block fits while i <= N - 8
.vloop
x9 = x5 - x7
BGT .tail         % fewer than 8 left
v1 = [x2]         % A[i .. i+7]
v2 = [x3]         % B[i .. i+7]
v1 = v1 * v2      % 8 products
[x4] = v1         % C[i .. i+7]
x2 = x2 + 32      % (x3, x4 also + 32; i + 8)
```
The tail does the last N mod 8 elements one at a time (`x10 = [x2]`, `x11 = [x3]`,
multiply, store, pointers + 4). For N < 8, N - 8 is negative and the vector loop does not
run at all.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `vec_product_data.byte`: N = 19 (2 vector blocks + 3 tail elements), A = 1..19,
  B[i] = 3i - 20 → all 19 products `-20 -34 -42 … 646` at 156..228, `[232] = 0` (nothing
  written after C), and the `.freq` report shows the vector multiply ran 2 times.
- `vec_product_data2.byte`: N = 5 (tail only), `300 -7 0 -12 1000` × `-400 -9 55 12 3` →
  `-120000 63 0 -144 3000` at 44..60.

## Viva
- **Why not one more full block instead of the tail?** It would write up to 7 words past the end of C and read past the end of B.
- **How is the vector use optimal?** One load per input block, one multiply, one store per output block, and no scalar work on the data inside the vector loop.
- **Could the tail be a vector too?** Only by overlapping: for N ≥ 8, one last full block starting at N - 8 recomputes a few products with the same values, which is harmless out of place.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q019_vec_product/solution.txt ../final_practice/q019_vec_product/vec_product_data.byte`
  (the final data memory is then in `lab6/vec_product_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference vec_product_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `vec_product_data.byte`, `vec_product_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

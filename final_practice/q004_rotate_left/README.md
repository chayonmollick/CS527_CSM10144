# Q004 — Rotate an array left by k positions

## Question
N is at address 0, k (≥ 0) at 4 and an array A[0..N-1] of integers at 8..4N + 4. Write a
program that rotates A left by k positions in place: A[i] moves to index i - k and the
first k elements go to the end. k may be larger than N (rotate by k mod N).

## Answer
Three reversals rotate an array in place: reverse A[0..k-1], reverse A[k..N-1], then
reverse the whole array (O(N), no second array).
1. `k = k - (k / N) * N` (k mod N). When N ≤ 1 nothing moves, and skipping also avoids a
   division by 0.
2. One reverse loop: pointers lo (x5) and hi (x6), swap `[lo]` and `[hi]`, lo + 4,
   hi - 4, until lo ≥ hi.
3. There is no call and return, so a pass counter (x10) picks the next range when the
   loop ends: pass 1 = &A[0]..&A[k-1], pass 2 = &A[k]..&A[N-1], pass 3 = &A[0]..&A[N-1].

Example, data1 (k = 3): `5 -1 8 | 13 0 21 -7 2 34` → `8 -1 5 | 34 2 -7 21 0 13` → reverse
all → `13 0 21 -7 2 34 5 -1 8`.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `rotate_left_data.byte`: N = 9, k = 3, `5 -1 8 13 0 21 -7 2 34` →
  `13 0 21 -7 2 34 5 -1 8` at 8..40.
- `rotate_left_data2.byte`: N = 6, k = 14 (14 mod 6 = 2), `1 2 3 4 5 6` → `3 4 5 6 1 2`
  at 8..28.

## Viva
- **Why does it work?** Write A = X Y with X the first k elements; the rotation is Y X. Reversing the parts gives Xʳ Yʳ, and reversing that whole gives (Xʳ Yʳ)ʳ = Y X.
- **Other ways?** Copy into a second array (`B[i] = A[(i + k) mod N]`, then copy back: N extra words and 2N stores), or rotate by one place k times (O(N·k)).
- **Rotate right by k?** Rotate left by N - (k mod N).

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q004_rotate_left/solution.txt ../final_practice/q004_rotate_left/rotate_left_data.byte`
  (the final data memory is then in `lab6/rotate_left_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference rotate_left_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `rotate_left_data.byte`, `rotate_left_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

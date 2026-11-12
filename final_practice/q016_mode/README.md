# Q016 — Mode of values 0..15

## Question
N (≥ 1) is at address 0 and an array A[0..N-1] whose values are all in 0..15 at 4..4N.
Write a program that finds the mode (the most frequent value; if several values are
equally frequent, the smallest of them) and stores it at 4N + 4, with its count at
4N + 8.

## Answer
A histogram, then one scan:
1. 16 counters H[0..15] at 4N + 12, cleared with two vector stores of a zero vector:
   `v0 = v0 - v0`, `[x20] = v0`, `[x21] = v0` (x21 = H + 32).
2. For each a: the counter address is H + 4a (`x4 = x4 * 4`, `x4 = x4 + x20`); load, add
   1, store.
3. Scan v = 1..15 starting from (0, H[0]). A value replaces the best only when its count
   is strictly larger (`BLE .snext` skips equal counts), so ties go to the smaller value.

O(N + 16), instead of O(N²) for a nested loop that counts how often each element occurs.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `mode_data.byte`: N = 12, `3 7 3 12 7 0 15 7 3 9 12 1` (3 and 7 both 3 times) →
  `[52] = 3`, `[56] = 3`.
- `mode_data2.byte`: N = 9, `15 2 15 15 0 2 15 9 0` → `[40] = 15`, `[44] = 4` (the top
  counter, cleared by the second vector store).

## Viva
- **Why clear the counters if new memory is 0 anyway?** They lie after the data, where a longer data file or earlier results may have left values; clearing costs only 2 vector stores.
- **Ties to the largest value instead?** Use BLT instead of BLE in the scan, so an equal count also replaces the best.
- **Values outside 0..15?** H + 4a would write outside the counters. Check `a < 0` (BMI) and `a > 15` (BGT) first, as `programs/histogram.txt` does.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q016_mode/solution.txt ../final_practice/q016_mode/mode_data.byte`
  (the final data memory is then in `lab6/mode_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference mode_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `mode_data.byte`, `mode_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

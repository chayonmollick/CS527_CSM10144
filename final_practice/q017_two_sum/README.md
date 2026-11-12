# Q017 — Two-sum: a pair that adds up to a target

## Question
N is at address 0, a target T at 4 and an array A[0..N-1] at 8..4N + 4. Write a program
that finds indices i < j with A[i] + A[j] = T (the first such pair: smallest i, then
smallest j) and stores i at 4N + 8 and j at 4N + 12. If there is no such pair, store -1
and -1.

## Answer
Nested loops over the pairs, with j starting at i + 1 so that an element is never paired
with itself. The partner that A[i] needs, `T - A[i]` (x8), is computed once per i, outside
the inner loop, which then is a plain search:
```
x12 = [x4]        % A[j]
x9 = x12 - x8     % A[j] - (T - A[i])
BEQ .found        % leaves both loops
```
At `.found` the indices come from the addresses: i = (&A[i] - 8) / 4, j = (&A[j] - 8) / 4.
Otherwise the -1s set at the start (`0 - 1`) are stored.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `two_sum_data.byte`: N = 8, T = 10, `4 8 -2 9 5 12 1 5` → `[40] = 2`, `[44] = 5`
  (-2 + 12; the later pairs 9 + 1 and 5 + 5 are not reported).
- `two_sum_data2.byte`: N = 5, T = 100, `10 20 30 40 50` → `[28] = -1`, `[32] = -1`
  (50 + 50 would use one element twice).

## Viva
- **Why does j start at i + 1?** j = i pairs an element with itself (50 + 50 in data2), and j < i only finds the same pairs again in the other order.
- **Faster ways?** On a sorted array, two pointers from both ends: O(N). For small values, a table of the values seen so far: O(N).
- **Why compare with T - A[i] instead of adding A[i] + A[j]?** The subtraction is done once per i instead of an add for every pair.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q017_two_sum/solution.txt ../final_practice/q017_two_sum/two_sum_data.byte`
  (the final data memory is then in `lab6/two_sum_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference two_sum_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `two_sum_data.byte`, `two_sum_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

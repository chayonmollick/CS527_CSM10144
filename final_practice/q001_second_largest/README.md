# Q001 — Second largest distinct element and its index

## Question
Write a program that finds the second largest **distinct** value of an array of N signed
integers (the largest value smaller than the maximum) and the index of its first
occurrence. N is at address 0 (N ≥ 1) and A[0..N-1] at 4, 8, …, 4N. Store the value at
4N + 4 and the index at 4N + 8. If there is no such value (all elements equal), store 0
and -1.

## Answer
One pass that keeps two (value, index) pairs: the maximum (x10, x11) and the second
(x12, x13, where index -1 means "none yet"). One compare `x9 = a - max` feeds three
branches, because branches do not change the flags:
```
x9 = x4 - x10     % a - max
BEQ .next         % a == max: not a new distinct value
BLT .lower        % a < max: candidate for the second
x12 = x10         % a > max: the old max becomes the second
x13 = x11
x10 = x4          % and a is the new max
x11 = x2
```
- At `.lower`, a becomes the second if there is none yet (`x9 = x13 - 0`, BMI) or if
  `a > second` (BLE skips it otherwise).
- Only strictly bigger values replace, so both indices are first occurrences.
- -1 is built as `0 - 1` (constants are 0..255). After the loop x3 = &A[N-1] = 4N, so the
  results go to x3 + 4 and x3 + 8.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `second_largest_data.byte`: N = 12, `12 5 9 41 -3 45 7 41 38 45 -20 30` (45 and 41 both
  appear twice, the 41s before and after the first 45) → `[52] = 41`, `[56] = 3`.
- `second_largest_data2.byte`: N = 5, all -7 → `[24] = 0`, `[28] = -1`.

## Viva
- **Why not sort and take the element before the last?** Sorting is O(N²) with these loops and changes the array, and with a repeated maximum the element before the last is the maximum again. One pass is O(N).
- **Why does the old maximum become the second when a new maximum appears?** Every value seen so far is ≤ the old maximum, so it is the largest value below the new one.
- **Why test BEQ before BLT?** An element equal to the maximum must be ignored; without the BEQ it would fall into the `a > max` case and make the maximum its own second.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q001_second_largest/solution.txt ../final_practice/q001_second_largest/second_largest_data.byte`
  (the final data memory is then in `lab6/second_largest_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference second_largest_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `second_largest_data.byte`, `second_largest_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

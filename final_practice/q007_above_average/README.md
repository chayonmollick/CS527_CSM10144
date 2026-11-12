# Q007 — Count the elements greater than the average

## Question
N (≥ 1) is at address 0 and an array A[0..N-1] at 4..4N. Write a program that computes
the integer average (sum / N with the machine's division) and counts how many elements
are strictly greater than it. Store the average at 4N + 4 and the count at 4N + 8.

## Answer
Two passes over the array. The first adds it up; one division `x11 = x10 / x1` gives the
average; the second pass counts the elements above it:
```
x4 = [x3]
x3 = x3 + 4       % moved up: an add between compare and branch would change the flags
x9 = x4 - x11     % A[i] - average
BLE .count        % A[i] <= average: not counted
x12 = x12 + 1
```
The average is known only after the whole sum, so one pass is not enough.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `above_average_data.byte`: N = 10, `12 7 30 -4 15 25 3 9 41 12`, sum 150 → `[44] = 15`,
  `[48] = 3` (30, 25 and 41; the 15 is not greater).
- `above_average_data2.byte`: N = 5, `-7 -5 -4 -10 -1`, sum -27 → `[24] = -5` (-5.4
  rounded towards zero), `[28] = 2` (-4 and -1).

## Viva
- **Why is -5 not counted in data2, although -5 > -5.4?** The question uses the integer average, and division rounds towards zero: -27 / 5 = -5. To compare with the exact average, test `N * A[i] > sum` instead: no division at all.
- **Can vectors help?** Only for the sum (8 lanes, then add the lanes, as in `programs/sum_squares_vec.txt`). Counting needs a compare per element, and there is no vector compare.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q007_above_average/solution.txt ../final_practice/q007_above_average/above_average_data.byte`
  (the final data memory is then in `lab6/above_average_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference above_average_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `above_average_data.byte`, `above_average_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

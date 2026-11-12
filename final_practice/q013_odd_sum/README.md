# Q013 — Sum of the first N odd numbers, compared with N²

## Question
N ≥ 0 is at address 0. Using a loop, compute S = 1 + 3 + 5 + … + (2N - 1), the sum of the
first N odd numbers, and store it at 4. Store N·N at 8, and at 12 store 1 if S = N·N,
otherwise 0.

## Answer
A count-down loop that runs N times. The current odd number is kept in a register and
grows by 2, so the loop needs no multiply:
```
.loop
x10 = x10 + x2    % S = S + odd
x2 = x2 + 2       % next odd number
x3 = x3 - 1       % counter; sets Z at 0
BNE .loop
```
A guard before the loop (`x9 = x3 - 0`, BLE) skips it for N = 0. Then N·N takes one
multiply, and the flag comes from `x9 = x10 - x11`, BNE.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `odd_sum_data.byte`: N = 300 (above 255, so it comes from the data file) →
  `[4] = 90000`, `[8] = 90000`, `[12] = 1`.
- `odd_sum_data2.byte`: N = 0 → `[4] = 0`, `[8] = 0`, `[12] = 1`.

## Viva
- **Why is the flag always 1?** N² - (N-1)² = 2N - 1: each odd number adds the L-shaped border that turns an (N-1)×(N-1) square into an N×N one. Only overflow (N > 46340) breaks it.
- **Why does BNE work without a compare?** `x3 = x3 - 1` is a subtraction, so it sets Z when the counter becomes 0.
- **What if the guard is missing and N = 0?** The body runs with the counter going -1, -2, … and never meets 0 in time: the simulator stops the task after 10 million instructions with an error.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q013_odd_sum/solution.txt ../final_practice/q013_odd_sum/odd_sum_data.byte`
  (the final data memory is then in `lab6/odd_sum_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference odd_sum_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `odd_sum_data.byte`, `odd_sum_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

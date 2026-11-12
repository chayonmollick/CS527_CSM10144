# Q011 — Collatz steps from n to 1

## Question
A number n ≥ 1 is at address 0. Repeat: if n is even, n = n / 2, otherwise n = 3n + 1,
until n = 1. Write a program that stores the number of steps at 4 and the largest value
reached (n itself included) at 8.

## Answer
A while loop with the test at the top (`x9 = x1 - 1`, BEQ to the end), so n = 1 takes 0
steps. Even or odd without a modulus or AND instruction:
```
x2 = x1 / 2
x3 = x2 * 2
x9 = x1 - x3      % n - (n/2)*2 is 0 when n is even
BEQ .even         % there: n = x2, already computed
x1 = x1 * 3
x1 = x1 + 1       % odd: n = 3n + 1
```
Only an odd step makes n bigger, so the peak (x11) is compared only after `3n + 1`.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `collatz_data.byte`: n = 27 → `[4] = 111` steps, `[8] = 9232`.
- `collatz_data2.byte`: n = 1 → `[4] = 0`, `[8] = 1` (the loop body never runs).

## Viva
- **Why is the subtraction needed for the even test?** Multiply and divide do not set flags; only add and subtract do, so `x1 - x3` is the compare.
- **Can the loop run forever?** No n tried so far does (the Collatz conjecture is open), and the simulator stops a task after 10 million instructions anyway.
- **Overflow?** 3n + 1 can pass 2³¹ - 1 for large n; for n = 27 the peak is only 9232.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q011_collatz/solution.txt ../final_practice/q011_collatz/collatz_data.byte`
  (the final data memory is then in `lab6/collatz_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference collatz_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `collatz_data.byte`, `collatz_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

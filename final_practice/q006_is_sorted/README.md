# Q006 — Is the array sorted?

## Question
N is at address 0 and an array A[0..N-1] of signed integers at 4..4N. Write a program that
stores 1 at 4N + 4 if the array is sorted in ascending order (A[i] ≤ A[i+1] for every i;
equal neighbours are allowed) and 0 if it is not. An array of 0 or 1 elements counts as
sorted.

## Answer
The answer x10 starts at 1. A pointer walks from &A[0] up to &A[N-1] (the last element has
no right neighbour) and compares each element with the next one:
```
x4 = [x3]         % A[i]
x3 = x3 + 4
x5 = [x3]         % A[i+1]
x9 = x5 - x4      % A[i+1] - A[i]
BGE .loop         % in order: go on
x10 = 0           % a descent: stop with 0
```
The first descent ends the loop at once; reaching &A[N-1] leaves the 1. For N = 0 or 1 the
loop never runs.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `is_sorted_data.byte`: N = 8, `-9 -9 -2 0 5 5 13 300` (equal neighbours) → `[36] = 1`.
- `is_sorted_data2.byte`: N = 7, `1 2 3 4 5 6 4` (only the last pair is wrong) → `[32] = 0`.

## Viva
- **Why BGE and not BGT?** Equal neighbours are allowed (non-decreasing). For strictly increasing, use BGT.
- **Why a signed branch?** BGE looks at N and V, so it is right for negative values and even when `A[i+1] - A[i]` overflows; an unsigned branch (BHI, BCS) would treat -1 as 4294967295.
- **What does data2 check?** That the last pair is compared: a loop that stops one pair early would answer 1.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q006_is_sorted/solution.txt ../final_practice/q006_is_sorted/is_sorted_data.byte`
  (the final data memory is then in `lab6/is_sorted_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference is_sorted_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `is_sorted_data.byte`, `is_sorted_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

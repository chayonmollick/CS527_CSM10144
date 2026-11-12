# Q012 — Integer square root

## Question
A number n ≥ 0 is at address 0. Write a program that stores at 4 the integer square root
r = ⌊√n⌋, the largest r with r·r ≤ n, and at 8 the remainder n - r·r. n can be large
(up to 2³¹ - 2): use a method faster than trying r = 0, 1, 2, … in turn.

## Answer
Newton's method in integers:
```
x = n;  y = (n + 1) / 2
while y < x:  x = y;  y = (x + n / x) / 2
r = x
```
In the program `x9 = x11 - x10` with `BGE .done` ends the loop. The guesses go down and
stop at ⌊√n⌋: 13 loops for n = 2000000, where the simple loop
`while (r+1)*(r+1) <= n: r = r + 1` (also correct, fine for small n) needs 1414.
n = 0 and n = 1 need no special case, and x ≥ 1 whenever n / x is computed.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `isqrt_data.byte`: n = 2000000 → `[4] = 1414`, `[8] = 604`
  (1414² = 1999396 ≤ n < 1415² = 2002225).
- `isqrt_data2.byte`: n = 9801 = 99² → `[4] = 99`, `[8] = 0` (a perfect square: an
  off-by-one gives 98 or 100 here).

## Viva
- **Why stop when y ≥ x?** While x > ⌊√n⌋ the next guess is smaller but still ≥ ⌊√n⌋; at x = ⌊√n⌋ it is not smaller. So the first guess that does not go down marks the answer.
- **Why (n + 1) / 2 for the first y?** It is (x + n/x)/2 with x = n, without dividing by n, which would be a division by 0 for n = 0.
- **Binary search?** Also works, in about log₂(range) steps, but with hi = n the product mid·mid overflows 32 bits once mid > 46340: test `mid <= n / mid` instead, or start hi at 46340.

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q012_isqrt/solution.txt ../final_practice/q012_isqrt/isqrt_data.byte`
  (the final data memory is then in `lab6/isqrt_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference isqrt_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `isqrt_data.byte`, `isqrt_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

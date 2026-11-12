# Q010 — Reverse the digits of a number, and its digit sum

## Question
An integer n, which may be negative, is at address 0. Write a program that stores at 4
the number with its decimal digits reversed, keeping the sign (12340 → 4321,
-507 → -705), and at 8 the sum of its digits.

## Answer
Take digits off the end until n is 0:
```
x2 = x1 / 10
x3 = x2 * 10
x3 = x1 - x3      % d = n - (n/10)*10, the last digit
x10 = x10 * 10
x10 = x10 + x3    % rev = rev * 10 + d
x1 = x2           % n = n / 10
```
- There is no remainder operator (`%` starts a comment), so the remainder is built from a
  division.
- Division rounds towards zero, so for a negative n every digit comes out ≤ 0 (-507 gives
  d = -7, 0, -5) and rev is negative with no special case.
- For the digit sum, a negative d is made positive first (`x3 = x20 - x3` when BPL is not
  taken).

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `reverse_digits_data.byte`: n = 12340 → `[4] = 4321` (the trailing zero disappears),
  `[8] = 10`.
- `reverse_digits_data2.byte`: n = -507 → `[4] = -705`, `[8] = 12`.

## Viva
- **Why does the same loop work for a negative n?** -507 / 10 = -50 (towards zero), so d = -507 - (-500) = -7: each digit has the sign of n, and rev goes -7, -70, -705.
- **What does `x1 = x2 % 10` do?** It compiles to `x1 = x2` without any error: everything after `%` is a comment.
- **Palindrome test?** Compare rev with the original n (`programs/digits.txt` does this for n > 0).

## Files
- `solution.txt`: the answer program. To run it on lab6:
  `cd ../../lab6 && make && ./sim -b ../final_practice/q010_reverse_digits/solution.txt ../final_practice/q010_reverse_digits/reverse_digits_data.byte`
  (the final data memory is then in `lab6/reverse_digits_data_1.out`).
- `reference.cpp`: the same task in C++.
  `g++ -std=c++17 reference.cpp -o reference && ./reference reverse_digits_data.byte` prints the expected result
  words, one `<address>: <value>` per line; use it to check your own program on new data.
- Inputs: `reverse_digits_data.byte`, `reverse_digits_data2.byte`.
- `test.sh`: runs `solution.txt` on a fresh copy of lab6 with both inputs and checks the results, then
  checks that the simulator's output has every word `reference.cpp` prints.

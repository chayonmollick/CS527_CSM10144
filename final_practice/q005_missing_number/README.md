# Q005 — The missing number in 1..N

## Question
N (≥ 1) is at address 0, followed by N - 1 different numbers from 1..N at 4, 8, …,
4(N - 1), in any order. Exactly one number of 1..N is missing. Write a program that
stores the missing number at 4N.

## Answer
The numbers 1..N add up to N(N+1)/2. Start from that value and subtract every number that
is present; what is left is the missing one:
```
x10 = x1 + 1
x10 = x10 * x1
x10 = x10 / 2     % N(N+1)/2
                  % then, in the loop over the list:
x10 = x10 - x4    % once for each number in the list
```
The list is N - 1 words from address 4, so it ends at 4N, which is also where the result
goes (`x6 = x1 * 4`, the loop runs while the pointer < x6). One pass, no sorting, no table.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `missing_number_data.byte`: N = 25, the numbers 1..25 except 13, shuffled →
  `[100] = 13`.
- `missing_number_data2.byte`: N = 1 and no numbers at all → `[4] = 1`.

## Viva
- **Is N(N+1)/2 always exact?** Yes: N and N + 1 are consecutive, so one of them is even.
- **Overflow?** N(N+1) fits in 32 bits up to N = 46340. For bigger N, start from N and add k - A[k-1] in the loop, so the running value stays small.
- **Other method?** Mark each number in a table of N words, then find the unmarked one: also O(N), but needs N words of memory. With vectors the sum could be done 8 at a time, as in `programs/sum_squares_vec.txt`.

# Q020 — Moving average (window of 3)

## Question
N (≥ 3) is at address 0 and an array A[0..N-1] at 4..4N. Write a program that computes the
moving average M[i] = (A[i] + A[i+1] + A[i+2]) / 3 (integer division) for i = 0..N-3 and
stores the N - 2 values from 4N + 4 on. Use vector instructions where they help.

## Answer
The sums are data-parallel; the division is not, because there is no vector divide.
1. **Vector sums**, like the Lab 3 FIR filter: three loads one word apart give 8 window
   sums with 2 adds, stored into M:
   ```
   v1 = [x2]         % A[i .. i+7]
   v2 = [x8]         % A[i+1 .. i+8]   (x8 = x2 + 4)
   v3 = [x10]        % A[i+2 .. i+9]   (x10 = x2 + 8)
   v1 = v1 + v2
   v1 = v1 + v3
   [x3] = v1         % M[i .. i+7] = 8 sums
   ```
   while i ≤ K - 8, with K = N - 2 outputs, so the loads stay inside A.
2. **Scalar tail** for the last K mod 8 sums.
3. **Scalar pass** over M: `M[i] = M[i] / 3`, which rounds towards zero.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `moving_average_data.byte`: N = 13 (11 outputs: 1 vector block + 3 tail sums) →
  `6 9 6 2 0 3 5 13 9 12 4` at 56..96 (e.g. (7 + 8 + 2) / 3 = 5), `[100] = 0`, and the
  `.freq` report shows 1 vector block.
- `moving_average_data2.byte`: N = 5 (3 outputs, tail only), `-5 -1 -2 4 -10` →
  `-2 0 -2` at 24..32 ((-5 - 1 - 2) / 3 = -8 / 3 = -2, not -3), `[36] = 0`.

## Viva
- **Why is the division scalar?** The machine has vector + - * only. Dividing by 3 with a multiply would need the high half of a 64-bit product, which it does not have.
- **Scalar alternative?** A sliding window: after each output, s = s - A[i] + A[i+3]: 2 loads per output instead of 3.
- **Why -2 and not -3 for -8 / 3?** Signed division rounds towards zero, as in C. Rounding down (floor) needs a correction when the remainder is negative.

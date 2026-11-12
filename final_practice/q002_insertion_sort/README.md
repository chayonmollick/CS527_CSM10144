# Q002 — Insertion sort (ascending, in place)

## Question
Write a program that sorts an array of N signed integers into ascending order in place
using insertion sort. N is at address 0 and A[0..N-1] at 4..4N; the sorted array must end
up in the same place.

## Answer
Keep A[0..i-1] sorted and insert A[i] into it, for i = 1..N-1:
1. `key = A[i]`, pointer `p = &A[i-1]` (x6).
2. Inner loop with two exits at the top: stop if `p < 4` (walked past A[0]) or if
   `A[p] <= key`; otherwise copy A[p] one word to the right and step p back:
   ```
   x9 = x6 - 4
   BLT .place        % p < &A[0]
   x7 = [x6]
   x9 = x7 - x5      % A[p] - key
   BLE .place        % A[p] <= key: the gap is at p + 4
   x8 = x6 + 4
   [x8] = x7         % [p + 4] = [p]: one word right
   x6 = x6 - 4
   ```
3. `.place`: `[p + 4] = key`.

Stopping at `A[p] <= key` (not `<`) keeps equal values in their original order (stable)
and saves moves.

## Changes
None: program only (`solution.txt`)

## Test
`./test.sh` runs `solution.txt` on:
- `insertion_sort_data.byte`: N = 10, `29 -4 17 0 300 -4 5 88 3 -61` →
  `-61 -4 -4 0 3 5 17 29 88 300` at 4..40.
- `insertion_sort_data2.byte`: N = 7, `50 40 30 20 10 0 -10` (descending, the worst case:
  every key walks to the front) → `-10 0 10 20 30 40 50` at 4..28.

## Viva
- **How does it differ from bubble and selection sort (`programs/`)?** It shifts instead of swapping (one store per move instead of two), and on a sorted array the inner loop stops at once: O(N) best case, O(N²) worst case (data2).
- **Why `p < 4` and not `j < 0`?** The loop walks addresses; &A[0] = 4, so j = -1 is address 0.
- **Is it stable?** Yes: the inner loop stops at an equal element, so the key goes after it.

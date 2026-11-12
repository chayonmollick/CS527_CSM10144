# Solved programs for the mini-computer

35 programs written in the course language, each with one or two data files and
checked results. In the test, find the closest match, copy it, and change the data
layout to the one in the question.

Every program starts with a comment header:
- **Task:** the question, as an examiner would set it.
- **Data:** which address holds what.
- **Result:** where the answer goes, with the expected values for each data file.
- **Method:** the technique used, and why it is vector or scalar.
- **Variants:** small changes for related questions, where there is an obvious one.
- **Regs:** what each register holds.

The course's own examples are in [`../examples/`](../examples/): `array_add.txt` and
`fir.txt` (the Lab 3 test, with vectors), `sum_n.txt`, `sum_array.txt`, `det3x3.txt`
and `complex_mul.txt`.

## Run and check
```bash
cd minicomputer/programs
./run_programs.sh                 # builds ../../lab6/sim if needed, runs all 35: 154 checks
./run_programs.sh sort            # only the programs whose name contains "sort"
SIM=~/exam/sim/sim ./run_programs.sh   # check your own (modified) simulator build instead

../../lab6/sim -c max_array.txt                           # compile only: errors with line numbers, byte-code listing
../../lab6/sim -b max_array.txt max_array_data.byte       # run one program
../tools/words.sh max_array_data_1.out                    # every word of the result, with its address
../../lab6/sim -b -t max_array.txt max_array_data.byte  # trace every instruction (stderr)
rm -f program.byte *.out *.freq log.txt                   # clean up after running here
```
- **Output files:** the final data memory is written to `<data file>_1.out` (1 is the pid), in the same format as the data file.
- **Reading a word:** each line of the `.out` file is one 4-byte word, so the word at byte address `a` is on line `a/4 + 1`. For example, `sed -n 12p max_array_data_1.out` gives the word at 44.
- **Line counts:** `<program>_1.freq` shows how many times each line ran. It is a quick way to show an examiner that a vector loop runs N/8 times.

Making a data file: words are 32-bit little-endian, 4 bytes per line (`10` →
`0A 00 00 00`, `-1` → `FF FF FF FF`, `300` → `2C 01 00 00`). This bash function
writes them for you:
```bash
tobyte() { for v in "$@"; do x=$(( v & 0xFFFFFFFF )); printf "%02X %02X %02X %02X\n" $(( x & 255 )) $(( x >> 8 & 255 )) $(( x >> 16 & 255 )) $(( x >> 24 & 255 )); done; }
tobyte 10 12 -5 37 8 37 -40 0 25 3 19 > my_data.byte     # N = 10, then the array
```

## The programs
In the result column, `N` is the word at address 0, and `4N + 4` is the first address after an
array of N words that starts at 4. `[4]` means the word at address 4.

### Arrays (scalar loops)
| File | Task | Technique | Result |
|---|---|---|---|
| `max_array` | Largest element and the index of its first occurrence | Counted loop; `x9 = A[i] - max`, BLE; BGE ends the loop | max at 4N+4, index at 4N+8 |
| `min_array` | Smallest element and its index | Pointer loop up to an end address; index = (address − 4) / 4 | min at 4N+4, index at 4N+8 |
| `count_even` | Count even and odd elements | Remainder `a - (a/2)*2`, BNE on the subtraction itself | even at 4N+4, odd at 4N+8 |
| `count_negative` | Count negatives, zeros and positives | Flags from `x9 = a - 0`, then BMI and BEQ on the same flags | 4N+4, 4N+8, 4N+12 |
| `count_key` | Count the occurrences of a key | Counted loop, BNE skips the increment | key at 4, A from 8; count at 4N+8 |
| `average` | Sum, integer average and remainder | Loop, then one division (rounds towards zero) | 4N+4, 4N+8, 4N+12 |
| `abs_array` | Absolute value of every element | BPL to skip, negate as `0 - a` (why not vectors: see header) | in place |
| `prefix_sum` | Running totals S[i] = A[0] + … + A[i] | Loop with a running sum | S at 4+4N |
| `histogram` | Counts of the values 0..7, plus the invalid ones | Computed address H + 4a; range check with BMI/BGT; one vector store clears the bins | H at 4N+4 .. 4N+32, invalid at 4N+36 |
| `reverse_array` | Reverse in place | While loop with two pointers, swap | in place |
| `linear_search` | Index of a key, or −1 | Loop with an early exit (BEQ); −1 built as 0 − 1 | key at 4, A from 8; index at 4N+8 |
| `binary_search` | Search a sorted array: index or −1, plus the number of probes | `while lo <= hi`, address of A[mid]; one compare feeds BEQ and BLT | 4N+8, 4N+12 |
| `bubble_sort` | Sort ascending | Nested loops, swap, stops early when a pass makes no swap | in place |
| `selection_sort` | Sort ascending | Nested loops over addresses, one swap per position | in place |
| `merge_sorted` | Merge two sorted arrays | Three while loops with end addresses (both arrays, rest of A, rest of B) | N1 at 0, N2 at 4, A from 8, B after A; C after B |

### Numbers
| File | Task | Technique | Result |
|---|---|---|---|
| `factorial` | n! (also `print`ed to log.txt) | Count-down loop: `x1 = x1 - 1` sets Z, so BNE needs no compare | [4] |
| `power` | a^b | Count-down loop | a at 0, b at 4; [8] |
| `fibonacci` | First N Fibonacci numbers | Counted loop, last two values in registers | F0..F(N-1) at 4 .. 4N |
| `gcd` | GCD (Euclid) and LCM | `while b != 0`, a mod b = `a - (a/b)*b` | gcd [8], lcm [12] |
| `is_prime` | Is n prime? And its smallest divisor | Trial division while d·d ≤ n; a loop with two exits | [4] = 1/0, [8] = divisor |
| `primes_upto` | Every prime ≤ N, and how many | Nested loops, the inner one exits early | count [4], primes from 8 |
| `digits` | Digit sum, reversed number, palindrome test | `/ 10` and remainder | [4], [8], [12] |
| `binary` | Count the 1 bits, list the binary digits | `/ 2` and remainder (there is no AND or shift) | [4] ones, [8] digits, bits from 12 |
| `polynomial` | p(x) by Horner's rule | Loop over the coefficients: r = r·x + c | x at 0, n at 4, c_n..c_0 from 8; p(x) at 12+4n |

### Vectors
| File | Task | Technique | Result |
|---|---|---|---|
| `copy_array_vec` | Copy N words, any N | **Vector** blocks of 8 while i ≤ N − 8, then a scalar tail | B at 4+4N |
| `scale_offset_vec` | Y = a·X + b, any N | **Vector** `v1 * x5`, `v1 + x6` (a, b in scalar registers) + scalar tail | a at 4, b at 8, X from 12; Y at 12+4N |
| `sum_squares_vec` | Sum of squares, any N (plain sum: drop the multiply) | **Vector** reduction into 8 lanes, scalar tail, shift-and-add lane sum | 4N+4 |
| `dot_product` | Dot product (scalar, for comparison) | Counted loop, two pointers | 4+8N |
| `dot_product_vec` | Dot product, N a multiple of 8 | **Vector** multiply-accumulate, then store and add the 8 lanes | 4+8N, lanes after it |
| `poly_vec` | Y[i] = p(X[i]) for a whole array | **Vector** Horner; broadcast = zero vector + scalar register; nested loop | Y after X |

### Matrices
| File | Task | Technique | Result |
|---|---|---|---|
| `matrix_add_vec` | C = A + B, 8×8 | **Vector:** one row is one vector; 4 vector instructions per row | A at 0, B at 256; C at 512 |
| `matvec_vec` | y = M·x, 8×8 | **Vector:** row × x in one multiply, then add the 8 lanes | M at 0, x at 256; y at 288 |
| `matmul4` | C = A·B, 4×4 | Scalar triple nested loop (i, j, k) | A at 0, B at 64; C at 128 |
| `matmul8_vec` | C = A·B, 8×8 | **Vector:** C row i = Σ A[i][k] · (B row k), with vector × scalar register | A at 0, B at 256; C at 512 |
| `transpose` | Transpose of an N×N matrix (4×4 in the data) | Nested loops, walking down a column of T (+4N per step) | N at 0, A from 4; T at 4+4N² |

## How to write a program for this machine

### The language in one screen
```
xD = xA + xB      xD = xA - 7      (+ - * /, signed; a constant 0..255 only as the 2nd operand)
xD = 5            xD = xA          (a constant 0..255 / a register copy)
xD = [xA]         xD = [12]        (load a word; a constant address only for 0..255)
[xA] = xS         [12] = xS        (store a word; the value must be a register)
.loop                              (a label: alone on its line, no colon)
BGE .loop                          (B<cond>: EQ NE MI PL GE LT GT LE HI LS CS CC VS VC AL; B alone = always)
vD = vA + vB      vD = vA * 3      vD = vA * xB     (+ - * on 8 lanes; xB = a scalar register)
vD = [xA]         [xA] = vD        vD = vA          (8 consecutive words; copy)
print xR                           (appends "Process id: 1   xR : <hex>" to log.txt)
% comment                          (everything after % is ignored)
```

### Plan the registers first
- **Write the register map as a comment** before the code, as the headers here do. A usual layout:
  - `x1`: N.
  - `x2`: loop counter i.
  - `x3`, `x4`: pointers (addresses).
  - `x5`..`x8`: loaded values.
  - `x9`: the throw-away result of compares.
  - `x10` and up: results and accumulators.
  - `x20` and up: addresses computed once, before the loop.
  - `v0`: accumulator; `v1` and up: loaded blocks.
- **There are 256 x registers:** give every quantity its own register, rather than reusing one for two jobs.
- **Set every register you use.** All registers are 0 when a task starts, but `x0` is an ordinary register, not a hard-wired zero. For `0 - a`, set `x20 = 0` and write `x4 = x20 - x4`.
- **Memory is byte-addressed.** Element i of an array at base B is at `B + 4i`. Step a pointer by 4 for one word and by 32 for one vector. Walking a pointer (`x3 = x3 + 4`) is cheaper than computing `base + 4*i` on every iteration.
- **Values that stay the same through the loop:** load them once into a register before the loop. Examples: `a` and `b` in `scale_offset_vec`, the vector x in `matvec_vec`, and the 8 rows of B in `matmul8_vec`.

### Constants bigger than 255, and negative constants
- **Bigger than 255:** use arithmetic.
  - 256: `x2 = 128` then `x2 = x2 * 2`.
  - 1000: `x2 = 250` then `x2 = x2 * 4`.
  - 300: `x2 = 255` then `x2 = x2 + 45`.
- **Negative:** subtract from 0. For −1: `x2 = 0` then `x2 = x2 - 1`. The compiler rejects `x2 = -1`.
- **Constant as the first operand:** `x1 = 10 - x2` is not allowed. Use `x9 = 10` then `x1 = x9 - x2`.
- **Arbitrary numbers:** put them in the data file and load them, e.g. `x5 = [4]`.
- **Storing a constant:** `[x1] = 5` is not allowed. Use `x6 = 5` then `[x1] = x6`.

### Compare = subtract, then branch
Only add and subtract (including the forms with a constant) set the flags N Z C V. Loads,
moves, `*` and `/` do not. To compare a with b, write `x9 = xA - xB` and branch right away:

| Want | Branch after `x9 = xA - xB` |
|---|---|
| a == b / a != b | `BEQ` / `BNE` |
| a < b, a <= b, a > b, a >= b (signed) | `BLT`, `BLE`, `BGT`, `BGE` |
| a < 0 / a >= 0 / a == 0 | `x9 = xA - 0`, then `BMI` / `BPL` / `BEQ` |
| unsigned a > b / a <= b | `BHI` / `BLS` |

Things to know about the flags:
- **C differs from ARM.** After a subtraction, C means a > b (unsigned), not a ≥ b as on ARM. So here `BCS`/`BHS` mean "a > b unsigned". Use the signed branches unless the question says unsigned.
- **Branches do not change the flags.** Several branches can test the result of one compare: BMI then BEQ in `count_negative`, BEQ then BLT in `binary_search`.
- **Nothing that adds or subtracts may sit between the compare and the branch,** not even a counter update. `binary_search` increments its probe counter *before* the compare for this reason.
- **A subtraction you need anyway is also a compare.** In `count_even`, `x5 = x4 - x5` followed by `BNE` tests the remainder. In `factorial`, `x1 = x1 - 1` followed by `BNE .loop` counts down with no separate compare.

### Loop skeletons
```
% counted loop: for (i = 0; i < N; i++)        % while loop: while (lo < hi)
x2 = 0                                         .loop
.loop                                          x9 = x3 - x4
x9 = x2 - x1                                   BGE .done          % branch on the OPPOSITE condition
BGE .done          % i >= N: leave                ...body...
   ...body...                                  BAL .loop
x2 = x2 + 1                                    .done
BAL .loop
.done

% count-down loop: repeat n times              % if / else
x9 = x1 - 0                                    x9 = x4 - x5
BLE .done          % n <= 0: skip              BLE .else          % if (a > b) ...
.loop                                             ...then part...
   ...body...                                  BAL .endif
x1 = x1 - 1        % sets Z at 0               .else
BNE .loop                                         ...else part...
.done                                          .endif

% nested loop: for i, for j
x2 = 0                                         % i
.outer
x9 = x2 - x1
BGE .done
x3 = 0                                         % j: reset INSIDE the outer loop
.inner
x9 = x3 - x1
BGE .nexti
   ...body...
x3 = x3 + 1
BAL .inner
.nexti
x2 = x2 + 1
BAL .outer
.done
```
- **Early exit:** branch straight out of the loop, e.g. `BEQ .found` in `linear_search` or `BEQ .next` in `primes_upto`.
- **Pointer loop:** compare an address with an end address instead of keeping a counter (`min_array`, `selection_sort`, `merge_sorted`).

### Vectors: 8 elements per instruction
- **Load and store:** `v1 = [x2]` loads the 8 words at x2 .. x2+28 into lanes 0..7, and `[x3] = v1` stores 8 words. Step the addresses by 32.
- **Operations:**
  - vector with vector: `+ - *`.
  - vector with a constant 0..255: `+ - *`.
  - vector with a scalar register: `v1 = v1 * x5` multiplies every lane by x5. Use this for weights and scale factors: `scale_offset_vec`, `matmul8_vec`, FIR.
- **What vectors cannot do:**
  - divide;
  - compare, max/min or select;
  - add across lanes;
  - move a lane into an x register.

  So per-element decisions stay scalar (`abs_array`, `max_array`, `count_even`).
- **Useful idioms:**
  - zero vector: `v0 = v0 - v0`;
  - copy: `v2 = v1`;
  - broadcast a scalar into all 8 lanes: `v2 = v0 + x5`, with v0 = 0 (`poly_vec`).
- **Sum of the 8 lanes:** there are two ways.
  - Store the vector, then load and add the 8 words with scalar instructions (`dot_product_vec`, `matvec_vec`).
  - Shift and add: store the vector, load it back from 16 bytes further on and add (lanes 0..3 now hold pair sums), then repeat with +8 and +4. Lane 0 is then the total (`sum_squares_vec`).
- **Matrices:** in a row-major 8×8 matrix one row is one vector.
  - Combinations of rows vectorise perfectly: `C = A + B`, and C row i = Σ A[i][k] · B row k (`matmul8_vec`).
  - Sums along a row (y = M·x) need lane sums (`matvec_vec`).
- **N not a multiple of 8:**
  1. **Vector part, then scalar tail** (always correct). Set `x7 = x1 - 8`. Run the vector loop while `i <= N - 8` (`x9 = x2 - x7`, `BGT .tail`), then a scalar loop for the last N mod 8 elements. Examples: `copy_array_vec`, `scale_offset_vec`, `sum_squares_vec`.
  2. **Pad the data with zeros** to a multiple of 8, if the question lets you choose the data.
  3. **Overlap the last block.** For out-of-place element-wise work with N ≥ 8, finish with one full block that starts at element N − 8. The few elements it repeats get the same values again. This is wrong for sums and for in-place updates.

  Never just run one more full block. It writes up to 7 words past the end of the output, and adds whatever is in memory after the array into a sum.
- **What "optimal vector use" means:**
  - Inside the vector loop: one load per input block, one instruction per arithmetic step, one store per output block.
  - Values that do not change are loaded once, before the loop.
  - Use the vector-with-scalar-register form instead of building a constant vector.
  - No scalar work on the data inside the vector loop.

  Compare `dot_product`, with 10 instructions per element (110 executed for N = 10), and `dot_product_vec`, with 10 instructions per 8 elements plus 25 once for the lane sum (54 executed for N = 16).

### Common mistakes
1. **`%` starts a comment, even in the middle of a line.** `x1 = x2 % x3` compiles *without an error* as `x1 = x2`. For a remainder write `x4 = x2 / x3`, `x4 = x4 * x3`, `x4 = x2 - x4`.
2. **Labels:** a label stands alone on its line, starts with `.`, and uses only letters, digits and `_`. `.loop:` and `.loop x1 = 1` both give "bad label". A branch needs the dot too: `BGE done` gives "usage: B<cond> .label".
3. **Constants:** they are 0..255, and only the second operand can be a constant (see above). The compiler says "constant 300 is out of range 0..255" or "the first operand must be an x register".
4. **One statement per line:** `x1 = 5 ; x2 = 6` and `x1 = x2 + x3 + x4` are errors ("expected `<dest> = <operand> <op> <operand>`"). A `;` is allowed only at the end of a line.
5. **Only add and subtract set flags.** After a load, `x9 = x4 - 0` is needed before `BMI`/`BEQ`. An add or subtract between the compare and the branch changes the flags.
6. **Division rounds towards zero:** `-7 / 2 = -3`, and the remainder is −1. Test "even" with `remainder == 0`, never `== 1`. An average of negative numbers is rounded towards zero too.
7. **Addresses are in bytes:** element i is at `base + 4*i`, and a vector step is 32. Constant addresses `[c]` only reach 0..255; the data space is 0..4095.
8. **Inner loops must be reset:** reset the inner loop's counter or pointer *inside* the outer loop.
9. **Size limits:** a branch can jump at most 127 instructions, and a program can have at most 255 instructions. The biggest program here (`matmul8_vec`) has 58.
10. **Vector registers:** `v1 = x2` and `x1 = v2` are not allowed. A vector can only be loaded from memory, copied, or computed.
11. **Writing past the end of the data file is fine.** The page is created (zeroed) and the `.out` file grows, which is why most programs here put results right after the input.

### A test-day routine
1. **Fix the data layout.** Write it as the header comment (Data / Result / Regs), then make the data file with `tobyte`.
2. **Copy the closest program** from this folder and change only the addresses and the core step.
3. **Compile with `-c`.** Errors come with line numbers.
4. **Run with `-b`** and check the result with `../tools/words.sh`. If it is wrong, run again with `-t` and follow the trace.
5. **Test the edge cases:** N = 1 (and 0), negative numbers, duplicates, a key that is not there, and N not a multiple of 8 for vector code.

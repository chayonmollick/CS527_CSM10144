# Q040 — Binary, hex and character constants

## Question
Let constants be written in binary (`0b1010`), in hex (`0xFF`) and as characters (`'A'` is
65), wherever a constant is allowed today, so `x1 = 0b1010`, `x4 = x3 + 0b11`,
`x5 = x5 - '0'` and `x6 = '%'` all work. They are still limited to 0..255. Which part of the
compiler do you change? Be careful with characters that are special in the language, such
as `%` and `,`.

## Answer
- **Hex already works:** `parse_number()` in lab6 has a `0x` branch (strtol, base 16).
  Nothing changes for `0xFF`.
- **Binary:** a `0b`/`0B` branch in `parse_number()`. It accepts only the digits 0 and 1
  (so `0b102` is rejected) and converts with `strtol(..., 2)`. `parse_simple()` already
  checks the range, so `0b100000000` (256) gives the usual "out of range 0..255" error.
- **Characters:** `'A'` cannot be handled at token level, because by then `'%'` has
  already been cut off as a comment, `','` has become a blank, and `' '` has been split
  into two tokens. So `clean_line()` first replaces every `'c'` (quote, one character,
  quote) by its decimal code, before it looks for `%`.
  - `x6 = '%'` becomes `x6 = 37`.
  - From then on it is an ordinary number, with the same rules as `37`: allowed as the
    second operand, inside `[ ]`, and so on.
- **Same machine code:** all three forms produce the same MOVC, ADDI, SUBI as a decimal
  constant. The listing still shows the original source (`x3 = 'A'` next to `x3 = 65`).

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `parse_number` | `0b`/`0B` prefix: binary, digits 0/1 only (hex `0x` was already there) |
| `compiler.cpp` | `clean_line` | Replace each `'c'` by its code (0..255) *before* the `%` comment is removed |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs `consts.txt`. It checks:
- the results 10, 255, 65, 68 (`'A' + 0b11`), 7 (`'7' - '0'`), 81 (`'%' + ','`) and
  33 (`' ' + 0B1`);
- three listing lines, which show the constant became an ordinary MOVC byte (`'%'` → `25`);
- that `0b100000000` is rejected as out of range;
- that `sum_array` still works.

## Viva
- **Why change `clean_line()` and not `tokenize()`?** `clean_line()` runs first, and it cuts the line at the first `%`. A `'%'` must become 37 before that happens, or `x6 = '%'` turns into `x6 = '`.
- **Is `x1 = 'A' - 1` allowed?** No. A constant may only be the second operand, as for decimals, so write `x1 = 'A'` then `x1 = x1 - 1`.
- **Why must binary reject other digits?** `strtol(..., 2)` stops at the first invalid digit, so without a check `0b102` would quietly become 2. Checking every character turns that into an error.
- **Where would a string go?** A string is several bytes in data memory. Since `.byte` files hold the data, a string is written there (`48 49 00 00` = "HI"). Character constants cover the single-byte case inside instructions.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `consts.txt`, `consts_bad.txt`, `consts_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.

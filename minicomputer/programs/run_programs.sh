#!/usr/bin/env bash
# Run every program in this folder on the simulator and check its results.
#   ./run_programs.sh                    all programs (first builds ../simulator/sim if needed)
#   ./run_programs.sh sort               only the programs whose name contains "sort"
#   SIM=~/exam/sim/sim ./run_programs.sh test another simulator build (no make then)
# Each program runs in its own scratch directory, so nothing is written in this folder.
# The expected values are worked out from the data files; each program's header
# comment explains them. Uses only bash and portable awk.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
FILTER="${1:-}"
if [ -z "${SIM:-}" ]; then
    SIM="$HERE/../simulator/sim"
    make -s -C "$HERE/../simulator" || { echo "could not build the simulator"; exit 1; }
fi
[ -x "$SIM" ] || { echo "no simulator at $SIM"; exit 1; }
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0

# word_at <file> <byte address>: signed 32-bit little-endian word from a .byte/.out file
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

# words_at <file> <byte address> <count>: <count> consecutive words, comma-separated
words_at() {
    awk -v A="$2" -v N="$3" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { for (k = 0; k < N; k++) { a = A + 4 * k
                  v = b[a] + b[a+1] * 256 + b[a+2] * 65536 + b[a+3] * 16777216
                  if (v >= 2147483648) v -= 4294967296
                  printf "%s%d", (k ? "," : ""), v }
              printf "\n" }' "$1"
}

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

# program <name>: print the name; false when a filter is given and the name does not match it
program() {
    case "$1" in *"$FILTER"*) echo "$1"; return 0 ;; *) return 1 ;; esac
}

# run <program> <data file> <what this data tests>: one task in batch mode in a fresh
# directory; sets DIR and OUT (the task's final data memory) and checks the exit code
run() {
    DIR="$WORK/$1-${2%.byte}"
    OUT="$DIR/${2%.byte}_1.out"
    mkdir -p "$DIR" && cp "$HERE/$1.txt" "$HERE/$2" "$DIR"/
    (cd "$DIR" && "$SIM" -b "$1.txt" "$2" > run.txt 2>&1)
    local rc=$?
    check "$2 ($3) compiles and runs to the end" "$rc" 0
    [ "$rc" -eq 0 ] || sed 's/^/          /' "$DIR/run.txt" | head -6
}

# One block per program: run it on each data file, then compare result words.
# In the labels, [44] is the word at byte address 44 and [4..40] the words from 4 to 40.
if program max_array; then
    run max_array max_array_data.byte "N = 10, the max occurs twice"
    check "max [44]" "$(word_at "$OUT" 44)" 37
    check "index of the max [48]" "$(word_at "$OUT" 48)" 2
    run max_array max_array_data2.byte "N = 6, all negative"
    check "max [28]" "$(word_at "$OUT" 28)" -3
    check "index of the max [32]" "$(word_at "$OUT" 32)" 1
fi
if program min_array; then
    run min_array min_array_data.byte "N = 8, the min occurs twice"
    check "min [36]" "$(word_at "$OUT" 36)" -7
    check "index of the min [40]" "$(word_at "$OUT" 40)" 2
fi
if program count_even; then
    run count_even count_even_data.byte "N = 10, negative and zero values too"
    check "even count [44]" "$(word_at "$OUT" 44)" 6
    check "odd count [48]" "$(word_at "$OUT" 48)" 4
fi
if program count_negative; then
    run count_negative count_negative_data.byte "N = 10"
    check "negatives [44]" "$(word_at "$OUT" 44)" 4
    check "zeros [48]" "$(word_at "$OUT" 48)" 2
    check "positives [52]" "$(word_at "$OUT" 52)" 4
fi
if program count_key; then
    run count_key count_key_data.byte "N = 12, key 5"
    check "count of 5 [56]" "$(word_at "$OUT" 56)" 4
fi
if program average; then
    run average average_data.byte "N = 7, positive sum"
    check "sum [32]" "$(word_at "$OUT" 32)" 123
    check "average [36]" "$(word_at "$OUT" 36)" 17
    check "remainder [40]" "$(word_at "$OUT" 40)" 4
    run average average_data2.byte "N = 4, negative sum"
    check "sum [20]" "$(word_at "$OUT" 20)" -15
    check "average [24]" "$(word_at "$OUT" 24)" -3
    check "remainder [28]" "$(word_at "$OUT" 28)" -3
fi
if program abs_array; then
    run abs_array abs_array_data.byte "N = 10"
    check "absolute values [4..40]" "$(words_at "$OUT" 4 10)" "3,7,0,1,15,128,42,9,6,2"
fi
if program prefix_sum; then
    run prefix_sum prefix_sum_data.byte "N = 10"
    check "prefix sums [44..80]" "$(words_at "$OUT" 44 10)" "3,4,8,9,14,23,25,31,36,39"
fi
if program histogram; then
    run histogram histogram_data.byte "N = 20, two values out of range"
    check "bins H[0..7] [84..112]" "$(words_at "$OUT" 84 8)" "2,3,4,2,1,4,1,1"
    check "invalid count [116]" "$(word_at "$OUT" 116)" 2
fi
if program reverse_array; then
    run reverse_array reverse_array_data.byte "N = 9, odd"
    check "reversed array [4..36]" "$(words_at "$OUT" 4 9)" "90,80,70,60,50,40,30,20,10"
    run reverse_array reverse_array_data2.byte "N = 8, even"
    check "reversed array [4..32]" "$(words_at "$OUT" 4 8)" "8,7,6,5,4,3,2,1"
fi
if program linear_search; then
    run linear_search linear_search_data.byte "key 25, present twice"
    check "index of 25 [48]" "$(word_at "$OUT" 48)" 2
    run linear_search linear_search_data2.byte "key 6, absent"
    check "index of 6 [48]" "$(word_at "$OUT" 48)" -1
fi
if program binary_search; then
    run binary_search binary_search_data.byte "key 31, present"
    check "index of 31 [56]" "$(word_at "$OUT" 56)" 8
    check "probes [60]" "$(word_at "$OUT" 60)" 2
    run binary_search binary_search_data2.byte "key 10, absent"
    check "index of 10 [56]" "$(word_at "$OUT" 56)" -1
    check "probes [60]" "$(word_at "$OUT" 60)" 3
fi
if program bubble_sort; then
    run bubble_sort bubble_sort_data.byte "N = 10, negatives and a duplicate"
    check "sorted array [4..40]" "$(words_at "$OUT" 4 10)" "-12,-4,0,3,7,8,15,15,29,42"
fi
if program selection_sort; then
    run selection_sort selection_sort_data.byte "N = 10, negatives and a duplicate"
    check "sorted array [4..40]" "$(words_at "$OUT" 4 10)" "-33,-5,0,11,12,12,22,25,64,90"
fi
if program merge_sorted; then
    run merge_sorted merge_sorted_data.byte "5 + 7 elements"
    check "merged C [56..100]" "$(words_at "$OUT" 56 12)" "-3,1,2,4,4,9,10,12,15,20,22,30"
fi
if program factorial; then
    run factorial factorial_data.byte "n = 12"
    check "12! [4]" "$(word_at "$OUT" 4)" 479001600
    check "print x10 wrote 12! (hex 1C8CFC00) to log.txt" "$(grep -c 'x10 : 1C8CFC00' "$DIR/log.txt")" 1
    run factorial factorial_data2.byte "n = 0"
    check "0! [4]" "$(word_at "$OUT" 4)" 1
fi
if program power; then
    run power power_data.byte "3^13"
    check "3^13 [8]" "$(word_at "$OUT" 8)" 1594323
    run power power_data2.byte "(-2)^7"
    check "-2^7 [8]" "$(word_at "$OUT" 8)" -128
fi
if program fibonacci; then
    run fibonacci fibonacci_data.byte "N = 20"
    check "F0 .. F19 [4..80]" "$(words_at "$OUT" 4 20)" "0,1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987,1597,2584,4181"
fi
if program gcd; then
    run gcd gcd_data.byte "1071 and 462"
    check "gcd(1071, 462) [8]" "$(word_at "$OUT" 8)" 21
    check "lcm [12]" "$(word_at "$OUT" 12)" 23562
    run gcd gcd_data2.byte "35 and 64, coprime"
    check "gcd(35, 64) [8]" "$(word_at "$OUT" 8)" 1
    check "lcm [12]" "$(word_at "$OUT" 12)" 2240
fi
if program is_prime; then
    run is_prime is_prime_data.byte "n = 97, prime"
    check "97 is prime? [4]" "$(word_at "$OUT" 4)" 1
    check "smallest divisor [8]" "$(word_at "$OUT" 8)" 97
    run is_prime is_prime_data2.byte "n = 91 = 7 * 13"
    check "91 is prime? [4]" "$(word_at "$OUT" 4)" 0
    check "smallest divisor [8]" "$(word_at "$OUT" 8)" 7
fi
if program primes_upto; then
    run primes_upto primes_upto_data.byte "N = 50"
    check "count [4]" "$(word_at "$OUT" 4)" 15
    check "primes [8..64]" "$(words_at "$OUT" 8 15)" "2,3,5,7,11,13,17,19,23,29,31,37,41,43,47"
fi
if program digits; then
    run digits digits_data.byte "n = 12321"
    check "digit sum [4]" "$(word_at "$OUT" 4)" 9
    check "reversed [8]" "$(word_at "$OUT" 8)" 12321
    check "palindrome [12]" "$(word_at "$OUT" 12)" 1
    run digits digits_data2.byte "n = 9075"
    check "digit sum [4]" "$(word_at "$OUT" 4)" 21
    check "reversed [8]" "$(word_at "$OUT" 8)" 5709
    check "palindrome [12]" "$(word_at "$OUT" 12)" 0
fi
if program binary; then
    run binary binary_data.byte "n = 181"
    check "ones [4]" "$(word_at "$OUT" 4)" 5
    check "digits [8]" "$(word_at "$OUT" 8)" 8
    check "bits, lowest first [12..40]" "$(words_at "$OUT" 12 8)" "1,0,1,0,1,1,0,1"
    run binary binary_data2.byte "n = 1024"
    check "ones [4]" "$(word_at "$OUT" 4)" 1
    check "digits [8]" "$(word_at "$OUT" 8)" 11
    check "bits, lowest first [12..52]" "$(words_at "$OUT" 12 11)" "0,0,0,0,0,0,0,0,0,0,1"
fi
if program polynomial; then
    run polynomial polynomial_data.byte "x = 2"
    check "p(2) [28]" "$(word_at "$OUT" 28)" 35
    run polynomial polynomial_data2.byte "x = -3"
    check "p(-3) [28]" "$(word_at "$OUT" 28)" 275
fi
if program copy_array_vec; then
    run copy_array_vec copy_array_vec_data.byte "N = 20: 2 vector blocks + 4 scalar"
    check "B = copy of A [84..160]" "$(words_at "$OUT" 84 20)" "5,-10,15,-20,25,-30,35,-40,45,-50,55,-60,65,-70,75,-80,85,-90,95,-100"
    check "A unchanged [4..80]" "$(words_at "$OUT" 4 20)" "5,-10,15,-20,25,-30,35,-40,45,-50,55,-60,65,-70,75,-80,85,-90,95,-100"
    run copy_array_vec copy_array_vec_data2.byte "N = 5: scalar tail only"
    check "B = copy of A [24..40]" "$(words_at "$OUT" 24 5)" "7,-8,9,-10,11"
    check "A unchanged [4..20]" "$(words_at "$OUT" 4 5)" "7,-8,9,-10,11"
fi
if program scale_offset_vec; then
    run scale_offset_vec scale_offset_vec_data.byte "N = 16, a = 3, b = 5"
    check "Y = 3*X + 5 [76..136]" "$(words_at "$OUT" 76 16)" "5,8,-1,14,-7,20,-13,26,29,-22,35,-28,41,-34,47,-40"
    run scale_offset_vec scale_offset_vec_data2.byte "N = 13, a = -2, b = 7: vector + scalar tail"
    check "Y = -2*X + 7 [64..112]" "$(words_at "$OUT" 64 13)" "5,3,1,-1,-3,-5,-7,-9,-11,-13,-15,-17,-19"
fi
if program sum_squares_vec; then
    run sum_squares_vec sum_squares_vec_data.byte "N = 13: 1 vector block + 5 scalar"
    check "sum of squares [56]" "$(word_at "$OUT" 56)" 377
    run sum_squares_vec sum_squares_vec_data2.byte "N = 16: 2 vector blocks, no tail"
    check "sum of squares [68]" "$(word_at "$OUT" 68)" 1496
fi
if program dot_product; then
    run dot_product dot_product_data.byte "N = 10"
    check "dot product [84]" "$(word_at "$OUT" 84)" -15
fi
if program dot_product_vec; then
    run dot_product_vec dot_product_vec_data.byte "N = 16"
    check "dot product [132]" "$(word_at "$OUT" 132)" -60
    check "8 partial sums (lanes) [136..164]" "$(words_at "$OUT" 136 8)" "2,3,-18,-20,2,-5,-24,0"
fi
if program poly_vec; then
    run poly_vec poly_vec_data.byte "N = 16"
    check "Y = p(X) [84..144]" "$(words_at "$OUT" 84 16)" "120,91,66,45,28,15,6,1,0,3,10,21,36,55,78,105"
fi
if program matrix_add_vec; then
    run matrix_add_vec matrix_add_vec_data.byte "8x8"
    check "C row 0 [512..540]" "$(words_at "$OUT" 512 8)" "1,12,23,34,45,56,67,78"
    check "C row 1 [544..572]" "$(words_at "$OUT" 544 8)" "8,19,30,41,52,63,74,85"
    check "C row 2 [576..604]" "$(words_at "$OUT" 576 8)" "15,26,37,48,59,70,81,92"
    check "C row 3 [608..636]" "$(words_at "$OUT" 608 8)" "22,33,44,55,66,77,88,99"
    check "C row 4 [640..668]" "$(words_at "$OUT" 640 8)" "29,40,51,62,73,84,95,106"
    check "C row 5 [672..700]" "$(words_at "$OUT" 672 8)" "36,47,58,69,80,91,102,113"
    check "C row 6 [704..732]" "$(words_at "$OUT" 704 8)" "43,54,65,76,87,98,109,120"
    check "C row 7 [736..764]" "$(words_at "$OUT" 736 8)" "50,61,72,83,94,105,116,127"
fi
if program matvec_vec; then
    run matvec_vec matvec_vec_data.byte "8x8"
    check "y = M x [288..316]" "$(words_at "$OUT" 288 8)" "-168,-132,-96,-60,-24,12,48,84"
fi
if program matmul4; then
    run matmul4 matmul4_data.byte "4x4"
    check "C row 0 [128..140]" "$(words_at "$OUT" 128 4)" "16,11,5,6"
    check "C row 1 [144..156]" "$(words_at "$OUT" 144 4)" "40,27,17,14"
    check "C row 2 [160..172]" "$(words_at "$OUT" 160 4)" "64,43,29,22"
    check "C row 3 [176..188]" "$(words_at "$OUT" 176 4)" "88,59,41,30"
fi
if program matmul8_vec; then
    run matmul8_vec matmul8_vec_data.byte "8x8"
    check "C row 0 [512..540]" "$(words_at "$OUT" 512 8)" "-8,-35,8,21,-36,-23,50,-37"
    check "C row 1 [544..572]" "$(words_at "$OUT" 544 8)" "32,-29,0,19,8,-3,-24,15"
    check "C row 2 [576..604]" "$(words_at "$OUT" 576 8)" "0,22,-26,26,-2,-10,-8,4"
    check "C row 3 [608..636]" "$(words_at "$OUT" 608 8)" "4,19,-16,-21,24,19,-46,29"
    check "C row 4 [640..668]" "$(words_at "$OUT" 640 8)" "-10,-29,12,13,14,-15,6,27"
    check "C row 5 [672..700]" "$(words_at "$OUT" 672 8)" "39,13,-23,11,-5,-31,13,7"
    check "C row 6 [704..732]" "$(words_at "$OUT" 704 8)" "-2,-8,-4,0,-6,-2,2,-4"
    check "C row 7 [736..764]" "$(words_at "$OUT" 736 8)" "20,-20,-30,-20,20,0,0,30"
fi
if program transpose; then
    run transpose transpose_data.byte "4x4"
    check "T row 0 [68..80]" "$(words_at "$OUT" 68 4)" "1,5,9,13"
    check "T row 1 [84..96]" "$(words_at "$OUT" 84 4)" "2,6,10,14"
    check "T row 2 [100..112]" "$(words_at "$OUT" 100 4)" "3,7,11,15"
    check "T row 3 [116..128]" "$(words_at "$OUT" 116 4)" "4,8,12,16"
    run transpose transpose_data2.byte "3x3"
    check "T row 0 [40..48]" "$(words_at "$OUT" 40 3)" "1,4,7"
    check "T row 1 [52..60]" "$(words_at "$OUT" 52 3)" "2,5,8"
    check "T row 2 [64..72]" "$(words_at "$OUT" 64 3)" "3,6,9"
fi

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]

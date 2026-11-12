#!/usr/bin/env bash
# Q015 checks: trace and anti-diagonal sum of an 8x8 matrix (results at 256 and 260).
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on diagonals_data (A[i][j] = i*j)" "$(run_sim -b solution.txt diagonals_data.byte)" 0
check "trace = 140" "$(word_at "$RUN/diagonals_data_1.out" 256)" 140
check "anti-diagonal = 56" "$(word_at "$RUN/diagonals_data_1.out" 260)" 56
check "runs on diagonals_data2 (values -300..300)" "$(run_sim -b solution.txt diagonals_data2.byte)" 0
check "trace = -275" "$(word_at "$RUN/diagonals_data2_1.out" 256)" -275
check "anti-diagonal = 185" "$(word_at "$RUN/diagonals_data2_1.out" 260)" 185
finish

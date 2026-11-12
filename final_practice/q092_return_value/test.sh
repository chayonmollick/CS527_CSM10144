#!/usr/bin/env bash
# Q092 checks: a task that halts returns x1: <program>_<pid>.ret and "pid N returned V".
. "$(dirname "$0")/../lib.sh"
build_solution

check "pid 3 fails, so the exit code is 1" \
      "$(run_sim -b fact.txt ret_data.byte negative.txt ret_data.byte fails.txt ret_data.byte fact.txt ret_data.byte)" 1
check "fact_1.ret holds 5! = 120" "$(cat "$RUN/fact_1.ret")" 120
check "negative_2.ret holds -7 (signed)" "$(cat "$RUN/negative_2.ret")" -7
check "the second copy of fact has its own file fact_4.ret" "$(cat "$RUN/fact_4.ret")" 120
check "a task stopped by an error writes no .ret" "$([ -e "$RUN/fails_3.ret" ] && echo yes || echo no)" no
check "messages for pids 2, 1, 4 in the order they finished" \
      "$(grep '^pid [0-9]* returned' "$RUN/run.txt" | tr '\n' ',')" "pid 2 returned -7,pid 1 returned 120,pid 4 returned 120,"

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
check "and return their x1 (sum_array's index ends at 9)" "$(cat "$RUN/sum_array_1.ret")" 9
finish

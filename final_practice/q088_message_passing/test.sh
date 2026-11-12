#!/usr/bin/env bash
# Q088 checks: send xR xP / xD = recv; recv blocks while the mailbox is empty, messages arrive in order.
. "$(dirname "$0")/../lib.sh"
build_solution

(cd "$RUN" && "$SIM" -c sender.txt > listing.txt)
check "send x1 x2 encodes as E8 00 01 02" "$(grep -c 'E8 00 01 02   send x1 x2' "$RUN/listing.txt")" 1
check "x5 = recv encodes as E9 05 00 00" "$(grep -c 'E9 05 00 00   x5 = recv' "$RUN/listing.txt")" 1

check "receiver (pid 1) + sender (pid 2) run without error" "$(run_sim -b receiver.txt msg_data.byte sender.txt msg_data.byte)" 0
check "messages arrive in the order sent" \
      "$(for a in 4 8 12 16 20; do printf '%s ' "$(word_at "$RUN/msg_data_1.out" $a)"; done)" "10 20 30 40 50 "
check "the receiver's sum" "$(word_at "$RUN/msg_data_1.out" 0)" 150
check "the reply reached the sender" "$(word_at "$RUN/msg_data_2.out" 0)" 150
check "blocked receiver executed nothing while waiting (42 instructions)" \
      "$(grep -c 'pid 1 (receiver) finished on processor 0 after 42 instructions' "$RUN/run.txt")" 1
check "blocked sender executed nothing while waiting (25 instructions)" \
      "$(grep -c 'pid 2 (sender) finished on processor 1 after 25 instructions' "$RUN/run.txt")" 1
check "each recv executed once per message" "$(grep -c $'^5\tx1 = recv' "$RUN/receiver_1.freq")" 1

printf 'x1 = 5\nx2 = 9\nsend x1 x2\n' > "$RUN/bad_send.txt"
check "send to a pid that does not exist stops the task" "$(run_sim -b bad_send.txt msg_data.byte)" 1
check "with an error message" "$(grep -c 'send: there is no task with pid 9' "$RUN/run.txt")" 1

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish

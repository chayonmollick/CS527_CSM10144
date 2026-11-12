#!/usr/bin/env bash
# Q073 checks: a task may use its data-file pages plus EXTRA_DATA_PAGES (1) more; another page is a segmentation fault.
. "$(dirname "$0")/../lib.sh"
build_solution
check "pages.txt fails, the others run: exit code 1" \
    "$(run_sim -b pages.txt pages_data.byte extra.txt extra_data.byte sum_array.txt sum_array_data.byte)" 1
# pages.txt (1-page data file): page 1 is its one extra page, page 3 (store at instruction 5) is one too many
check "segmentation fault on data page 3, at instruction 5" \
    "$(grep -c 'error at instruction 5: segmentation fault: data page 3 is not in the data file' "$RUN/run.txt")" 1
check "pages.txt stopped after 6 instructions, 1 page fault" \
    "$(grep -c 'pid 1 (pages) STOPPED WITH AN ERROR on processor 0 after 6 instructions, 1 page fault(s)' "$RUN/run.txt")" 1
check "its store to the extra page happened" "$(word_at "$RUN/pages_data_1.out" 1020)" 7
check "extra.txt (data page 2 only) finished with 1 page fault" \
    "$(grep -c 'pid 2 (extra) finished on processor 1 after 9 instructions, 1 page fault(s)' "$RUN/run.txt")" 1
check "extra.txt copied 41 into page 2" "$(word_at "$RUN/extra_data_2.out" 1024)" 41
check "and wrote 42 next to it (same page, no new fault)" "$(word_at "$RUN/extra_data_2.out" 1028)" 42
check "sum_array in the same run" "$(word_at "$RUN/sum_array_data_3.out" 36)" 45
# with a 2-page data file (516 bytes) page 1 belongs to the file, page 3 is the extra one, page 7 is too many
awk 'BEGIN { for (i = 0; i < 129; i++) print "00 00 00 00" }' > "$RUN/two_pages_data.byte"
check "pages.txt on a 2-page data file gets further, then fails" "$(run_sim -b pages.txt two_pages_data.byte)" 1
check "now the fault is on page 7, at instruction 7" \
    "$(grep -c 'error at instruction 7: segmentation fault: data page 7' "$RUN/run.txt")" 1
check "after 8 instructions, still only 1 page fault" "$(grep -c 'after 8 instructions, 1 page fault(s)' "$RUN/run.txt")" 1
check "the store to page 3 (the extra page) happened" "$(word_at "$RUN/two_pages_data_1.out" 2040)" 7
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish

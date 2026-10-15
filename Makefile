# CS527 OpenMP Lab — build all tasks with: make
# On Linux (lab/grader machine) the default gcc has OpenMP built in.
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -fopenmp
LDFLAGS ?= -fopenmp
LDLIBS  ?= -lm

TASKS = task1_vecadd task2_sum task3_maxmin task4_matadd task5_matmul

all: $(TASKS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TASKS)

.PHONY: all clean

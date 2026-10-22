# CS527 OpenMP Lab 8 (session 2) — build all tasks with: make
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -fopenmp
LDFLAGS ?= -fopenmp
LDLIBS  ?= -lm

TASKS = task1_search task2_sort task3_histogram task4_transpose task5_convolution

all: $(TASKS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TASKS)

.PHONY: all clean

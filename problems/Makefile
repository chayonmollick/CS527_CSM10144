# CS527 OpenMP problem bank: builds every p*.c (one program per problem; all include common.h)
# `make` works on Linux (native gcc) and macOS (Homebrew gcc, else clang+libomp).
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
LDLIBS  ?= -lm

# Use -fopenmp natively if the compiler supports it (gcc on Linux); otherwise
# prefer a Homebrew GNU gcc; otherwise fall back to Apple clang + libomp.
OMP_NATIVE := $(shell printf 'int main(){return 0;}' | $(CC) -fopenmp -x c - -o /dev/null >/dev/null 2>&1 && echo 1)
BREW_GCC   := $(shell ls /opt/homebrew/bin/gcc-[0-9]* /usr/local/bin/gcc-[0-9]* 2>/dev/null | head -1)

ifeq ($(OMP_NATIVE),1)
  OMP_CFLAGS := -fopenmp
  OMP_LDLIBS := -fopenmp
else ifneq ($(BREW_GCC),)
  CC         := $(BREW_GCC)
  OMP_CFLAGS := -fopenmp
  OMP_LDLIBS := -fopenmp
else
  LIBOMP     := $(shell brew --prefix libomp 2>/dev/null)
  OMP_CFLAGS := -Xpreprocessor -fopenmp -I$(LIBOMP)/include
  OMP_LDLIBS := -L$(LIBOMP)/lib -lomp
endif

TASKS = $(basename $(wildcard p[0-9]*.c))

all: $(TASKS)

%: %.c common.h
	$(CC) $(CFLAGS) $(OMP_CFLAGS) $< -o $@ $(OMP_LDLIBS) $(LDLIBS)

clean:
	rm -f $(TASKS)

.PHONY: all clean

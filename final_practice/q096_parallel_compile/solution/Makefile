# CS527 Lab 6: the complete mini-computer simulator (includes Labs 1-5)
#   make                 build ./sim
#   make test            build and run the regression tests (../minicomputer/tests/run_tests.sh)
#   ./sim -b ../minicomputer/examples/sum_array.txt ../minicomputer/examples/sum_array_data.byte
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

# Q096: OpenMP flags (OMPFLAGS to compile, OMPLIBS to link, used below), found as in e22:
# g++ -fopenmp on Linux; Apple's clang++ has none, so Homebrew g++-<N>, else clang++ + libomp.
OMP_NATIVE := $(shell printf 'int main(){return 0;}' | $(CXX) -fopenmp -x c++ - -o /dev/null >/dev/null 2>&1 && echo 1)
BREW_GXX   := $(shell ls /opt/homebrew/bin/g++-[0-9]* /usr/local/bin/g++-[0-9]* 2>/dev/null | head -1)
ifeq ($(OMP_NATIVE),1)
  OMPFLAGS := -fopenmp
  OMPLIBS  := -fopenmp
else ifneq ($(BREW_GXX),)
  CXX      := $(BREW_GXX)
  OMPFLAGS := -fopenmp
  OMPLIBS  := -fopenmp
else
  LIBOMP   := $(shell brew --prefix libomp 2>/dev/null)
  OMPFLAGS := -Xpreprocessor -fopenmp -I$(LIBOMP)/include
  OMPLIBS  := -L$(LIBOMP)/lib -lomp
endif

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(OMPLIBS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) -c $<

test: sim
	cd ../minicomputer/tests && ./run_tests.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt

.PHONY: test clean

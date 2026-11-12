# CS527 Lab 2: mini-computer simulator with constants, flags, [ ] memory access,
# labels, branches and comments
#   make          build ./sim
#   make test     build, then run test.sh (every example, byte code, flags and error checks)
#   make clean    remove the build and the files a run writes
#   ./sim examples/sum_array.txt examples/sum_array_data.byte     -> program.byte, sum_array_data.out
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o
HDRS = config.h compiler.h memory.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim
	bash test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out

.PHONY: test clean

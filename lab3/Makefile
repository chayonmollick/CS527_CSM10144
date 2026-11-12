# CS527 Lab 3: mini-computer with vector instructions (one processor)
#   make          build ./sim
#   make test     build, run every example and check the results (test.sh)
#   make clean    remove the build and the run outputs
#   ./sim examples/array_add.txt examples/array_add_data.byte     -> program.byte, array_add_data.out
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o
HDRS = config.h compiler.h memory.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim
	./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out

.PHONY: test clean

# CS527 Lab 1: mini-computer simulator (compiler, processor, memory)
#   make          build ./sim
#   make test     build, then run test.sh (every example, byte code and error checks)
#   make clean    remove the build and the files a run writes
#   ./sim examples/sum4.txt examples/sum4_data.byte     -> program.byte, sum4_data.out
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

# CS527 Lab 5: the mini-computer simulator with logical and physical memory (paging)
#   make           build ./sim
#   make test      build, then run ./test.sh (every example in a temp dir, results checked)
#   make clean     remove the build and the run output
#   ./sim -b examples/pages.txt examples/pages_data.byte
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim
	./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out log.txt

.PHONY: test clean

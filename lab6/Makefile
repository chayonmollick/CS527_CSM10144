# CS527 Lab 6: the complete mini-computer simulator (includes Labs 1-5)
#   make                 build ./sim
#   make test            build and run the regression tests (../minicomputer/tests/run_tests.sh)
#   ./sim -b ../minicomputer/examples/sum_array.txt ../minicomputer/examples/sum_array_data.byte
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim
	cd ../minicomputer/tests && ./run_tests.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt

.PHONY: test clean

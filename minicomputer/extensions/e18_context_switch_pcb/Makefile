# CS527 mini-computer simulator (Labs 1-6) + EXT e18: context switching with a PCB per task, NP = 1
#   make                 build ./sim
#   make test            base regression tests (../../tests/run_tests.sh) + ./test.sh   (EXT e18)
#   ./sim -b ../../examples/counter.txt ../../examples/counter_data.byte ../../examples/counter_by100.txt ../../examples/counter_data.byte
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim                                  # EXT e18: base tests (1 check fails by design, see README), then ./test.sh
	-SIM=$(CURDIR)/sim ../../tests/run_tests.sh
	./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt

.PHONY: test clean

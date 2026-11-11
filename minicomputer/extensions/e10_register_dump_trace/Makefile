# CS527 mini-computer simulator (Labs 1-6)
# EXT e10: register dump (<program>_<pid>.regs) and per-task trace with -T (<program>_<pid>.trace)
#   make                 build ./sim
#   make test            EXT e10: base regression tests on this build, then ./test.sh
#   ./sim -b -T ../../examples/sum_array.txt ../../examples/sum_array_data.byte   (EXT e10: path from extensions/<dir>)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim                       # EXT e10: run the base tests on this build, then ./test.sh
	SIM=$(CURDIR)/sim ../../tests/run_tests.sh
	./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt *.regs *.trace   # EXT e10: + .regs .trace

.PHONY: test clean

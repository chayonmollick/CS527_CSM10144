# CS527 mini-computer simulator (Labs 1-6)
# EXT e08: 5-stage pipeline hazard model (writes <program>_<pid>.pipe)
#   make                 build ./sim
#   make test            EXT e08: base regression tests on this build, then ./test.sh
#   ./sim -b ../../examples/sum_array.txt ../../examples/sum_array_data.byte   (EXT e08: path from extensions/<dir>)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim                       # EXT e08: run the base tests on this build, then ./test.sh
	SIM=$(CURDIR)/sim ../../tests/run_tests.sh
	./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt *.pipe   # EXT e08: + .pipe

.PHONY: test clean

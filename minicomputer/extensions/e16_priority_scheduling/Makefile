# CS527 mini-computer simulator (Labs 1-6)
#   make                 build ./sim
#   make test            build, run the base regression tests and ./test.sh   (EXT e16)
#   ./sim -b -s priority job_long.txt job_data.byte 1 job_short.txt job_data.byte 5   (EXT e16)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim                     # EXT e16: base regression tests on this build, then this extension's tests
	SIM=$(CURDIR)/sim ../../tests/run_tests.sh && ./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt

.PHONY: test clean

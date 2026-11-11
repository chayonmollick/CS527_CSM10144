# CS527 mini-computer simulator (Labs 1-6)
#   make                 build ./sim
#   make test            build, run the base regression tests and ./test.sh   (EXT e19)
#   ./sim -b work.txt work3.byte work.txt work1.byte; cat timeline.txt          (EXT e19)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim                     # EXT e19: base regression tests on this build, then this extension's tests
	SIM=$(CURDIR)/sim ../../tests/run_tests.sh && ./test.sh

clean:                        # EXT e19: also timeline.txt
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt timeline.txt

.PHONY: test clean

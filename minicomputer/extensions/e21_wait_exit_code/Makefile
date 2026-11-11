# CS527 mini-computer simulator (Labs 1-6) + EXT e21: exit codes, fork() and wait()
#   make                 build ./sim
#   make test            base regression tests on this build, then ./test.sh   (EXT e21)
#   ./sim -b wait_demo.txt wait_demo_data.byte                                  (EXT e21)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim    # EXT e21: run the base tests against this build, then this extension's tests
	SIM=$(CURDIR)/sim ../../tests/run_tests.sh && ./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt

.PHONY: test clean

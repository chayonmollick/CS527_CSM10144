# CS527 mini-computer simulator (Labs 1-6) + EXT e17: shell commands ps, kill, mem, help
#   make                 build ./sim
#   make test            base regression tests (../../tests/run_tests.sh) + ./test.sh   (EXT e17)
#   printf 'loop.txt loop_data.byte\nsleep 3\nps\nkill 1\nps\nexit\n' | ./sim
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim                                  # EXT e17: base tests on this build, then this extension's tests
	SIM=$(CURDIR)/sim ../../tests/run_tests.sh && ./test.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt

.PHONY: test clean

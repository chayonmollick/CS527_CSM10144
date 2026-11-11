# CS527 mini-computer simulator (Labs 1-6)
#   make                 build ./sim
#   make test            e11 tests (./test.sh), then the 33 base checks   (EXT e11)
#   ./sim -b ../../examples/sum_array.txt ../../examples/sum_array_data.byte
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
OBJS = main.o compiler.o processor.o memory.o os.o
HDRS = config.h compiler.h memory.h os.h processor.h

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $<

test: sim                       # EXT e11: run from an extension folder
	./test.sh
	SIM=$(CURDIR)/sim ../../tests/run_tests.sh

clean:
	rm -f sim $(OBJS) program.byte *.out *.freq log.txt

.PHONY: test clean

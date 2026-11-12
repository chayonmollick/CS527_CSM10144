# CS527 Lab 4: mini-computer with NP processors and an OS (scheduler, shell, loader, print)
#   make          build ./sim
#   make test     build, run the examples and check the results (test.sh)
#   make clean    remove the build and the run outputs
#   ./sim -b examples/counter.txt examples/counter_data.byte examples/fir.txt examples/fir_data.byte
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

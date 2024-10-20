CC = gcc
CXX = g++
CFLAGS = -O3 -std=gnu11
CXXFLAGS = -O3 -std=c++20

brainfuck: brainfuck.cc
	$(CXX) $(CXXFLAGS) -o brainfuck brainfuck.cc

clean:
	rm -f brainfuck

.PHONY: all clean

CXXFLAGS = -std=c++23 -Wall -Wextra -O3

CXX ?= clang++

all: main server

main.cpp: index.hpp ;

server: server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

main: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	$(RM) main server

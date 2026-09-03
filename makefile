.PHONY: all clean

CXXFLAGS = -std=c++23 -Wall -Wextra -O3

CXXFLAGS += -I.

DEPS = src/index.hpp

CXX ?= clang++

all: main server

server: src/server.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ $<

main: src/main.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) main server

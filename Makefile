CXX := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)

all: ember

ember: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

repl: ember
	./ember

build/%.o: src/%.cpp $(wildcard src/*.h) | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

debug: CXXFLAGS := -std=c++17 -g -O0 -Wall -Wextra -fsanitize=address
debug: clean ember

test: ember
	./tests/run_tests.sh

bench: ember
	./bench/run_bench.sh

clean:
	rm -rf build ember

.PHONY: all repl debug test bench clean

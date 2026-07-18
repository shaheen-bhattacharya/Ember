CXX := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -MMD -MP
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
DEP := $(OBJ:.o=.d)

all: ember

ember: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

repl: ember
	./ember

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compiler-generated header dependencies: touching one header only rebuilds
# the objects that actually include it.
-include $(DEP)

build:
	mkdir -p build

debug: CXXFLAGS := -std=c++17 -g -O0 -Wall -Wextra -fsanitize=address -MMD -MP
debug: clean ember

test: ember
	./tests/run_tests.sh

bench: ember
	./bench/run_bench.sh

profile: ember
	EMBER_PROFILE=1 EMBER_LOG_HOT=1 ./ember bench/fib.em

clean:
	rm -rf build ember

.PHONY: all repl debug test bench profile clean

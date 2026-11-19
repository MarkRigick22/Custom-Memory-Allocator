CXXFLAGS ?= -O2 -g
FLAGS = -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude
WRAP = -Wl,--wrap=sbrk,--wrap=mmap,--wrap=munmap
.PHONY: test bench clean
test: build/stage1 build/stage2 build/buddy
	timeout 120 ./build/stage1
	timeout 120 ./build/stage2
	timeout 120 ./build/buddy
build:
	mkdir -p $@
build/stage1: src/malloc_1.cpp tests/stages.cpp include/os_malloc.h | build
	$(CXX) $(FLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@
build/stage2: src/malloc_2.cpp tests/stages.cpp include/os_malloc.h | build
	$(CXX) $(FLAGS) $(CXXFLAGS) -DFIRST_FIT $(filter %.cpp,$^) -o $@
build/buddy: src/malloc_3.cpp tests/buddy.cpp tests/invariants.cpp include/os_malloc.h | build
	$(CXX) $(FLAGS) $(CXXFLAGS) tests/buddy.cpp tests/invariants.cpp $(WRAP) -o $@
build/bench: src/malloc_3.cpp bench/trace.cpp include/os_malloc.h | build
	$(CXX) $(FLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@
bench: build/bench
	timeout 120 ./build/bench
clean:
	rm -rf build

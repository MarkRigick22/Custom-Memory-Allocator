# Custom Memory Allocator

A single-threaded Linux allocator in C++17. The final stage manages a 4 MiB heap arena with 11 buddy size classes and uses anonymous mappings for requests that cannot fit in a root block. The project also preserves the two earlier stages, making it possible to compare byte-exact bump allocation, first-fit reuse and buddy coalescing.

| Stage | Storage | Behavior |
| --- | --- | --- |
| 1 | Process break | Only smalloc. Requests grow the heap by exactly the requested byte count. |
| 2 | Process break | First-fit reuse in address order, no splitting or alignment padding. Tracks blocks, free space and metadata. |
| 3 | Aligned arena and independent mappings | Split, coalesce, grow by merging available buddies, or relocate while preserving contents. Large mappings return to Linux on free. |

### Build and run

On Linux with GNU Make, coreutils, GCC or Clang and a GNU-compatible linker:

```sh
make -j2 test
make bench
```

Each stage exports the same symbols and links independently. To use stage three from another C++ file, compile with `g++ -std=c++17 -O2 -Iinclude demo.cpp src/malloc_3.cpp -o demo`.

```cpp
#include "os_malloc.h"
int main() {
    auto* p = static_cast<unsigned char*>(scalloc(32, 1));
    if (!p) return 1;
    p[0] = 42;
    void* larger = srealloc(p, 1024);
    if (!larger) { sfree(p); return 1; }
    bool preserved = static_cast<unsigned char*>(larger)[0] == 42;
    sfree(larger);
    return preserved ? 0 : 1;
}
```

### Engineering evidence

The Linux test suite exercises all three stages. A seeded buddy test performs 15,000 allocation, reallocation and free decisions, verifies live payloads and checks for overlap. A separate structural walker reconciles the partition with the ordered free lists, live-start bitmap, mapping list and statistics. System-call fault injection checks ownership and accounting on failure.

In one ARM64 emulated Linux microbenchmark, 32 simultaneously live small requests left 5,424 bytes of unused payload capacity inside buddy blocks. Median batch-average small allocation and free times were 94 ns and 125 ns per operation. Mapping-backed allocations had substantially different costs. See [benchmark conditions and libc comparison](docs/benchmarks.md).

### Origin and scope

The three stages derive from a returned joint Technion Operating Systems homework submission. Both the algorithms required and the API names were specified by the course. The returned sources implement the list management, splitting, merging, growth policy and statistics. Individual authorship of specific lines cannot be established from that snapshot. The local checker and optional fourth-stage working file are excluded. The header here restates the API. The first implementation commit preserves the returned stages; the subsequent fixes, tests, measurements and documentation were prepared with Codex assistance. Redistribution rights for the entire joint submission are unconfirmed.

Read the [architecture and invariants](docs/architecture.md) and [test results and limitations](docs/testing.md). This is a teaching allocator for a single-threaded process with exclusive control over its use of the program break. A future experiment would compare ordered-list insertion with a bitmap-based free-block index under fragmentation.

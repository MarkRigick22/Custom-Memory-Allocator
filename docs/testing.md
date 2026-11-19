# Verification

Each stage builds as a separate executable, with warnings as errors. From a fresh checkout:

```sh
make -j2 test
make clean
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  make -j2 test CXX=clang++ CXXFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined'
```

Repeat with g++ for the GCC build. Stage tests have 120-second process timeouts. The workflow exercises optimized and ASan/UBSan configurations with each compiler.

| Coverage | Specific assertions |
| --- | --- |
| Stages one and two | Odd sizes, non-overlap, byte-exact accounting, first-fit reuse, zero fill, null/size handling, retained payloads and metadata overhead. |
| Stage three | Each order boundary and adjacent sizes, max_align_t alignment, allocation exhaustion, mapping threshold, two realloc merge directions, move-on-resize for mappings, failure preservation, repeated frees and complete return to 32 free roots. |
| Stress | Three seeds (1, 7, 1337), 5,000 trace steps each, up to 64 live slots. Payload marker checks before operations and after realloc; pairwise disjoint live ranges on success. |
| Independent structure checks | Every 50 stress steps and at full-recovery checkpoints, walk the complete arena and check exact coverage, metadata sizes, free-list order and links, live-start marks, tracked mappings and totals. |
| System-call failures | Link-time wrappers reject arena acquisition, mapping growth and unmap on demand. Check old contents and counts remain valid. Production builds use the real Linux calls. |

## Defects corrected

Stage two placed ordinary aligned C++ metadata at potentially odd addresses. The assignment prohibits padding between blocks, so the implementation uses a packed Linux header retaining the original field layout and explicitly starts the metadata lifetime. Stage three previously read a released header before checking mapping ownership, and a retired upper-half header could be mistaken for a current block after coalescing. The live-start bitmap and mapping membership check resolve those paths. The original unmap path updated lists and byte counts before checking the system result. The corrected path preserves ownership on failure. Arena initialization now reserves padding and the arena together, avoiding a partially consumed break if reservation fails. Invalid sizes are rejected before lazy arena initialization. The original calloc division guard and the two-pass buddy growth policy were retained.

ASan and UBSan instrument allocator code and the tests. These tools cannot automatically put red zones around individual chunks inside a custom arena or identify leaks of every logical allocation. Independent pattern, partition and accounting assertions cover that gap. Valgrind was unavailable in the guest. The implementation is single-threaded; it has no concurrency guarantee.

## Recorded results

On Ubuntu 24.04.5 ARM64, Linux 7.0.12-linuxkit, all three test executables passed in each of four configurations: GCC 13.3 optimized, GCC ASan/UBSan, Clang 18.1.3 optimized and Clang ASan/UBSan. No sanitizer findings were reported. The three recovered local stage checker programs also compiled and reported passes. They are excluded from the repository; the independently written tests above are included. Hosted CI has not been verified.

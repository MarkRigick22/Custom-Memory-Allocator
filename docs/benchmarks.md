# Linux microbenchmark

Run `make clean && make bench CXX=g++ CXXFLAGS='-O2 -g'`. Host: Apple M2 MacBook Pro, 8 GB. Guest: Ubuntu 24.04.5 ARM64, Linux 7.0.12-linuxkit, 4 KiB pages, two emulated CPUs and 1.5 GiB RAM under QEMU 10.1.2 TCG. GCC 13.3, C++17. These are guest monotonic-clock measurements under software emulation.

Each row starts a fresh child process, runs one warm-up and 101 measured rounds. Each round allocates 32 buffers, touches the first byte, grows each by 320 bytes, checks the marker, then frees in allocation order or reverse order. Small sizes follow 33 + (149 × index modulo 1024); the large workload adds 131,072. The libc comparison uses identical inputs and order. Values are medians of batch-average nanoseconds per operation, including loop and marker costs. The benchmark does not measure single-call tail latency or worst-case execution time.

| Allocator | Sizes | Free order | Alloc ns | Realloc ns | Free ns |
| --- | --- | --- | ---: | ---: | ---: |
| Buddy | Small | FIFO | 94 | 219 | 125 |
| Buddy | Small | LIFO | 94 | 219 | 125 |
| Buddy | Large | FIFO | 27,656 | 583,844 | 146,312 |
| Buddy | Large | LIFO | 26,406 | 566,344 | 139,656 |
| libc | Small | FIFO | 62 | 344 | 156 |
| libc | Small | LIFO | 62 | 344 | 156 |
| libc | Large | FIFO | 22,250 | 679,188 | 70,469 |
| libc | Large | LIFO | 22,750 | 672,281 | 63,688 |

At the end of the allocation phase the buddy small workload held 5,424 bytes of payload slack. Mapped large requests had zero slack according to allocator accounting; the kernel still rounds to pages. An integration test separately creates 32 free half-roots with their partners live; a whole-root request fails until the partners are freed. This distinguishes fragmentation from a shortage of total free bytes. Results characterize these synthetic traces only.

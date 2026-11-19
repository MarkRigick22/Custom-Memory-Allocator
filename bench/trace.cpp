#include "os_malloc.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
using Clock = std::chrono::steady_clock;
using Batch = std::array<void *, 32>;
static double ns(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::nano>(end - start).count();
}
// Compare identical requested sizes and touches in separate child processes.
static void run(bool libc, bool large, bool reverse) {
    auto alloc = libc ? std::malloc : smalloc;
    auto resize = libc ? std::realloc : srealloc;
    auto release = libc ? std::free : sfree;
    double samples[3][101]{};
    size_t waste = 0;
    for (int round = 0; round < 102; round++) {
        Batch p{};
        size_t requested = 0;
        auto size = [&](size_t i) { return (large ? 131072u : 0u) + 33 + (i * 149) % 1024; };
        auto start = Clock::now();
        for (size_t i = 0; i < p.size(); i++) {
            size_t n = size(i);
            requested += n;
            p[i] = alloc(n);
            assert(p[i]);
            static_cast<unsigned char *>(p[i])[0] = 42;
        }
        auto allocated = Clock::now();
        if (!libc)
            waste = _num_allocated_bytes() - _num_free_bytes() - requested;
        auto resize_start = Clock::now();
        for (size_t i = 0; i < p.size(); i++) {
            p[i] = resize(p[i], size(i) + 320);
            assert(p[i] && static_cast<unsigned char *>(p[i])[0] == 42);
        }
        auto resized = Clock::now();
        for (size_t i = 0; i < p.size(); i++)
            release(p[reverse ? p.size() - 1 - i : i]);
        auto freed = Clock::now();
        if (round) {
            samples[0][round - 1] = ns(start, allocated) / 32;
            samples[1][round - 1] = ns(resize_start, resized) / 32;
            samples[2][round - 1] = ns(resized, freed) / 32;
        }
    }
    for (auto &row : samples)
        std::sort(row, row + 101);
    std::printf("%s %s %s %.0f %.0f %.0f ", libc ? "libc" : "buddy", large ? "large" : "small",
                reverse ? "LIFO" : "FIFO", samples[0][50], samples[1][50], samples[2][50]);
    if (libc)
        std::puts("-");
    else
        std::printf("%zu\n", waste);
    std::fflush(stdout);
}
int main() {
    for (int mode = 0; mode < 8; mode++) {
        auto child = fork();
        if (child < 0)
            return 1;
        if (!child) {
            run(mode & 4, mode & 2, mode & 1);
            _exit(0);
        }
        int status;
        if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status))
            return 1;
    }
}

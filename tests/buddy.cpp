#include "os_malloc.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <sys/mman.h>
#include <unistd.h>

constexpr size_t root = 128 * 1024, arena = 32 * root;
static bool deny_break, deny_map, deny_unmap;
extern "C" void *__real_sbrk(intptr_t);
extern "C" void *__wrap_sbrk(intptr_t n) {
    return deny_break && n ? reinterpret_cast<void *>(-1) : __real_sbrk(n);
}
extern "C" void *__real_mmap(void *, size_t, int, int, int, off_t);
extern "C" void *__wrap_mmap(void *p, size_t n, int prot, int flags, int fd, off_t off) {
    return deny_map ? MAP_FAILED : __real_mmap(p, n, prot, flags, fd, off);
}
extern "C" int __real_munmap(void *, size_t);
extern "C" int __wrap_munmap(void *p, size_t n) { return deny_unmap ? -1 : __real_munmap(p, n); }
void check_structure();

static void recovered() {
    check_structure();
    assert(_num_free_blocks() == 32 && _num_allocated_blocks() == 32);
    assert(_num_free_bytes() == arena - 32 * _size_meta_data());
    assert(_num_allocated_bytes() == _num_free_bytes());
}
static void cases() {
    assert(!smalloc(0) && !smalloc(100000001) && !scalloc(SIZE_MAX, 2));
    assert(!_num_allocated_blocks());
    deny_break = true;
    assert(!smalloc(1));
    deny_break = false;
    assert(!_num_allocated_blocks());
    size_t h = _size_meta_data();
    for (size_t b = 128; b <= root; b *= 2)
        for (size_t n : {b - h - 1, b - h, b - h + 1}) {
            auto *p = static_cast<unsigned char *>(smalloc(n));
            assert(p && uintptr_t(p) % alignof(std::max_align_t) == 0);
            memset(p, 91, n);
            assert(_num_meta_data_bytes() == _num_allocated_blocks() * h);
            sfree(p);
            recovered();
        }
    auto *left = static_cast<unsigned char *>(smalloc(8));
    auto *right = static_cast<unsigned char *>(smalloc(8));
    assert(right == left + 128);
    memset(right, 73, 8);
    sfree(left);
    auto *merged = static_cast<unsigned char *>(srealloc(right, 250));
    assert(merged == left && merged[7] == 73 && merged[0] == 73);
    assert(srealloc(merged, 1) == merged && srealloc(merged, 800) == merged);
    sfree(merged);
    sfree(right);
    sfree(merged);
    recovered();
    auto *cleared = static_cast<unsigned char *>(scalloc(8, 4));
    for (size_t i = 0; i < 32; i++)
        assert(!cleared[i]);
    sfree(cleared);
    void *roots[32];
    for (auto &p : roots) {
        p = smalloc(root - h);
        assert(p);
    }
    assert(!_num_free_blocks() && !smalloc(1));
    sfree(roots[15]);
    assert(smalloc(root - h) == roots[15]);
    for (auto p : roots)
        sfree(p);
    recovered();
    auto *map = static_cast<unsigned char *>(srealloc(nullptr, root));
    assert(map && _num_allocated_blocks() == 33);
    map[0] = 99;
    deny_map = true;
    assert(!srealloc(map, 2 * root));
    deny_map = false;
    assert(map[0] == 99 && !srealloc(map, 0));
    auto *other = static_cast<unsigned char *>(srealloc(map, 2 * root));
    assert(other && other != map && other[0] == 99);
    deny_unmap = true;
    sfree(other);
    deny_unmap = false;
    assert(_num_allocated_blocks() == 33 && other[0] == 99);
    sfree(other);
    sfree(other);
    sfree(nullptr);
    recovered();
    // Free alternating leaves to show external fragmentation at root order.
    void *halves[64];
    for (auto &p : halves) {
        p = smalloc(root / 2 - h);
        assert(p);
    }
    for (int i = 0; i < 64; i += 2)
        sfree(halves[i]);
    assert(_num_free_blocks() == 32 && !smalloc(root - h));
    for (int i = 1; i < 64; i += 2)
        sfree(halves[i]);
    recovered();
}
struct Slot {
    unsigned char *p;
    size_t n;
    unsigned char tag;
};
static void check(const Slot &s) {
    for (size_t i = 0; i < s.n; i++)
        assert(s.p[i] == s.tag);
}
static void stress(uint32_t seed) {
    Slot slots[64]{};
    auto next = [&]() {
        seed = 1664525u * seed + 1013904223u;
        return seed;
    };
    for (int step = 0; step < 5000; step++) {
        if (step % 50 == 0)
            check_structure();
        Slot &s = slots[(next() >> 8) % 64];
        size_t n = 1 + ((next() >> 8) % 4096);
        if (step % 31 == 0)
            n += root;
        check(s);
        if (s.p && (next() >> 16) % 3 == 0) {
            sfree(s.p);
            s = {};
            continue;
        }
        auto *p = static_cast<unsigned char *>(srealloc(s.p, n));
        if (!p) {
            check(s);
            continue;
        }
        if (s.p)
            check({p, s.n < n ? s.n : n, s.tag});
        for (auto &t : slots)
            if (&t != &s && t.p)
                assert(uintptr_t(p) + n <= uintptr_t(t.p) || uintptr_t(t.p) + t.n <= uintptr_t(p));
        s = {p, n, static_cast<unsigned char>(next() >> 24)};
        memset(p, s.tag, n);
        assert(_num_free_bytes() <= _num_allocated_bytes());
        assert(_num_meta_data_bytes() == _num_allocated_blocks() * _size_meta_data());
    }
    for (auto &s : slots) {
        check(s);
        sfree(s.p);
    }
    recovered();
}
int main() {
    cases();
    for (uint32_t seed : {1u, 7u, 1337u})
        stress(seed);
    puts("buddy cases and 15000 trace steps passed");
}

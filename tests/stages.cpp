#include "os_malloc.h"
#include <cassert>
#include <cstdint>
#include <cstring>
int main() {
    assert(!smalloc(0) && !smalloc(100000001));
    auto *first = static_cast<unsigned char *>(smalloc(7));
    auto *next = static_cast<unsigned char *>(smalloc(13));
    assert(first && next && uintptr_t(next) >= uintptr_t(first) + 7);
    memset(first, 42, 7);
    memset(next, 11, 13);
#ifdef FIRST_FIT
    assert(_num_allocated_blocks() == 2 && _num_allocated_bytes() == 20);
    assert(!scalloc(SIZE_MAX, 2));
    sfree(first);
    sfree(first);
    assert(_num_free_blocks() == 1 && _num_free_bytes() == 7);
    auto *zero = static_cast<unsigned char *>(scalloc(1, 5));
    assert(zero == first);
    for (int i = 0; i < 5; i++)
        assert(zero[i] == 0);
    auto *grown = static_cast<unsigned char *>(srealloc(next, 39));
    assert(grown && grown != next);
    for (int i = 0; i < 13; i++)
        assert(grown[i] == 11);
    assert(srealloc(grown, 2) == grown && !srealloc(grown, 0));
    sfree(nullptr);
    sfree(zero);
    sfree(grown);
    assert(_num_free_bytes() == _num_allocated_bytes());
    assert(_num_meta_data_bytes() == 3 * _size_meta_data());
#else
    for (int i = 0; i < 7; i++)
        assert(first[i] == 42);
#endif
}

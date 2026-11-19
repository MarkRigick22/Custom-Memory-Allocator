// White-box checker built only into the buddy test executable.
#include "../src/malloc_3.cpp"
#include <bitset>
#include <cassert>

void check_structure() {
    std::bitset<REGION_SIZE / BASE_BLOCK_SIZE> listed;
    size_t free_count = 0, free_bytes = 0, managed = 0, blocks = 0;
    for (int order = 0; order <= MAX_ORDER; ++order) {
        MallocMetadata *prev = nullptr;
        for (auto *p = g_free_lists[order]; p; p = p->next) {
            assert(ptr_in_buddy_region(p) && p->prev == prev);
            assert(!prev || uintptr_t(prev) < uintptr_t(p));
            assert(p->is_free && p->size == (BASE_BLOCK_SIZE << order));
            const auto slot = (uintptr_t(p) - uintptr_t(g_region_base)) / BASE_BLOCK_SIZE;
            assert(!listed.test(slot));
            listed.set(slot);
            ++free_count;
            free_bytes += usable_bytes(p);
            prev = p;
        }
    }
    std::bitset<REGION_SIZE / BASE_BLOCK_SIZE> active;
    while (managed < REGION_SIZE) {
        auto *p = reinterpret_cast<MallocMetadata *>(g_region_base + managed);
        assert(p->size >= BASE_BLOCK_SIZE && p->size <= MAX_BLOCK_SIZE);
        assert(!(p->size & (p->size - 1)) && managed % p->size == 0);
        const size_t slot = managed / BASE_BLOCK_SIZE;
        assert(p->is_free == listed.test(slot));
        if (p->is_free)
            listed.reset(slot);
        else
            active.set(slot);
        ++blocks;
        managed += p->size;
    }
    assert(managed == REGION_SIZE && listed.none());
    for (size_t i = 0; i < active.size(); ++i)
        assert(active.test(i) == is_live(i));
    MallocMetadata *prev = nullptr;
    for (auto *p = g_mmap_head; p; p = p->next) {
        assert(p->prev == prev && !ptr_in_buddy_region(p) && !p->is_free);
        ++blocks;
        managed += p->size;
        prev = p;
    }
    assert(free_count == _num_free_blocks() && free_bytes == _num_free_bytes());
    assert(blocks == _num_allocated_blocks() &&
           managed - blocks * meta_size() == _num_allocated_bytes());
}

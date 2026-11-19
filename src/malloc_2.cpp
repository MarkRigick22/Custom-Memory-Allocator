#include "os_malloc.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <unistd.h>

// Stage two requires byte-exact heap growth even after odd-size requests.
struct __attribute__((packed)) MallocMetadata {
    size_t size;
    bool is_free;
    unsigned char reserved[sizeof(void *) - 1]; // Preserve the original Linux header layout.
    MallocMetadata *next;
    MallocMetadata *prev;
};
// the headstart of the blocks
static MallocMetadata *g_head = nullptr;
static MallocMetadata *g_tail = nullptr;
static size_t g_allocated_blocks = 0;
static size_t g_allocated_bytes = 0;
static size_t g_free_blocks = 0;
static size_t g_free_bytes = 0;

size_t _size_meta_data() { return sizeof(MallocMetadata); }
size_t _num_allocated_blocks() { return g_allocated_blocks; }
size_t _num_allocated_bytes() { return g_allocated_bytes; }
size_t _num_free_blocks() { return g_free_blocks; }
size_t _num_free_bytes() { return g_free_bytes; }
size_t _num_meta_data_bytes() { return g_allocated_blocks * sizeof(MallocMetadata); }

static MallocMetadata *find_free_block_first_fit(size_t req_size) {
    for (MallocMetadata *cur = g_head; cur != nullptr; cur = cur->next) {
        if (cur->is_free && cur->size >= req_size) {
            return cur;
        }
    }
    return nullptr;
}

void sfree(void *p) {
    if (p == NULL) {
        return;
    }
    MallocMetadata *meta = ((MallocMetadata *)p) - 1;
    if (meta->is_free) {
        return;
    }
    meta->is_free = true;
    g_free_blocks++;
    g_free_bytes += meta->size;
}
void *srealloc(void *oldp, size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }
    if (oldp == NULL) {
        return smalloc(size);
    }
    MallocMetadata *old_meta = ((MallocMetadata *)oldp) - 1;
    if (size <= old_meta->size) {
        return oldp;
    }
    void *newp = smalloc(size);
    if (!newp) {
        return NULL;
    }
    std::memmove(newp, oldp, old_meta->size);
    sfree(oldp);
    return newp;
}
void *smalloc(size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }

    MallocMetadata *free_block = find_free_block_first_fit(size);
    if (free_block) {
        free_block->is_free = false;

        g_free_blocks--;
        g_free_bytes -= free_block->size;

        return (void *)(free_block + 1);
    }

    intptr_t increment = (intptr_t)(sizeof(MallocMetadata) + size);
    void *base = sbrk(increment);
    if (base == (void *)(-1)) {
        return NULL;
    }

    MallocMetadata *meta = ::new (base) MallocMetadata;
    meta->size = size;
    meta->is_free = false;
    meta->next = nullptr;
    meta->prev = g_tail;

    if (!g_head) {
        g_head = g_tail = meta;
    } else {
        g_tail->next = meta;
        g_tail = meta;
    }
    g_allocated_blocks++;
    g_allocated_bytes += size;
    return (void *)(meta + 1);
}

void *scalloc(size_t num, size_t size) {
    if (num == 0 || size == 0) {
        return NULL;
    }

    if (size > 100000000 / num) {
        return NULL;
    }

    size_t total = num * size;
    void *p = smalloc(total);
    if (!p) {
        return NULL;
    }

    std::memset(p, 0, total);
    return p;
}

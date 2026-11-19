//
// Created by mohamed hijazi on 29/01/2026.
//
//
// Created by mohamed hijazi on 14/01/2026.
//
// malloc_3.cpp

#include "os_malloc.h"
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdint>

struct MallocMetadata{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};
//we use the same as before part 2 - taking it from whatsapp
static const size_t BASE_BLOCK_SIZE   = 128;
static const int    MAX_ORDER         = 10;
static const size_t MAX_BLOCK_SIZE    = BASE_BLOCK_SIZE << MAX_ORDER;
static const int    INITIAL_BLOCKS    = 32;
static const size_t REGION_SIZE       = (size_t)INITIAL_BLOCKS * MAX_BLOCK_SIZE;
static const size_t MMAP_THRESHOLD    = 128 * 1024;
static const size_t MAX_REQUEST       = 100000000;

static bool g_inited = false;
static char* g_region_base = nullptr;

static MallocMetadata* g_free_lists[MAX_ORDER + 1] = {nullptr};

static size_t g_total_os_bytes = 0;
static size_t g_allocated_blocks = 0;
static size_t g_free_blocks = 0;
static size_t g_free_bytes = 0;

static MallocMetadata* g_mmap_head = nullptr;

static inline size_t meta_size() { return sizeof(MallocMetadata); }
static inline size_t usable_bytes(const MallocMetadata* m) { return (m->size >= meta_size()) ? (m->size - meta_size()) : 0; }
//same methods as before
size_t _num_free_blocks()
{return g_free_blocks;}

size_t _num_free_bytes(){
    return g_free_bytes;}

size_t _num_allocated_blocks(){return g_allocated_blocks;}

size_t _num_meta_data_bytes(){return g_allocated_blocks * meta_size();}

size_t _num_allocated_bytes(){
    size_t meta_bytes = _num_meta_data_bytes();
    if (g_total_os_bytes < meta_bytes) return 0;
    return g_total_os_bytes - meta_bytes;}

size_t _size_meta_data(){return meta_size();}

static void insert(MallocMetadata* m, int order);
static void remove(MallocMetadata* m, int order);


static size_t __attribute__((unused)) block_size_from_order(int order){return BASE_BLOCK_SIZE << order;}

static int order_from_block_size(size_t block_size){
    int order = 0;
    size_t s = BASE_BLOCK_SIZE;
    while (s < block_size && order < MAX_ORDER) {
        s <<= 1;
        order++;
    }
    return order;
}

static int required_order_for_request(size_t user_size){
    size_t need = user_size + meta_size();
    int order = 0;
    size_t bs = BASE_BLOCK_SIZE;
    while (bs < need && order < MAX_ORDER) {
        bs <<= 1;
        order++;
    }
    return order;}

static bool init(){
    if (g_inited) return true;

    void* cur_brk = sbrk(0);
    if (cur_brk == (void*)(-1)) return false;

    uintptr_t cur = (uintptr_t)cur_brk;
    uintptr_t aligned = (cur + (REGION_SIZE - 1)) & ~(uintptr_t)(REGION_SIZE - 1);
    intptr_t padding = (intptr_t)(aligned - cur);

    if (padding > 0) {
        void* pad_res = sbrk(padding);
        if (pad_res == (void*)(-1)) return false;
    }

    void* region = sbrk((intptr_t)REGION_SIZE);
    if (region == (void*)(-1)) return false;

    g_region_base = (char*)region;

    for (int i = 0; i <= MAX_ORDER; ++i) g_free_lists[i] = nullptr;

    for (int i = 0; i < INITIAL_BLOCKS; ++i) {
        char* block_start = g_region_base + (size_t)i * MAX_BLOCK_SIZE;
        MallocMetadata* m = (MallocMetadata*)block_start;
        m->size = MAX_BLOCK_SIZE;
        m->is_free = true;
        m->next = nullptr;
        m->prev = nullptr;
        insert(m, MAX_ORDER);
    }

    g_total_os_bytes = REGION_SIZE;
    g_allocated_blocks = (size_t)INITIAL_BLOCKS;
    g_free_blocks = (size_t)INITIAL_BLOCKS;
    g_free_bytes  = (size_t)INITIAL_BLOCKS * (MAX_BLOCK_SIZE - meta_size());

    g_inited = true;
    return true;
}

static bool ptr_in_buddy_region(const void* p)
{
    if (!g_inited || !g_region_base) return false;
    uintptr_t addr = (uintptr_t)p;
    uintptr_t base = (uintptr_t)g_region_base;
    return (addr >= base) && (addr < base + REGION_SIZE);
}

static void insert(MallocMetadata* m, int order){
    m->prev = nullptr;
    m->next = nullptr;

    MallocMetadata* head = g_free_lists[order];
    if (!head) {
        g_free_lists[order] = m;
        return;
    }

    uintptr_t addr = (uintptr_t)m;
    MallocMetadata* cur = head;
    MallocMetadata* prev = nullptr;

    while (cur && (uintptr_t)cur < addr) {
        prev = cur;
        cur = cur->next;
    }

    m->prev = prev;
    m->next = cur;

    if (prev) prev->next = m;
    else      g_free_lists[order] = m;

    if (cur) cur->prev = m;
}

static void remove(MallocMetadata* m, int order){
    if (!m) return;

    if (m->prev) m->prev->next = m->next;
    else         g_free_lists[order] = m->next;

    if (m->next) m->next->prev = m->prev;

    m->prev = nullptr;
    m->next = nullptr;
}

static void add_mmap_block(MallocMetadata* m){
    m->prev = nullptr;
    m->next = g_mmap_head;
    if (g_mmap_head) g_mmap_head->prev = m;
    g_mmap_head = m;
}

static void remove_mmap_block(MallocMetadata* m){
    if (m->prev) m->prev->next = m->next;
    else         g_mmap_head = m->next;

    if (m->next) m->next->prev = m->prev;

    m->prev = nullptr;
    m->next = nullptr;
}

static MallocMetadata* buddy_of(MallocMetadata* m){
    uintptr_t base = (uintptr_t)g_region_base;
    uintptr_t addr = (uintptr_t)m;
    uintptr_t buddy_addr = base + ((addr - base) ^ (uintptr_t)m->size);
    return (MallocMetadata*)buddy_addr;
}
static void* alloc_mmap(size_t user_size){
    size_t total = user_size + meta_size();
    void* p = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return NULL;

    MallocMetadata* m = (MallocMetadata*)p;
    m->size = total;
    m->is_free = false;
    m->next = nullptr;
    m->prev = nullptr;

    add_mmap_block(m);

    g_total_os_bytes += total;
    g_allocated_blocks += 1;

    return (void*)(m + 1);
}

void sfree(void* p){
    if (p == NULL) return;

    MallocMetadata* m = ((MallocMetadata*)p) - 1;
    if (m->is_free) return;

    if (!ptr_in_buddy_region(m)) {
        remove_mmap_block(m);
        size_t total = m->size;
        g_total_os_bytes -= total;
        g_allocated_blocks -= 1;
        munmap((void*)m, total);
        return;
    }

    m->is_free = true;
    m->next = nullptr;
    m->prev = nullptr;

    g_free_blocks += 1;
    g_free_bytes  += usable_bytes(m);

    while (m->size < MAX_BLOCK_SIZE) {
        MallocMetadata* b = buddy_of(m);

        if (!ptr_in_buddy_region(b)) break;
        if (!b->is_free) break;
        if (b->size != m->size) break;

        int ord = order_from_block_size(m->size);

        remove(b, ord);

        g_free_blocks -= 1;
        g_free_bytes  += meta_size();
        g_allocated_blocks -= 1;

        MallocMetadata* merged = ((uintptr_t)b < (uintptr_t)m) ? b : m;
        MallocMetadata* dead   = (merged == m) ? b : m;

        merged->size = merged->size * 2;
        merged->is_free = true;
        merged->next = nullptr;
        merged->prev = nullptr;
        dead->size = 0;
        dead->is_free = false;
        dead->next = nullptr;
        dead->prev = nullptr;

        m = merged;
    }

    int final_ord = order_from_block_size(m->size);
    insert(m, final_ord);
}

void* smalloc(size_t size){
    if (!init()) return NULL;

    if (size == 0 || size > MAX_REQUEST) return NULL;

    if (size >= MMAP_THRESHOLD) {
        return alloc_mmap(size);
    }

    if (size + meta_size() > MAX_BLOCK_SIZE) {
        return alloc_mmap(size);
    }

    int need_order = required_order_for_request(size);

    int found_order = -1;
    for (int o = need_order; o <= MAX_ORDER; ++o) {
        if (g_free_lists[o]) { found_order = o; break; }
    }
    if (found_order == -1) {
        return NULL;
    }

    MallocMetadata* m = g_free_lists[found_order];
    remove(m, found_order);

    g_free_blocks -= 1;
    g_free_bytes  -= usable_bytes(m);
    m->is_free = false;

    while (found_order > need_order) {
        size_t cur_size = m->size;
        size_t half = cur_size / 2;
        int new_order = found_order - 1;

        MallocMetadata* right = (MallocMetadata*)((char*)m + half);
        right->size = half;
        right->is_free = true;
        right->next = nullptr;
        right->prev = nullptr;

        m->size = half;
        m->is_free = false;
        m->next = nullptr;
        m->prev = nullptr;

        insert(right, new_order);

        g_allocated_blocks += 1;

        g_free_blocks += 1;
        g_free_bytes  += usable_bytes(right);

        found_order = new_order;
    }

    return (void*)(m + 1);
}

void* scalloc(size_t num, size_t size){
    if (!init()) return NULL;
    if (num == 0 || size == 0) return NULL;
    if (size > MAX_REQUEST / num) return NULL;
    size_t total = num * size;
    void* p = smalloc(total);
    if (!p) return NULL;

    std::memset(p, 0, total);
    return p;
}

static void* try_grow_in_place(void* oldp, size_t new_size)
{
    MallocMetadata* m = ((MallocMetadata*)oldp) - 1;

    size_t old_cap = usable_bytes(m);
    if (new_size <= old_cap) return oldp;

    if (!ptr_in_buddy_region(m)) return NULL;

    if (new_size + meta_size() > MAX_BLOCK_SIZE) return NULL;
    int need_order = required_order_for_request(new_size);
    int cur_order  = order_from_block_size(m->size);
    if (cur_order >= need_order) return oldp;

    uintptr_t base = (uintptr_t)g_region_base;
    uintptr_t sim_addr = (uintptr_t)m;
    size_t    sim_size = m->size;
    int       sim_order = cur_order;

    while (sim_order < need_order) {
        uintptr_t buddy_addr = base + ((sim_addr - base) ^ (uintptr_t)sim_size);
        MallocMetadata* b = (MallocMetadata*)buddy_addr;
        if (!ptr_in_buddy_region(b) || !b->is_free || b->size != sim_size) break;

        sim_addr = (buddy_addr < sim_addr) ? buddy_addr : sim_addr;
        sim_size *= 2;
        sim_order += 1;
    }

    if (sim_order < need_order) return NULL;

    void* old_user_ptr = oldp;
    while (order_from_block_size(m->size) < need_order) {
        MallocMetadata* b = buddy_of(m);
        if (!b->is_free || b->size != m->size) return NULL;

        int ord = order_from_block_size(m->size);

        remove(b, ord);
        g_free_blocks -= 1;
        g_free_bytes  -= usable_bytes(b);

        g_allocated_blocks -= 1;

        MallocMetadata* merged = ((uintptr_t)b < (uintptr_t)m) ? b : m;
        MallocMetadata* dead   = (merged == m) ? b : m;

        merged->size = merged->size * 2;
        merged->is_free = false;
        merged->next = nullptr;
        merged->prev = nullptr;

        dead->size = 0;
        dead->is_free = false;
        dead->next = nullptr;
        dead->prev = nullptr;

        m = merged;
    }

    void* new_user_ptr = (void*)(m + 1);
    if (new_user_ptr != old_user_ptr) {
        size_t to_copy = old_cap;
        if (to_copy > new_size) to_copy = new_size;
        std::memmove(new_user_ptr, old_user_ptr, to_copy);
    }

    return new_user_ptr;
}

void* srealloc(void* oldp, size_t size){
    if (size == 0 || size > MAX_REQUEST) return NULL;

    if (oldp == NULL) {
        return smalloc(size);
    }

    MallocMetadata* m = ((MallocMetadata*)oldp) - 1;

    if (!ptr_in_buddy_region(m)) {
        size_t old_size = usable_bytes(m);
        if (size == old_size) return oldp;

        void* newp = smalloc(size);
        if (!newp) return NULL;

        size_t to_copy = old_size < size ? old_size : size;
        std::memmove(newp, oldp, to_copy);
        sfree(oldp);
        return newp;
    }

    size_t old_cap = usable_bytes(m);
    if (size <= old_cap) {
        return oldp;
    }

    void* grown = try_grow_in_place(oldp, size);
    if (grown) return grown;

    void* newp = smalloc(size);
    if (!newp) return NULL;

    size_t to_copy = old_cap < size ? old_cap : size;
    std::memmove(newp, oldp, to_copy);
    sfree(oldp);
    return newp;
}

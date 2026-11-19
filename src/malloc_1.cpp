//
// Created by mohamed hijazi on 14/01/2026.
//

#include "os_malloc.h"
#include <unistd.h>

void *smalloc(size_t size) {
    if (size == 0 || size > 100000000UL) {
        return (void *)0;
    }
    void *prev = sbrk((long)size);
    if (prev == (void *)-1) {
        return (void *)0;
    }
    return prev;
}

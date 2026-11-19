#pragma once
#include <cstddef>

// The three stages are separate link targets with the same course API.
void *smalloc(std::size_t size);
void *scalloc(std::size_t count, std::size_t size);
void sfree(void *pointer);
void *srealloc(void *pointer, std::size_t size);

std::size_t _num_free_blocks();
std::size_t _num_free_bytes();
std::size_t _num_allocated_blocks();
std::size_t _num_allocated_bytes();
std::size_t _num_meta_data_bytes();
std::size_t _size_meta_data();

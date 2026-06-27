#pragma once

#include "mem.hpp"

namespace mem {

struct Arena {
    unsigned char *buffer;
    // Total number of bytes in `buffer` we can use.
    std::size_t cap;

    // Track the usage of our arena.
    std::size_t prev_index, curr_index;
};

static inline std::size_t
arena_align_forward(Arena *a, std::size_t index, std::size_t align)
{
    std::uintptr_t base_addr, curr_addr, modulo;

    base_addr = cast(std::uintptr_t)a->buffer;
    curr_addr = base_addr + index;

    // Modulo by power of 2.
    modulo = curr_addr & (align - 1);

    // Address isn't yet aligned to the given boundary?
    if (modulo != 0) {
        // Push the address to the next aligned one.
        curr_addr += align - modulo;
    }

    // Get the byte index.
    return cast(size_t)(curr_addr - base_addr);
}

extern inline void *
arena_alloc_bytes(Arena *a,
    std::size_t size,
    std::size_t align,
    Allocator::Error *err = NULL)
{
    std::size_t next_index;

    next_index = arena_align_forward(a, a->curr_index, align);
    if (next_index <= a->cap) {
        a->prev_index = a->curr_index;
        a->curr_index = next_index;
        return zero(&a->buffer[next_index], size);
    }
    return nullptr;
}

};

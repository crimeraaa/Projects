#ifndef MEM_ARENA_H
#define MEM_ARENA_H

#include "mem.h"

typedef struct mem_Arena mem_Arena;
struct mem_Arena {
    unsigned char *buffer;
    size_t curr_offset;
    size_t prev_offset;
    size_t cap;
};

mem_Allocator
mem_arena_allocator(mem_Arena *a);

void
mem_arena_init(mem_Arena *a, void *buffer, size_t cap);

void *
mem_arena_alloc_bytes_align(mem_Arena *a, size_t size, size_t align);

#define arena_alloc_bytes(a, size) \
    arena_alloc_bytes_align(a, size, MEM_DEFAULT_ALIGN)

void *
mem_arena_resize_bytes_align(mem_Arena *a, void *memory, size_t old_size, size_t new_size, size_t align);

#define arena_resize_bytes(a, memory, old_size, new_size) \
    arena_resize_bytes_align(a, memory, old_size, new_size, MEM_DEFAULT_ALIGN)

void
mem_arena_free_all(mem_Arena *a);

#ifdef MEM_ARENA_IMPLEMENTATION

#include <stdint.h> // uintptr_t
#include <string.h> // memset

static void *
mem_arena_allocator_fn(
    mem_Allocator_Error *err,
    mem_Allocator_Mode mode,
    void *memory,
    size_t old_size,
    size_t new_size,
    size_t align,
    void *user_data)
{
    mem_Arena *a = cast(mem_Arena *)user_data;
    void *new_memory = NULL;
    switch (mode) {
    case MEM_ALLOC:
    case MEM_RESIZE:
        new_memory = mem_arena_resize_bytes_align(a, memory, old_size, new_size, align);
        if (new_memory == NULL) {
            if (err != NULL) {
                *err = MEM_OUT_OF_MEMORY;
            }
        } else if (new_size > old_size) {
            unsigned char *growth_begin;
            size_t growth_size;

            growth_begin = cast(unsigned char *)new_memory + old_size;
            growth_size  = new_size - old_size;
            memset(growth_begin, 0, growth_size);
        }
        break;
    case MEM_ALLOC_NON_ZEROED:
    case MEM_RESIZE_NON_ZEROED:
        new_memory = mem_arena_resize_bytes_align(a, memory, old_size, new_size, align);
        if (new_memory == NULL && err != NULL) {
            *err = MEM_OUT_OF_MEMORY;
        }
        break;
    case MEM_FREE:
        if (err != NULL) {
            *err = MEM_NOT_IMPLEMENTED;
        }
        break;
    case MEM_FREE_ALL:
        mem_arena_free_all(a);
        break;
    }
    return new_memory;
}

mem_Allocator
mem_arena_allocator(mem_Arena *a)
{
    mem_Allocator allocator = {mem_arena_allocator_fn, a};
    return allocator;
}


void
mem_arena_init(mem_Arena *a, void *buffer, size_t cap)
{
    a->buffer      = cast(unsigned char *)buffer;
    a->curr_offset = 0;
    a->prev_offset = 0;
    a->cap         = cap;
}

void
mem_arena_free_all(mem_Arena *a)
{
    a->curr_offset = 0;
    a->prev_offset = 0;
}

static uintptr_t
mem_arena_align_forward(uintptr_t address, size_t align)
{
    uintptr_t modulo = address & cast(uintptr_t)align;
    // Address is not aligned to an `align`-byte boundary?
    // If so, bump up the address to the next aligned one.
    if (modulo != 0) {
        address += align - modulo;
    }
    return address;
}

void *
mem_arena_alloc_bytes_align(mem_Arena *a, size_t size, size_t align)
{
    uintptr_t address;
    size_t offset, next_offset;

    // Ensure the would-be allocation is aligned properly.
    address = cast(uintptr_t)a->buffer + cast(uintptr_t)a->curr_offset;
    address = mem_arena_align_forward(address, align);

    // Get the would-be relative index of the allocation in the buffer.
    offset = cast(size_t)(address - cast(uintptr_t)a->buffer);

    // We can fit the (now-aligned) allocation?
    next_offset = offset + size;
    if (next_offset > a->cap) {
        void *pointer = &a->buffer[offset];
        return pointer;
    }

    // Otherwise, no more backing memory remaining.
    return NULL;
}

void *
mem_arena_resize_bytes_align(mem_Arena *a, void *memory, size_t old_size, size_t new_size, size_t align)
{
    unsigned char *old_memory;
    old_memory = cast(unsigned char *)memory;

    // Requesting for a new block of memory?
    if (old_memory == NULL && old_size == 0) {
        return mem_arena_alloc_bytes_align(a, new_size, align);
    }
    // Otherwise, trying to resize an existing block of memory?
    else if (a->buffer <= old_memory && old_memory < a->buffer + a->cap) {
        // Old memory block can be resized in-place?
        if (a->buffer + a->prev_offset == old_memory) {
            a->curr_offset = a->prev_offset + new_size;
            return old_memory;
        }
        // Otherwise, can't resize the block in-place so allocate a new one.
        else {
            void *new_memory = mem_arena_alloc_bytes_align(a, new_size, align);
            size_t copy_size = (old_size < new_size) ? old_size : new_size;
            memmove(new_memory, old_memory, copy_size);
            return new_memory;
        }
    }
    // Given pointer is out of bounds!
    return NULL;
}


#endif // MEM_ARENA_IMPLEMENTATION


#endif // MEM_ARENA_H

#ifndef LULU_ARENA_H
#define LULU_ARENA_H

// standard
#include <stdint.h>

// local
#include "internal.h"

typedef uintptr_t uintptr;

typedef struct Arena Arena;
struct Arena {
    // Backing buffer information.
    unsigned char *buf;
    size_t buf_size;

    // Current usage.
    size_t prev_index, curr_index;
};

LULU_INTERNAL_FUNC Arena
arena_make(void *buf, size_t buf_size);

LULU_INTERNAL_FUNC void *
arena_alloc(Arena *a, size_t size);

LULU_INTERNAL_FUNC void *
arena_resize(Arena *a, void *old_mem, size_t old_size, size_t new_size);

LULU_INTERNAL_FUNC void
arena_free_all(Arena *a);

#endif // !LULU_ARENA_H

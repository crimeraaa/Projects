#ifndef LULU_ARENA_H
#define LULU_ARENA_H

// standard
#include <stdint.h>

// local
#include "internal.h"

typedef struct Arena Arena;
struct Arena {
    // Backing buffer information.
    u8   *buf;
    usize buf_size;

    // Current usage.
    usize prev_offset;
    usize curr_offset;
};

LULU_INTERNAL_FUNC Arena
arena_make(void *buf, usize buf_size);

LULU_INTERNAL_FUNC void *
arena_alloc(Arena *a, usize size);

LULU_INTERNAL_FUNC void *
arena_resize(Arena *a, void *p, usize old_size, usize new_size);

LULU_INTERNAL_FUNC void
arena_free_all(Arena *a);

#endif // !LULU_ARENA_H

#ifndef LULU_MEMORY_H
#define LULU_MEMORY_H

#include "internal.h"

typedef struct Arena Arena;
struct Arena {
    // Backing buffer information.
    u8 *  buf;
    usize buf_size;

    // Current usage.
    usize prev_offset;
    usize curr_offset;
};

static inline Arena
arena_make(void *buf, usize buf_size)
{
    Arena a = {cast(u8 *)buf, buf_size, /*prev_offset=*/0, /*curr_offset=*/0};
    return a;
}

static inline void
arena_free_all(Arena *a)
{
    a->prev_offset = 0;
    a->curr_offset = 0;
}

LULU_INTERNAL_FUNC void *
mem_arena_alloc(lulu_State *L, usize size);

LULU_INTERNAL_FUNC void *
mem_arena_resize(lulu_State *L, void *pointer, usize old_size, usize new_size);

#define mem_arena_alloc_item(T, L)                                             \
    cast(T *)mem_arena_alloc(L, sizeof(T))

#define mem_arena_alloc_array(T, L, count)                                     \
    cast(T *)mem_arena_alloc(L, sizeof(T) * (count))

#define mem_arena_resize_array(T, L, old_mem, old_count, new_count)            \
    cast(T *)mem_arena_resize(L, old_mem,                                      \
        /*old_size=*/sizeof(T) * (old_count),                                  \
        /*new_size=*/sizeof(T) * (new_count))

#endif  // LULU_MEMORY_H

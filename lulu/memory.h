#ifndef LULU_MEMORY_H
#define LULU_MEMORY_H

#include "internal.h"

LULU_INTERNAL_FUNC void *
mem_arena_alloc(lulu_State *L, usize size);

LULU_INTERNAL_FUNC void *
mem_arena_resize(lulu_State *L, void *old_mem, usize old_size, usize new_size);

#define mem_arena_alloc_item(T, L)                                             \
    cast(T *)mem_arena_alloc(L, sizeof(T))

#define mem_arena_alloc_array(T, L, count)                                     \
    cast(T *)mem_arena_alloc(L, sizeof(T) * (count))

#define mem_arena_resize_array(T, L, old_mem, old_count, new_count)            \
    cast(T *)mem_arena_resize(L, old_mem,                                      \
        /*old_size=*/sizeof(T) * (old_count),                                  \
        /*new_size=*/sizeof(T) * (new_count))


#endif  // LULU_MEMORY_H

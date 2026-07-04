#ifndef LULU_MEMORY_H
#define LULU_MEMORY_H

#include "internal.h"
#include "lulu.h"

/// Defined in `memory.c`.
typedef struct Page  Page;
typedef struct Arena Arena;
struct Arena {
    /*
        The first page is only freed when the entire arena is explicitly
        destroyed via `arena_destroy()`. It should not be freed when
        `arena_free_all()` is called.
     */
    Page *head;

    /*
        Beyond the first page, all succeeding pages we allocate can be
        freed whenever `arena_free_all()` is called.
     */
    Page *tail;
};

LULU_INTERNAL_FUNC bool
arena_init(Arena *a);

LULU_INTERNAL_FUNC void
arena_free_all(Arena *a);

LULU_INTERNAL_FUNC void
arena_destroy(Arena *a);

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

#ifndef LULU_MEM_H
#define LULU_MEM_H

#include "internal.h"
#include "lulu.h"

/// Defined in `mem.c`.
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

typedef struct Scratch Scratch;
struct Scratch {
    Arena *backing;
    Page * saved_page;

    // Track these manually as the underlying page itself may update.
    usize prev_offset, curr_offset;
};

LULU_INTERNAL_FUNC void
mem_arena_init(lulu_State *L, Arena *a);

LULU_INTERNAL_FUNC void
mem_arena_free_all(Arena *a);

LULU_INTERNAL_FUNC void
mem_arena_destroy(Arena *a);

LULU_INTERNAL_FUNC Scratch
mem_scratch_begin(Arena *a);

LULU_INTERNAL_FUNC void
mem_scratch_end(Scratch *x);

LULU_INTERNAL_FUNC void *
mem_arena_alloc(lulu_State *L, usize size);

LULU_INTERNAL_FUNC void *
mem_arena_resize(lulu_State *L, void *old_ptr, usize old_size, usize new_size);

#define mem_arena_alloc_item(L, T)                                             \
    cast(T *)mem_arena_alloc(L, sizeof(T))

#define mem_arena_alloc_array(L, T, count)                                     \
    cast(T *)mem_arena_alloc(L, sizeof(T) * (count))

#define mem_arena_resize_array(L, T, old_mem, old_count, new_count)            \
    cast(T *)mem_arena_resize(L, old_mem,                                      \
        /*old_size=*/sizeof(T) * (old_count),                                  \
        /*new_size=*/sizeof(T) * (new_count))


LULU_INTERNAL_FUNC void *
mem_heap_resize(lulu_State *L, void *old_ptr, usize old_size, usize new_size);

#define mem_heap_resize_array(L, T, old_ptr, old_cap, new_cap)                 \
    cast(T *)mem_heap_resize(L,                                                \
        old_ptr, sizeof(T) * (old_cap),                                        \
        sizeof(T) * (new_cap))

#define mem_heap_grow_array(L, T, pmem, pcap)                                  \
do {                                                                           \
    T *   _omem = *(pmem);                                                     \
    usize _ocap = *(pcap);                                                     \
    usize _ncap = _ocap > 8 ? _ocap * 2 : 8;                                   \
    *(pmem)     = mem_heap_resize_array(L, T, _omem, _ocap, _ncap);            \
    *(pcap)     = _ncap;                                                       \
} while (0)

#define mem_heap_free_array(L, mem, cap)                                       \
    mem_heap_resize(L, mem, sizeof((mem)[0]) * cap, 0)

#endif  // LULU_MEM_H

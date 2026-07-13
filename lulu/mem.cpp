#include <stdlib.h> // realloc, free
#include <string.h> // memcpy

#include "mem.hpp"
#include "state.hpp"

// 4 kilobyte page.
#define PAGE_SIZE   (1024 * 4)

/*
 Description:
    A header to a page of memory we can distribute in smaller pieces.
    The bytes we can actually distribute come after the header, so
    the actual alloctable size is PAGE_SIZE - header size.

 NOTE(2026-07-04):
    It is tempting to replace `usize` with `u16` in order to shave off
    8 bytes. But is it worth it?
 */
struct Page {
    Page *prev_page;

    // Current usage information.
    usize prev_offset;
    usize curr_offset;
};

#if 1
#define page_log(p, s, f, ...)  LULU_LOGF("[" s "] " #p " = " f, __VA_ARGS__)
#else
#define page_log(...)           cast(void)0
#endif

#define page_log_usage(p, info) \
    page_log(p, info, \
        "Page{prev_offset = %zu, curr_offset = %zu}", \
        (p)->prev_offset, \
        (p)->curr_offset)

static inline u8 *
page_data(Page *p)
{
    return cast(u8 *)(p + 1);
}

static inline usize
page_cap(Page *p)
{
    return PAGE_SIZE - sizeof(*p);
}

static inline u8 *
page_at(Page *p, usize offset)
{
    LULU_ASSERTF(offset < page_cap(p), "Out of bounds index %zu", offset);
    return page_data(p) + offset;
}

static void
page_init(Page *p, Page *prev)
{
    p->prev_page   = prev;
    p->prev_offset = 0;
    p->curr_offset = 0;
}

static Page *
page_new(lulu_State *L, Page *prev)
{
    unused(L);

    // TODO(2026-07-04): Grow the arena!
    if (prev) {
        return nullptr;
    }

    // Must be pointer-aligned.
    static union {
        void *dummy_align;
        u8    data[PAGE_SIZE];
    } buf;

    Page *p = cast(Page *)&buf.data;
    page_init(p, prev);
    return p;
}

static void
page_free(Page *p)
{
    unused(p);
    // free(p);
}

LULU_INTERNAL_FUNC void
mem_arena_init(lulu_State *L, Arena *a)
{
    a->head = page_new(L, nullptr);
    a->tail = a->head;
    page_log_usage(a->head, "ALLOC ");
    page_log_usage(a->tail, "ALLOC ");
}

LULU_INTERNAL_FUNC void
mem_arena_free_all(Arena *a)
{
    Page *p = a->tail;
    for (; p != a->head; p = p->prev_page) {
        page_free(p);
    }
    // `p` is now the head.
    page_init(p, nullptr);
    a->tail = p;
    page_log_usage(a->tail, "FREE  ");
}

LULU_INTERNAL_FUNC void
mem_arena_destroy(Arena *a)
{
    mem_arena_free_all(a);
    page_free(a->head);
}

// Align the given offset to the given alignment.
static usize
page_align_forward(Page *p, usize offset, usize align)
{
    uintptr_t base_addr, curr_addr, modulo;

    base_addr = cast(uintptr)page_data(p);
    curr_addr = base_addr + offset;

    // Check if the address is indeed aligned.
    modulo = curr_addr & (align - 1);

    // It's not aligned, so bump it up to the next aligned address.
    if (modulo != 0) {
        curr_addr += align - modulo;
    }
    return cast(usize)(curr_addr - base_addr);
}

static usize
arena_align_forward(Arena *a, usize offset)
{
    return page_align_forward(a->tail, offset, LULU_DEFAULT_ALIGN);
}


LULU_INTERNAL_FUNC void *
mem_arena_alloc(lulu_State *L, usize size)
{
    Arena *a     = &L->arena;
    usize  start = arena_align_forward(a, a->tail->curr_offset);
    usize  stop  = start + size;

    // Otherwise, we'd end up with aliased pointers!
    LULU_ASSERT(size != 0);
    page_log_usage(a->tail, "BEFORE");
    if (stop > page_cap(a->tail)) {
        Page *next = page_new(L, a->tail);
        if (!next) {
            state_throw(L, LULU_MEMORY_ERROR);
        }
        page_log(next, "MUTATE", "%p{prev_page = %p}", next, next->prev_page);
        a->tail = next;
    }

    // Note that our new previous must be the start of the aligned allocation,
    // not necessarily whatever our old current was.
    a->tail->prev_offset = start;
    a->tail->curr_offset = stop;
    page_log_usage(a->tail, "AFTER ");
    return page_at(a->tail, start);
}

LULU_INTERNAL_FUNC void *
mem_arena_resize(lulu_State *L, void *old_ptr, usize old_size, usize new_size)
{
    Arena *a       = &L->arena;
    u8 *   old_mem = cast(u8 *)old_ptr;
    if (old_mem == nullptr && old_size == 0) {
        // Nothing to resize, so we must be allocating a new block.
        return mem_arena_alloc(L, new_size);
    } else if (old_mem == page_at(a->tail, a->tail->prev_offset)) {
        // Resize in-place.
        if (new_size > old_size) {
            usize growth = new_size - old_size;
            // We don't fit in this page anymore, so try need a new one.
            if (a->tail->curr_offset + growth > page_cap(a->tail)) {
                goto resize_copy;
            }
            a->tail->curr_offset += growth;
        } else {
            // Also handles resizing the last allocation to zero.
            a->tail->curr_offset -= old_size - new_size;
        }
        return old_mem;
    } else resize_copy: {
        // Resize by creating an appropriately-sized copy.
        u8 *  new_mem   = cast(u8 *)mem_arena_alloc(L, new_size);
        usize copy_size = (new_size > old_size) ? old_size : new_size;
        return memcpy(new_mem, old_mem, copy_size);
    }
}

LULU_INTERNAL_FUNC Scratch
mem_scratch_begin(Arena *a)
{
    Page *  p = a->tail;
    Scratch x = {a, /*saved_page=*/p, p->prev_offset, p->curr_offset};
    page_log_usage(a->tail, "SCRATCH BEGIN");
    return x;
}

LULU_INTERNAL_FUNC void
mem_scratch_end(Scratch *x)
{
    Arena *a = x->backing;
    Page * p = nullptr;
    // Free all pages the scratch itself allocated.
    for (p = a->tail; p != x->saved_page; p = p->prev_page) {
        page_free(p);
    }

    // We are now at the page we saved at.
    a->tail        = p;
    p->prev_offset = x->prev_offset;
    p->curr_offset = x->curr_offset;
    page_log_usage(a->tail, "SCRATCH END");
}

LULU_INTERNAL_FUNC void *
mem_heap_resize_bytes(lulu_State *L, void *old_ptr, usize old_size, usize new_size)
{
    unused(old_size);
    if (new_size == 0) {
        free(old_ptr);
        return nullptr;
    } else {
        void *new_ptr = realloc(old_ptr, new_size);
        // NOTE(2026-07-07): Leaks other memory blocks!
        if (!new_ptr) {
            state_throw(L, LULU_MEMORY_ERROR);
        }
        return new_ptr;
    }
}

#undef page_log_usage
#undef page_log
#undef PAGE_SIZE


#pragma once

#include <cstdlib> // realloc, free
#include <cstring> // memcpy

#include "lulu.h"
#include "internal.hpp"
#include "slice.cpp"

// 4 kilobyte page.
#define PAGE_SIZE   (1024 * 4)

#if 0
#define page_log(p, s, f, ...)  LULU_LOGF("[" s "] " #p " = " f, __VA_ARGS__)
#else
#define page_log(...)           cast(void)0
#endif

#define page_log_usage(p, info) \
    page_log(p, info, \
        "Page{prev_offset = %zu, curr_offset = %zu}", \
        (p)->prev_offset, \
        (p)->curr_offset)

[[noreturn]] void
state_throw(lulu_State *L, lulu_Error error);

/*
 Description:
    A header to a page of memory we can distribute in smaller pieces.
    The bytes we can actually distribute come after the header, so
    the actual alloctable size is PAGE_SIZE - header size.

 NOTE(2026-07-04):
    It is tempting to replace `usize` with `u16` in order to shave off
    8 bytes. But is it worth it?
 */
class Page {
    friend class Arena;
    friend class Scratch;

    Page *prev_page;

    // Current usage information.
    usize prev_offset;
    usize curr_offset;

public:
    static Page *
    make(lulu_State *L, Page *prev)
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
        p->init(prev);
        return p;
    }

    void
    init(Page *prev)
    {
        this->prev_page   = prev;
        this->prev_offset = 0;
        this->curr_offset = 0;
    }

    inline u8 *
    data()
    {
        return cast(u8 *)(this + 1);
    }

    inline usize
    cap() const noexcept
    {
        return PAGE_SIZE - sizeof(*this);
    }

    inline u8 *
    at(usize offset)
    {
        LULU_ASSERTF(offset < this->cap(), "Out of bounds index %zu", offset);
        return this->data() + offset;
    }

    void
    destroy()
    {
        // std::free(p);
    }

    // Align the paged address at the given offset to the given alignment.
    usize
    align_forward(usize offset, usize align)
    {
        uintptr_t base_addr, curr_addr, modulo;

        base_addr = cast(uintptr)this->data();
        curr_addr = base_addr + offset;

        // Check if the address is indeed aligned.
        modulo = curr_addr & (align - 1);

        // It's not aligned, so bump it up to the next aligned address.
        if (modulo != 0) {
            curr_addr += align - modulo;
        }
        return cast(usize)(curr_addr - base_addr);
    }

}; // class Page

class Arena {
    friend class Scratch;

    /*
     The first page is only freed when the entire arena is explicitly destroyed
     via `arena_destroy()`. It should not be freed when `arena_free_all()` is called.
     */
    Page *head = nullptr;

    /*
     Beyond the first page, all succeeding pages we allocate can be freed
     whenever `arena_free_all()` is called.
     */
    Page *tail = nullptr;

public:
    void
    init(lulu_State *L)
    {
        this->head = Page::make(L, nullptr);
        this->tail = this->head;
        page_log_usage(this->head, "ALLOC ");
        page_log_usage(this->tail, "ALLOC ");
    }

    void
    free_all()
    {
        Page *p = this->tail;
        for (; p != this->head; p = p->prev_page) {
            p->destroy();
        }
        // `p` is now the head.
        p->init(nullptr);
        this->tail = p;
        page_log_usage(this->tail, "FREE  ");
    }

    void
    destroy()
    {
        this->free_all();
        this->head->destroy();
    }

    usize
    align_forward(usize offset)
    {
        return this->tail->align_forward(offset, LULU_DEFAULT_ALIGN);
    }

    [[nodiscard]] u8 *
    alloc_bytes(lulu_State *L, usize size)
    {
        usize  start = this->align_forward(this->tail->curr_offset);
        usize  stop  = start + size;

        // Otherwise, we'd end up with aliased pointers!
        LULU_ASSERT(size != 0);
        page_log_usage(this->tail, "BEFORE");
        if (stop > this->tail->cap()) {
            Page *next = Page::make(L, this->tail);
            if (!next) {
                state_throw(L, LULU_MEMORY_ERROR);
            }
            page_log(next, "MUTATE", "%p{prev_page = %p}", next, next->prev_page);
            this->tail = next;
        }

        // Note that our new previous must be the start of the aligned allocation,
        // not necessarily whatever our old current was.
        this->tail->prev_offset = start;
        this->tail->curr_offset = stop;
        page_log_usage(this->tail, "AFTER ");
        return this->tail->at(start);
    }

    [[nodiscard]] u8 *
    resize_bytes(lulu_State *L, void *old_ptr, usize old_size, usize new_size)
    {
        u8 *old_mem = cast(u8 *)old_ptr;
        if (old_mem == nullptr && old_size == 0) {
            // Nothing to resize, so we must be allocating a new block.
            return this->alloc_bytes(L, new_size);
        } else if (old_mem == this->tail->at(this->tail->prev_offset)) {
            // Resize in-place.
            if (new_size > old_size) {
                usize growth = new_size - old_size;
                // We don't fit in this page anymore, so try need a new one.
                if (this->tail->curr_offset + growth > this->tail->cap()) {
                    goto resize_copy;
                }
                this->tail->curr_offset += growth;
            } else {
                // Also handles resizing the last allocation to zero.
                this->tail->curr_offset -= old_size - new_size;
            }
            return old_mem;
        } else resize_copy: {
            // Resize by creating an appropriately-sized copy.
            u8 *  new_mem   = this->alloc_bytes(L, new_size);
            usize copy_size = min(new_size, old_size);
            return cast(u8 *)std::memcpy(new_mem, old_mem, copy_size);
        }
    }

    template<class T>
    [[nodiscard]] inline T *
    alloc(lulu_State *L, usize count = 1)
    {
        return cast(T *)this->alloc_bytes(L, sizeof(T) * count);
    }

    template<class T>
    [[nodiscard]] inline T *
    arena_resize(lulu_State *L, T *old_mem, usize old_cap, usize new_cap)
    {
        auto old_size = sizeof(T) * old_cap;
        auto new_size = sizeof(T) * new_cap;
        return cast(T *)this->resize_bytes(L, old_mem, old_size, new_size);
    }

}; // class Arena

class Scratch {
    Arena *backing    = nullptr;
    Page * saved_page = nullptr;

    // Track these manually as the underlying page itself may update.
    usize prev_offset = 0, curr_offset = 0;

public:
    static Scratch
    make(Arena *a)
    {
        Page *  p = a->tail;
        Scratch x;
        x.backing    = a;
        x.saved_page = p;
        x.prev_offset = p->prev_offset;
        x.curr_offset = p->curr_offset;
        page_log_usage(a->tail, "SCRATCH BEGIN");
        return x;
    }

    template<class T>
    [[nodiscard]] inline T *
    alloc(lulu_State *L, usize count = 1)
    {
        return this->backing->alloc<T>(L, count);
    }

    void
    destroy()
    {
        Arena *a = this->backing;
        Page * p = nullptr;
        // Free all pages the scratch itself allocated.
        for (p = a->tail; p != this->saved_page; p = p->prev_page) {
            p->destroy();
        }

        // We are now at the page we saved at.
        a->tail        = p;
        p->prev_offset = this->prev_offset;
        p->curr_offset = this->curr_offset;
        page_log_usage(this->tail, "SCRATCH END");
    }
};

#undef page_log_usage
#undef page_log
#undef PAGE_SIZE

[[nodiscard]] u8 *
mem_heap_resize_bytes(lulu_State *L, void *old_ptr, usize old_size, usize new_size)
{
    unused(old_size);
    if (new_size == 0) {
        std::free(old_ptr);
        return nullptr;
    } else {
        void *new_ptr = std::realloc(old_ptr, new_size);
        // NOTE(2026-07-07): Leaks other memory blocks!
        if (!new_ptr) {
            state_throw(L, LULU_MEMORY_ERROR);
        }
        return cast(u8 *)new_ptr;
    }
}

template<class T>
[[nodiscard]] static inline T *
mem_heap_alloc(lulu_State *L, usize count)
{
    return cast(T *)mem_heap_resize_bytes(L, nullptr, 0, sizeof(T) * count);
}

template<class T>
[[nodiscard]] static inline T *
mem_heap_resize(lulu_State *L, T *old_mem, usize old_cap, usize new_cap)
{
    auto old_size = sizeof(T) * old_cap;
    auto new_size = sizeof(T) * new_cap;
    return cast(T *)mem_heap_resize_bytes(L, old_mem, old_size, new_size);
}

template<class T, class Z>
[[nodiscard]] static inline T *
mem_heap_grow(lulu_State *L, T *mem, Z *count)
{
    T *   old_mem  = mem;
    usize old_cap  = cast(usize)*count;
    usize new_cap  = max(old_cap * 2, usize(8));
    *count = cast(Z)new_cap;
    return mem_heap_resize(L, old_mem, old_cap, new_cap);
}

template<class T>
static inline void
mem_heap_free(lulu_State *L, T *old_mem, usize old_count)
{
    cast(void)mem_heap_resize<T>(L, old_mem, old_count, 0);
}

template<class T>
[[nodiscard]] static inline Slice<T>
mem_alloc_slice(lulu_State *L, usize count)
{
    T *data = mem_heap_alloc<T>(L, count);
    return {data, count};
}

template<class T>
static inline void
mem_free_slice(lulu_State *L, Slice<T> s)
{
    mem_heap_free(L, s.raw_data(), s.len());
}


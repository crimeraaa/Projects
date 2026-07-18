#pragma once

#include <cstring> // memset

#include "lulu.h"
#include "slice.hpp"
#include "internal.hpp"

/// Defined in `mem.c`.
struct Page;
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

LULU_INTERNAL_FUNC [[nodiscard]] void *
mem_arena_alloc(lulu_State *L, usize size);

LULU_INTERNAL_FUNC [[nodiscard]] void *
mem_arena_resize(lulu_State *L, void *old_ptr, usize old_size, usize new_size);

template<class T>
[[nodiscard]] static inline T *
mem_arena_alloc_item(lulu_State *L)
{
    return cast(T *)mem_arena_alloc(L, sizeof(T));
}

template<class T>
[[nodiscard]] static inline T *
mem_arena_alloc_array(lulu_State *L, usize count)
{
    return cast(T *)mem_arena_alloc(L, sizeof(T) * count);
}

template<class T>
[[nodiscard]] static inline T *
mem_arena_resize_array(lulu_State *L, T *old_mem, usize old_cap, usize new_cap)
{
    auto old_size = sizeof(T) * old_cap;
    auto new_size = sizeof(T) * new_cap;
    return cast(T *)mem_arena_resize(L, old_mem, old_size, new_size);
}


LULU_INTERNAL_FUNC [[nodiscard]] void *
mem_heap_resize_bytes(lulu_State *L, void *old_ptr, usize old_size, usize new_size);

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
    usize new_cap  = (old_cap > 8) ? old_cap * 2 : 8;
    *count = cast(Z)new_cap;
    return mem_heap_resize(L, old_mem, old_cap, new_cap);
}

template<class T>
static inline void
mem_zero_slice(Slice<T> s)
{
    std::memset(raw_data(s), 0, sizeof(T) * len(s));
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
mem_resize_slice(lulu_State *L, Slice<T> *s, usize count)
{
    s->data = mem_heap_resize(L, s->data, len(*s), count);
    s->len  = count;
}

template<class T>
static inline void
mem_grow_slice(lulu_State *L, Slice<T> *s)
{
    s->data = mem_heap_grow(L, s->data, &s->len);
}

template<class T>
static inline void
mem_free_slice(lulu_State *L, Slice<T> s)
{
    mem_heap_free(L, s.data, len(s));
}

template<class T>
struct Dynamic {
    Slice<T> slice;
    usize    cap = 0;

    // Defer to underlying Slice implementation.
    template<class N> T &      operator[](N index)       { return this->slice[index]; }
    template<class N> T const &operator[](N index) const { return this->slice[index]; }
};

template<class T> static inline T *   raw_data (Dynamic<T> d) { return raw_data(d.slice); }
template<class T> static inline usize len      (Dynamic<T> d) { return len(d.slice); }
template<class T> static inline usize cap      (Dynamic<T> d) { return d.cap; }
template<class T> static inline T *   begin    (Dynamic<T> d) { return begin(d.slice); }
template<class T> static inline T *   end      (Dynamic<T> d) { return end(d.slice); }

template<class T>
static inline void
mem_append(lulu_State *L, Dynamic<T> *d, T const &value)
{
    if (len(*d) + 1 > cap(*d)) {
        d->slice.data = mem_heap_grow(L, d->slice.data, &d->cap);
    }

    // Raw access because we assign to a (currently) out of bounds index.
    // Only once length is updated can we use operator[] again.
    d->slice.data[d->slice.len++] = value;
}


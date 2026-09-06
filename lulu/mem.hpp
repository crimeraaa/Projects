#pragma once

#include <cstring> // memset

#include "lulu.h"
#include "slice.hpp"
#include "internal.hpp"

/// Defined in `mem.c`.
struct Page;
struct Arena {
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
};

struct Scratch {
    Arena *backing    = nullptr;
    Page * saved_page = nullptr;

    // Track these manually as the underlying page itself may update.
    usize prev_offset = 0, curr_offset = 0;
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
mem_scratch_free_all(Scratch *x);

[[nodiscard]] LULU_INTERNAL_FUNC u8 *
mem_arena_alloc_bytes(lulu_State *L, Arena *a, usize size);

[[nodiscard]] LULU_INTERNAL_FUNC u8 *
mem_arena_resize_bytes(lulu_State *L, Arena *a, void *old_ptr, usize old_size, usize new_size);

template<class T>
[[nodiscard]] static inline T *
mem_arena_alloc(lulu_State *L, Arena *a, usize count = 1)
{
    return cast(T *)mem_arena_alloc_bytes(L, a, sizeof(T) * count);
}

template<class T>
[[nodiscard]] static inline T *
mem_arena_resize(lulu_State *L, Arena *a, T *old_mem, usize old_cap, usize new_cap)
{
    auto old_size = sizeof(T) * old_cap;
    auto new_size = sizeof(T) * new_cap;
    return cast(T *)mem_arena_resize_bytes(L, a, old_mem, old_size, new_size);
}

[[nodiscard]] LULU_INTERNAL_FUNC u8 *
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
    usize new_cap  = max(old_cap * 2, usize(8));
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
    s->data = mem_heap_resize(L, s->data, s->len, count);
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
template<class T> static inline usize len      (Dynamic<T> d) { return len(d.slice);      }
template<class T> static inline usize cap      (Dynamic<T> d) { return d.cap;             }
template<class T> static inline T *   begin    (Dynamic<T> d) { return begin(d.slice);    }
template<class T> static inline T *   end      (Dynamic<T> d) { return end(d.slice);      }

template<class T>
static inline void
mem_append_dynamic(lulu_State *L, Dynamic<T> *d, T const &value)
{
    if (len(*d) + 1 > cap(*d)) {
        d->slice.data = mem_heap_grow(L, d->slice.data, &d->cap);
    }

    // Raw access because we assign to a (currently) out of bounds index.
    // Only once the length is updated can we use operator[] again.
    d->slice.data[d->slice.len++] = value;
}

template<class T>
static inline void
mem_shrink_dynamic(lulu_State *L, Dynamic<T> *d)
{
    usize n       = d->slice.len;
    d->slice.data = mem_heap_resize(L, d->slice.data, d->cap, n);
    d->cap        = n;
}

template<class T>
static inline void
mem_free_dynamic(lulu_State *L, Dynamic<T> *d)
{
    mem_free_slice(L, d->slice);
}

template<class T>
[[nodiscard]] static inline T *
mem_scratch_alloc(lulu_State *L, Scratch *x, usize count = 1)
{
    return mem_arena_alloc<T>(L, x->backing, count);
}

template<class T>
static inline RevSlice<T>
reverse(Dynamic<T> d)
{
    return reverse(d.slice);
}

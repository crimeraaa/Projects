#include "arena.h"

LULU_INTERNAL_FUNC Arena
arena_make(void *buf, size_t buf_size)
{
    Arena a = {buf, buf_size, /*prev_index=*/0, /*curr_index=*/0};
    return a;
}

LULU_INTERNAL_FUNC size_t
arena_align_forward(Arena *a, size_t index, size_t align)
{
    uintptr_t base_addr, curr_addr, modulo;

    base_addr = cast(uintptr)a->buf;
    curr_addr = base_addr + index;

    // Check if the address is indeed aligned.
    modulo = curr_addr & (cast(uintptr)align - 1);

    // It's not aligned, so bump it up to the next aligned address.
    if (modulo != 0) {
        curr_addr += align - modulo;
    }
    return cast(size_t)(curr_addr - base_addr);
}

LULU_INTERNAL_FUNC void *
arena_alloc(Arena *a, size_t size)
{
    size_t start, stop;

    // For simplicity, assume all allocations are pointer-aligned.
    start = arena_align_forward(a, a->curr_index, sizeof(void *));
    stop  = start + size;
    if (stop < a->buf_size) {
        a->prev_index = start;
        a->curr_index = stop;
        return &a->buf[start];
    }
    return NULL;
}

LULU_INTERNAL_FUNC void *
arena_resize(Arena *a,
    void *old_mem, size_t old_size,
    size_t new_size)
{
    unsigned char *old_ptr = cast(unsigned char *)old_mem;
    if (old_mem == NULL && old_size == 0) {
        return arena_alloc(a, new_size);
    } else if (old_ptr == a->buf + a->prev_index) {
        if (new_size > old_size) {
            size_t growth     = new_size - old_size;
            size_t next_index = a->curr_index + growth;
            if (next_index > a->buf_size) {
                return NULL;
            } else {
                a->curr_index = next_index;
            }
        } else {
            size_t shrink     = old_size - new_size;
            size_t next_index = a->curr_index - shrink;
            a->curr_index     = next_index;
        }
        return old_ptr;
    } else {
        unsigned char *copy_mem;
        size_t copy_size;

        copy_mem = cast(unsigned char *)arena_alloc(a, new_size);
        if (!copy_mem) {
            return NULL;
        }

        copy_size = (new_size > old_size) ? old_size : new_size;
        for (size_t i = 0; i < copy_size; i++) {
            copy_mem[i] = old_ptr[i];
        }
        return copy_mem;
    }
}

LULU_INTERNAL_FUNC void
arena_free_all(Arena *a)
{
    a->prev_index = 0;
    a->curr_index = 0;
}

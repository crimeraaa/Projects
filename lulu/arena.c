#include "arena.h"

// For simplicity, assume all allocations are pointer-aligned.
// This may also help in vectorizing memory copies (or not!).
#define ARENA_ALIGN sizeof(void *)

LULU_INTERNAL_FUNC Arena
arena_make(void *buf, usize buf_size)
{
    Arena a = {buf, buf_size, /*prev_offset=*/0, /*curr_offset=*/0};
    return a;
}

// Align the given offset to the given alignment.
static usize
arena_align_forward(Arena *a, usize offset, usize align)
{
    uintptr_t base_addr, curr_addr, modulo;

    base_addr = cast(uintptr)a->buf;
    curr_addr = base_addr + offset;

    // Check if the address is indeed aligned.
    modulo = curr_addr & (cast(uintptr)align - 1);

    // It's not aligned, so bump it up to the next aligned address.
    if (modulo != 0) {
        curr_addr += align - modulo;
    }
    return cast(usize)(curr_addr - base_addr);
}

LULU_INTERNAL_FUNC void *
arena_alloc(Arena *a, usize size)
{
    usize start, stop;

    start = arena_align_forward(a, a->curr_offset, ARENA_ALIGN);
    stop  = start + size;
    if (stop < a->buf_size) {
        a->prev_offset = start;
        a->curr_offset = stop;
        return &a->buf[start];
    }
    return NULL;
}

static void *
arena_resize_inplace(Arena *a, u8 *old_mem, usize old_size, usize new_size)
{
    if (new_size > old_size) {
        usize growth      = new_size - old_size;
        usize next_offset = a->curr_offset + growth;
        if (next_offset > a->buf_size) {
            return NULL;
        }
        a->curr_offset = next_offset;
    } else {
        usize shrink      = old_size - new_size;
        usize next_offset = a->curr_offset - shrink;
        a->curr_offset     = next_offset;
    }
    return old_mem;

}

static void *
arena_resize_copy(Arena *a, u8 *old_mem, usize old_size, usize new_size)
{
    u8 *new_mem = cast(u8 *)arena_alloc(a, new_size);
    if (new_mem) {
        usize i, n;

        i = 0;
        n = (new_size > old_size) ? old_size : new_size;
        for (; i + ARENA_ALIGN <= n; i += ARENA_ALIGN) {
            // Pls unroll the loop thx :)
            for (usize j = 0; j < ARENA_ALIGN; j++) {
                new_mem[i + j] = old_mem[i + j];
            }
        }

        for (; i < n; i++) {
            new_mem[i] = old_mem[i];
        }
    }
    return new_mem;
}

LULU_INTERNAL_FUNC void *
arena_resize(Arena *a,
    void  *p, usize old_size,
    usize new_size)
{
    u8 *old_mem = cast(u8 *)p;
    if (old_mem == NULL && old_size == 0) {
        return arena_alloc(a, new_size);
    } else if (old_mem == a->buf + a->prev_offset) {
        return arena_resize_inplace(a, old_mem, old_size, new_size);
    } else {
        return arena_resize_copy(a, old_mem, old_size, new_size);
    }
}

LULU_INTERNAL_FUNC void
arena_free_all(Arena *a)
{
    a->prev_offset = 0;
    a->curr_offset = 0;
}

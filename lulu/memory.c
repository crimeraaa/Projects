#include <string.h> // memcpy

#include "memory.h"
#include "state.h"

// Align the given offset to the given alignment.
static usize
arena_align_forward(Arena *a, usize offset, usize align)
{
    uintptr_t base_addr, curr_addr, modulo;

    base_addr = cast(uintptr)a->buf;
    curr_addr = base_addr + offset;

    // Check if the address is indeed aligned.
    modulo = curr_addr & (align - 1);

    // It's not aligned, so bump it up to the next aligned address.
    if (modulo != 0) {
        curr_addr += align - modulo;
    }
    return cast(usize)(curr_addr - base_addr);
}

LULU_INTERNAL_FUNC void *
mem_arena_alloc(lulu_State *L, usize size)
{
    Arena *a     = &L->arena;
    usize  start = arena_align_forward(a, a->curr_offset, LULU_DEFAULT_ALIGN);
    usize  stop  = start + size;
    if (stop >= a->buf_size) {
        state_throw(L, LULU_MEMORY_ERROR);
    }

    // Note that our new previous must be the start of the aligned allocation,
    // not necessarily whatever our old current was.
    a->prev_offset = start;
    a->curr_offset = stop;
    return &a->buf[start];
}

LULU_INTERNAL_FUNC void *
mem_arena_resize(lulu_State *L, void *pointer, usize old_size, usize new_size)
{
    Arena *a       = &L->arena;
    u8    *old_mem = cast(u8 *)pointer;
    if (old_mem == NULL && old_size == 0) {
        return mem_arena_alloc(L, new_size);
    } else if (old_mem == a->buf + a->prev_offset) {
        // Resize in-place.
        if (new_size > old_size) {
            usize growth      = new_size - old_size;
            usize next_offset = a->curr_offset + growth;
            if (next_offset > a->buf_size) {
                state_throw(L, LULU_MEMORY_ERROR);
            }
            a->curr_offset = next_offset;
        } else {
            usize shrink      = old_size - new_size;
            usize next_offset = a->curr_offset - shrink;
            a->curr_offset    = next_offset;
        }
        return old_mem;
    } else {
        // Resize by creating an appropriately-sized copy.
        u8 *  new_mem   = cast(u8 *)mem_arena_alloc(L, new_size);
        usize copy_size = (new_size > old_size) ? old_size : new_size;
        return memcpy(new_mem, old_mem, copy_size);
    }
}

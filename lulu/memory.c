#include "memory.h"
#include "state.h"

LULU_INTERNAL_FUNC void *
mem_arena_alloc(lulu_State *L, usize size)
{
    void *p = arena_alloc(&L->arena, size);
    if (!p) {
        state_throw(L, LULU_MEMORY_ERROR);
    }
    return p;
}

LULU_INTERNAL_FUNC void *
mem_arena_resize(lulu_State *L, void *old_mem, usize old_size, usize new_size)
{
    void *new_mem = arena_resize(&L->arena, old_mem, old_size, new_size);
    if (!new_mem) {
        state_throw(L, LULU_MEMORY_ERROR);
    }
    return new_mem;
}

#include "chunk.h"
#include "mem.h"

LULU_INTERNAL_FUNC void
chunk_destroy(lulu_State *L, Chunk *c)
{
    mem_heap_free_array(L, c->code,   c->code_cap);
    mem_heap_free_array(L, c->values, c->values_cap);

    // In case we throw errors, ensure we don't double free.
    *c = chunk_make();
}

LULU_INTERNAL_FUNC void
chunk_add_instruction(lulu_State *L, Chunk *c, Instruction i)
{
    if (c->code_len + 1 > c->code_cap) {
        mem_heap_grow_array(L, Instruction, &c->code, &c->code_cap);
    }
    c->code[c->code_len++] = i;
}

LULU_INTERNAL_FUNC u32
chunk_add_constant(lulu_State *L, Chunk *c, TValue v)
{
    for (u32 i = 0; i < c->values_len; i++) {
        if (tvalue_eq(v, c->values[i])) {
            return i;
        }
    }

    if (c->values_len + 1 > c->values_cap) {
        usize tmp = cast(usize)c->values_cap;
        mem_heap_grow_array(L, TValue, &c->values, &tmp);
        c->values_cap = cast(u32)tmp;
    }
    c->values[c->values_len++] = v;
    return c->values_len - 1;
}

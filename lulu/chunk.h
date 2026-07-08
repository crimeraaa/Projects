#ifndef LULU_CHUNK_H
#define LULU_CHUNK_H

#include "lulu.h"
#include "internal.h"
#include "opcode.h"
#include "value.h"

#define CHUNK_MAX_CONSTANTS     ARG_MAX_Bx

typedef struct Chunk Chunk;
struct Chunk {
    // Bytecode array.
    Instruction *code;
    usize        code_len;
    usize        code_cap;

    // Constants dynamic array.
    TValue *values;
    u32     values_len;
    u32     values_cap;

    // Usage information.
    u16 stack_size;
};

static inline Chunk
chunk_make(void)
{
    Chunk c = {/*code=*/nullptr, 0, 0,
        /*values     =*/nullptr, 0, 0,
        /*stack_size =*/0};
    return c;
}

LULU_INTERNAL_FUNC void
chunk_destroy(lulu_State *L, Chunk *c);

LULU_INTERNAL_FUNC void
chunk_add_instruction(lulu_State *L, Chunk *c, Instruction i);

LULU_INTERNAL_FUNC u32
chunk_add_constant(lulu_State *L, Chunk *c, TValue v);

#endif // !LULU_CHUNK_H

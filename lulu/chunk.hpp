#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "opcode.hpp"
#include "value.hpp"

#define CHUNK_MAX_CONSTANTS     ARG_MAX_Bx

struct Chunk {
    // Bytecode array.
    Instruction *code     = nullptr;
    i32          code_cap = 0;

    // Constants dynamic array.
    TValue *values        = nullptr;
    u32     values_cap    = 0;

    // Usage information.
    u16 stack_size = 0;
};

LULU_INTERNAL_FUNC void
chunk_destroy(lulu_State *L, Chunk *c);


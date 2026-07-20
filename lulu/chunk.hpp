#pragma once

#include "internal.hpp"
#include "opcode.hpp"
#include "value.hpp"

#define CHUNK_MAX_CONSTANTS     ARG_MAX_Bx

struct Chunk {
    // Bytecode array.
    Instruction *code           = nullptr;
    i32          code_cap       = 0;

    // Constants dynamic array.
    TValue *     constants      = nullptr;
    u32          constants_cap  = 0;

    // Usage information.
    u16          stack_size     = 0;
};

struct StackInfo {
    i32       pc_born, pc_died;
    ValueKind kind;
};


#pragma once

#include "internal.hpp"
#include "opcode.hpp"
#include "mem.hpp"
#include "value.hpp"
#include "type.hpp"

#define CHUNK_MAX_CONSTANTS     ARG_MAX_Bx

static inline i32 PC_NONE = -1;

struct StackInfo {
    u8          reg;
    Type const *type;
    i32         pc_born, pc_died;
};

struct Chunk {
    Dynamic<Instruction> code;
    Dynamic<StackInfo>   stack_info;
    Dynamic<TValue>      constants;
    u8                   stack_size = 0;
};


// Fixed chunk. The structure is a header, the actual in-memory data
// comes after.
struct FChunk {
    u32 code_len;
    u32 stack_info_len;
    u32 constants_len;
    u8  stack_size;
};



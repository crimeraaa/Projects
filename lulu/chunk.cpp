#pragma once

#include "internal.hpp"
#include "dynamic.cpp"

#define CHUNK_MAX_CONSTANTS     ARG_MAX_Bx

static inline i32 PC_NONE = -1;

struct Type;
struct Instruction;
struct TValue;

struct RegInfo {
    u8          reg;
    i32         pc_born;
    i32         pc_died; // Note that this is inclusive!
    Type const *type;
};

struct Chunk {
    Dynamic<Instruction> code;
    Dynamic<RegInfo>     reg_info;
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



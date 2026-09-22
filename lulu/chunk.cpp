#pragma once

#include "internal.hpp"
#include "dynamic.cpp"
#include "opcode.cpp"
#include "value.cpp"

#define CHUNK_MAX_CONSTANTS     ARG_MAX_Bx

static inline i32 PC_NONE = -1;

struct Type;

struct RegInfo {
    u8          reg;
    i32         pc_born;
    i32         pc_died; // Note that this is inclusive!
    Type const *type;
};

// Dynamic chunk. Only used for compile time.
struct Chunk {
    Dynamic<Instruction> code;
    Dynamic<RegInfo>     reg_info;
    Dynamic<TValue>      constants;
    u8                   stack_size = 0;
};


/*
 Description:
    This is a flexible-array header for a fixed chunk. The actual in-memory
    data comes after the size of the header. Follow this diagram, where `$`
    represents the last byte offset we used. We order them specifically this
    way so that we can assume the register info and constants are always
    pointer-aligned. Only the code segment may have an end-pointer that is not
    pointer-aligned, mainly on x86-64 where `alignof(u32) != alignof(void *)`.

              byte | 0 1 2 3 4 5 6 7 8 9 A B C D E F|
              0x00 | nc     |   nk  | ninfo | nstk  |
              0x10 | info[ninfo]....................|
    sizeof(info)+$ | k[nk]..........................|
    sizeof(k)   +$ | code[nc].......................|
 */
struct alignas(LULU_DEFAULT_ALIGN) FChunk {
    u32 code_len;
    u32 constants_len;
    u32 reg_info_len;
    u8  stack_size;

    Slice<RegInfo>
    reg_info()
    {
        RegInfo *info = cast(RegInfo *)(this + 1);
        return {info, cast(usize)this->reg_info_len};
    }

    Slice<TValue>
    constants()
    {
        TValue *k = cast(TValue *)this->reg_info().end();
        return {k, cast(usize)this->constants_len};
    }

    Slice<Instruction>
    code()
    {
        Instruction *ip = cast(Instruction *)this->constants().end();
        return {ip, cast(usize)this->code_len};
    }
};



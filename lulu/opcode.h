#ifndef LULU_OPCODE_H
#define LULU_OPCODE_H

#include "internal.h"

typedef enum OpCode {
    Op_None,

    // Literals
    Op_Load_imm,  // R(A) := (uint)Bx
    Op_Load_int,  // R(A) := K(Bx)
    Op_Load_real, // R(A) := K(Bx).(real)

    // Integer arithmetic: common operations that apply to uint and int
    // R(A) := R(B) op R(C)
    Op_Neg_int, Op_Add_int, Op_Sub_int, Op_Mul_int,

    // Integer arithmetic: discriminated operations
    // R(A) := R(B).(real) op R(C).(real)
    Op_Div_uint, Op_Mod_uint,
    Op_Div_int,  Op_Mod_int,

    // Floating-point arithmetic
    Op_Neg_real, Op_Add_real, Op_Sub_real, Op_Mul_real, Op_Div_real, Op_Mod_real,

    // return
    Op_Return,
} OpCode;

#define INSTRUCTION_TYPE        u32

// Instruction argument bitfield sizes.
#define INSTRUCTION_OP_WIDTH    6
#define INSTRUCTION_A_WIDTH     8
#define INSTRUCTION_C_WIDTH     9
#define INSTRUCTION_B_WIDTH     9

// Instruction argument limits.
#define INSTRUCTION_OP_MAX      ((1 << INSTRUCTION_OP_WIDTH) - 1)
#define INSTRUCTION_A_MAX       ((1 << INSTRUCTION_A_WIDTH)  - 1)
#define INSTRUCTION_B_MAX       ((1 << INSTRUCTION_B_WIDTH)  - 1)
#define INSTRUCTION_C_MAX       ((1 << INSTRUCTION_C_WIDTH)  - 1)

// Instruction argument bitfield offsets.
#define INSTRUCTION_OP_OFFSET   0
#define INSTRUCTION_A_OFFSET    (INSTRUCTION_OP_OFFSET + INSTRUCTION_OP_WIDTH)
#define INSTRUCTION_C_OFFSET    (INSTRUCTION_A_OFFSET  + INSTRUCTION_A_WIDTH)
#define INSTRUCTION_B_OFFSET    (INSTRUCTION_C_OFFSET  + INSTRUCTION_C_WIDTH)

// Variant bits
#define INSTRUCTION_Bx_WIDTH    (INSTRUCTION_B_WIDTH + INSTRUCTION_C_WIDTH)
#define INSTRUCTION_Bx_OFFSET   INSTRUCTION_C_OFFSET
#define INSTRUCTION_Bx_MAX      ((1 << INSTRUCTION_Bx_WIDTH) - 1)

#define INSTRUCTION_sBx_MAX     (INSTRUCTION_Bx_MAX >> 1)
#define INSTRUCTION_sBx_MIN     (0 - INSTRUCTION_sBx_MAX)

#define MASK1(max, offset)      ((max) << (offset))
#define MASK0(max, offset)      (~(MASK1(max, offset)))

#define INSTRUCTION_OP_MASK0    MASK0(INSTRUCTION_OP_MAX, INSTRUCTION_OP_OFFSET)
#define INSTRUCTION_A_MASK0     MASK0(INSTRUCTION_A_MAX,  INSTRUCTION_A_OFFSET)
#define INSTRUCTION_B_MASK0     MASK0(INSTRUCTION_B_MAX,  INSTRUCTION_B_OFFSET)
#define INSTRUCTION_C_MASK0     MASK0(INSTRUCTION_C_MAX,  INSTRUCTION_C_OFFSET)
#define INSTRUCTION_Bx_MASK0    MASK0(INSTRUCTION_Bx_MAX, INSTRUCTION_Bx_OFFSET)

#if (INSTRUCTION_OP_WIDTH \
    + INSTRUCTION_A_WIDTH \
    + INSTRUCTION_B_WIDTH \
    + INSTRUCTION_C_WIDTH) != 32

#error Instruction width must be 32 exactly.
#endif

/*
 Instruction format:

 tens |0.....|...1....|.....2...|......3..|
 ones |123456|78901234|567890123|456789012|
 ABC  |Op....|A.......|C........|B........|
 ABx  |Op....|A.......|Bx.................|
 */
typedef INSTRUCTION_TYPE Instruction;

static inline Instruction
instruction_make_ABC(OpCode Op, u8 A, u16 B, u16 C)
{
    Instruction i = 0;
    LULU_ASSERT(B <= INSTRUCTION_B_MAX);
    LULU_ASSERT(C <= INSTRUCTION_C_MAX);

    i |= cast(u32)Op << INSTRUCTION_OP_OFFSET;
    i |= cast(u32)A  << INSTRUCTION_A_OFFSET;
    i |= cast(u32)C  << INSTRUCTION_C_OFFSET;
    i |= cast(u32)B  << INSTRUCTION_B_OFFSET;
    return i;
}

static inline Instruction
instruction_make_ABx(OpCode Op, u8 A, u32 Bx)
{
    Instruction i = 0;
    LULU_ASSERT(Bx <= INSTRUCTION_Bx_MAX);

    i |= cast(u32)Op << INSTRUCTION_OP_OFFSET;
    i |= cast(u32)A  << INSTRUCTION_A_OFFSET;
    i |=          Bx << INSTRUCTION_Bx_OFFSET;
    return i;
}

static inline OpCode
instruction_get_Op(Instruction i)
{
    return cast(OpCode)((i >> INSTRUCTION_OP_OFFSET) & INSTRUCTION_OP_MAX);
}

static inline u8
instruction_get_A(Instruction i)
{
    return cast(u8)((i >> INSTRUCTION_A_OFFSET) & INSTRUCTION_A_MAX);
}

static inline u16
instruction_get_B(Instruction i)
{
    return cast(u16)((i >> INSTRUCTION_B_OFFSET) & INSTRUCTION_B_MAX);
}

static inline u16
instruction_get_C(Instruction i)
{
    return cast(u16)((i >> INSTRUCTION_C_OFFSET) & INSTRUCTION_C_MAX);
}

static inline u32
instruction_get_Bx(Instruction i)
{
    return (i >> INSTRUCTION_Bx_OFFSET) & INSTRUCTION_Bx_MAX;
}

static inline void
instruction_set_Op(Instruction *i, OpCode Op)
{
    *i = (*i & INSTRUCTION_OP_MASK0) | (cast(u32)Op << INSTRUCTION_OP_OFFSET);
}

static inline void
instruction_set_A(Instruction *i, u8 A)
{
    LULU_ASSERT(A <= INSTRUCTION_A_MAX);

    // Clear the original A's bits then set the new one's into place.
    *i = (*i & INSTRUCTION_A_MASK0) | (cast(u32)A << INSTRUCTION_A_OFFSET);
}

static inline void
instruction_set_B(Instruction *i, u16 B)
{
    LULU_ASSERT(B <= INSTRUCTION_B_MAX);
    *i = (*i & INSTRUCTION_B_MASK0) | (cast(u32)B << INSTRUCTION_B_OFFSET);
}

static inline void
instruction_set_C(Instruction *i, u16 C)
{
    LULU_ASSERT(C <= INSTRUCTION_C_MAX);
    *i = (*i & INSTRUCTION_C_MASK0) | (cast(u32)C << INSTRUCTION_C_OFFSET);
}

static inline void
instruction_set_Bx(Instruction *i, u32 Bx)
{
    LULU_ASSERT(Bx <= INSTRUCTION_Bx_MAX);
    *i = (*i & INSTRUCTION_Bx_MASK0) | (Bx << INSTRUCTION_Bx_OFFSET);
}

#endif // !LULU_OPCODE_H

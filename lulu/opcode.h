#ifndef LULU_OPCODE_H
#define LULU_OPCODE_H

#include "internal.h"

typedef enum OpCode {
    Op_None,

    // Literals
    Op_Load_imm,  // R(A) := (uint)Bx
    Op_Load_int,  // R(A) := K(Bx)
    Op_Load_real, // R(A) := K(Bx).(real)

    // Conversion operations

    // Integer arithmetic: common operations that apply to uint and int
    // R(A) := R(B) op R(C)
    Op_Neg_int, Op_Add_int, Op_Sub_int, Op_Mul_int,

    // Integer arithmetic: discriminated operations
    // R(A) := R(B) op R(C)
    Op_Div_uint, Op_Mod_uint,
    Op_Div_int,  Op_Mod_int,

    // Floating-point arithmetic
    // R(A) := R(B) op R(C)
    Op_Neg_real, Op_Add_real, Op_Sub_real, Op_Mul_real, Op_Div_real,

    // return
    Op_Return,
} OpCode;

// Instruction argument bitfield sizes.
#define ARG_OP_WIDTH        6
#define ARG_A_WIDTH         8
#define ARG_C_WIDTH         9
#define ARG_B_WIDTH         9

// Instruction argument limits.
#define ARG_OP_MAX          ((1 << ARG_OP_WIDTH) - 1)
#define ARG_A_MAX           ((1 << ARG_A_WIDTH)  - 1)
#define ARG_B_MAX           ((1 << ARG_B_WIDTH)  - 1)
#define ARG_C_MAX           ((1 << ARG_C_WIDTH)  - 1)

// Instruction argument bitfield offsets.
#define ARG_OP_OFFSET       0
#define ARG_A_OFFSET        (ARG_OP_OFFSET + ARG_OP_WIDTH)
#define ARG_C_OFFSET        (ARG_A_OFFSET  + ARG_A_WIDTH)
#define ARG_B_OFFSET        (ARG_C_OFFSET  + ARG_C_WIDTH)

// Variant bits
#define ARG_Bx_WIDTH        (ARG_B_WIDTH + ARG_C_WIDTH)
#define ARG_Bx_OFFSET       ARG_C_OFFSET
#define ARG_Bx_MAX          ((1 << ARG_Bx_WIDTH) - 1)

#define ARG_sBx_MAX         (ARG_Bx_MAX >> 1)
#define ARG_sBx_MIN         (0 - ARG_sBx_MAX)

#define MASK1(max, offset)  (cast(Instruction)(max) << cast(Instruction)(offset))
#define MASK0(max, offset)  (~(MASK1(max, offset)))

// Zero-masks to help clear out the arguments when setting them.
#define ARG_OP_MASK0        MASK0(ARG_OP_MAX, ARG_OP_OFFSET)
#define ARG_A_MASK0         MASK0(ARG_A_MAX,  ARG_A_OFFSET)
#define ARG_B_MASK0         MASK0(ARG_B_MAX,  ARG_B_OFFSET)
#define ARG_C_MASK0         MASK0(ARG_C_MAX,  ARG_C_OFFSET)
#define ARG_Bx_MASK0        MASK0(ARG_Bx_MAX, ARG_Bx_OFFSET)

#if (ARG_OP_WIDTH \
    + ARG_A_WIDTH \
    + ARG_B_WIDTH \
    + ARG_C_WIDTH) != 32

#error Instruction width must be 32 exactly.
#endif

/*
 Instruction format:

 +------------------------------------------+
 | tens |0.....|...1....|.....2...|......3..|
 | ones |123456|78901234|567890123|456789012|
 | ABC  |Op....|A.......|C........|B........|
 | ABx  |Op....|A.......|Bx.................|
 | AsBx |Op....|A.......|sBx................|
 +------------------------------------------+
 */
typedef u32 Instruction;

#define MAKE_ABC(Op, A, B, C)                                                  \
    ( ( LULU_ASSERT(B <= ARG_B_MAX), LULU_ASSERT(C <= ARG_C_MAX) ),            \
    (cast(Instruction)Op << ARG_OP_OFFSET)                                     \
        | (cast(Instruction)A << ARG_A_OFFSET)                                 \
        | (cast(Instruction)C << ARG_C_OFFSET)                                 \
        | (cast(Instruction)B << ARG_B_OFFSET) )


#define MAKE_ABx(Op, A, Bx)                                                    \
    ( LULU_ASSERT(Bx <= ARG_Bx_MAX),                                           \
    (cast(Instruction)Op << ARG_OP_OFFSET)                                     \
        | (cast(Instruction)A  << ARG_A_OFFSET)                                \
        | (cast(Instruction)Bx << ARG_Bx_OFFSET) )

static inline OpCode
GET_OPCODE(Instruction i)
{
    return cast(OpCode)((i >> ARG_OP_OFFSET) & ARG_OP_MAX);
}

static inline u8
GETARG_A(Instruction i)
{
    return cast(u8)((i >> ARG_A_OFFSET) & ARG_A_MAX);
}

static inline u16
GETARG_B(Instruction i)
{
    return cast(u16)((i >> ARG_B_OFFSET) & ARG_B_MAX);
}

static inline u16
GETARG_C(Instruction i)
{
    return cast(u16)((i >> ARG_C_OFFSET) & ARG_C_MAX);
}

static inline u32
GETARG_Bx(Instruction i)
{
    return (i >> ARG_Bx_OFFSET) & ARG_Bx_MAX;
}

#define SET_OPCODE(ip, Op) \
    (*(ip) = (*(ip) & ARG_OP_MASK0) | (cast(u32)Op << ARG_OP_OFFSET))

// Clear the original A's bits then set the new one's into place.
// We use macros so asserts report at the site of their usage, not in an
// inline function.
#define SETARG_A(ip, A)                                                        \
    ( LULU_ASSERT((A) <= ARG_A_MAX),                                           \
    *(ip) = (*(ip) & ARG_A_MASK0)                                              \
          | (cast(u32)(A) << ARG_A_OFFSET) )

#define SETARG_B(ip, B)                                                        \
    ( LULU_ASSERT((B) <= ARG_B_MAX),                                           \
    *(ip) = (*(ip) & ARG_B_MASK0)                                              \
          | (cast(u32)(B) << ARG_B_OFFSET) )

#define SETARG_C(ip, C)                                                        \
    ( LULU_ASSERT((C) <= ARG_C_MAX),                                           \
    *(ip) = (*(ip) & ARG_C_MASK0)                                              \
          | (cast(u32)(C) << ARG_C_OFFSET) )

#define SETARG_Bx(ip, Bx)                                                      \
    ( LULU_ASSERT((Bx) <= ARG_Bx_MAX),                                         \
    *(ip) = (*(ip) & ARG_Bx_MASK0)                                             \
          | ((Bx) << ARG_Bx_OFFSET) )

#endif // !LULU_OPCODE_H

#pragma once

#include "lulu.h"
#include "internal.hpp"

#define OPCODE_KINDS(X)                                                        \
    X(Op_None, "<none>", ABC)                                                  \
    X(Op_move, "move",   ABC) /* R(A) := R(B) */                               \
/* Literals                                                                 */ \
    X(Op_bool,    "bool",    vABC) /* R(A).bool := B; if (k) then ip++      */ \
    X(Op_int_imm, "int.imm", AsBx) /* R(A) := int(Bx - offset)              */ \
    X(Op_int_k,   "int.k",   ABx)  /* R(A) := K(Bx).int                     */ \
    X(Op_real,    "real",    ABx)  /* R(A) := K(Bx).real                    */ \
/* Conversion operations                                                    */ \
    X(Op_not,  "not",  ABC) /* R(A) := not R(B).bool                        */ \
    X(Op_cast, "cast", ABC) /* R(A) := cast(B)R(A).(C)                      */ \
/* Arithmetic (1): Integral operations                                      */ \
    X(Op_neg,  "neg", ABC) /* R(A).int := -R(B).int                         */ \
    X(Op_add,  "add", ABC) /* R(A).int := R(B).int +  R(C).int              */ \
    X(Op_sub,  "sub", ABC) /* R(A).int := R(B).int -  R(C).int              */ \
    X(Op_mul,  "mul", ABC) /* R(A).int := R(B).int *  R(C).int              */ \
    X(Op_div,  "div", ABC) /* R(A).int := R(B).int /  R(C).int              */ \
    X(Op_mod,  "mod", ABC) /* R(A).int := R(B).int %  R(C).int              */ \
/* Arithmetic (2): Floating-point operations                                */ \
    X(Op_fneg, "neg.real", ABC)  /* R(A).real := -R(B).real                 */ \
    X(Op_fadd, "add.real", ABC)  /* R(A).real := R(B).real +  R(C).real     */ \
    X(Op_fsub, "sub.real", ABC)  /* R(A).real := R(B).real -  R(C).real     */ \
    X(Op_fmul, "mul.real", ABC)  /* R(A).real := R(B).real *  R(C).real     */ \
    X(Op_fdiv, "div.real", ABC)  /* R(A).real := R(B).real /  R(C).real     */ \
/* Comparison (1): Integral operations                                      */ \
    X(Op_eq,   "eq",       vABC) /* if (R(A).int == R(B).int) == k ip++     */ \
    X(Op_lt,   "lt",       vABC) /* if (R(A).int <  R(B).int) != k ip++     */ \
    X(Op_leq,  "leq",      vABC) /* if (R(A).int <= R(B).int) != k ip++     */ \
/* Comparison (2): Floating-point operations                                */ \
    X(Op_feq,  "eq.real",  vABC) /* if (R(A).real == R(B).real) != k ip++   */ \
    X(Op_flt,  "lt.real",  vABC) /* if (R(A).real <  R(B).real) != k ip++   */ \
    X(Op_fleq, "leq.real", vABC) /* if (R(A).real <= R(B).real) != k ip++   */ \
/* Other */                                                                    \
    X(Op_return, "return", ABC)

enum OpCode : u8 {
#define X(e, s, fmt) e,
    OPCODE_KINDS(X)
#undef X
    // OpCode__COUNT,
};

enum OpForm : u8 {
    OpForm_ABC, OpForm_vABC, OpForm_ABx, OpForm_AsBx,
};

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

#define INVALID_REG         ARG_A_MAX

// Instruction argument bitfield offsets.
#define ARG_OP_OFFSET       0
#define ARG_A_OFFSET        (ARG_OP_OFFSET + ARG_OP_WIDTH)
#define ARG_C_OFFSET        (ARG_A_OFFSET  + ARG_A_WIDTH)
#define ARG_B_OFFSET        (ARG_C_OFFSET  + ARG_C_WIDTH)


// Variant bits
// ABx: Bx (extended B)
#define ARG_Bx_WIDTH        (ARG_B_WIDTH + ARG_C_WIDTH)
#define ARG_Bx_OFFSET       ARG_C_OFFSET
#define ARG_Bx_MAX          ((1 << ARG_Bx_WIDTH) - 1)

// AsBx: sBx (extended B, signed)
#define ARG_sBx_MAX         (ARG_Bx_MAX >> 1)
#define ARG_sBx_MIN         (0 - ARG_sBx_MAX)

// vABC: vC (variant C)
#define ARG_vC_WIDTH        (ARG_C_WIDTH - 1)
#define ARG_vC_MAX          ((1 << ARG_vC_WIDTH) - 1)
#define ARG_vC_OFFSET       ARG_C_OFFSET

// vABC: k (variant flag k)
#define ARG_k_WIDTH         1
#define ARG_k_MAX           ((1 << ARG_k_WIDTH)  - 1)
#define ARG_k_OFFSET        (ARG_vC_OFFSET + ARG_vC_WIDTH)

#define MASK1(max, offset)  (cast(Instruction)(max) << cast(Instruction)(offset))
#define MASK0(max, offset)  (~(MASK1(max, offset)))

// Zero-masks to help clear out the arguments when setting them.
#define ARG_OP_MASK0        MASK0(ARG_OP_MAX, ARG_OP_OFFSET)
#define ARG_A_MASK0         MASK0(ARG_A_MAX,  ARG_A_OFFSET)
#define ARG_B_MASK0         MASK0(ARG_B_MAX,  ARG_B_OFFSET)
#define ARG_C_MASK0         MASK0(ARG_C_MAX,  ARG_C_OFFSET)
#define ARG_Bx_MASK0        MASK0(ARG_Bx_MAX, ARG_Bx_OFFSET)
#define ARG_vC_MASK0        MASK0(ARG_vC_MAX, ARG_vC_OFFSET)
#define ARG_k_MASK0         MASK0(ARG_k_MAX,  ARG_k_OFFSET)

#if (ARG_OP_WIDTH + ARG_A_WIDTH + ARG_B_WIDTH + ARG_C_WIDTH) != 32
#error Instruction width must be 32 exactly.
#endif

/*
 Instruction format:

 +------------------------------------------+
 | tens |0.....|...1....|.....2...|......3..|
 | ones |123456|78901234|567890123|456789012|
 | ABC  |Op....|A.......|C........|B........|
 | vABC |Op....|A.......|C.......k|B........|
 | ABx  |Op....|A.......|Bx.................|
 | AsBx |Op....|A.......|sBx................|
 +------------------------------------------+
 */
using Instruction = u32;

LULU_INTERNAL_FUNC OpForm
opform_get(OpCode op);

#define opcode_is_(op, f) opform_get(op) == (OpForm_##f)
static inline bool
opcode_is_ABC(OpCode op)
{
    return opcode_is_(op, ABC);
}

static inline bool
opcode_is_vABC(OpCode op)
{
    return opcode_is_(op, vABC);
}

static inline bool
opcode_is_ABx(OpCode op)
{
    return opcode_is_(op, ABx);
}

static inline bool
opcode_is_AsBx(OpCode op)
{
    return opcode_is_(op, AsBx);
}
#undef opcode_is_


static inline Instruction
MAKE_ABC(OpCode Op, u8 A, u8 B, u8 C)
{
    return (cast(Instruction)Op << ARG_OP_OFFSET)
        |  (cast(Instruction)A  << ARG_A_OFFSET)
        |  (cast(Instruction)C  << ARG_C_OFFSET)
        |  (cast(Instruction)B  << ARG_B_OFFSET);
}

static inline Instruction
MAKE_vABC(OpCode Op, u8 A, u16 B, u16 vC, bool k)
{
    return (cast(Instruction)Op << ARG_OP_OFFSET)
        |  (cast(Instruction)A  << ARG_A_OFFSET)
        |  (cast(Instruction)vC << ARG_vC_OFFSET)
        |  (cast(Instruction)k  << ARG_k_OFFSET)
        |  (cast(Instruction)B  << ARG_B_OFFSET);
}

static inline Instruction
MAKE_ABx(OpCode Op, u8 A, u32 Bx)
{
    return (cast(Instruction)Op << ARG_OP_OFFSET)
        |  (cast(Instruction)A  << ARG_A_OFFSET)
        |  (cast(Instruction)Bx << ARG_Bx_OFFSET);
}

static inline Instruction
MAKE_AsBx(OpCode Op, u8 A, i32 sBx)
{
    return MAKE_ABx(Op, A, cast(u32)(sBx + ARG_sBx_MAX));
}

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

static inline i32
GETARG_sBx(Instruction i)
{
    return cast(i32)(GETARG_Bx(i) - ARG_sBx_MAX);
}

static inline u16
GETARG_vC(Instruction i)
{
    return cast(u32)((i >> ARG_vC_OFFSET) & ARG_vC_MAX);
}

static inline bool
GETARG_k(Instruction i)
{
    return cast(bool)((i >> ARG_k_OFFSET)  & ARG_k_MAX);
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

#define SETARG_k(ip, k)                                                        \
    ( *(ip) = (*(ip) & ARG_k_MASK0)                                            \
          | (cast(Instruction)(k) << ARG_k_OFFSET) )


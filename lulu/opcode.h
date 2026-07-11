#ifndef LULU_OPCODE_H
#define LULU_OPCODE_H

#include "internal.h"

#define OPCODE_KINDS(X) \
    X(Op_None, "<none>", ABC)                                                  \
/* Literals */                                                                 \
    X(Op_bool,     "bool",     ABC)  /* R(A) := (bool)B       */               \
    X(Op_uint_imm, "uint.imm", ABx)  /* R(A) := (uint)Bx      */               \
    X(Op_uint_k,   "uint.k",   ABx)  /* R(A) := K(Bx).(uint)  */               \
    X(Op_int_imm,  "int.imm",  AsBx) /* R(A) := (int)(Bx - k) */               \
    X(Op_int_k,    "int.k",    ABx)  /* R(A) := K(Bx).(int)   */               \
    X(Op_real,     "real",     ABx)  /* R(A) := K(Bx).(real)  */               \
/* Conversion operations */                                                    \
    X(Op_not, "not", ABC)                                                      \
/* Integer Arithmetic (1): Shared signed and unsigned operations */            \
    X(Op_neg, "neg", ABC) /* R(A) := -R(B)        */                           \
    X(Op_add, "add", ABC) /* R(A) := R(B) + R(C)  */                           \
    X(Op_sub, "sub", ABC) /* R(A) := R(B) - R(C)  */                           \
/* Integer Arithmetic (2): Dedicated signed and unsigned operations */         \
    X(Op_mul,  "mul",      ABC) /* R(A) := R(B) * R(C)  */                     \
    X(Op_div,  "div",      ABC) /* R(A) := R(B) / R(C) */                      \
    X(Op_mod,  "mod",      ABC) /* R(A) := R(B) % R(C) */                      \
    X(Op_imul, "mul.int",  ABC) /* R(A) := R(B) * R(C)  */                     \
    X(Op_idiv, "div.int",  ABC) /* R(A) := R(B) / R(C) */                      \
    X(Op_imod, "mod.int",  ABC) /* R(A) := R(B) % R(C) */                      \
/* Floating-point arithmetic */                                                \
    X(Op_fneg, "neg.real", ABC) /* R(A) := -R(B)       */                      \
    X(Op_fadd, "add.real", ABC) /* R(A) := R(B) + R(C) */                      \
    X(Op_fsub, "sub.real", ABC) /* R(A) := R(B) + R(C) */                      \
    X(Op_fmul, "mul.real", ABC) /* R(A) := R(B) + R(C) */                      \
    X(Op_fdiv, "div.real", ABC) /* R(A) := R(B) + R(C) */                      \
/* Other */                                                                    \
    X(Op_return, "return", ABC)

typedef enum OpCode {
#define X(e, s, fmt) e,
    OPCODE_KINDS(X)
#undef X
} OpCode;

typedef enum OpFormat {
    fABC, fABx, fAsBx,
} OpFormat;

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

#define GET_OPCODE(i)   cast(OpCode)((i >> ARG_OP_OFFSET) & ARG_OP_MAX)
#define GETARG_A(i)     cast(u8) ((i >> ARG_A_OFFSET) & ARG_A_MAX)
#define GETARG_B(i)     cast(u16)((i >> ARG_B_OFFSET) & ARG_B_MAX)
#define GETARG_C(i)     cast(u16)((i >> ARG_C_OFFSET) & ARG_C_MAX)
#define GETARG_Bx(i)    ((i >> ARG_Bx_OFFSET) & ARG_Bx_MAX)
#define GETARG_sBx(i)   cast(i32)(GETARG_Bx(i) - ARG_sBx_MAX)

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

#pragma once

#include "lulu.h"
#include "internal.hpp"

#define OPCODE_KINDS(X)                                                        \
/*    enum   | form | args... |                                             */ \
    X(move,     AB0, 1, Reg)   /* R(A) := R(B)                              */ \
/* Literals                                                                 */ \
    X(bool,    vAB0, 1, Imm)   /* R(A).bool := B; if (k) then ip++          */ \
    X(int_imm, AsBx, 1, Imm)   /* R(A).int  := sBx                          */ \
    X(int_k,    ABx, 1, Const) /* R(A).int  := K(Bx).int                    */ \
    X(real,     ABx, 1, Const) /* R(A).real := K(Bx).real                   */ \
/* Conversion operations                                                    */ \
    X(not,      AB0, 1, Reg) /* R(A).bool := not R(B).bool                  */ \
    X(int2real, AB0, 1, Reg) /* R(A).real := cast(real)R(B).int             */ \
    X(real2int, AB0, 1, Reg) /* R(A).int  := cast(int) R(B).real            */ \
/* Integral operations (1a): register-register bit manipulation             */ \
    X(bnot,     AB0, 1, Reg) /* R(A).int := ~R(B).int                       */ \
    X(band,     ABr, 1, Reg) /* R(A).int :=  R(B).int & R(B).int            */ \
    X(bor,      ABr, 1, Reg) /* R(A).int :=  R(B).int | R(B).int            */ \
    X(bxor,     ABr, 1, Reg) /* R(A).int :=  R(B).int ^ R(B).int            */ \
/* Integral operations (1a): register-register arithmetic                   */ \
    X(neg,      AB0, 1, Reg) /* R(A).int := -R(B).int                       */ \
    X(add,      ABr, 1, Reg) /* R(A).int := R(B).int + R(C).int             */ \
    X(sub,      ABr, 1, Reg) /* R(A).int := R(B).int - R(C).int             */ \
    X(mul,      ABr, 1, Reg) /* R(A).int := R(B).int * R(C).int             */ \
    X(div,      ABr, 1, Reg) /* R(A).int := R(B).int / R(C).int             */ \
    X(mod,      ABr, 1, Reg) /* R(A).int := R(B).int % R(C).int             */ \
/* Integral operations (1b): register-register comparisons                  */ \
    X(eq,      vAB0, 0, Reg) /* if (R(A).int == R(B).int) != k then ip++    */ \
    X(lt,      vAB0, 0, Reg) /* if (R(A).int <  R(B).int) != k then ip++    */ \
    X(leq,     vAB0, 0, Reg) /* if (R(A).int <= R(B).int) != k then ip++    */ \
/* Integral operations (1c): register-immediate bitwise manipulation        */ \
    X(bandi,    ABi, 1, Imm) /* R(A).int :=  R(B).int & C                   */ \
    X(bori,     ABi, 1, Imm) /* R(A).int :=  R(B).int | C                   */ \
    X(bxori,    ABi, 1, Imm) /* R(A).int :=  R(B).int ^ C                   */ \
/* Integral operations (1c): register-immediate arithmetic                  */ \
    X(addi,     ABi, 1, Reg) /* R(A).int := R(B).int + C                    */ \
    X(subi,     ABi, 1, Reg) /* R(A).int := R(B).int - C                    */ \
/* Integral operations (1d): register-immediate comparisons                 */ \
    X(eqi,    vAsBx, 0, Imm) /* if (R(A).int == vsBx) != k then ip++        */ \
    X(lti,    vAsBx, 0, Imm) /* if (R(A).int <  vsBx) != k then ip++        */ \
    X(leqi,   vAsBx, 0, Imm) /* if (R(A).int <= vsBx) != k then ip++        */ \
/* Floating-point operations (2a): register-register arithmetic             */ \
    X(fneg,     AB0, 1, Reg) /* R(A).real := -R(B).real                     */ \
    X(fadd,     ABr, 1, Reg) /* R(A).real := R(B).real + R(C).real          */ \
    X(fsub,     ABr, 1, Reg) /* R(A).real := R(B).real - R(C).real          */ \
    X(fmul,     ABr, 1, Reg) /* R(A).real := R(B).real * R(C).real          */ \
    X(fdiv,     ABr, 1, Reg) /* R(A).real := R(B).real / R(C).real          */ \
    X(fmod,     ABr, 1, Reg) /* R(A).real := R(B).real % R(C).real          */ \
/* Floating-point operations (2b): register-register comparisons            */ \
    X(feq,     vAB0, 0, Reg) /* if (R(A).real == R(B).real) != k then ip++  */ \
    X(flt,     vAB0, 0, Reg) /* if (R(A).real <  R(B).real) != k then ip++  */ \
    X(fleq,    vAB0, 0, Reg) /* if (R(A).real <= R(B).real) != k then ip++  */ \
/* Floating-point operations (2c): register-immediate arithmetic            */ \
    X(faddi,    ABi, 1, Reg) /* R(A).real := R(B).real + C                  */ \
    X(fsubi,    ABi, 1, Reg) /* R(A).real := R(B).real + C                  */ \
/* Floating-point operations (2d): register-register comparisons            */ \
    X(feqi,   vAsBx, 0, Imm) /* if (R(A).real == vsBx) != k then ip++       */ \
    X(flti,   vAsBx, 0, Imm) /* if (R(A).real <  vsBx) != k then ip++       */ \
    X(fleqi,  vAsBx, 0, Imm) /* if (R(A).real <= vsBx) != k then ip++       */ \
/* Other                                                                    */ \
    X(return, AB0, 0, Unused)

enum OpCode : u8 {
#define X(e, ...) Op_##e,
    OPCODE_KINDS(X)
#undef X
    // OpCode__COUNT,
};

static inline OpCode constexpr
operator+(OpCode a, int b)
{
    int e = cast(int)a + b;
    LULU_ASSERT(0 <= e && e <= Op_return);
    return cast(OpCode)e;
}

static inline OpCode constexpr
operator-(OpCode a, int b)
{
    int e = cast(int)a - b;
    LULU_ASSERT(0 <= e && e <= Op_return);
    return cast(OpCode)e;
}

enum OpCode_FormFlag : u8 {
    OpForm_Variant  = (1 << 0), // 001
    OpForm_Extended = (1 << 1), // 010
    OpForm_Signed   = (1 << 2), // 100
};

/*
 Implementation notes:
 1) 'v' means "variant".

 2) 's' means "signed". The argument represents a signed integer in
    excess-K representation (a.k.a. offset binary). Our offset is
    half of the unsigned counterpart's maximum value. Encoding adds
    this offset, whereas decoding subtracts it.

    E.g. sBx is the signed counterpart of the unsigned 18-bit Bx
    argument. Let `max = (2^18) - 1`.

    So to store sBx(0), we encode Bx(0 - max). This allows us
    to store sBx(max) itself as Bx(0).

 3) 'x' means extended.
 */
enum OpCode_Format : u8 {
    OpForm_ABC,
    OpForm_ABx   =                  OpForm_Extended,                 // 010
    OpForm_AsBx  =                  OpForm_Extended | OpForm_Signed, // 110
    OpForm_vABC  = OpForm_Variant,                                   // 001
    OpForm_vABx  = OpForm_Variant | OpForm_Extended,                 // 011
    OpForm_vAsBx = OpForm_Variant | OpForm_Extended | OpForm_Signed, // 111
};

enum OpCode_Arg : u8 {
    OpArg_Unused,
    OpArg_Reg,   // Input-only register, e.g. `B` in `R(A) := R(B)`.
    OpArg_Imm,   // Immediate integer, e.g. Bx in `R(A) := Bx`.
    OpArg_Const, // Index of constant value, e.g. Bx in `R(A) := K(Bx)`.
};

LULU_INTERNAL_FUNC OpCode_Format
OPCODE_INFO_FORMAT(OpCode op);

LULU_INTERNAL_FUNC bool
OPCODE_INFO_A(OpCode op);

LULU_INTERNAL_FUNC OpCode_Arg
OPCODE_INFO_B(OpCode op);

LULU_INTERNAL_FUNC OpCode_Arg
OPCODE_INFO_C(OpCode op);

LULU_INTERNAL_FUNC bool
OPCODE_INFO_k(OpCode op);

/*
 Instruction format, in big-endian form:

 +-------------------------------------------------------------------------+
 | tens  | 3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 |
 | ones  | 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 |
 | ABC   |       C(10)        |     B(8)      |     A(8)      |    Op(6)   |
 | vABC  |       C(9)       |k|     B(8)      |     A(8)      |    Op(6)   |
 | ABx   |               Bx(18)               |     A(8)      |    Op(6)   |
 | AsBx  |           sBx (signed) (18)        |     A(8)      |    Op(6)   |
 | vABx  |
 | vAsBx |
 +-------------------------------------------+
 */
using Instruction = u32;
static Instruction constexpr
// Instruction argument bitfield sizes and limits.
ARG_OP_WIDTH = 6,  ARG_OP_MAX = (1 << ARG_OP_WIDTH) - 1,
ARG_A_WIDTH  = 8,  ARG_A_MAX  = (1 << ARG_A_WIDTH)  - 1,
ARG_B_WIDTH  = 8,  ARG_B_MAX  = (1 << ARG_B_WIDTH)  - 1,
ARG_C_WIDTH  = 10, ARG_C_MAX  = (1 << ARG_C_WIDTH)  - 1,

// Instruction argument bitfield offsets.
ARG_OP_OFFSET = 0,
ARG_A_OFFSET  = ARG_OP_OFFSET + ARG_OP_WIDTH,
ARG_B_OFFSET  = ARG_A_OFFSET  + ARG_A_WIDTH,
ARG_C_OFFSET  = ARG_B_OFFSET  + ARG_B_WIDTH,

// Variant bits
// vABC and vAsBx: flag k
ARG_k_WIDTH  = 1,
ARG_k_MAX    = (1 << ARG_k_WIDTH) - 1,
ARG_k_OFFSET = ARG_C_OFFSET,

// vABC: variant C
ARG_vC_WIDTH  = ARG_C_WIDTH - 1,
ARG_vC_MAX    = (1 << ARG_vC_WIDTH) - 1,
ARG_vC_OFFSET = ARG_k_OFFSET + 1,

// Bx (extended B, unsigned)
ARG_Bx_WIDTH  = ARG_B_WIDTH + ARG_C_WIDTH,
ARG_Bx_OFFSET = ARG_B_OFFSET,
ARG_Bx_MAX    = (1 << ARG_Bx_WIDTH) - 1,

// variant Bx and sBx
ARG_vBx_WIDTH  = (ARG_Bx_WIDTH) - 1,
ARG_vBx_MAX    = ARG_Bx_MAX >> 1,
ARG_vBx_OFFSET = ARG_B_OFFSET;


static inline i32 constexpr
// sBx (extended B, signed)
ARG_sBx_MAX  = cast(i32)ARG_Bx_MAX / 2, ARG_sBx_MIN  = -ARG_sBx_MAX,

// vsBx (variant extended B, signed)
ARG_vsBx_MAX = cast(i32)ARG_Bx_MAX / 2, ARG_vsBx_MIN = -ARG_vsBx_MAX;

#define MASK1(max, offset)  ((max) << (offset))
#define MASK0(max, offset)  (~(MASK1(max, offset)))

static inline Instruction constexpr
// Zero-masks to help clear out the arguments when setting them.
ARG_OP_MASK0  = MASK0(ARG_OP_MAX,  ARG_OP_OFFSET),
ARG_A_MASK0   = MASK0(ARG_A_MAX,   ARG_A_OFFSET),
ARG_B_MASK0   = MASK0(ARG_B_MAX,   ARG_B_OFFSET),
ARG_C_MASK0   = MASK0(ARG_C_MAX,   ARG_C_OFFSET),
ARG_vC_MASK0  = MASK0(ARG_vC_MAX,  ARG_vC_OFFSET),
ARG_k_MASK0   = MASK0(ARG_k_MAX,   ARG_k_OFFSET),
ARG_Bx_MASK0  = MASK0(ARG_Bx_MAX,  ARG_Bx_OFFSET),
ARG_vBx_MASK0 = MASK0(ARG_vBx_MAX, ARG_vBx_OFFSET);

// We reserve this value for agument A as an invalid argument.
static inline u16 constexpr
NO_REG = ARG_A_MAX;

#undef MASK0
#undef MASK1

static_assert(ARG_OP_WIDTH + ARG_A_WIDTH + ARG_B_WIDTH + ARG_C_WIDTH == 32);

#define opcode_is_(op, f) (OPCODE_INFO_FORMAT(op) == (OpForm_##f))
static inline bool opcode_is_ABC  (OpCode op) { return opcode_is_(op, ABC);   }
static inline bool opcode_is_vABC (OpCode op) { return opcode_is_(op, vABC);  }
static inline bool opcode_is_ABx  (OpCode op) { return opcode_is_(op, ABx);   }
static inline bool opcode_is_AsBx (OpCode op) { return opcode_is_(op, AsBx);  }
static inline bool opcode_is_vABx (OpCode op) { return opcode_is_(op, vABx);  }
static inline bool opcode_is_vAsBx(OpCode op) { return opcode_is_(op, vAsBx); }
#undef opcode_is_

static inline Instruction
MAKE_ABC(OpCode Op, u16 A, u16 B, u16 C)
{
    return (cast(Instruction)Op << ARG_OP_OFFSET)
        |  (cast(Instruction)A  << ARG_A_OFFSET)
        |  (cast(Instruction)B  << ARG_B_OFFSET)
        |  (cast(Instruction)C  << ARG_C_OFFSET);
}

static inline Instruction
MAKE_vABC(OpCode Op, u16 A, u16 B, u16 vC, bool k)
{
    return (cast(Instruction)Op << ARG_OP_OFFSET)
        |  (cast(Instruction)A  << ARG_A_OFFSET)
        |  (cast(Instruction)B  << ARG_B_OFFSET)
        |  (cast(Instruction)k  << ARG_k_OFFSET) 
        |  (cast(Instruction)vC << ARG_vC_OFFSET);
}

static inline Instruction
MAKE_ABx(OpCode Op, u16 A, u32 Bx)
{
    return (cast(Instruction)Op << ARG_OP_OFFSET)
        |  (cast(Instruction)A  << ARG_A_OFFSET)
        |  (cast(Instruction)Bx << ARG_Bx_OFFSET);
}

static inline Instruction
MAKE_AsBx(OpCode Op, u16 A, i32 sBx)
{
    u32 Bx = cast(u32)(sBx + ARG_sBx_MAX);
    return MAKE_ABx(Op, cast(u8)A, Bx);
}

static inline Instruction
MAKE_AvBx(OpCode Op, u16 A, u32 vBx, bool k)
{
    return (cast(Instruction)Op  << ARG_OP_OFFSET)
        |  (cast(Instruction)A   << ARG_A_OFFSET)
        |  (cast(Instruction)vBx << ARG_vBx_OFFSET)
        |  (cast(Instruction)k   << ARG_k_OFFSET);
}

static inline Instruction
MAKE_AvsBx(OpCode Op, u16 A, i32 vsBx, bool k)
{
    return MAKE_AvBx(Op, cast(u8)A, cast(u32)(vsBx + ARG_vsBx_MAX), k);
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

static inline u8
GETARG_B(Instruction i)
{
    return cast(u16)((i >> ARG_B_OFFSET) & ARG_B_MAX);
}

static inline u16
GETARG_C(Instruction i)
{
    return cast(u16)((i >> ARG_C_OFFSET) & ARG_C_MAX);
}

static inline bool
GETARG_k(Instruction i)
{
    return cast(bool)((i >> ARG_k_OFFSET) & ARG_k_MAX);
}

static inline u16
GETARG_vC(Instruction i)
{
    return cast(u16)((i >> ARG_vC_OFFSET) & ARG_vC_MAX);
}

static inline u32
GETARG_Bx(Instruction i)
{
    return cast(u32)((i >> ARG_Bx_OFFSET) & ARG_Bx_MAX);
}

static inline i32
GETARG_sBx(Instruction i)
{
    return cast(i32)(GETARG_Bx(i)) - ARG_sBx_MAX;
}

static inline u32
GETARG_vBx(Instruction i)
{
    return cast(u32)((i >> ARG_vBx_OFFSET) & ARG_vBx_MAX);
}

static inline i32
GETARG_vsBx(Instruction i)
{
    return cast(i32)(GETARG_vBx(i) - ARG_vBx_MAX);
}

static inline void
SET_OPCODE(Instruction *ip, OpCode Op)
{
    *ip = (*ip & ARG_OP_MASK0) | (cast(Instruction)Op << ARG_OP_OFFSET);
}

static inline void
SETARG_A(Instruction *ip, u16 A)
{
    *ip = (*ip & ARG_A_MASK0) | (cast(Instruction)A << ARG_A_OFFSET);
}

static inline void
SETARG_B(Instruction *ip, u16 B)
{
    *ip = (*ip & ARG_B_MASK0) | (cast(Instruction)B << ARG_B_OFFSET);
}

static inline void
SETARG_C(Instruction *ip, u16 B)
{
    *ip = (*ip & ARG_C_MASK0) | (cast(Instruction)B << ARG_C_OFFSET);
}

static inline void
SETARG_k(Instruction *ip, bool k)
{
    *ip = (*ip & ARG_k_MASK0) | (cast(Instruction)k << ARG_k_OFFSET);
}

static inline void
SETARG_Bx(Instruction *ip, u32 B)
{
    *ip = (*ip & ARG_Bx_MASK0) | (cast(Instruction)B << ARG_Bx_OFFSET);
}

static inline void
SETARG_sBx(Instruction *ip, i32 sBx)
{
    SETARG_Bx(ip, cast(u32)(sBx + ARG_sBx_MAX));
}

#define SETARG_SAFE(ip, arg_name, arg_value)                                   \
do {                                                                           \
    Instruction *_ip = (ip);                                                   \
    OpCode       _op = GET_OPCODE(*_ip);                                       \
    LULU_ASSERT(OPCODE_INFO_##arg_name(_op));                                  \
    SETARG_##arg_name(_ip, arg_value);                                         \
} while (0)

#define SETARG_k(ip, arg)   SETARG_SAFE(ip, k,   arg)
#define SETARG_Bx(ip, arg)  SETARG_SAFE(ip, Bx,  arg)
#define SETARG_sBx(ip, arg) SETARG_SAFE(ip, sBx, arg)


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
    X(not,      AB0, 1, Reg)   /* R(A).bool := not R(B).bool                */ \
    X(int2real, AB0, 1, Reg)   /* R(A).real := cast(real)R(B).int           */ \
    X(real2int, AB0, 1, Reg)   /* R(A).int  := cast(int) R(B).real          */ \
/* Integral operations (1a): register-register bit manipulation             */ \
    X(bnot,     AB0, 1, Reg)   /* R(A).int := ~R(B).int                     */ \
    X(band,     ABr, 1, Reg)   /* R(A).int :=  R(B).int & R(B).int          */ \
    X(bor,      ABr, 1, Reg)   /* R(A).int :=  R(B).int | R(B).int          */ \
    X(bxor,     ABr, 1, Reg)   /* R(A).int :=  R(B).int ^ R(B).int          */ \
/* Integral operations (1b): register-register arithmetic                   */ \
    X(neg,      AB0, 1, Reg)   /* R(A).int := -R(B).int                     */ \
    X(add,      ABr, 1, Reg)   /* R(A).int := R(B).int + R(C).int           */ \
    X(sub,      ABr, 1, Reg)   /* R(A).int := R(B).int - R(C).int           */ \
    X(mul,      ABr, 1, Reg)   /* R(A).int := R(B).int * R(C).int           */ \
    X(div,      ABr, 1, Reg)   /* R(A).int := R(B).int / R(C).int           */ \
    X(mod,      ABr, 1, Reg)   /* R(A).int := R(B).int % R(C).int           */ \
/* Integral operations (1c): register-immediate bitwise manipulation        */ \
    X(bandi,    ABi, 1, Imm)   /* R(A).int := R(B).int & C                  */ \
    X(bori,     ABi, 1, Imm)   /* R(A).int := R(B).int | C                  */ \
    X(bxori,    ABi, 1, Imm)   /* R(A).int := R(B).int ^ C                  */ \
/* Integral operations (1d): register-immediate arithmetic                  */ \
    X(addi,     ABi, 1, Reg)   /* R(A).int := R(B).int + C                  */ \
    X(subi,     ABi, 1, Reg)   /* R(A).int := R(B).int - C                  */ \
/* Integral operations (1e): register-constant bitwise                      */ \
    X(bandk,    ABk, 1, Reg)   /* R(A).int :=  R(B).int & K(B).int          */ \
    X(bork,     ABk, 1, Reg)   /* R(A).int :=  R(B).int | K(B).int          */ \
    X(bxork,    ABk, 1, Reg)   /* R(A).int :=  R(B).int ^ K(B).int          */ \
/* Integral operations (1f): register-constant arithmetic                   */ \
    X(addk,     ABk, 1, Reg)   /* R(A).int := R(B).int + K(C).int           */ \
    X(subk,     ABk, 1, Reg)   /* R(A).int := R(B).int - K(C).int           */ \
    X(mulk,     ABk, 1, Reg)   /* R(A).int := R(B).int * K(C).int           */ \
    X(divk,     ABk, 1, Reg)   /* R(A).int := R(B).int / K(C).int           */ \
    X(modk,     ABk, 1, Reg)   /* R(A).int := R(B).int % K(C).int           */ \
/* Floating-point operations (2a): register-register arithmetic             */ \
    X(fneg,     AB0, 1, Reg)   /* R(A).real := -R(B).real                   */ \
    X(fadd,     ABr, 1, Reg)   /* R(A).real := R(B).real + R(C).real        */ \
    X(fsub,     ABr, 1, Reg)   /* R(A).real := R(B).real - R(C).real        */ \
    X(fmul,     ABr, 1, Reg)   /* R(A).real := R(B).real * R(C).real        */ \
    X(fdiv,     ABr, 1, Reg)   /* R(A).real := R(B).real / R(C).real        */ \
    X(fmod,     ABr, 1, Reg)   /* R(A).real := R(B).real % R(C).real        */ \
/* Floating-point operations (2b): register-immediate arithmetic            */ \
    X(faddi,    ABi, 1, Reg)   /* R(A).real := R(B).real + C                */ \
    X(fsubi,    ABi, 1, Reg)   /* R(A).real := R(B).real + C                */ \
/* Floating-point operations (2c): register-constant arithmetic             */ \
    X(faddk,    ABk, 1, Reg)   /* R(A).real := R(B).real + K(C).real        */ \
    X(fsubk,    ABk, 1, Reg)   /* R(A).real := R(B).real - K(C).real        */ \
    X(fmulk,    ABk, 1, Reg)   /* R(A).real := R(B).real * K(C).real        */ \
    X(fdivk,    ABk, 1, Reg)   /* R(A).real := R(B).real / K(C).real        */ \
    X(fmodk,    ABk, 1, Reg)   /* R(A).real := R(B).real % K(C).real        */ \
/* Integral operations (3a): register-register comparisons                  */ \
    X(eq,      vAB0, 0, Reg)   /* if R(A).int == R(B).int != k then ip++    */ \
    X(lt,      vAB0, 0, Reg)   /* if R(A).int <  R(B).int != k then ip++    */ \
    X(leq,     vAB0, 0, Reg)   /* if R(A).int <= R(B).int != k then ip++    */ \
/* Integral operations (3b): register-immediate comparisons                 */ \
    X(eqi,     vAB0, 0, Imm)   /* if R(A).int == B != k then ip++           */ \
    X(lti,     vAB0, 0, Imm)   /* if R(A).int <  B != k then ip++           */ \
    X(leqi,    vAB0, 0, Imm)   /* if R(A).int <= B != k then ip++           */ \
/* Integral operations (3c): register-constant comparisons                  */ \
    X(eqk,     vAB0, 0, Const) /* if R(A).int == K(B).int != k then ip++    */ \
    X(ltk,     vAB0, 0, Const) /* if R(A).int <  K(B).int != k then ip++    */ \
    X(leqk,    vAB0, 0, Const) /* if R(A).int <= K(B).int != k then ip++    */ \
/* Floating-point operations (4a): register-register comparisons            */ \
    X(feq,     vAB0, 0, Reg)   /* if R(A).real == R(B).real != k then ip++  */ \
    X(flt,     vAB0, 0, Reg)   /* if R(A).real <  R(B).real != k then ip++  */ \
    X(fleq,    vAB0, 0, Reg)   /* if R(A).real <= R(B).real != k then ip++  */ \
/* Floating-point operations (4b): register-immediate comparisons           */ \
    X(feqi,    vAB0, 0, Imm)   /* if R(A).real == B != k then ip++          */ \
    X(flti,    vAB0, 0, Imm)   /* if R(A).real <  B != k then ip++          */ \
    X(fleqi,   vAB0, 0, Imm)   /* if R(A).real <= B != k then ip++          */ \
/* Floating-point operations (4c): register-immediate comparisons           */ \
    X(feqk,    vAB0, 0, Const) /* if R(A).real == R(B).real != k then ip++  */ \
    X(fltk,    vAB0, 0, Const) /* if R(A).real <  R(B).real != k then ip++  */ \
    X(fleqk,   vAB0, 0, Const) /* if R(A).real <= R(B).real != k then ip++  */ \
/* Other                                                                    */ \
    X(return0, AB0, 0, Unused) /* return                                    */ \
    X(return,  AB0, 0, Imm)    /* return R(A:B) if B > 0 else R(A:)         */

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

enum OpFormFlag : u8 {
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
enum OpForm : u8 {
    OpForm_ABC,
    OpForm_ABx   = OpForm_Extended,
    OpForm_AsBx  = OpForm_Extended | OpForm_Signed,
    OpForm_vABC  = OpForm_Variant,
};

enum OpArg : u8 {
    OpArg_Unused,
    OpArg_Reg,   // Input-only register, e.g. `B` in `R(A) := R(B)`.
    OpArg_Imm,   // Immediate integer, e.g. Bx in `R(A) := Bx`.
    OpArg_Const, // Index of constant value, e.g. Bx in `R(A) := K(Bx)`.
};

struct OpCodeInfo {
    OpForm FORMAT;
    bool   A;
    OpArg  B;
    OpArg  C;
    bool   k;
};

LULU_INTERNAL_FUNC OpCodeInfo
opcode_info(OpCode op);

struct InstructionInfo {
    u8  const WIDTH;
    u8  const OFFSET;
    i32 MIN;
    u32 MAX;


    // One-masks help us read just the argument.
    // Zero-masks help clear the arguments so we can overwrite them.
    u32 const MASK1, MASK0;

    constexpr
    InstructionInfo(u8 width, u8 offset)
        : WIDTH {width}
        , OFFSET{offset}
        , MIN   {0}
        , MAX   {cast(u32)(1 << width) - 1}
        , MASK1 {MAX << offset}
        , MASK0 {~MASK1}
    {}

    constexpr
    InstructionInfo(u8 width, InstructionInfo prev)
        : InstructionInfo(width, prev.OFFSET + prev.WIDTH)
    {}

    constexpr
    InstructionInfo(InstructionInfo prev, int)
        : InstructionInfo(prev.WIDTH, prev.OFFSET)
    {
        this->MAX = prev.MAX / 2;
        this->MIN = -cast(i32)this->MAX;
    }

    constexpr InstructionInfo
    operator+(InstructionInfo other) const
    {
        u8 width = this->WIDTH + other.WIDTH;
        return InstructionInfo(width, /*offset=*/this->OFFSET);
    }
};

static InstructionInfo constexpr
// Instruction argument bitfield sizes and limits.
ARG_OP {7,                     0},
ARG_A  {8,                     ARG_OP},
ARG_B  {8,                     ARG_A},
ARG_C  {9,                     ARG_B},
ARG_k  {1,                     ARG_B}, // vABC: flag k
ARG_vC {ARG_C.WIDTH - 1,       ARG_k}, // vABC: variant C
ARG_Bx {(ARG_B + ARG_C).WIDTH, ARG_A},
ARG_sBx{ARG_Bx, -1};

// We reserve this value for agument A as an invalid argument.
static inline u16 constexpr
REG_NONE = ARG_A.MAX;

static_assert(ARG_OP.WIDTH + ARG_A.WIDTH + ARG_B.WIDTH + ARG_C.WIDTH == 32);

#define opcode_is_(op, f) (opcode_info(op).FORMAT == (OpForm_##f))
static inline bool opcode_is_ABC (OpCode op) { return opcode_is_(op, ABC);   }
static inline bool opcode_is_vABC(OpCode op) { return opcode_is_(op, vABC);  }
static inline bool opcode_is_ABx (OpCode op) { return opcode_is_(op, ABx);   }
static inline bool opcode_is_AsBx(OpCode op) { return opcode_is_(op, AsBx);  }
#undef opcode_is_

/*
 Instruction format, in big-endian form:

 +-------------------------------------------------------------------------+
 | tens  | 3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 |
 | ones  | 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 |
 | ABC   |       C(9)       |     B(8)      |     A(8)      |     Op(7)    |
 | vABC  |       C(8)     |k|     B(8)      |     A(8)      |     Op(7)    |
 | ABx   |               Bx(17)             |     A(8)      |     Op(7)    |
 | AsBx  |          sBx (signed) (17)       |     A(8)      |     Op(7)    |
 +-------------------------------------------------------------------------+
 */
struct Instruction {
    u32 data;

    static inline Instruction
    ABC(OpCode Op, u16 A, u16 B, u16 C)
    {
        return {(cast(u32)Op << ARG_OP.OFFSET)
            |   (cast(u32)A  << ARG_A.OFFSET)
            |   (cast(u32)B  << ARG_B.OFFSET)
            |   (cast(u32)C  << ARG_C.OFFSET)};
    }

    static inline Instruction
    vABC(OpCode Op, u16 A, u16 B, u16 vC, bool k)
    {
        return {(cast(u32)Op << ARG_OP.OFFSET)
            |   (cast(u32)A  << ARG_A.OFFSET)
            |   (cast(u32)B  << ARG_B.OFFSET)
            |   (cast(u32)k  << ARG_k.OFFSET) 
            |   (cast(u32)vC << ARG_vC.OFFSET)};
    }

    static inline Instruction
    ABx(OpCode Op, u16 A, u32 Bx)
    {
        return {(cast(u32)Op << ARG_OP.OFFSET)
            |   (cast(u32)A  << ARG_A.OFFSET)
            |   (cast(u32)Bx << ARG_Bx.OFFSET)};
    }

    static inline Instruction
    AsBx(OpCode Op, u16 A, i32 sBx)
    {
        return ABx(Op, A, cast(u32)(sBx + cast(i32)ARG_sBx.MAX));
    }

    inline OpCode Op() const { return this->get_arg<OpCode>(ARG_OP); }
    inline u8     A () const { return this->get_arg<u8>    (ARG_A);  }
    inline u8     B () const { return this->get_arg<u8>    (ARG_B);  }
    inline u16    C () const { return this->get_arg<u16>   (ARG_C);  }
    inline bool   k () const { return this->get_arg<bool>  (ARG_k);  }
    inline u16   vC () const { return this->get_arg<u16>   (ARG_vC); }
    inline u32    Bx() const { return this->get_arg<u32>   (ARG_Bx); }
    inline i32   sBx() const { return cast(i32)this->Bx() - cast(i32)ARG_sBx.MAX; }

    template<class T>
    inline T
    get_arg(InstructionInfo info) const
    {
        return cast(T)((this->data >> cast(u32)info.OFFSET) & info.MAX);
    }
    
    template<class T>
    inline void
    set_arg(T arg, InstructionInfo info)
    {
        this->data = (this->data & info.MASK0) | (cast(u32)arg << info.OFFSET);
    }

    // inline void  Op(OpCode Op) { this->set_arg(Op, ARG_OP); }
    inline void  A (u16    A)  { this->set_arg(A,  ARG_A);  }
    inline void  B (u16    B)  { this->set_arg(B,  ARG_B);  }
    inline void  C (u16    C)  { this->set_arg(C,  ARG_C);  }
    inline void  k (bool   k)  { this->set_arg(k,  ARG_k);  }
    inline void  Bx(u32    Bx) { this->set_arg(Bx, ARG_Bx); }
    inline void sBx(i32   sBx) { this->Bx(cast(u32)(sBx + ARG_sBx.MAX)); }
};


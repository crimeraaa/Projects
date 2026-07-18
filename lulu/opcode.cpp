#include "opcode.hpp"

/*
 Opcode information format, in big-endian form:

 | tens | 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 |
 | ones | 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 |
 |------|----------|C(3) |B(3) |A|k| F(3) |
 */
using OpCode_Info = u8;

static inline u8 constexpr
OPCODE_INFO_F_WIDTH  = 3, OPCODE_INFO_F_MAX = (1 << OPCODE_INFO_F_WIDTH) - 1,
OPCODE_INFO_k_WIDTH  = 1, OPCODE_INFO_k_MAX = (1 << OPCODE_INFO_k_WIDTH) - 1,
OPCODE_INFO_A_WIDTH  = 1, OPCODE_INFO_A_MAX = (1 << OPCODE_INFO_A_WIDTH) - 1,
OPCODE_INFO_B_WIDTH  = 4, OPCODE_INFO_B_MAX = (1 << OPCODE_INFO_B_WIDTH) - 1,
OPCODE_INFO_C_WIDTH  = 4, OPCODE_INFO_C_MAX = (1 << OPCODE_INFO_C_WIDTH) - 1,

OPCODE_INFO_F_OFFSET = 0, 
OPCODE_INFO_k_OFFSET = OPCODE_INFO_F_OFFSET + OPCODE_INFO_F_WIDTH,
OPCODE_INFO_A_OFFSET = OPCODE_INFO_k_OFFSET + OPCODE_INFO_k_WIDTH,
OPCODE_INFO_B_OFFSET = OPCODE_INFO_A_OFFSET + OPCODE_INFO_A_WIDTH,
OPCODE_INFO_C_OFFSET = OPCODE_INFO_B_OFFSET + OPCODE_INFO_B_WIDTH;

[[nodiscard]] static constexpr OpCode_Info
opcode_info_make(OpCode_Format F, bool A, OpCode_Arg B, OpCode_Arg C, bool k)
{
    return (cast(OpCode_Info)F << OPCODE_INFO_F_OFFSET)
        |  (cast(OpCode_Info)k << OPCODE_INFO_k_OFFSET)
        |  (cast(OpCode_Info)A << OPCODE_INFO_A_OFFSET)
        |  (cast(OpCode_Info)B << OPCODE_INFO_B_OFFSET)
        |  (cast(OpCode_Info)C << OPCODE_INFO_C_OFFSET);
}

static constexpr OpCode_Info
opcode_info_make_ABC(bool A, OpCode_Arg B, OpCode_Arg C)
{
    return opcode_info_make(OpForm_ABC, A, B, C, false);
}

static constexpr OpCode_Info
opcode_info_make_vABC(bool A, OpCode_Arg B, OpCode_Arg vC)
{
    return opcode_info_make(OpForm_vABC, A, B, vC, true);
}

static constexpr OpCode_Info
opcode_info_make_ABx(bool A, OpCode_Arg Bx)
{
    return opcode_info_make(OpForm_ABx, A, Bx, OpArg_Unused, false);
}

static constexpr OpCode_Info
opcode_info_make_AsBx(bool A, OpCode_Arg sBx)
{
    return opcode_info_make(OpForm_AsBx, A, sBx, OpArg_Unused, false);
}

static constexpr OpCode_Info
opcode_info_make_vAsBx(bool A, OpCode_Arg vsBx)
{
    return opcode_info_make(OpForm_vAsBx, A, vsBx, OpArg_Unused, true);
}

#define OpArg_(T)       OpArg_ ## T
#define ABC(A, B, C)    opcode_info_make_ABC(A, OpArg_(B), OpArg_(C))
#define AB0(A, B)       ABC(A, B, Unused)
#define ABr(A, B)       ABC(A, B, Reg)
#define ABi(A, B)       ABC(A, B, Imm)

#define vABC(A, B, vC)  opcode_info_make_vABC(A, OpArg_(B), OpArg_(vC))
#define vAB0(A, B)      vABC(A, B, Unused)

#define ABx(A, Bx)      opcode_info_make_ABx  (A, OpArg_(Bx))
#define AsBx(A, sBx)    opcode_info_make_AsBx (A, OpArg_(sBx))
#define vAsBx(A, vsBx)  opcode_info_make_vAsBx(A, OpArg_(vsBx))

static OpCode_Info
opcode_info(OpCode op)
{
    /*
     RANT(2026-07-19): What the hell is MSVC doing?

     `X2(e, F, ...)` because for some reason the `__VA_ARGS__` doesn't expand
     and/or token-paste properly, resulting in function calls with `Reg`, for
     example, which end up error-ing out as undefined identifiers.

     This works `X2(e, F, A, B)` but it's a pain to maange as we don't
     necessarily want to limit ourselves to two (2) arguments. Or mayb we do?
     */
#define X2(e, F, A, B) case Op_##e: return F(A, B);
    switch (op) {
    OPCODE_KINDS(X2)
    }
#undef X2
    LULU_UNREACHABLE();
    return 0;
}

#undef vAsBx
#undef AsBx
#undef ABx
#undef vAB0
#undef vABC
#undef ABi
#undef ABr
#undef AB0
#undef ABC
#undef OpArg_

#define opcode_info(op, arg) \
( (opcode_info(op) >> OPCODE_INFO_##arg##_OFFSET) & OPCODE_INFO_##arg##_MAX )

LULU_INTERNAL_FUNC OpCode_Format
OPCODE_INFO_FORMAT(OpCode op)
{
    return cast(OpCode_Format)opcode_info(op, F);
}

LULU_INTERNAL_FUNC bool
OPCODE_INFO_A(OpCode op)
{
    return cast(bool)opcode_info(op, A);
}

LULU_INTERNAL_FUNC OpCode_Arg
OPCODE_INFO_B(OpCode op)
{
    return cast(OpCode_Arg)opcode_info(op, B);
}

LULU_INTERNAL_FUNC OpCode_Arg
OPCODE_INFO_C(OpCode op)
{
    return cast(OpCode_Arg)opcode_info(op, A);
}

LULU_INTERNAL_FUNC bool
OPCODE_INFO_k(OpCode op)
{
    return cast(bool)opcode_info(op, k);
}

#undef opcode_info


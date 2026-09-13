#include "opcode.hpp"

#define ABC( A, B, C)  {OpForm_ABC,  A, B,   C,            /*k=*/false}
#define ABx( A, Bx)    {OpForm_ABx,  A, Bx,  OpArg_Unused, /*k=*/false}
#define AsBx(A, sBx)   {OpForm_AsBx, A, sBx, OpArg_Unused, /*k=*/false}
#define vABC(A, B, vC) {OpForm_vABC, A, B,   vC,           /*k=*/true }

#define AB0( A, B)     ABC(A, B, /*C =*/OpArg_Unused)
#define ABr( A, B)     ABC(A, B, /*C =*/OpArg_Reg)
#define ABi( A, B)     ABC(A, B, /*C =*/OpArg_Imm)
#define ABk( A, B)     ABC(A, B, /*C =*/OpArg_Const)
#define vAB0(A, B)    vABC(A, B, /*vC=*/OpArg_Unused) 

LULU_INTERNAL_FUNC OpCodeInfo
opcode_info(OpCode op)
{
    /*
     RANT(2026-07-19): What the hell is MSVC doing?

     `X2(e, F, ...)` because for some reason the `__VA_ARGS__` doesn't expand
     and/or token-paste properly, resulting in function calls with `Reg`, for
     example, which end up error-ing out as undefined identifiers.

     This works `X2(e, F, A, B)` but it's a pain to maange as we don't
     necessarily want to limit ourselves to two (2) arguments. Or maybe we do?
     */
#define X2(e, F, A, B) case Op_##e: return F(A, OpArg_##B);
    switch (op) {
    OPCODE_KINDS(X2)
    }
#undef X2
    LULU_UNREACHABLE();
    return {};
}

#undef vAB0
#undef ABk
#undef ABi
#undef Abr
#undef AB0

#undef vABC
#undef AsBx
#undef ABx
#undef ABC

#include "vm.h"
#include "opcode.h"
#include "debug.h"

#define B   GETARG_B(i)
#define C   GETARG_C(i)
#define Bx  GETARG_Bx(i)
#define sBx GETARG_sBx(i)
#define Op_cast_stuff(T, dst)                                                  \
    switch (cast(ValueKind)C) {                                                \
    case Value_bool: dst(*RA) = cast(T)RA->b; break;                           \
    case Value_int:  dst(*RA) = cast(T)RA->i; break;                           \
    case Value_uint: dst(*RA) = cast(T)RA->u; break;                           \
    case Value_real: dst(*RA) = cast(T)RA->f; break;                           \
    default:                                                                   \
        LULU_UNREACHABLE();                                                    \
        break;                                                                 \
    }

LULU_INTERNAL_FUNC void
vm_execute(lulu_State *L, Chunk *c)
{
    Value        R[ARG_A_MAX];
    Instruction *ip = c->code;
    TValue *     K  = c->values;
    printf("======== EXECUTION ========\n");
    for (;;) {
        Instruction i  = *ip++;
        Value      *RA = &R[GETARG_A(i)];
        debug_disassemble_at(c, ip - c->code - 1);
        switch (GET_OPCODE(i)) {
        case Op_None: LULU_UNREACHABLE();
        case Op_bool: 
            RA->b = cast(bool)GETARG_B(i);
            if (cast(bool)GETARG_C(i)) {
                ip++;
            }
            break;
        case Op_uint_imm: RA->u = Bx;  break;
        case Op_int_imm:  RA->i = sBx; break;
        case Op_uint_k:   RA->u = tvalue_uint(K[Bx]); break;
        case Op_int_k:    RA->i = tvalue_int (K[Bx]); break;
        case Op_real:     RA->f = tvalue_real(K[Bx]); break;
        case Op_not:      RA->b = !value_bool(R[B]);  break;
        case Op_cast: {
            switch (cast(ValueKind)B) {
            case Value_bool: Op_cast_stuff(bool,      value_bool); break;
            case Value_uint: Op_cast_stuff(lulu_uint, value_uint); break;
            case Value_int:  Op_cast_stuff(lulu_int,  value_int);  break;
            case Value_real: Op_cast_stuff(lulu_real, value_real); break;
            default:
                LULU_UNREACHABLE();
                break;
            }
            break;
        }

        // Arithmetic
#define arith_unary(X, op)   X(*RA) = op X(R[B]);         break
#define arith_binary(X, op)  X(*RA) = X(R[B]) op X(R[C]); break
        case Op_neg:  arith_unary (value_int,  -);
        case Op_add:  arith_binary(value_uint, +);
        case Op_sub:  arith_binary(value_uint, -);
        case Op_mul:  arith_binary(value_uint, *);
        case Op_div:  arith_binary(value_uint, /);
        case Op_mod:  arith_binary(value_uint, %);
        case Op_imul: arith_binary(value_int,  *);
        case Op_idiv: arith_binary(value_int,  /);
        case Op_imod: arith_binary(value_int,  %);
        case Op_fneg: arith_unary (value_real, -);
        case Op_fadd: arith_binary(value_real, +);
        case Op_fsub: arith_binary(value_real, -);
        case Op_fmul: arith_binary(value_real, *);
        case Op_fdiv: arith_binary(value_real, /);
#undef arith_binary
#undef arith_unary

        // Comparison
#define compare(X, op)  value_bool(*RA) = X(R[B]) op X(R[C]); break
        case Op_eq:   compare(value_uint, ==);
        case Op_lt:   compare(value_uint, < );
        case Op_leq:  compare(value_uint, <=);
        case Op_ilt:  compare(value_int,  < );
        case Op_ileq: compare(value_int,  <=);
        case Op_feq:  compare(value_real, ==);
        case Op_flt:  compare(value_real, < );
        case Op_fleq: compare(value_real, <=);
#undef compare

        case Op_return:
            printf("===========================\n");
            return;
        }
    }
}

#undef Op_cast_stuff
#undef sBx
#undef Bx
#undef C
#undef B

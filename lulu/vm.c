#include "vm.h"
#include "opcode.h"
#include "debug.h"
#include "value.h"

#define RB(i)   R[GETARG_B(i)]
#define RC(i)   R[GETARG_C(i)]
#define KBx(i)  K[GETARG_Bx(i)]
#define Op_cast_type(T)                                                        \
    switch (cast(ValueKind)GETARG_C(i)) {                                      \
    case Value_bool: value_set_##T(RA, cast(lulu_##T)value_bool(*RA)); break;  \
    case Value_int:  value_set_##T(RA, cast(lulu_##T)value_int (*RA)); break;  \
    case Value_uint: value_set_##T(RA, cast(lulu_##T)value_uint(*RA)); break;  \
    case Value_real: value_set_##T(RA, cast(lulu_##T)value_real(*RA)); break;  \
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
        case Op_move: *RA = RB(i); break;
        case Op_bool: 
            value_set_bool(RA, cast(bool)GETARG_B(i));
            if (GETARG_k(i)) {
                ip++;
            }
            break;
        case Op_uint_imm: value_set_uint(RA, GETARG_Bx(i));         break;
        case Op_int_imm:  value_set_int (RA, GETARG_sBx(i));        break;
        // TODO(2026-07-13): Can we just copy the union directly?
        case Op_uint_k:   value_set_uint(RA, tvalue_uint(KBx(i)) ); break;
        case Op_int_k:    value_set_int (RA, tvalue_int (KBx(i)) ); break;
        case Op_real:     value_set_real(RA, tvalue_real(KBx(i)) ); break;
        case Op_not:      value_set_bool(RA, !value_bool(RB(i))  ); break;

        case Op_cast: {
            switch (cast(ValueKind)GETARG_B(i)) {
            case Value_bool: Op_cast_type(bool);  break;
            case Value_uint: Op_cast_type(uint);  break;
            case Value_int:  Op_cast_type(int);   break;
            case Value_real: Op_cast_type(real);  break;
            default:
                LULU_UNREACHABLE();
                break;
            }
            break;
        }

// Arithmetic
#define binary_ABC(T, op)   value_##T(RB(i)) op value_##T(RC(i))
#define arith_unary(T, op)  value_set_##T(RA, op value_##T(RB(i)) ); break
#define arith_binary(T, op) value_set_##T(RA, binary_ABC(T, op)   ); break
        case Op_neg:  arith_unary (int,  -);
        case Op_add:  arith_binary(uint, +);
        case Op_sub:  arith_binary(uint, -);
        case Op_mul:  arith_binary(uint, *);
        case Op_div:  arith_binary(uint, /);
        case Op_mod:  arith_binary(uint, %);
        case Op_imul: arith_binary(int,  *);
        case Op_idiv: arith_binary(int,  /);
        case Op_imod: arith_binary(int,  %);
        case Op_fneg: arith_unary (real, -);
        case Op_fadd: arith_binary(real, +);
        case Op_fsub: arith_binary(real, -);
        case Op_fmul: arith_binary(real, *);
        case Op_fdiv: arith_binary(real, /);
#undef arith_binary
#undef arith_unary

// Comparison
#define binary_vABC(T, op)  value_##T(RB(i)) op value_##T(R[GETARG_vC(i)])
#define compare(T, op)      if (binary_vABC(T, op) != GETARG_k(i)) { ip++; }; break;
        case Op_eq:   compare(uint, ==);
        case Op_lt:   compare(uint, < );
        case Op_leq:  compare(uint, <=);
        case Op_ilt:  compare(int,  < );
        case Op_ileq: compare(int,  <=);
        case Op_feq:  compare(real, ==);
        case Op_flt:  compare(real, < );
        case Op_fleq: compare(real, <=);
#undef compare
#undef binary_get

        case Op_return:
            printf("===========================\n");
            return;
        }
    }
}

#undef Op_cast_type
#undef KBx
#undef RC
#undef RB

#include "vm.hpp"
#include "opcode.hpp"
#include "debug.hpp"
#include "value.hpp"

#define RB(i)   R[GETARG_B(i)]
#define RC(i)   R[GETARG_C(i)]
#define KBx(i)  K[GETARG_Bx(i)]

template<class T>
static inline void
vm_cast(Value *a, Instruction i)
{
    switch (cast(ValueKind)GETARG_C(i)) {
    case Value_bool: value_set(a, cast(T)value_bool(*a)); break;
    case Value_int:  value_set(a, cast(T)value_int (*a)); break;
    case Value_real: value_set(a, cast(T)value_real(*a)); break;
    default:
        LULU_UNREACHABLE();
        break;
    }
}

template<class T>
static inline void
vm_arith1(T (*op)(T a), Value *RA, Value RB)
{
    auto arg = value_get<T>(RB);
    auto res = (*op)(arg);
    value_set<T>(RA, res);
}

template<class T>
static inline void
vm_arith2(T (*op)(T a, T b), Value *RA, Value RB, Value RC)
{
    auto lhs = value_get<T>(RB);
    auto rhs = value_get<T>(RC);
    auto res = (*op)(lhs, rhs);
    value_set<T>(RA, res);
}

template<class T>
static inline bool
vm_compare(bool (*op)(T a, T b), Value RA, Value RB)
{
    auto lhs = value_get<T>(RA);
    auto rhs = value_get<T>(RB);
    return (*op)(lhs, rhs);
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
        case Op_int_imm:  value_set_int (RA, GETARG_sBx(i));     break;
        // TODO(2026-07-13): Can we just copy the union directly?
        case Op_int_k:    value_set_int( RA, tvalue_int (KBx(i)) );   break;
        case Op_real:     value_set_real(RA, tvalue_real(KBx(i)) );   break;
        case Op_not:      value_set_bool(RA, !value_bool(RB(i))); break;

        case Op_cast: {
            switch (cast(ValueKind)GETARG_B(i)) {
            case Value_bool: vm_cast<lulu_bool>(RA, i); break;
            case Value_int:  vm_cast<lulu_int> (RA, i); break;
            case Value_real: vm_cast<lulu_real>(RA, i); break;
            default:
                LULU_UNREACHABLE();
                break;
            }
            break;
        }

// Arithmetic (1)
#define vm_arith1(T, f) vm_arith1<T>(f<T>, RA, RB(i))
#define vm_arith2(T, f) vm_arith2<T>(f<T>, RA, RB(i), RC(i))
        case Op_neg:  vm_arith1(lulu_int,  num_neg); break;
        case Op_add:  vm_arith2(lulu_int,  num_add); break;
        case Op_sub:  vm_arith2(lulu_int,  num_sub); break;

        // Arithmetic (2)
        case Op_mul:  vm_arith2(lulu_int,  num_mul); break;
        case Op_div:  vm_arith2(lulu_int,  num_div); break;
        case Op_mod:  vm_arith2(lulu_int,  num_mod); break;
        case Op_fneg: vm_arith1(lulu_real, num_neg); break;
        case Op_fadd: vm_arith2(lulu_real, num_add); break;
        case Op_fsub: vm_arith2(lulu_real, num_sub); break;
        case Op_fmul: vm_arith2(lulu_real, num_mul); break;
        case Op_fdiv: vm_arith2(lulu_real, num_div); break;
#undef vm_arith2
#undef vm_arith1

// Comparison
#define vm_compare(T, f) vm_compare<T>(f<T>, *RA, RB(i))
#define compare(T, f)    if (vm_compare(T, f) != GETARG_k(i)) ip++
        case Op_eq:   compare(lulu_int,  num_eq);   break;
        case Op_lt:   compare(lulu_int,  num_lt);   break;
        case Op_leq:  compare(lulu_int,  num_leq);  break;
        case Op_feq:  compare(lulu_real, num_eq);   break;
        case Op_flt:  compare(lulu_real, num_lt);   break;
        case Op_fleq: compare(lulu_real, num_leq);  break;
#undef compare
#undef vm_compare

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

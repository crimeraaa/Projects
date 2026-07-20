#include "vm.hpp"
#include "opcode.hpp"
#include "debug.hpp"
#include "value.hpp"

#define RB(i)   R[GETARG_B(i)]
#define RC(i)   R[GETARG_C(i)]
#define KBx(i)  K[GETARG_Bx(i)]

template<class T> static inline void
vm_arith1(T (*op)(T a), Value *RA, Value RB)
{
    auto res = (*op)(value_get<T>(RB));
    value_set<T>(RA, res);
}

template<class T>
static inline void
vm_arith2(T (*op)(T a, T b), Value *RA, Value RB, Value RC)
{
    auto res = (*op)(value_get<T>(RB), value_get<T>(RC));
    value_set<T>(RA, res);
}

template<class T>
static inline void
vm_arithi(T (*op)(T a, T b), Value *RA, Value RB, T imm)
{
    auto res = (*op)(value_get<T>(RB), imm);
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

template<class T>
static inline bool
vm_comparei(bool (*op)(T a, T b), Value RA, T imm)
{
    return (*op)(value_get<T>(RA), imm);
}

LULU_INTERNAL_FUNC void
vm_execute(lulu_State *L, Chunk *c)
{
    Value        R[ARG_A_MAX];
    Instruction *ip = c->code;
    TValue *     K  = c->constants;
    printf("======== EXECUTION ========\n");
    for (;;) {
        Instruction i  = *ip++;
        Value      *RA = &R[GETARG_A(i)];
        debug_disassemble_at(c, ip - c->code - 1);
        switch (GET_OPCODE(i)) {
        case Op_move: *RA = RB(i); break;
        case Op_bool: 
            value_set_bool(RA, cast(bool)GETARG_B(i));
            if (GETARG_k(i)) {
                ip++;
            }
            break;
        case Op_int_imm:  value_set_int (RA, GETARG_sBx(i)); break;
        // TODO(2026-07-13): Can we just copy the union directly?
        case Op_int_k:    value_set_int( RA, tvalue_int (KBx(i)) ); break;
        case Op_real:     value_set_real(RA, tvalue_real(KBx(i)) ); break;

        // Conversion operations
        case Op_not:      value_set_bool(RA, !value_bool(RB(i))); break;
        case Op_int2real: value_set_real(RA, cast(lulu_real)value_int (RB(i))); break;
        case Op_real2int: value_set_int (RA, cast(lulu_int) value_real(RB(i))); break;
// Arithmetic
#define vm_arith1(T, f) vm_arith1<T>(f<T>, RA, RB(i))
#define vm_arith2(T, f) vm_arith2<T>(f<T>, RA, RB(i), RC(i))
#define vm_arithi(T, f) vm_arithi<T>(f<T>, RA, RB(i), cast(T)GETARG_vsBx(i))
        // Integer bitwise (register-register)
        case Op_bnot:  vm_arith1(lulu_int, num_bnot); break;
        case Op_band:  vm_arith2(lulu_int, num_band); break;
        case Op_bor:   vm_arith2(lulu_int, num_bor);  break;
        case Op_bxor:  vm_arith2(lulu_int, num_bxor); break;

        // Integer arithmetic (register-register)
        case Op_neg:   vm_arith1(lulu_int, num_neg); break;
        case Op_add:   vm_arith2(lulu_int, num_add); break;
        case Op_sub:   vm_arith2(lulu_int, num_sub); break;
        case Op_mul:   vm_arith2(lulu_int, num_mul); break;
        case Op_div:   vm_arith2(lulu_int, num_div); break;
        case Op_mod:   vm_arith2(lulu_int, num_mod); break;

        // Integer bitwise (register-immediate)
        case Op_bandi: vm_arithi(lulu_int, num_band); break;
        case Op_bori:  vm_arithi(lulu_int, num_bor);  break;
        case Op_bxori: vm_arithi(lulu_int, num_bxor); break;

        // Integer arithmetic (register-immediate)
        case Op_addi:  vm_arithi(lulu_int, num_add); break;
        case Op_subi:  vm_arithi(lulu_int, num_sub); break;
        
        // Floating-point arithmetic
        case Op_fneg:  vm_arith1(lulu_real, num_neg); break;
        case Op_fadd:  vm_arith2(lulu_real, num_add); break;
        case Op_fsub:  vm_arith2(lulu_real, num_sub); break;
        case Op_fmul:  vm_arith2(lulu_real, num_mul); break;
        case Op_fdiv:  vm_arith2(lulu_real, num_div); break;
        case Op_fmod:  vm_arith2(lulu_real, num_mod); break;
        case Op_faddi: vm_arithi(lulu_real, num_add); break;
        case Op_fsubi: vm_arithi(lulu_real, num_sub); break;
        
#undef vm_arithi
#undef vm_arith2
#undef vm_arith1

// Comparison
#define C                 GETARG_C(i)
#define k                 GETARG_k(i)
#define vm_compare(T, f)  if (vm_compare <T>(f<T>, *RA, RB(i))    != k) ip++
#define vm_comparei(T, f) if (vm_comparei<T>(f<T>, *RA, cast(T)C) != k) ip++
        case Op_eq:    vm_compare (lulu_int,  num_eq);  break;
        case Op_lt:    vm_compare (lulu_int,  num_lt);  break;
        case Op_leq:   vm_compare (lulu_int,  num_leq); break;
        case Op_eqi:   vm_comparei(lulu_int,  num_eq);  break;
        case Op_lti:   vm_comparei(lulu_int,  num_lt);  break;
        case Op_leqi:  vm_comparei(lulu_int,  num_leq); break;
        case Op_feq:   vm_compare (lulu_real, num_eq);  break;
        case Op_flt:   vm_compare (lulu_real, num_lt);  break;
        case Op_fleq:  vm_compare (lulu_real, num_leq); break;
        case Op_feqi:  vm_comparei(lulu_real, num_eq);  break;
        case Op_flti:  vm_comparei(lulu_real, num_lt);  break;
        case Op_fleqi: vm_comparei(lulu_real, num_leq); break;
#undef k
#undef C
#undef vm_comparei
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

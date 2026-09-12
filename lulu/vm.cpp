#include "vm.hpp"
#include "opcode.hpp"
#include "debug.hpp"
#include "value.hpp"

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

static void
vm_dump_stack(Slice<Value> regs)
{
    printf("===========================\n");
    for (Value &reg : regs) {
        auto i = &reg - raw_data(regs);
        printf("R(%ti) = {i = " LULU_INT_FMT ", f = " LULU_REAL_FMT ", p = 0x%p}\n",
            i, reg.i, reg.r, reg.p);
    }
}

#define RB(i)   R[getarg_B(i)]
#define RC(i)   R[getarg_C(i)]
#define KBx(i)  K[getarg_Bx(i)]
#define KB(i)   K[getarg_B(i)]
#define KC(i)   K[getarg_C(i)]

LULU_INTERNAL_FUNC void
vm_execute(lulu_State *L, Chunk *c)
{
    Value        R[ARG_A_MAX];
    Instruction *start_ip = raw_data(c->code);
    Instruction *ip       = start_ip;
    TValue *     K        = raw_data(c->constants);
    printf("======== EXECUTION ========\n");
    for (;;) {
        Instruction i  = *ip++;
        Value      *RA = &R[getarg_A(i)];

        // Since we incremented the ip, undo that to get the actual index.
        debug_disassemble_at(c, ip - start_ip - 1);
        switch (get_opcode(i)) {
        case Op_move: *RA = RB(i); break;
        case Op_bool: 
            value_set_bool(RA, cast(bool)getarg_B(i));
            if (getarg_k(i)) {
                ip++;
            }
            break;
        case Op_int_imm:  value_set_int (RA, getarg_sBx(i)); break;
        // TODO(2026-07-13): Can we just copy the union directly?
        case Op_int_k:    value_set_int( RA, tvalue_int (KBx(i)) ); break;
        case Op_real:     value_set_real(RA, tvalue_real(KBx(i)) ); break;

        // Conversion operations
        case Op_not:      value_set_bool(RA, !value_bool(RB(i))); break;
        case Op_int2real: value_set_real(RA, cast(lulu_real)value_int (RB(i))); break;
        case Op_real2int: value_set_int (RA, cast(lulu_int) value_real(RB(i))); break;
// Arithmetic
#define un(T, f)    vm_arith1<T>(f<T>, RA, RB(i))
#define bin(T, f)   vm_arith2<T>(f<T>, RA, RB(i), RC(i))
#define bini(T, f)  vm_arithi<T>(f<T>, RA, RB(i), cast(T)getarg_C(i))
#define bink(T, f)  vm_arith2<T>(f<T>, RA, RB(i), KC(i).value)

        // Integer arithmetic
        case Op_bnot:  un  (lulu_int, num_bnot); break;
        case Op_band:  bin (lulu_int, num_band); break;
        case Op_bor:   bin (lulu_int, num_bor);  break;
        case Op_bxor:  bin (lulu_int, num_bxor); break;
        case Op_neg:   un  (lulu_int, num_neg);  break;
        case Op_add:   bin (lulu_int, num_add);  break;
        case Op_sub:   bin (lulu_int, num_sub);  break;
        case Op_mul:   bin (lulu_int, num_mul);  break;
        case Op_div:   bin (lulu_int, num_div);  break;
        case Op_mod:   bin (lulu_int, num_mod);  break;
        case Op_bandi: bini(lulu_int, num_band); break;
        case Op_bori:  bini(lulu_int, num_bor);  break;
        case Op_bxori: bini(lulu_int, num_bxor); break;
        case Op_addi:  bini(lulu_int, num_add);  break;
        case Op_subi:  bini(lulu_int, num_sub);  break;
        case Op_bandk: bink(lulu_int, num_band); break;
        case Op_bork:  bink(lulu_int, num_bor);  break;
        case Op_bxork: bink(lulu_int, num_bxor); break;
        case Op_addk:  bink(lulu_int, num_add);  break;
        case Op_subk:  bink(lulu_int, num_sub);  break;
        case Op_mulk:  bink(lulu_int, num_mul);  break;
        case Op_divk:  bink(lulu_int, num_div);  break;
        case Op_modk:  bink(lulu_int, num_mod);  break;
        
        // Floating-point arithmetic
        case Op_fneg:  un  (lulu_real, num_neg); break;
        case Op_fadd:  bin (lulu_real, num_add); break;
        case Op_fsub:  bin (lulu_real, num_sub); break;
        case Op_fmul:  bin (lulu_real, num_mul); break;
        case Op_fdiv:  bin (lulu_real, num_div); break;
        case Op_fmod:  bin (lulu_real, num_mod); break;
        case Op_faddi: bini(lulu_real, num_add); break;
        case Op_fsubi: bini(lulu_real, num_sub); break;
        case Op_faddk: bink(lulu_real, num_add); break;
        case Op_fsubk: bink(lulu_real, num_sub); break;
        case Op_fmulk: bink(lulu_real, num_mul); break;
        case Op_fdivk: bink(lulu_real, num_div); break;
        case Op_fmodk: bink(lulu_real, num_mod); break;
        
#undef bink
#undef bini
#undef bin
#undef un

// Comparison
#define bin(T, f)   if (vm_compare <T>(f<T>, *RA, RB(i))              != getarg_k(i)) ip++
#define bini(T, f)  if (vm_comparei<T>(f<T>, *RA, cast(T)getarg_B(i)) != getarg_k(i)) ip++
#define bink(T, f)  if (vm_compare <T>(f<T>, *RA, KB(i).value)        != getarg_k(i)) ip++
        case Op_eq:    bin (lulu_int,  num_eq);  break;
        case Op_lt:    bin (lulu_int,  num_lt);  break;
        case Op_leq:   bin (lulu_int,  num_leq); break;
        case Op_eqi:   bini(lulu_int,  num_eq);  break;
        case Op_lti:   bini(lulu_int,  num_lt);  break;
        case Op_leqi:  bini(lulu_int,  num_leq); break;
        case Op_eqk:   bink(lulu_int,  num_eq);  break;
        case Op_ltk:   bink(lulu_int,  num_lt);  break;
        case Op_leqk:  bink(lulu_int,  num_leq); break;

        case Op_feq:   bin (lulu_real, num_eq);  break;
        case Op_flt:   bin (lulu_real, num_lt);  break;
        case Op_fleq:  bin (lulu_real, num_leq); break;
        case Op_feqi:  bini(lulu_real, num_eq);  break;
        case Op_flti:  bini(lulu_real, num_lt);  break;
        case Op_fleqi: bini(lulu_real, num_leq); break;
        case Op_feqk:  bink(lulu_real, num_eq);  break;
        case Op_fltk:  bink(lulu_real, num_lt);  break;
        case Op_fleqk: bink(lulu_real, num_leq); break;
#undef bink
#undef bini
#undef bin

        case Op_return0: vm_dump_stack(slice_array(R, 0, c->stack_size)); return;
        case Op_return:  vm_dump_stack(slice_array(R, getarg_A(i), getarg_B(i))); return;
        }
    }
}

#undef KC
#undef KB
#undef KBx
#undef RC
#undef RB


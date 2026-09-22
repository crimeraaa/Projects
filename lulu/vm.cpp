#pragma once

#include "opcode.cpp"
#include "value.cpp"
#include "debug.cpp"

template<class T> static inline void
vm_arith1(T (*op)(T a), Value *RA, Value RB)
{
    auto res = (*op)(RB.get<T>());
    RA->set<T>(res);
}

template<class T>
static inline void
vm_arith2(T (*op)(T a, T b), Value *RA, Value RB, Value RC)
{
    auto res = (*op)(RB.get<T>(), RC.get<T>());
    RA->set<T>(res);
}

template<class T>
static inline void
vm_arithi(T (*op)(T a, T b), Value *RA, Value RB, T imm)
{
    auto res = (*op)(RB.get<T>(), imm);
    RA->set<T>(res);
}

template<class T>
static inline bool
vm_compare(bool (*op)(T a, T b), Value RA, Value RB)
{
    auto lhs = RA.get<T>();
    auto rhs = RB.get<T>();
    return (*op)(lhs, rhs);
}

template<class T>
static inline bool
vm_comparei(bool (*op)(T a, T b), Value RA, T imm)
{
    return (*op)(RA.get<T>(), imm);
}

static void
vm_dump_stack(Slice<Value> regs)
{
    printf("===========================\n");
    for (Value &reg : regs) {
        auto i = &reg - regs.raw_data();
        printf("R(%ti) = {i = " LULU_INT_FMT ", f = " LULU_REAL_FMT ", p = 0x%p}\n",
            i, reg.i, reg.r, reg.p);
    }
}

#define RB(i)   R[(i).B()]
#define RC(i)   R[(i).C()]
#define KBx(i)  K[(i).Bx()]
#define KB(i)   K[(i).B()]
#define KC(i)   K[(i).C()]

void
vm_execute(lulu_State *L, Chunk *c)
{
    Value        R[ARG_A.MAX];
    Instruction *start_ip = c->code.begin();
    Instruction *ip       = start_ip;
    TValue *     K        = c->constants.raw_data();
    printf("======== EXECUTION ========\n");
    for (;;) {
        Instruction i  = *ip++;
        Value      *RA = &R[i.A()];

        // Since we incremented the ip, undo that to get the actual index.
        debug_disassemble_at(c, ip - start_ip - 1);
        switch (i.Op()) {
        case Op_move: *RA = RB(i); break;
        case Op_bool: 
            RA->set_bool(cast(bool)i.B());
            if (i.k()) {
                ip++;
            }
            break;
        case Op_int_imm:  RA->set_intr(i.sBx()); break;
        // TODO(2026-07-13): Can we just copy the union directly?
        case Op_int_k:    RA->set_intr(KBx(i).get_intr()); break;
        case Op_real:     RA->set_real(KBx(i).get_real()); break;

        // Conversion operations
        case Op_not:      RA->set_bool(!RB(i).get_bool()); break;
        case Op_int2real: RA->set_real(cast(real)RB(i).get_intr()); break;
        case Op_real2int: RA->set_intr(cast(intr)RB(i).get_real()); break;
// Arithmetic
#define un(T, f)    vm_arith1<T>(f<T>, RA, RB(i))
#define bin(T, f)   vm_arith2<T>(f<T>, RA, RB(i), RC(i))
#define bini(T, f)  vm_arithi<T>(f<T>, RA, RB(i), cast(T)i.C())
#define bink(T, f)  vm_arith2<T>(f<T>, RA, RB(i), KC(i).value)

        // Integer arithmetic
        case Op_bnot:  un  (intr, num_bnot); break;
        case Op_band:  bin (intr, num_band); break;
        case Op_bor:   bin (intr, num_bor);  break;
        case Op_bxor:  bin (intr, num_bxor); break;
        case Op_neg:   un  (intr, num_neg);  break;
        case Op_add:   bin (intr, num_add);  break;
        case Op_sub:   bin (intr, num_sub);  break;
        case Op_mul:   bin (intr, num_mul);  break;
        case Op_div:   bin (intr, num_div);  break;
        case Op_mod:   bin (intr, num_mod);  break;
        case Op_bandi: bini(intr, num_band); break;
        case Op_bori:  bini(intr, num_bor);  break;
        case Op_bxori: bini(intr, num_bxor); break;
        case Op_addi:  bini(intr, num_add);  break;
        case Op_subi:  bini(intr, num_sub);  break;
        case Op_bandk: bink(intr, num_band); break;
        case Op_bork:  bink(intr, num_bor);  break;
        case Op_bxork: bink(intr, num_bxor); break;
        case Op_addk:  bink(intr, num_add);  break;
        case Op_subk:  bink(intr, num_sub);  break;
        case Op_mulk:  bink(intr, num_mul);  break;
        case Op_divk:  bink(intr, num_div);  break;
        case Op_modk:  bink(intr, num_mod);  break;
        
        // Floating-point arithmetic
        case Op_fneg:  un  (real, num_neg); break;
        case Op_fadd:  bin (real, num_add); break;
        case Op_fsub:  bin (real, num_sub); break;
        case Op_fmul:  bin (real, num_mul); break;
        case Op_fdiv:  bin (real, num_div); break;
        case Op_fmod:  bin (real, num_mod); break;
        case Op_faddi: bini(real, num_add); break;
        case Op_fsubi: bini(real, num_sub); break;
        case Op_faddk: bink(real, num_add); break;
        case Op_fsubk: bink(real, num_sub); break;
        case Op_fmulk: bink(real, num_mul); break;
        case Op_fdivk: bink(real, num_div); break;
        case Op_fmodk: bink(real, num_mod); break;
        
#undef bink
#undef bini
#undef bin
#undef un

// Comparison
#define bin(T, f)   if (vm_compare <T>(f<T>, *RA, RB(i))        != i.k()) ip++
#define bini(T, f)  if (vm_comparei<T>(f<T>, *RA, cast(T)i.B()) != i.k()) ip++
#define bink(T, f)  if (vm_compare <T>(f<T>, *RA, KB(i).value)  != i.k()) ip++
        case Op_eq:    bin (intr,  num_eq);  break;
        case Op_lt:    bin (intr,  num_lt);  break;
        case Op_leq:   bin (intr,  num_leq); break;
        case Op_eqi:   bini(intr,  num_eq);  break;
        case Op_lti:   bini(intr,  num_lt);  break;
        case Op_leqi:  bini(intr,  num_leq); break;
        case Op_eqk:   bink(intr,  num_eq);  break;
        case Op_ltk:   bink(intr,  num_lt);  break;
        case Op_leqk:  bink(intr,  num_leq); break;

        case Op_feq:   bin (real, num_eq);  break;
        case Op_flt:   bin (real, num_lt);  break;
        case Op_fleq:  bin (real, num_leq); break;
        case Op_feqi:  bini(real, num_eq);  break;
        case Op_flti:  bini(real, num_lt);  break;
        case Op_fleqi: bini(real, num_leq); break;
        case Op_feqk:  bink(real, num_eq);  break;
        case Op_fltk:  bink(real, num_lt);  break;
        case Op_fleqk: bink(real, num_leq); break;
#undef bink
#undef bini
#undef bin

        case Op_return0: vm_dump_stack(slice_array(R, 0, c->stack_size)); return;
        case Op_return:  vm_dump_stack(slice_array(R, i.A(), i.B())); return;
        }
    }
}

#undef KC
#undef KB
#undef KBx
#undef RC
#undef RB


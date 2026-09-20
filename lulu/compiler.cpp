#include "lulu.h"
#include "internal.hpp"
#include "mem.hpp"
#include "value.hpp"
#include "opcode.hpp"
#include "type.hpp"
#include "expr.hpp"
#include "parser.hpp"
#include "chunk.hpp"
#include "compiler.hpp"
#include "checker.hpp"

LULU_INTERNAL_FUNC void
compiler_finish(Compiler *c)
{
    lulu_State *L     = c->L;
    Chunk *     chunk = c->chunk;
    compiler_return(c, {});

    i32  pc       = c->pc;
    auto reg_info = chunk->reg_info;
    for (VarInfo v : compiler_slice_active_locals(c)) {
        reg_info[v.reg_info_index].pc_died = pc;
    }

    // Shrink chunk to fit.
    mem_shrink_dynamic(L, &chunk->code);
    mem_shrink_dynamic(L, &chunk->constants);
    mem_shrink_dynamic(L, &chunk->reg_info);
}

[[noreturn]] static void
compiler_error(Compiler *c, char const *info, Expr const *e)
{
    parser_error_expr(c->parser, info, e);
}

static i32
compiler_code(Compiler *c, Instruction i)
{
    Chunk *chunk = c->chunk;
    i32    index = c->pc++;
    mem_append_dynamic(c->L, &chunk->code, i);
    return index;
}

static u32
compiler_add_constant(Compiler *c, TValue tv)
{
    Chunk *chunk = c->chunk;
    auto   K     = chunk->constants;
    u32    n     = cast(u32)len(K);

    // Try to reuse an existing value.
    for (u32 i = 0; i < n; i++) {
        if (tv == K[i]) {
            LULU_LOGF("Reused constant index %u / %u", i, n);
            return i;
        }
    }
    mem_append_dynamic(c->L, &chunk->constants, tv);
    LULU_LOGF("Added constant index %u / %zu", n, len(chunk->constants));
    return n;
}

static i32
compiler_code_ABC(Compiler *c, OpCode Op, u16 A, u16 B, u16 C)
{
    LULU_ASSERT(opcode_is_ABC(Op));
    LULU_ASSERT(A <= ARG_A.MAX);
    LULU_ASSERT(B <= ARG_B.MAX);
    LULU_ASSERT(C <= ARG_C.MAX);
    return compiler_code(c, Instruction::ABC(Op, A, B, C));
}

static i32
compiler_code_AB0(Compiler *c, OpCode Op, u16 A, u16 B)
{
    return compiler_code_ABC(c, Op, A, B, 0);
}

static i32
compiler_code_vABC(Compiler *c, OpCode Op, u16 A, u16 B, u16 vC, bool k)
{
    LULU_ASSERT(opcode_is_vABC(Op));
    LULU_ASSERT(A <= ARG_A.MAX);
    LULU_ASSERT(B <= ARG_B.MAX);
    LULU_ASSERT(vC <= ARG_vC.MAX);
    return compiler_code(c, Instruction::vABC(Op, A, B, vC, cast(u8)k));
}

static i32
compiler_code_ABx(Compiler *c, OpCode Op, u16 A, u32 Bx)
{
    LULU_ASSERT(opcode_is_ABx(Op));
    LULU_ASSERT(A  <= ARG_A.MAX);
    LULU_ASSERT(Bx <= ARG_Bx.MAX);
    return compiler_code(c, Instruction::ABx(Op, A, Bx));
}

static i32
compiler_code_AsBx(Compiler *c, OpCode Op, u16 A, i32 sBx)
{
    LULU_ASSERT(opcode_is_AsBx(Op));
    LULU_ASSERT(A <= ARG_A.MAX);
    LULU_ASSERT(ARG_sBx.MIN <= sBx && sBx <= cast(i32)ARG_sBx.MAX);
    return compiler_code(c, Instruction::AsBx(Op, A, sBx));
}

static bool
compiler_cast_bool(Compiler *c, Expr *e)
{
    u16    reg = e->get_reg();
    OpCode op;
    switch (e->type_get_basic_kind()) {
    case Value_bool: LULU_UNREACHABLE(); return false;
    case Value_int:  op = Op_eqi;        break;
    case Value_real: op = Op_feqi;       break;
    default:         return false;
    }
    // Comparison: R(A) == B
    e->set_compare(compiler_code_vABC(c, op, reg, 1, 0, true));
    return true;
}

static bool
compiler_cast_int(Compiler *c, Expr *e)
{
    u16 reg = e->get_reg();
    switch (e->type_get_basic_kind()) {
    case Value_bool:
        e->set_pending(compiler_code_ABC(c, Op_bandi, REG_NONE, reg, 1));
        return true;
    case Value_int:  LULU_UNREACHABLE(); break;
    case Value_real:
        e->set_pending(compiler_code_AB0(c, Op_real2int, REG_NONE, reg));
        return true;
    default:
        break;
    }
    return false;
}

static bool
compiler_cast_real(Compiler *c, Expr *e)
{
    u16 reg = e->get_reg();
    switch (e->type_get_basic_kind()) {
    // Since bool is just implemented in terms of int, use that conversion.
    case Value_bool:
    case Value_int:
        e->set_pending(compiler_code_AB0(c, Op_int2real, REG_NONE, reg));
        return true;
    case Value_real: LULU_UNREACHABLE(); break;
    default:         break;
    }
    return false;
}

static bool
compiler_cast_basic_type(Compiler *c, Expr *e, ValueKind basic_kind)
{
    LULU_ASSERT(e->type != nullptr);

    // Check for supported expressions and discharge to registers as needed.
    switch (e->kind) {
    case Expr_Literal: return checker_cast_literal(e, basic_kind);
    case Expr_Local:
    case Expr_Compare:
    case Expr_Pending:
        compiler_expr_any_reg(c, e);
        compiler_expr_pop(c, e);
        break;
    case Expr_Discharged:
        break;
    default:
        goto nodice;
    }

    // We should haae been discharged into a register by now.
    switch (basic_kind) {
    case Value_bool:
        if (!compiler_cast_bool(c, e)) {
            goto nodice;
        }
        break;
    case Value_int:
        if (!compiler_cast_int(c, e)) {
            goto nodice;
        }
        break;
    case Value_real:
        if (!compiler_cast_real(c, e)) {
            goto nodice;
        }
        break;
    default:
        LULU_UNREACHABLE();
        break;
    }
    e->type = basic_type_get(basic_kind);
    return true;

nodice:
    LULU_PANICF("Got ExprKind(%i)", e->kind);
    compiler_error(c, "Cannot cast to type", e);
    return false;
}

LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, Expr *restrict t, Expr *restrict arg)
{   
    LULU_ASSERT(t->kind == Expr_Type);
    // CHECK(2026-07-16): literal is typed or untyped
    LULU_ASSERT(arg->type != nullptr);

    Type const *type = t->type;

    // Nothing to do?
    if (arg->type == type) {
        return;
    }

    if (type->kind == TypeKind_Basic) {
        ValueKind k = type->basic.kind;
        switch (k) {
        case Value_bool:
        case Value_int:
        case Value_real:
            if (compiler_cast_basic_type(c, arg, k)) {
                return;
            }
        default:
            break;
        }
    }

    // Report the bad type, not the variable?
    arg->loc = t->loc;
    compiler_error(c, "Cannot cast to type", arg);
}

LULU_INTERNAL_FUNC void
compiler_call(Compiler *c, Expr *restrict func, Expr *restrict arg)
{
    if (func->kind == Expr_Type) {
        compiler_cast(c, func, arg);

        // Special case, because we generally want to use func as the
        // out-parameter.
        *func = *arg;
    } else {
        compiler_error(c, "Function calls not yet supported", func);
    }
}

static bool
reg_pop(Compiler *c, u16 reg)
{
    if (reg >= c->active_locals_len) {
        return reg == --c->free_reg;
    }
    return true;
}

// Necessary so we can better report the location of assertions.
#define reg_pop(c, reg)                                                    \
    if (!(reg_pop)(c, reg)) {                                              \
        LULU_PANICF("Expected free_reg = %u, got %u", reg, (c)->free_reg); \
    }

static void
reg_push(Compiler *c, u16 reg_count)
{
    // TODO(2026-07-07): Check stack size!
    c->free_reg += reg_count;
    if (c->free_reg > cast(u16)c->chunk->stack_size) {
        c->chunk->stack_size = cast(u8)c->free_reg;
    }
}

LULU_INTERNAL_FUNC void
compiler_expr_pop(Compiler *c, Expr *e)
{
    if (e->kind == Expr_Discharged) {
        u16 reg = e->get_reg();
        reg_pop(c, reg);
    }
}

static void
expr_discharge_vars(Compiler *c, Expr *e)
{
    // TODO(2026-07-13): Differentiate globals and locals?
    switch (e->kind) {
    case Expr_Local:
        e->kind = Expr_Discharged;
        break;
    default:
        break;
    }
}

static void
compiler_load_bool(Compiler *c, u16 reg, bool b, bool skip = false)
{
    compiler_code_vABC(c, Op_bool, reg, cast(u16)b, 0, skip);
}

static void
compiler_load_int(Compiler *c, u16 reg, intr i)
{
    if (ARG_sBx.MIN <= i && i <= ARG_sBx.MAX) {
        compiler_code_AsBx(c, Op_int_imm, reg, cast(i32)i);
    } else {
        TValue tv = TValue::make_intr(i);
        u32    k  = compiler_add_constant(c, tv);
        compiler_code_ABx(c, Op_int_k, reg, k);
    }
}

static void
compiler_load_real(Compiler *c, u16 reg, real r)
{
    TValue tv = TValue::make_real(r);
    u32    k  = compiler_add_constant(c, tv);
    compiler_code_ABx(c, Op_real, reg, k);
}

/*
 Description:
    Emits the code needed to retrieve the expression from the given register.
    This is 'discharging', indicating the expression has a finalized
    register it can refer to from here on.

 Analog:
    Lua 5.1.5: `lcode.c:discharge2reg (FuncState *fs, expdesc *e, int reg)`
 */
static void
expr_discharge_reg(Compiler *c, Expr *e, u16 reg)
{
    expr_discharge_vars(c, e);
    switch (e->kind) {
    case Expr_Literal: 
        // CHECK(2026-07-16): Literal is typed or untyped
        LULU_ASSERT(e->type != nullptr);
        switch (e->get_literal_kind()) {
        case Value_bool: compiler_load_bool(c, reg, e->get_bool()); break;
        case Value_int:  compiler_load_int (c, reg, e->get_intr()); break;
        case Value_real: compiler_load_real(c, reg, e->get_real()); break;
        default:
            compiler_error(c, "Unsupported literal", e);
            break;
        }
        break;
    case Expr_Discharged:
        // Moving to the same register is a no-op, e.g. `x := 1; x = x;`.
        if (reg != e->reg) {
            compiler_code_AB0(c, Op_move, reg, e->reg);
        }
        break;
    case Expr_Compare:
        compiler_load_bool(c, reg, true, /*skip=*/true);
        compiler_load_bool(c, reg, false);
        break;
    case Expr_Pending:
        c->chunk->code[e->get_pc()].set_A(reg);
        break;
    default:
        LULU_ASSERTF(!e->kind, "Got ExprKind(%i)", e->kind);
        break;
    }
    e->set_discharged(reg);
}

static u16
expr_to_reg(Compiler *c, Expr *e, u16 reg)
{
    expr_discharge_reg(c, e, reg);
    e->set_discharged(reg);
    return reg;
}


LULU_INTERNAL_FUNC u16
compiler_expr_next_reg(Compiler *c, Expr *e)
{
    expr_discharge_vars(c, e);
    compiler_expr_pop(c, e);
    reg_push(c, 1);
    return expr_to_reg(c, e, c->free_reg - 1);
}

LULU_INTERNAL_FUNC u16
compiler_expr_any_reg(Compiler *c, Expr *e)
{
    expr_discharge_vars(c, e);
    // Already have a register?
    if (e->is_reg()) {
        // TODO(2026-07-13): Handle jumps
        return e->get_reg();
    }
    return compiler_expr_next_reg(c, e);
}

static bool
compiler_unary_bnot(Compiler *c, Expr *e)
{
    if (e->is_literal_intr()) {
        e->set_intr(~e->get_intr());
    } else {
        // Non-literal (e.g. discharged register) that is NOT of type `int`?
        if (!e->type_is_basic_kind(Value_int)) {
            return false;
        }
        u16 reg = compiler_expr_any_reg(c, e);
        e->set_pending(compiler_code_AB0(c, Op_bnot, REG_NONE, reg));
    }
    return true;
}

static bool
compiler_unary_neg(Compiler *c, Expr *e)
{
    if (!checker_negate_expr(e)) {
        u16    reg = compiler_expr_any_reg(c, e);
        OpCode op;
        if (e->type_is_basic()) switch (e->type_get_basic_kind()) {
        case Value_int:  op = Op_neg;  break;
        case Value_real: op = Op_fneg; break;
        default:         return false;
        }
        e->set_pending(compiler_code_ABC(c, op, REG_NONE, reg, 0));
    }
    return true;
}

static bool
compiler_unary_not(Compiler *c, Expr *e)
{
    switch (e->kind) {
    case Expr_Literal:
        // Only boolean literals can have `not` applied to them.
        if (!e->type_is_basic_kind(Value_bool)) {
            return false;
        }
        e->set_bool(!e->literal.get_bool());
        return true;
    case Expr_Compare: {
        // E.g. `not (x == y)`
        Instruction *ip = &c->chunk->code[e->pc];
        bool const   k  = ip->k();
        ip->set_k(!k);
        return true;
    }
    default:
        break;
    }

    u16 reg = compiler_expr_any_reg(c, e);
    if (e->type_is_basic_kind(Value_bool)) {
        e->set_pending(compiler_code_ABC(c, Op_not, REG_NONE, reg, 0));
        return true;
    }
    return false;
}

static bool
compiler_unary_dispatch(Compiler *c, Token const &op, Expr *e)
{
    switch (op.kind) {
    case Token_Tilde: return compiler_unary_bnot(c, e);
    case Token_Dash:  return compiler_unary_neg(c, e);
    case Token_not:   return compiler_unary_not(c, e);
    default:
        break;
    }
    LULU_UNREACHABLE();
    return false;
}

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, Token const &op, Expr *e)
{
    if (!compiler_unary_dispatch(c, op, e)) {
        compiler_error(c, "Invalid unary operand", e);
    }
}

struct CompilerBinaryResult {
    bool ok;      // Did we successfuly compile the binary expression?
    bool swapped; // Did we need to swap the contents of the operands?
};

static CompilerBinaryResult
compiler_arithi(Compiler *c, OpCode iop, Expr *restrict lhs, Expr *restrict rhs)
{
    auto r = checker_fix_arithi(&iop, lhs, rhs);
    if (r.ok) {
        /*
         Assumes any of the following forms:
         1) x +   imm
         2) x + (-imm) <=> x - |imm|
         3) x -   imm
         4) x - (-imm) <=> x + |imm|

         Note that since we may have swapped the expressions, there's no guarantee
         that what was once `rhs` was already discharged, e.g. it could be a local.
         Remember that we don't automatically discharge rhs expressions for binary
         so we have to manually manage them.
         */
        u16 reg = compiler_expr_any_reg(c, lhs);
        compiler_expr_pop(c, lhs);
        lhs->set_pending(compiler_code_ABC(c, iop, REG_NONE, reg, cast(u16)r.imm));
    }
    return {r.ok, r.swapped};
}

static CompilerBinaryResult
compiler_comparei(Compiler *c,
    OpCode         iop,
    Expr *restrict lhs,
    Expr *restrict rhs,
    bool           k)
{
    auto r = checker_fix_comparei(&iop, lhs, rhs, &k);
    if (r.ok) {
        u16 reg = compiler_expr_any_reg(c, lhs);
        compiler_expr_pop(c, lhs);

        /*
         Consider the following forms:

         1) x == true  <=>     x ; b = true,  k = true
         2) x ~= true  <=> not x ; b = true,  k = false
         2) x == false <=> not x ; b = false, k = true
         4) x ~= false <=>     x ; b = false, k = false

         We assume that for ordered comparisons, i.e. < and <=, we already threw
         an error. So we can guarantee that boolean comparisons are only ever
         checking for equality.
         */
        if (rhs->is_literal_bool()) {
            bool b = rhs->get_bool();
            if (b != k) {
                lhs->set_pending(compiler_code_ABC(c, Op_not, REG_NONE, reg, 0));
            }
        } else {
            lhs->set_compare(compiler_code_vABC(c, iop, reg, cast(u16)r.imm, 0, k));
        }
    }
    return {r.ok, r.swapped};
}

/*
 Assumptions:
 1) Both arguments are of the same underlying type.

 2) The opcodes and types we support immediate operations for are commutative.
    I.e. `x op y` has the same effect as `y op x`, which is only true for some
    operations.
 */
static CompilerBinaryResult
compiler_binary_imm(Compiler *c,
    OpCode const    op,
    Expr * restrict lhs,
    Expr * restrict rhs,
    bool   const    k)
{
    // If both are literals, then we should've folded them.
    // If they are both discharged or pending, we shouldn't have called this.
    LULU_ASSERT(lhs->is_literal() != rhs->is_literal());
    switch (op) {
    // For the bitwise operators, we assume that reals already caused
    // an error previously.
    case Op_band: return compiler_arithi  (c, Op_bandi, lhs, rhs);
    case Op_bor:  return compiler_arithi  (c, Op_bori,  lhs, rhs);
    case Op_bxor: return compiler_arithi  (c, Op_bxori, lhs, rhs);
    case Op_add:  return compiler_arithi  (c, Op_addi,  lhs, rhs);
    case Op_sub:  return compiler_arithi  (c, Op_subi,  lhs, rhs);
    case Op_eq:   return compiler_comparei(c, Op_eqi,   lhs, rhs, k);
    case Op_lt:   return compiler_comparei(c, Op_lti,   lhs, rhs, k);
    case Op_leq:  return compiler_comparei(c, Op_leqi,  lhs, rhs, k);
    case Op_fadd: return compiler_arithi  (c, Op_faddi, lhs, rhs);
    case Op_fsub: return compiler_arithi  (c, Op_fsubi, lhs, rhs);
    case Op_feq:  return compiler_comparei(c, Op_feqi,  lhs, rhs, k);
    case Op_flt:  return compiler_comparei(c, Op_flti,  lhs, rhs, k);
    case Op_fleq: return compiler_comparei(c, Op_fleqi, lhs, rhs, k);
    default:      return {};
    }
}

static CompilerBinaryResult
compiler_arithk(Compiler *c, OpCode kop, Expr *restrict lhs, Expr *restrict rhs)
{
    auto r = checker_fix_arithk(&kop, lhs, rhs);
    if (r.ok) {
        // May be a temporary register.
        u16 reg = compiler_expr_any_reg(c, lhs);
        compiler_expr_pop(c, lhs);

        u32 i = compiler_add_constant(c, r.constant);
        // TODO(2026-09-13): Handle resolving their registers later on?
        rhs->kind     = Expr_Constant;
        rhs->constant = i;

        r.ok = (i <= ARG_C.MAX);
        if (r.ok) {
            lhs->set_pending(compiler_code_ABC(c, kop, REG_NONE, reg, cast(u16)i));
        }
    }
    return {r.ok, r.swapped};
}

static CompilerBinaryResult
compiler_comparek(Compiler *c, OpCode kop, Expr *restrict lhs, Expr *restrict rhs, bool k)
{
    auto r = checker_fix_comparek(&kop, lhs, rhs, &k);
    if (r.ok) {
        u16 reg = compiler_expr_any_reg(c, lhs);
        compiler_expr_pop(c, lhs);

        u32 i = compiler_add_constant(c, r.constant);
        // TODO(2026-09-13): Handle resolving their registers later on?
        rhs->kind     = Expr_Constant;
        rhs->constant = i;

        r.ok = (i <= ARG_C.MAX);
        if (r.ok) {
            lhs->set_compare(compiler_code_vABC(c, kop, reg, cast(u16)i, 0, k));
        }
    }
    return {r.ok, r.swapped};

}

static CompilerBinaryResult
compiler_binaryk(Compiler *c, OpCode op, Expr *restrict lhs, Expr *restrict rhs, bool k)
{
    LULU_ASSERT(lhs->is_literal() != rhs->is_literal());
    switch (op) {
    case Op_band:   return compiler_arithk  (c, Op_bandk, lhs, rhs);
    case Op_bor:    return compiler_arithk  (c, Op_bork,  lhs, rhs);
    case Op_bxor:   return compiler_arithk  (c, Op_bxork, lhs, rhs);
    case Op_add:    return compiler_arithk  (c, Op_addk,  lhs, rhs);
    case Op_sub:    return compiler_arithk  (c, Op_subk,  lhs, rhs);
    case Op_mul:    return compiler_arithk  (c, Op_mulk,  lhs, rhs);
    case Op_div:    return compiler_arithk  (c, Op_divk,  lhs, rhs);
    case Op_mod:    return compiler_arithk  (c, Op_modk,  lhs, rhs);
    case Op_eq:     return compiler_comparek(c, Op_eqk,   lhs, rhs, k);
    case Op_lt:     return compiler_comparek(c, Op_ltk,   lhs, rhs, k);
    case Op_leq:    return compiler_comparek(c, Op_leqk,  lhs, rhs, k);
    case Op_fadd:   return compiler_arithk  (c, Op_faddk, lhs, rhs);
    case Op_fsub:   return compiler_arithk  (c, Op_fsubk, lhs, rhs);
    case Op_fmul:   return compiler_arithk  (c, Op_fmulk, lhs, rhs);
    case Op_fdiv:   return compiler_arithk  (c, Op_fdivk, lhs, rhs);
    case Op_fmod:   return compiler_arithk  (c, Op_fmodk, lhs, rhs);
    case Op_feq:    return compiler_comparek(c, Op_feqk,  lhs, rhs, k);
    case Op_flt:    return compiler_comparek(c, Op_fltk,  lhs, rhs, k);
    case Op_fleq:   return compiler_comparek(c, Op_fleqk, lhs, rhs, k);
    default:        return {};
    }
}

LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c, Token const &op, Expr *restrict lhs, Expr *restrict rhs)
{
    switch (checker_fold_binary(op, lhs, rhs)) {
    case Checker_Ok:
        lhs->loc = rhs->loc;
        return;
    case Checker_Cannot_Fold:
        break;
    case Checker_Divide_By_Zero:
        compiler_error(c, "Cannot divide/modulo by 0", rhs);
        break;
    }

    auto r = checker_fix_binary(op, lhs, rhs);
    if (!r.ok) {
        // Leaky abstraction but who tf cares amirite
        if (r.op) {
            compiler_error(c, "Invalid left hand side operand", lhs);
        } else {
            compiler_error(c, "Inconsistent right hand side type", rhs);
        }
    }

    bool k = !r.is_not;
    if (lhs->is_literal() || rhs->is_literal()) {
        auto immr = compiler_binary_imm(c, r.op, lhs, rhs, k);
        if (immr.ok) {
            if (!immr.swapped) {
                // Propagate this change because we won't do it any place else.
                lhs->loc = rhs->loc;
            }
            return;
        }

        // If we didn't emit an immediate-addressed opcode, ensure we reset the
        // order of the operands to their original.
        if (immr.swapped) {
            swap(lhs, rhs);
        }
        
        auto kr = compiler_binaryk(c, r.op, lhs, rhs, k);
        if (kr.ok) {
            if (!kr.swapped) {
                lhs->loc = rhs->loc;
            }
            return;
        }
    }

    u16 r1 = lhs->get_reg();
    u16 r2 = compiler_expr_any_reg(c, rhs);
    if (r1 > r2) {
        compiler_expr_pop(c, lhs);
        compiler_expr_pop(c, rhs);
    } else {
        compiler_expr_pop(c, rhs);
        compiler_expr_pop(c, lhs);
    }

    if (r.is_compare) {
        /*
         Allows us to implement complements of the 3 basic comparison
         instructions. This is useful for both assignments and conditional 
         locks.

         0 = proceed label(true) if not result else goto label(false)
         1 = proceed label(true) if     result else goto label(false)
         */
        lhs->type  = basic_type_get(Value_bool);
        lhs->loc = rhs->loc;

        // R(A) is not a destination register here!
        lhs->set_compare(compiler_code_vABC(c, r.op, r1, r2, 0, k));
    } else {
        lhs->loc = rhs->loc;
        lhs->set_pending(compiler_code_ABC(c, r.op, REG_NONE, r1, r2));
    }
}

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, ExprList list)
{
    u16 start_reg, stop_reg;
    switch (list.count) {
    case 0:
        compiler_code_ABC(c, Op_return0, 0, 0, 0);
        return;
    case 1:
        start_reg = compiler_expr_any_reg(c, &*list);
        stop_reg  = start_reg + 1;
        compiler_code_ABC(c, Op_return, start_reg, stop_reg, 0);
        return;
    default:
        break;
    }

    stop_reg = REG_NONE;
    for (Expr &e : list) {
        stop_reg = compiler_expr_next_reg(c, &e);
    }

    stop_reg  += 1;
    start_reg  = stop_reg - cast(u16)list.count;
    compiler_code_ABC(c, Op_return, start_reg, stop_reg, 0);

    // The range is exclusive, so the actual last register is off-by-one.
    for (u16 reg = stop_reg; reg-- >= start_reg;) {
        reg_pop(c, reg);
    }
}

LULU_INTERNAL_FUNC void
compiler_declare_local(Compiler *c, ExprList lhs_list)
{
    lulu_State *L = c->L;

    // Declare from left to right.
    u16   reg           = c->active_locals_len;
    i32   pc            = c->pc;
    auto *reg_info      = &c->chunk->reg_info;
    auto  active_locals = compiler_slice_active_locals(c);
    for (Expr &lhs : lhs_list) {
        if (lhs.kind != Expr_Local) {
            compiler_error(c, "Unassignable target", &lhs);
        }

        // If non-null then that means this variable already exists in some scope.
        // TODO(2026-09-05): Check scopes?
        if (lhs.type != nullptr) {
            compiler_error(c, "Shadowing of variable", &lhs);
        }

        mem_append_dynamic(L, reg_info, {
            /*reg    =*/cast(u8)reg,
            /*pc_born=*/pc,
            /*pc_died=*/PC_NONE,
            /*type   =*/nullptr,
        });

        // Reset the type to indicate we don't know it (yet).
        lhs.type = nullptr;
        active_locals[reg++] = VarInfo{
            /*loc           =*/lhs.loc,
            /*type          =*/nullptr,
            /*scope         =*/-1,
            /*reg_info_index=*/cast(u32)(len(*reg_info) - 1),
        };
    }
}

LULU_INTERNAL_FUNC void
compiler_define_local(Compiler *c, ExprList lhs_list, ExprList rhs_list)
{
    LULU_ASSERT(lhs_list.count == rhs_list.count);

    auto reg_info      = c->chunk->reg_info;
    auto active_locals = compiler_slice_active_locals(c);
    int  scope         = c->scope;
    u16  reg           = cast(u16)len(active_locals);
    for (Expr &rhs : rhs_list) {
        Expr &lhs = *lhs_list++;

        // Ensure both sides had type inference done by the parser.
        LULU_ASSERT(lhs.type != nullptr);
        LULU_ASSERT(rhs.type != nullptr);

        // We have an assigning expression but it's the wrong target type.
        if (lhs.type != rhs.type) {
            // Only literals that can be implicitly converted to the destination
            // type without any loss of data will pass this check.
            if (!checker_coerce_rhs(&lhs, &rhs)) {
                compiler_error(c, "Invalid implicit cast", &rhs);
            }
        }

        LULU_ASSERT(rhs.type == lhs.type);

        u16 tmp = compiler_expr_next_reg(c, &rhs);
        VarInfo *v = &active_locals[reg++];
        v->scope   = scope;
        v->type    = lhs.type;
        reg_info[v->reg_info_index].type = lhs.type;
    }

    // The last register is the length.
    c->active_locals_len = reg;
}

LULU_INTERNAL_FUNC void
compiler_assign(Compiler *c, ExprList lhs_list, ExprList rhs_list)
{
    LULU_ASSERT(lhs_list.count == rhs_list.count);
    for (Expr &lhs : lhs_list) {
        Expr &rhs = *rhs_list++;
        if (lhs.type != rhs.type){
            if (!checker_coerce_rhs(&lhs, &rhs)) {
                compiler_error(c, "Invalid implicit cast", &rhs);
            }
        }

        switch (lhs.kind) {
        case Expr_Local:
            lhs.kind = Expr_Discharged;
            expr_to_reg(c, &rhs, lhs.get_reg());
            break;
        default:
            compiler_error(c, "Invalid assignment target", &lhs);
            break;
        }
    }
}


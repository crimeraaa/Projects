#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "opcode.cpp"
#include "value.cpp"
#include "chunk.cpp"
#include "type.cpp"
#include "expr.cpp"
#include "checker.cpp"

#define LOCALS_MAX_COUNT 0x10

class Parser;

/*
 Description:
    Represents the compile-time, meta-information about a named register.
 */
struct VarInfo {
    Loc         loc;
    Type const *type;
    int         scope; // 0 indicates global scope.
    u32         reg_info_index;
};

struct CompilerBinaryResult {
    bool ok;      // Did we successfuly compile the binary expression?
    bool swapped; // Did we need to swap the contents of the operands?
};

class Compiler {
    // Shared state. All nested compilers must share the same pointers/references
    // to these.
    lulu_State *L;
    Parser &    parser;

    // Compiler state. These are local to the current compiler.
    Chunk &     chunk;
    int         scope;
    i32         pc;
    u16         free_reg;

    // Track the list of currently active local variables. Their registers
    // match the indices to be used here.
    u16         active_locals_len;
    VarInfo     active_locals[LOCALS_MAX_COUNT];

public:
    Compiler(lulu_State *L, Parser &parser, Chunk &chunk)
        : L{L}
        , parser{parser}
        , chunk{chunk}
        , scope{0}
        , pc{0}
        , free_reg{0}
        , active_locals_len{0}
    {}

    void
    finish();

    Slice<VarInfo>
    slice_active_locals()
    {
        return slice_array(this->active_locals, 0, this->active_locals_len);
    }

// HIGH-LEVEL EXPR MANIPULATION ============================================ {{{
public:
    void
    explicit_cast(Expr &restrict t, Expr &restrict arg);

private:
    bool
    cast_basic_type(Expr &e, ValueKind basic_kind);

    bool
    cast_bool(Expr &e);

    bool
    cast_int(Expr &e);

    bool
    cast_real(Expr &e);

public:
    void
    call(Expr &restrict func, Expr &restrict arg);

    void
    unary(Token const &op, Expr &e);

private:
    bool
    unary_dispatch(Token const &op, Expr &e);

    bool
    unary_bnot(Expr &e);

    bool
    unary_neg(Expr &e);

    bool
    unary_not(Expr &e);

public:
    void
    binary(Token const &op, Expr &restrict lhs, Expr &restrict rhs);

private:
    // TODO(2026-09-22): Convert most of these to checker functions?
    CompilerBinaryResult
    binary_imm(OpCode op, Expr &restrict lhs, Expr &restrict rhs, bool k);

    CompilerBinaryResult
    arithi(OpCode iop, Expr &restrict lhs, Expr &restrict rhs);

    CompilerBinaryResult
    comparei(OpCode iop, Expr &restrict lhs, Expr &restrict rhs, bool k);

    CompilerBinaryResult
    binaryk(OpCode op, Expr &restrict lhs, Expr &restrict rhs, bool k);

    CompilerBinaryResult
    arithk(OpCode kop, Expr &restrict lhs, Expr &restrict rhs);

    CompilerBinaryResult
    comparek(OpCode kop, Expr &restrict lhs, Expr &restrict rhs, bool k);

public:
    void
    explicit_return(ExprList list);

    void
    implicit_return() { this->explicit_return({}); }

    void
    declare_local(ExprList lhs_list);

    void
    define_local(ExprList lhs_list, ExprList rhs_list);

    void
    assign(ExprList lhs_list, ExprList rhs_list);

// ========================================================================= }}}
// LOW-LEVEL EXPR MANIPULATION ============================================= {{{
public:
    u16
    expr_next_reg(Expr &e);

    u16
    expr_any_reg(Expr &e);

    void
    pop_expr(Expr &e);

private:
    u16
    expr_to_reg(Expr &e, u16 reg);

    void
    discharge_vars(Expr &e);

    void
    discharge_reg(Expr &e, u16 reg);

    void
    load_bool(u16 reg, bool b, bool skip = false);

    void
    load_int(u16 reg, intr i);

    void
    load_real(u16 reg, real r);

    void
    push_reg(u16 reg_count);

    bool
    pop_reg(u16 reg);

// ========================================================================= }}}
// BYTECODE MANIPULATION =================================================== {{{
private:
    u32
    add_constant(TValue tv);

    i32
    code_ABC(OpCode Op, u16 A, u16 B, u16 C);

    i32
    code_AB0(OpCode Op, u16 A, u16 B) { return this->code_ABC(Op, A, B, 0); }

    i32
    code_vABC(OpCode Op, u16 A, u16 B, u16 vC, bool k);

    i32
    code_ABx(OpCode Op, u16 A, u32 Bx);

    i32
    code_AsBx(OpCode Op, u16 A, i32 sBx);

    i32
    code(Instruction i);

// ========================================================================= }}}
private:
    // Delegates error handling to the Parser.
    [[noreturn]] void
    error(char const *info, Expr const &e);
}; // struct Compiler

void
Compiler::finish()
{
    lulu_State *L     = this->L;
    Chunk &     chunk = this->chunk;
    this->implicit_return();

    i32 const      pc       = this->pc;
    Slice<RegInfo> reg_info = chunk.reg_info.slice();
    for (VarInfo v : this->slice_active_locals()) {
        reg_info[v.reg_info_index].pc_died = pc;
    }

    // Shrink chunk to fit.
    chunk.code.shrink(L);
    chunk.constants.shrink(L);
    chunk.reg_info.shrink(L);
}

// HIGH-LEVEL EXPR MANIPULATION ============================================ {{{
// CAST/CALL =============================================================== {{{

/*
 Description:
    Emits the bytecode needed to perform `cast(t)e`.
 */
void
Compiler::explicit_cast(Expr &restrict t, Expr &restrict arg)
{   
    LULU_ASSERT(t.kind == Expr_Type);
    // CHECK(2026-07-16): literal is typed or untyped
    LULU_ASSERT(arg.type != nullptr);

    Type const *type = t.type;

    // Nothing to do?
    if (arg.type == type) {
        return;
    }

    if (type->kind == TypeKind_Basic) {
        ValueKind k = type->basic.kind;
        switch (k) {
        case Value_bool:
        case Value_int:
        case Value_real:
            if (this->cast_basic_type(arg, k)) {
                return;
            }
        default:
            break;
        }
    }

    // Report the bad type, not the variable?
    arg.loc = t.loc;
    this->error("Cannot cast to type", arg);
}

bool
Compiler::cast_basic_type(Expr &e, ValueKind basic_kind)
{
    LULU_ASSERT(e.type != nullptr);

    // Check for supported expressions and discharge to registers as needed.
    switch (e.kind) {
    case Expr_Literal: return checker_cast_literal(&e, basic_kind);
    case Expr_Local:
    case Expr_Compare:
    case Expr_Pending:
        this->expr_any_reg(e);
        this->pop_expr(e);
        break;
    case Expr_Discharged:
        break;
    default:
        goto nodice;
    }

    // We should haae been discharged into a register by now.
    switch (basic_kind) {
    case Value_bool:
        if (!this->cast_bool(e)) {
            goto nodice;
        }
        break;
    case Value_int:
        if (!this->cast_int(e)) {
            goto nodice;
        }
        break;
    case Value_real:
        if (!this->cast_real(e)) {
            goto nodice;
        }
        break;
    default:
        LULU_UNREACHABLE();
        break;
    }
    e.type = basic_type_get(basic_kind);
    return true;

nodice:
    LULU_PANICF("Got ExprKind(%i)", e.kind);
    this->error("Cannot cast to type", e);
    return false;
}

bool
Compiler::cast_bool(Expr &e)
{
    u16    reg = e.get_reg();
    OpCode op;
    switch (e.type_get_basic_kind()) {
    case Value_bool: LULU_UNREACHABLE(); return false;
    case Value_int:  op = Op_eqi;        break;
    case Value_real: op = Op_feqi;       break;
    default:         return false;
    }
    // Comparison: R(A) == B
    e.set_compare(this->code_vABC(op, reg, 1, 0, true));
    return true;
}

bool
Compiler::cast_int(Expr &e)
{
    u16 reg = e.get_reg();
    switch (e.type_get_basic_kind()) {
    case Value_bool:
        e.set_pending(this->code_ABC(Op_bandi, REG_NONE, reg, 1));
        return true;
    case Value_int:  LULU_UNREACHABLE(); break;
    case Value_real:
        e.set_pending(this->code_AB0(Op_real2int, REG_NONE, reg));
        return true;
    default:
        break;
    }
    return false;
}

bool
Compiler::cast_real(Expr &e)
{
    u16 reg = e.get_reg();
    switch (e.type_get_basic_kind()) {
    // Since bool is just implemented in terms of int, use that conversion.
    case Value_bool:
    case Value_int:
        e.set_pending(this->code_AB0(Op_int2real, REG_NONE, reg));
        return true;
    case Value_real: LULU_UNREACHABLE(); break;
    default:         break;
    }
    return false;
}

/*
 Description:
    Emits the bytecode needed to perform `func(arg)`.
 */
void
Compiler::call(Expr &restrict func, Expr &restrict arg)
{
    if (func.kind == Expr_Type) {
        this->explicit_cast(func, arg);

        // Special case, because we generally want to use func as the
        // out-parameter.
        func = arg;
    } else {
        this->error("Function calls not yet supported", func);
    }
}

// ========================================================================= }}}
// UNARY =================================================================== {{{

/*
 Description:
    Emits the bytecode for `op e`.

 Arguments:
    op [in]
    e  [out]
 */
void
Compiler::unary(Token const &op, Expr &e)
{
    if (!this->unary_dispatch(op, e)) {
        this->error("Invalid unary operand", e);
    }
}

bool
Compiler::unary_dispatch(Token const &op, Expr &e)
{
    switch (op.kind) {
    case Token_Tilde: return this->unary_bnot(e);
    case Token_Dash:  return this->unary_neg(e);
    case Token_not:   return this->unary_not(e);
    default:
        break;
    }
    LULU_UNREACHABLE();
    return false;
}

bool
Compiler::unary_bnot(Expr &e)
{
    if (e.is_literal_intr()) {
        e.set_intr(~e.get_intr());
    } else {
        // Non-literal (e.g. discharged register) that is NOT of type `int`?
        if (!e.type_is_basic_kind(Value_int)) {
            return false;
        }
        u16 reg = this->expr_any_reg(e);
        e.set_pending(this->code_AB0(Op_bnot, REG_NONE, reg));
    }
    return true;
}

bool
Compiler::unary_neg(Expr &e)
{
    if (!checker_negate_expr(&e)) {
        u16    reg = this->expr_any_reg(e);
        OpCode op;
        if (e.type_is_basic()) switch (e.type_get_basic_kind()) {
        case Value_int:  op = Op_neg;  break;
        case Value_real: op = Op_fneg; break;
        default:         return false;
        }
        e.set_pending(this->code_ABC(op, REG_NONE, reg, 0));
    }
    return true;
}

bool
Compiler::unary_not(Expr &e)
{
    switch (e.kind) {
    case Expr_Literal:
        // Only boolean literals can have `not` applied to them.
        if (!e.type_is_basic_kind(Value_bool)) {
            return false;
        }
        e.set_bool(!e.literal.get_bool());
        return true;
    case Expr_Compare: {
        // E.g. `not (x == y)`
        Instruction *ip = &this->chunk.code[e.pc];
        bool const   k  = ip->k();
        ip->set_k(!k);
        return true;
    }
    default:
        break;
    }

    u16 reg = this->expr_any_reg(e);
    if (e.type_is_basic_kind(Value_bool)) {
        e.set_pending(this->code_ABC(Op_not, REG_NONE, reg, 0));
        return true;
    }
    return false;
}
// ========================================================================= }}}
// BINARY ================================================================== {{{

/*
 Description:
    Emits the bytecode for `lhs op rhs`.

 Arguments:
    op  [in]
    lhs [in, out] - The final output state (register or pc) goes here.
    rhs [in, out] - May be transformed, but does not store the main output.
 */
void
Compiler::binary(Token const &op, Expr &restrict lhs, Expr &restrict rhs)
{
    switch (checker_fold_binary(op, lhs, rhs)) {
    case Checker_Ok:
        lhs.loc = rhs.loc;
        return;
    case Checker_Cannot_Fold:
        break;
    case Checker_Divide_By_Zero:
        this->error("Cannot divide/modulo by 0", rhs);
        break;
    }

    auto r = checker_fix_binary(op, lhs, rhs);
    if (!r.ok) {
        // Leaky abstraction but who tf cares amirite
        if (r.op) {
            this->error("Invalid left hand side operand", lhs);
        } else {
            this->error("Inconsistent right hand side type", rhs);
        }
    }

    bool k = !r.is_not;
    if (lhs.is_literal() || rhs.is_literal()) {
        auto immr = this->binary_imm(r.op, lhs, rhs, k);
        if (immr.ok) {
            if (!immr.swapped) {
                // Propagate this change because we won't do it any place else.
                lhs.loc = rhs.loc;
            }
            return;
        }

        // If we didn't emit an immediate-addressed opcode, ensure we reset the
        // order of the operands to their original.
        if (immr.swapped) {
            swap(&lhs, &rhs);
        }
        
        auto kr = this->binaryk(r.op, lhs, rhs, k);
        if (kr.ok) {
            if (!kr.swapped) {
                lhs.loc = rhs.loc;
            }
            return;
        }
    }

    u16 r1 = lhs.get_reg();
    u16 r2 = this->expr_any_reg(rhs);
    if (r1 > r2) {
        this->pop_expr(lhs);
        this->pop_expr(rhs);
    } else {
        this->pop_expr(rhs);
        this->pop_expr(lhs);
    }

    if (r.is_compare) {
        /*
         Allows us to implement complements of the 3 basic comparison
         instructions. This is useful for both assignments and conditional 
         locks.

         0 = proceed label(true) if not result else goto label(false)
         1 = proceed label(true) if     result else goto label(false)
         */
        lhs.type  = basic_type_get(Value_bool);
        lhs.loc = rhs.loc;

        // R(A) is not a destination register here!
        lhs.set_compare(this->code_vABC(r.op, r1, r2, 0, k));
    } else {
        lhs.loc = rhs.loc;
        lhs.set_pending(this->code_ABC(r.op, REG_NONE, r1, r2));
    }
}

/*
 Assumptions:
 1) Both arguments are of the same underlying type.

 2) The opcodes and types we support immediate operations for are commutative.
    I.e. `x op y` has the same effect as `y op x`, which is only true for some
    operations.
 */
CompilerBinaryResult
Compiler::binary_imm(OpCode op, Expr &restrict lhs, Expr &restrict rhs, bool k)
{
    // If both are literals, then we should've folded them.
    // If they are both discharged or pending, we shouldn't have called this.
    LULU_ASSERT(lhs.is_literal() != rhs.is_literal());
    switch (op) {
    // For the bitwise operators, we assume that reals already caused
    // an error previously.
    case Op_band: return this->arithi  (Op_bandi, lhs, rhs);
    case Op_bor:  return this->arithi  (Op_bori,  lhs, rhs);
    case Op_bxor: return this->arithi  (Op_bxori, lhs, rhs);
    case Op_add:  return this->arithi  (Op_addi,  lhs, rhs);
    case Op_sub:  return this->arithi  (Op_subi,  lhs, rhs);
    case Op_eq:   return this->comparei(Op_eqi,   lhs, rhs, k);
    case Op_lt:   return this->comparei(Op_lti,   lhs, rhs, k);
    case Op_leq:  return this->comparei(Op_leqi,  lhs, rhs, k);
    case Op_fadd: return this->arithi  (Op_faddi, lhs, rhs);
    case Op_fsub: return this->arithi  (Op_fsubi, lhs, rhs);
    case Op_feq:  return this->comparei(Op_feqi,  lhs, rhs, k);
    case Op_flt:  return this->comparei(Op_flti,  lhs, rhs, k);
    case Op_fleq: return this->comparei(Op_fleqi, lhs, rhs, k);
    default:      return {};
    }
}

CompilerBinaryResult
Compiler::arithi(OpCode iop, Expr &restrict lhs, Expr &restrict rhs)
{
    auto r = checker_fix_arithi(iop, lhs, rhs);
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
        u16 reg = this->expr_any_reg(lhs);
        this->pop_expr(lhs);
        lhs.set_pending(this->code_ABC(iop, REG_NONE, reg, cast(u16)r.imm));
    }
    return {r.ok, r.swapped};
}

CompilerBinaryResult
Compiler::comparei(OpCode iop, Expr &restrict lhs, Expr &restrict rhs, bool k)
{
    auto r = checker_fix_comparei(iop, lhs, rhs, k);
    if (r.ok) {
        u16 reg = this->expr_any_reg(lhs);
        this->pop_expr(lhs);

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
        if (rhs.is_literal_bool()) {
            bool b = rhs.get_bool();
            if (b != k) {
                lhs.set_pending(this->code_ABC(Op_not, REG_NONE, reg, 0));
            }
        } else {
            lhs.set_compare(this->code_vABC(iop, reg, cast(u16)r.imm, 0, k));
        }
    }
    return {r.ok, r.swapped};
}

CompilerBinaryResult
Compiler::binaryk(OpCode op, Expr &restrict lhs, Expr &restrict rhs, bool k)
{
    LULU_ASSERT(lhs.is_literal() != rhs.is_literal());
    switch (op) {
    case Op_band:   return this->arithk  (Op_bandk, lhs, rhs);
    case Op_bor:    return this->arithk  (Op_bork,  lhs, rhs);
    case Op_bxor:   return this->arithk  (Op_bxork, lhs, rhs);
    case Op_add:    return this->arithk  (Op_addk,  lhs, rhs);
    case Op_sub:    return this->arithk  (Op_subk,  lhs, rhs);
    case Op_mul:    return this->arithk  (Op_mulk,  lhs, rhs);
    case Op_div:    return this->arithk  (Op_divk,  lhs, rhs);
    case Op_mod:    return this->arithk  (Op_modk,  lhs, rhs);
    case Op_eq:     return this->comparek(Op_eqk,   lhs, rhs, k);
    case Op_lt:     return this->comparek(Op_ltk,   lhs, rhs, k);
    case Op_leq:    return this->comparek(Op_leqk,  lhs, rhs, k);
    case Op_fadd:   return this->arithk  (Op_faddk, lhs, rhs);
    case Op_fsub:   return this->arithk  (Op_fsubk, lhs, rhs);
    case Op_fmul:   return this->arithk  (Op_fmulk, lhs, rhs);
    case Op_fdiv:   return this->arithk  (Op_fdivk, lhs, rhs);
    case Op_fmod:   return this->arithk  (Op_fmodk, lhs, rhs);
    case Op_feq:    return this->comparek(Op_feqk,  lhs, rhs, k);
    case Op_flt:    return this->comparek(Op_fltk,  lhs, rhs, k);
    case Op_fleq:   return this->comparek(Op_fleqk, lhs, rhs, k);
    default:        return {};
    }
}

CompilerBinaryResult
Compiler::arithk(OpCode kop, Expr &restrict lhs, Expr &restrict rhs)
{
    auto r = checker_fix_arithk(kop, lhs, rhs);
    if (r.ok) {
        // May be a temporary register.
        u16 reg = this->expr_any_reg(lhs);
        this->pop_expr(lhs);

        u32 i = this->add_constant(r.constant);
        // TODO(2026-09-13): Handle resolving their registers later on?
        rhs.kind     = Expr_Constant;
        rhs.constant = i;

        r.ok = (i <= ARG_C.MAX);
        if (r.ok) {
            lhs.set_pending(this->code_ABC(kop, REG_NONE, reg, cast(u16)i));
        }
    }
    return {r.ok, r.swapped};
}

CompilerBinaryResult
Compiler::comparek(OpCode kop, Expr &restrict lhs, Expr &restrict rhs, bool k)
{
    auto r = checker_fix_comparek(&kop, lhs, rhs, &k);
    if (r.ok) {
        u16 reg = this->expr_any_reg(lhs);
        this->pop_expr(lhs);

        u32 i = this->add_constant(r.constant);
        // TODO(2026-09-13): Handle resolving their registers later on?
        rhs.kind     = Expr_Constant;
        rhs.constant = i;

        r.ok = (i <= ARG_C.MAX);
        if (r.ok) {
            lhs.set_compare(this->code_vABC(kop, reg, cast(u16)i, 0, k));
        }
    }
    return {r.ok, r.swapped};

}
// ========================================================================= }}}

/*
 Description:
    Emits the bytecode for `return a, b, ...x`.
 */
void
Compiler::explicit_return(ExprList list)
{
    u16 start_reg, stop_reg;
    switch (list.count) {
    case 0:
        this->code_ABC(Op_return0, 0, 0, 0);
        return;
    case 1:
        start_reg = this->expr_any_reg(*list);
        stop_reg  = start_reg + 1;
        this->code_ABC(Op_return, start_reg, stop_reg, 0);
        return;
    default:
        break;
    }

    stop_reg = REG_NONE;
    for (Expr &e : list) {
        stop_reg = this->expr_next_reg(e);
    }

    stop_reg  += 1;
    start_reg  = stop_reg - cast(u16)list.count;
    this->code_ABC(Op_return, start_reg, stop_reg, 0);

    // The range is exclusive, so the actual last register is off-by-one.
    for (u16 reg = stop_reg; reg-- >= start_reg;) {
        pop_reg(reg);
    }
}

/*
 Description:
    Creating new local variables is defined into two (2) steps, and this is
    the first. We simply mark the existence of this local variable but don't
    consider it 'active'.

 Arguments:
    lhs [in, out] - Must contain the identifiers we wish to use.
 */
void
Compiler::declare_local(ExprList lhs_list)
{
    lulu_State *L = this->L;

    // Declare from left to right.
    i32   pc       = this->pc;
    auto &reg_info = this->chunk.reg_info;

    // Don't use a slice because we are going to write beyond the active length.
    // This declares new locals but does not yet mark them as 'active' to prevent
    // their identifiers from being evaluated as themselves in the assigning
    // expression/s.
    VarInfo *active_locals = this->active_locals;
    u16      reg           = this->active_locals_len;
    for (Expr &lhs : lhs_list) {
        if (lhs.kind != Expr_Local) {
            this->error("Unassignable target", lhs);
        }

        // If non-null then that means this variable already exists in some scope.
        // TODO(2026-09-05): Check scopes?
        if (lhs.type != nullptr) {
            this->error("Shadowing of variable", lhs);
        }

        reg_info.append(L, RegInfo{
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
            /*reg_info_index=*/cast(u32)(reg_info.len() - 1),
        };
    }
}

void
Compiler::define_local(ExprList lhs_list, ExprList rhs_list)
{
    LULU_ASSERT(lhs_list.count == rhs_list.count);

    Slice<RegInfo> reg_info      = this->chunk.reg_info.slice();
    VarInfo *      active_locals = this->active_locals;
    int            scope         = this->scope;
    u16            reg           = this->active_locals_len;
    for (Expr &rhs : rhs_list) {
        Expr &lhs = *lhs_list++;

        // Ensure both sides had type inference done by the parser.
        LULU_ASSERT(lhs.type != nullptr);
        LULU_ASSERT(rhs.type != nullptr);

        // We have an assigning expression but it's the wrong target type.
        if (lhs.type != rhs.type) {
            // Only literals that can be implicitly converted to the destination
            // type without any loss of data will pass this check.
            if (!checker_coerce_rhs(lhs, rhs)) {
                this->error("Invalid implicit cast", rhs);
            }
        }

        LULU_ASSERT(rhs.type == lhs.type);

        this->expr_next_reg(rhs);
        VarInfo &v = active_locals[reg++];
        v.scope    = scope;
        v.type     = lhs.type;
        reg_info[v.reg_info_index].type = lhs.type;
    }

    // The last register is the length.
    this->active_locals_len = reg;
}

/*
 Description:
    Does the equivalent of `lhs = rhs`, with very rudimentary type-coercion
    and strict type-checking.
 */
void
Compiler::assign(ExprList lhs_list, ExprList rhs_list)
{
    LULU_ASSERT(lhs_list.count == rhs_list.count);
    for (Expr &lhs : lhs_list) {
        Expr &rhs = *rhs_list++;
        if (lhs.type != rhs.type){
            if (!checker_coerce_rhs(lhs, rhs)) {
                this->error("Invalid implicit cast", rhs);
            }
        }

        switch (lhs.kind) {
        case Expr_Local:
            lhs.kind = Expr_Discharged;
            expr_to_reg(rhs, lhs.get_reg());
            break;
        default:
            this->error("Invalid assignment target", lhs);
            break;
        }
    }
}

// ========================================================================= }}}
// LOW-LEVEL EXPR MANIPULATION ============================================= {{{

bool
Compiler::pop_reg(u16 reg)
{
    if (reg >= this->active_locals_len) {
        return reg == --this->free_reg;
    }
    return true;
}

// Necessary so we can better report the location of assertions.
#define pop_reg(reg)                                                          \
    if (!(this->pop_reg)(reg)) {                                              \
        LULU_PANICF("Expected free_reg = %u, got %u", reg, (this)->free_reg); \
    }

void
Compiler::push_reg(u16 reg_count)
{
    // TODO(2026-07-07): Check stack size!
    this->free_reg += reg_count;
    if (this->free_reg > cast(u16)this->chunk.stack_size) {
        this->chunk.stack_size = cast(u8)this->free_reg;
    }
}

/*
 Description:
    If the given expression already has a register, then it is reused.
    Otherwise the expression is stored in the next available register.

 Returns:
    The register we stored the expression was stored in. Note that in the
    case it doesn't already have a register, it is modified in-place.
 */
u16
Compiler::expr_any_reg(Expr &e)
{
    this->discharge_vars(e);
    // Already have a register?
    if (e.is_reg()) {
        // TODO(2026-07-13): Handle jumps
        return e.get_reg();
    }
    return this->expr_next_reg(e);
}

/*
 Description:
    Unconditionally pushes the given expression to the next available register,
    erroring out if we exceed the maximum number of registers.

 Returns:
    The register we stored the expression in. Note that the expression is also
    modified in-place.
 */
u16
Compiler::expr_next_reg(Expr &e)
{
    this->discharge_vars(e);
    this->pop_expr(e);
    this->push_reg(1);
    return this->expr_to_reg(e, this->free_reg - 1);
}

/*
 Description:
    Frees the register used by the given expression. For simplicity, we require
    stack-like semantics. So the most recent register is to be popped, followed
    by the register right before that, etc.
 */
void
Compiler::pop_expr(Expr &e)
{
    if (e.kind == Expr_Discharged) {
        u16 reg = e.get_reg();
        pop_reg(reg);
    }
}

u16
Compiler::expr_to_reg(Expr &e, u16 reg)
{
    this->discharge_reg(e, reg);
    e.set_discharged(reg);
    return reg;
}

void
Compiler::discharge_vars(Expr &e)
{
    // TODO(2026-07-13): Differentiate globals and locals?
    switch (e.kind) {
    case Expr_Local:
        e.kind = Expr_Discharged;
        break;
    default:
        break;
    }
}

/*
 Description:
    Emits the code needed to retrieve the expression from the given register.
    This is 'discharging', indicating the expression has a finalized
    register it can refer to from here on.

 Analog:
    Lua 5.1.5: `lcode.c:discharge2reg (FuncState *fs, expdesc *e, int reg)`
 */
void
Compiler::discharge_reg(Expr &e, u16 reg)
{
    this->discharge_vars(e);
    switch (e.kind) {
    case Expr_Literal: 
        // CHECK(2026-07-16): Literal is typed or untyped
        LULU_ASSERT(e.type != nullptr);
        switch (e.get_literal_kind()) {
        case Value_bool: this->load_bool(reg, e.get_bool()); break;
        case Value_int:  this->load_int (reg, e.get_intr()); break;
        case Value_real: this->load_real(reg, e.get_real()); break;
        default:
            this->error("Unsupported literal", e);
            break;
        }
        break;
    case Expr_Discharged:
        // Moving to the same register is a no-op, e.g. `x := 1; x = x;`.
        if (reg != e.reg) {
            this->code_AB0(Op_move, reg, e.reg);
        }
        break;
    case Expr_Compare:
        this->load_bool(reg, true, /*skip=*/true);
        this->load_bool(reg, false);
        break;
    case Expr_Pending:
        this->chunk.code[e.get_pc()].set_A(reg);
        break;
    default:
        LULU_ASSERTF(!e.kind, "Got ExprKind(%i)", e.kind);
        break;
    }
    e.set_discharged(reg);
}

void
Compiler::load_bool(u16 reg, bool b, bool skip)
{
    this->code_vABC(Op_bool, reg, cast(u16)b, 0, skip);
}

void
Compiler::load_int(u16 reg, intr i)
{
    if (ARG_sBx.MIN <= i && i <= ARG_sBx.MAX) {
        this->code_AsBx(Op_int_imm, reg, cast(i32)i);
    } else {
        TValue tv = TValue::make_intr(i);
        u32    k  = this->add_constant(tv);
        this->code_ABx(Op_int_k, reg, k);
    }
}

void
Compiler::load_real(u16 reg, real r)
{
    TValue tv = TValue::make_real(r);
    u32    k  = this->add_constant(tv);
    this->code_ABx(Op_real, reg, k);
}

// ========================================================================= }}}
// BYTECODE MANIPULATION =================================================== {{{

u32
Compiler::add_constant(TValue tv)
{
    Chunk &chunk = this->chunk;
    auto   K     = chunk.constants;
    u32    n     = cast(u32)K.len();

    // Try to reuse an existing value.
    for (u32 i = 0; i < n; i++) {
        if (tv == K[i]) {
            return i;
        }
    }
    chunk.constants.append(this->L, tv);
    return n;
}

i32
Compiler::code_ABC(OpCode Op, u16 A, u16 B, u16 C)
{
    LULU_ASSERT(opcode_is_ABC(Op));
    LULU_ASSERT(A <= ARG_A.MAX);
    LULU_ASSERT(B <= ARG_B.MAX);
    LULU_ASSERT(C <= ARG_C.MAX);
    return this->code(Instruction::ABC(Op, A, B, C));
}

i32
Compiler::code_vABC(OpCode Op, u16 A, u16 B, u16 vC, bool k)
{
    LULU_ASSERT(opcode_is_vABC(Op));
    LULU_ASSERT(A <= ARG_A.MAX);
    LULU_ASSERT(B <= ARG_B.MAX);
    LULU_ASSERT(vC <= ARG_vC.MAX);
    return this->code(Instruction::vABC(Op, A, B, vC, cast(u8)k));
}

i32
Compiler::code_ABx(OpCode Op, u16 A, u32 Bx)
{
    LULU_ASSERT(opcode_is_ABx(Op));
    LULU_ASSERT(A  <= ARG_A.MAX);
    LULU_ASSERT(Bx <= ARG_Bx.MAX);
    return this->code(Instruction::ABx(Op, A, Bx));
}

i32
Compiler::code_AsBx(OpCode Op, u16 A, i32 sBx)
{
    LULU_ASSERT(opcode_is_AsBx(Op));
    LULU_ASSERT(A <= ARG_A.MAX);
    LULU_ASSERT(ARG_sBx.MIN <= sBx && sBx <= cast(i32)ARG_sBx.MAX);
    return this->code(Instruction::AsBx(Op, A, sBx));
}

i32
Compiler::code(Instruction i)
{
    Chunk &chunk = this->chunk;
    i32    index = this->pc++;
    chunk.code.append(this->L, i);
    return index;
}

// ========================================================================= }}}

#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "chunk.hpp"
#include "parser.hpp"
#include "expr.hpp"
#include "slice.hpp"

#define LOCALS_MAX_COUNT 0x10

/*
 Description:
    Represents the meta-information about a named register.
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

struct Compiler {
    // Shared state.
    lulu_State *L;
    Parser &    parser;

    // Compiler state.
    Chunk *  chunk;
    int      scope     = 0;
    i32      pc        = 0;
    u16      free_reg  = 0;

    // Track the list of currently active local variables. Their registers
    // match the indices to be used here.
    u16      active_locals_len = 0;
    VarInfo  active_locals[LOCALS_MAX_COUNT];

public:
    Compiler(lulu_State *L, Parser &parser, Chunk *chunk)
        : L{L}
        , parser{parser}
        , chunk{chunk}
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
    /*
     Description:
        Emits the bytecode needed to perform `cast(t)e`.
     */
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
    /*
     Description:
        Emits the bytecode needed to perform `func(arg)`.
     */
    void
    call(Expr &restrict func, Expr &restrict arg);

    /*
     Description:
        Emits the bytecode for `op e`.

     Arguments:
        op [in]
        e  [out]
     */
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
    /*
     Description:
        Emits the bytecode for `lhs op rhs`.

     Arguments:
        op  [in]
        lhs [in, out] - The final output state (register or pc) goes here.
        rhs [in, out] - May be transformed, but does not store the main output.
     */
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
    /*
     Description:
        Emits the bytecode for `return a, b, ...x`.
     */
    void
    explicit_return(ExprList list);

    void
    implicit_return() { this->explicit_return({}); }

    /*
     Description:
        Creating new local variables is defined into two (2) steps, and this is
        the first. We simply mark the existence of this local variable but don't
        consider it 'active'.

     Arguments:
        lhs [in, out] - Must contain the identifiers we wish to use.
     */
    void
    declare_local(ExprList lhs_list);

    void
    define_local(ExprList lhs_list, ExprList rhs_list);

    /*
     Description:
        Does the equivalent of `lhs = rhs`, with very rudimentary type-coercion
        and strict type-checking.
     */
    void
    assign(ExprList lhs_list, ExprList rhs_list);

// ========================================================================= }}}
// LOW-LEVEL EXPR MANIPULATION ============================================= {{{
public:
    /*
     Description:
        Unconditionally pushes the given expression to the next available register,
        erroring out if we exceed the maximum number of registers.

     Returns:
        The register we stored the expression in. Note that the expression is also
        modified in-place.
     */
    u16
    expr_next_reg(Expr &e);

    /*
     Description:
        If the given expression already has a register, then it is reused.
        Otherwise the expression is stored in the next available register.

     Returns:
        The register we stored the expression was stored in. Note that in the
        case it doesn't already have a register, it is modified in-place.
     */
    u16
    expr_any_reg(Expr &e);

    /*
     Description:
        Frees the register used by the given expression. For simplicity, we require
        stack-like semantics. So the most recent register is to be popped, followed
        by the register right before that, etc.
     */
    void
    expr_pop(Expr &e);

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
    reg_push(u16 reg_count);

    bool
    reg_pop(u16 reg);

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
    void
    error(char const *info, Expr const &e)
    {
        this->parser.error_at(info, e);
    }
}; // struct Compiler


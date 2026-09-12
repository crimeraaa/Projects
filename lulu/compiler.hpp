#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "chunk.hpp"
#include "parser.hpp"
#include "expr.hpp"

#define LOCALS_MAX_COUNT 0x10

/*
 Description:
    Represents the meta-information about a named register.
 */
struct VarInfo {
    Token       token;
    Type const *type;
    int         scope; // 0 indicates global scope.
    u32         reg_info_index;
};

struct Compiler {
    // Shared state.
    lulu_State *L      = nullptr;
    Parser *    parser = nullptr;

    // Compiler state.
    Chunk *  chunk     = nullptr;
    int      scope     = 0;
    i32      pc        = 0;
    u16      free_reg  = 0;

    // Track the list of currently active local variables. Their registers
    // match the indices to be used here.
    u16      active_locals_len = 0;
    VarInfo  active_locals[LOCALS_MAX_COUNT];
};

LULU_INTERNAL_FUNC void
compiler_finish(Compiler *c);

static inline Slice<VarInfo>
compiler_slice_active_locals(Compiler *c)
{
    return slice_array(c->active_locals, 0, c->active_locals_len);
}

// LOW-LEVEL EXPR MANIPULATION ============================================= {{{

/*
 Description:
    Unconditionally pushes the given expression to the next available register,
    erroring out if we exceed the maximum number of registers.

 Returns:
    The register we stored the expression in. Note that the expression is also
    modified in-place.
 */
LULU_INTERNAL_FUNC u16
compiler_expr_next_reg(Compiler *c, Expr *e);


/*
 Description:
    If the given expression already has a register, then it is reused.
    Otherwise the expression is stored in the next available register.

 Returns:
    The register we stored the expression was stored in. Note that in the
    case it doesn't already have a register, it is modified in-place.
 */
LULU_INTERNAL_FUNC u16
compiler_expr_any_reg(Compiler *c, Expr *e);

/*
 Description:
    Frees the register used by the given expression. For simplicity, we require
    stack-like semantics. So the most recent register is to be popped, followed
    by the register right before that, etc.
 */
LULU_INTERNAL_FUNC void
compiler_expr_pop(Compiler *c, Expr *e);

// ========================================================================= }}}

// HIGH-LEVEL EXPR MANIPULATION ============================================ {{{

/*
 Description:
    Emits the bytecode needed to perform `cast(t)e`.
 */
LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, Expr *restrict t, Expr *restrict arg);

/*
 Description:
    Emits the bytecode needed to perform `func(arg)`.
 */
LULU_INTERNAL_FUNC void
compiler_call(Compiler *c, Expr *restrict func, Expr *restrict arg);

/*
 Description:
    Emits the bytecode for `op e`.

 Arguments:
    op [in]
    e  [out]
 */
LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, Token const &op, Expr *e);

/*
 Description:
    Emits the bytecode for `lhs op rhs`.

 Arguments:
    op_token  [in]
    lhs [in, out] - The final output state (register or pc) goes here.
    rhs [in, out] - May be transformed, but does not store the main output.
 */
LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c,
    Token const &  op_token,
    Expr *restrict lhs,
    Expr *restrict rhs);


/*
 Description:
    Emits the bytecode for `return a, b, ...x`.
 */
LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, ExprList list);

/*
 Description:
    Creating new local variables is defined into two (2) steps, and this is
    the first. We simply mark the existence of this local variable but don't
    consider it 'active'.

 Arguments:
    lhs [in, out] - Must contain the identifiers we wish to use.
 */
LULU_INTERNAL_FUNC void
compiler_declare_local(Compiler *c, ExprList lhs_list);

LULU_INTERNAL_FUNC void
compiler_define_local(Compiler *c, ExprList lhs_list, ExprList rhs_list);

/*
 Description:
    Does the equivalent of `lhs = rhs`, with very rudimentary type-coercion
    and strict type-checking.
 */
LULU_INTERNAL_FUNC void
compiler_assign(Compiler *c, ExprList lhs_list, ExprList rhs_list);

// ========================================================================= }}}

